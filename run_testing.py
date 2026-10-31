import argparse
from contextlib import contextmanager, nullcontext
import csv
from datetime import datetime
import fcntl
import math
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

import yaml
from google.protobuf.struct_pb2 import ListValue


ROOT = Path(__file__).resolve().parent
TESTING_CONFIG = ListValue.FromString(
    (ROOT / "verifyDataBase/inputTemplates/autopilot_testing.tmp").read_bytes())
POLICY_CONFIG_KEYS = {
    item.list_value.values[0].string_value: item.list_value.values[1].string_value
    for item in TESTING_CONFIG.values
}
POLICY_INPUT_SKIPS = {
    item.list_value.values[0].string_value: {
        int(rule.list_value.values[0].number_value): tuple(
            value.string_value for value in rule.list_value.values[1].list_value.values)
        for rule in item.list_value.values[2].list_value.values
    }
    for item in TESTING_CONFIG.values if len(item.list_value.values) > 2
}
FIELDS = [
    "policy_id", "depth", "result", "locations", "transitions",
    "model_time_s", "testing_time_s", "inputs", "status", "error",
]
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ERROR = re.compile(
    r"\[ERROR\]|\b(?:error:|syntax error|type error|fatal error|segmentation fault)"
    r"|Traceback \(most recent call last\)", re.IGNORECASE,
)


def model_worker(output):
    from pyscript.modelTrans import llvmtouppaal

    config = yaml.safe_load((ROOT / "configs/IR_config.yml").read_text())
    bounds = None
    if config.get("isTime"):
        from pyscript.initializer.parseTimeConstraint import getTimeBounds

        logs = sorted((ROOT / "verifyDataBase/model_inputs/runtime_data" /
                       config["property"]).glob(config["property"] + "_Time_*.in"))
        if not logs:
            raise FileNotFoundError("No runtime timing data for " + config["property"])
        print("Timing source:", logs[0], "block_size=100, alpha=0.98", flush=True)
        bounds = getTimeBounds(str(logs[0]), block_size=100, alpha=0.98)
    llvmtouppaal.run(str(ROOT / "InterpreterR/build/libbase.so"), str(output), bounds)


def stop_process(process):
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            process.wait()
            return
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()


def run_process(command, log, env, cwd, timeout):
    started = time.monotonic()
    if timeout <= 0:
        return None, "Timeout", 0.0
    with (log.open("a") if isinstance(log, Path) else nullcontext(log)) as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                   cwd=cwd, env=env, start_new_session=True)
        try:
            while process.poll() is None:
                remaining = timeout - (time.monotonic() - started)
                if remaining <= 0:
                    return None, "Timeout", time.monotonic() - started
                try:
                    process.wait(timeout=min(remaining, 0.2))
                except subprocess.TimeoutExpired:
                    pass
            return process.returncode, "Finished", time.monotonic() - started
        finally:
            stop_process(process)


def error_detail(text):
    lines = [line.strip() for line in ANSI.sub("", text).splitlines() if line.strip()]
    for line in reversed(lines):
        if re.match(r"[\w.]*(?:Error|Exception):", line) or "[ERROR]" in line:
            return line
    return lines[-1] if lines else "No diagnostic output"


@contextmanager
def testing_lock(path):
    descriptor = os.open(path, os.O_RDONLY)
    try:
        yield descriptor
    finally:
        os.close(descriptor)


@contextmanager
def testing_environment(env, key, inputs, skipped):
    if not skipped:
        yield env
        return
    with tempfile.TemporaryDirectory(prefix="rvat-testing-") as temporary:
        stage = Path(temporary)
        for entry in ROOT.iterdir():
            if entry.name != "verifyDataBase":
                (stage / entry.name).symlink_to(entry)
        database = stage / "verifyDataBase"
        database.mkdir()
        for entry in (ROOT / "verifyDataBase").iterdir():
            if entry.name != "model_inputs":
                (database / entry.name).symlink_to(entry)
        input_root = database / "model_inputs"
        input_root.mkdir()
        for entry in (ROOT / "verifyDataBase/model_inputs").iterdir():
            if entry.name != key:
                (input_root / entry.name).symlink_to(entry)
        selected = input_root / key
        selected.mkdir()
        for source in inputs:
            if source.name not in skipped:
                (selected / source.name).symlink_to(source)
        yield dict(env, DTMC=str(stage))


def run_policy(row, config, formula, args, env, directory, skip_testing=False):
    key = POLICY_CONFIG_KEYS[row["policy_id"]]
    if not config or not formula or not formula.get("uppaal_formula"):
        raise ValueError("Missing property configuration or UPPAAL formula")
    if not (ROOT / "verifyDataBase/func_slice" / (key + "_Slice_FS.yml")).is_file():
        raise FileNotFoundError("Missing function slice for " + key)
    active = {"property": key, **config, "call_depth": args.depth}
    config_text = yaml.safe_dump(active, sort_keys=False, allow_unicode=True)
    (ROOT / "configs/IR_config.yml").write_text(config_text)
    (directory / "IR_config.yml").write_text(config_text)
    (directory / "formula.yml").write_text(yaml.safe_dump(formula, sort_keys=False))
    model = directory / "model.xml"
    started = time.monotonic()
    with tempfile.TemporaryFile(mode="w+t", encoding="utf-8", errors="replace") as log:
        try:
            code, state, _ = run_process(
                [sys.executable, str(Path(__file__).resolve()), "--model-worker", str(model)],
                log, env, ROOT, args.model_timeout)
        finally:
            row["model_time_s"] = round(time.monotonic() - started, 3)
        log.seek(0)
        text = ANSI.sub("", log.read())
    if code != 0 or state != "Finished" or not model.is_file():
        if state == "Timeout":
            row["status"] = "ModelTimeout"
        raise RuntimeError(f"Model generation {state}, exit={code}: {error_detail(text)}")
    if ERROR.search(text):
        raise RuntimeError("Model generation reported an error: " + error_detail(text))
    root = ET.parse(model).getroot()
    row["locations"] = len(root.findall("./template/location"))
    row["transitions"] = len(root.findall("./template/transition"))
    queries = root.findall("./queries/query/formula")
    if not row["locations"] or not queries or any(not (q.text or "").strip() for q in queries):
        raise ValueError("Generated model has no locations or contains an empty query")
    inputs = sorted((ROOT / "verifyDataBase/model_inputs" / key).glob("*.mi"))
    row["inputs"] = len(inputs)
    skipped = set(POLICY_INPUT_SKIPS.get(row["policy_id"], {}).get(args.depth, ()))
    unknown = skipped - {path.name for path in inputs}
    if unknown:
        raise ValueError("Configured skipped inputs do not exist: " + ", ".join(sorted(unknown)))
    if skipped and len(skipped) == len(inputs):
        raise ValueError("Input selection leaves no inputs to verify")
    active_model = ROOT / "verifyDataBase/uppaal_models" / key / (key + ".xml")
    active_model.parent.mkdir(parents=True, exist_ok=True)
    previous_model = active_model.read_bytes() if active_model.exists() else None
    started = time.monotonic()
    with tempfile.TemporaryFile(mode="w+t", encoding="utf-8", errors="replace") as log:
        try:
            shutil.copy2(model, active_model)
            with testing_environment(env, key, inputs, skipped) as testing_env:
                code, state, _ = run_process(
                    [str(ROOT / "run_verify.sh"), "-u", "verify"],
                    log, testing_env, ROOT, args.testing_timeout)
        finally:
            row["testing_time_s"] = round(time.monotonic() - started, 3)
            if previous_model is None:
                active_model.unlink(missing_ok=True)
            else:
                active_model.write_bytes(previous_model)
            log.seek(0)
            text = ANSI.sub("", log.read())
    if state == "Timeout":
        row.update(result="Timeout", status="Timeout", error="Testing command exceeded the policy timeout")
        return
    if code != 0 or state != "Finished" or ERROR.search(text):
        detail = error_detail(text)
        raise RuntimeError(f"Original testing entry exit={code}: {detail}")
    outcome = re.search(r"Verification result: Property (\w+) is (NOT )?Satisfied\.", text)
    if not outcome or outcome.group(1) != key:
        raise RuntimeError("Original testing entry produced no matching result: " + error_detail(text))
    elapsed = re.search(r"Verification cost time: ([\d.]+) seconds", text)
    if elapsed:
        row["testing_time_s"] = float(elapsed.group(1))
    row.update(result="✗" if outcome.group(2) else "✓", status="Complete")


def positive(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def interrupt(signum, frame):
    raise KeyboardInterrupt()


def read_results(output, depth=None):
    with (output / "results.csv").open(newline="") as stream:
        reader = csv.DictReader(stream)
        if not set(FIELDS).issubset(reader.fieldnames or []):
            raise ValueError("results.csv has an unexpected header")
        rows = {}
        for row in reader:
            if (not row.get("policy_id") or row.get("depth") not in {"3", "4", "5"}
                    or None in row.values() or None in row
                    or (depth is not None and row["depth"] != str(depth))):
                raise ValueError("results.csv contains an invalid depth record")
            rows[row["policy_id"]] = {field: row[field] for field in FIELDS}
        if len({row["depth"] for row in rows.values()}) > 1:
            raise ValueError("results.csv contains mixed depths")
        return rows


def write_results(output, rows):
    order = {policy: index for index, policy in enumerate(POLICY_CONFIG_KEYS)}
    temporary = output / "results.csv.tmp"
    try:
        with temporary.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(rows[policy] for policy in sorted(
                rows, key=lambda policy: (
                    not policy.startswith(("PX.", "PX_")),
                    order.get(policy, len(order)),
                )))
        temporary.replace(output / "results.csv")
    finally:
        temporary.unlink(missing_ok=True)


def read_testing_schedule(depth):
    """Read per-policy [depth, rank, skip_testing] rules from the template."""
    return {
        item.list_value.values[0].string_value: (
            int(rule.list_value.values[1].number_value),
            rule.list_value.values[2].bool_value,
        )
        for item in TESTING_CONFIG.values
        for rule in item.list_value.values[3].list_value.values
        if rule.list_value.values[0].number_value == depth
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Process policies sequentially at depth 3, 4, or 5 using the original testing entry.")
    parser.add_argument("--depth", type=int, choices=(3, 4, 5), help="Inlining depth (default: 3, or CSV depth on resume)")
    parser.add_argument("--testing-timeout", type=positive, default=86400, help="Seconds for the entire input batch per policy (default: 86400)")
    parser.add_argument("--model-timeout", type=positive, default=3600, help="Seconds for model generation per policy (default: 3600)")
    destination = parser.add_mutually_exclusive_group()
    destination.add_argument("--output", type=Path, help="New output directory; default: verifyDataBase/results/depth<depth>/<timestamp>")
    destination.add_argument("--resume", type=Path, help="Existing output directory; run only policies without a CSV record")
    args = parser.parse_args(argv)
    if args.resume:
        args.resume = args.resume.resolve()
        try:
            saved_rows = read_results(args.resume)
            saved_depth = next((int(row["depth"]) for row in saved_rows.values()), None)
            if saved_depth is None and args.resume.parent.name in {"depth3", "depth4", "depth5"}:
                saved_depth = int(args.resume.parent.name[-1])
            if args.depth is not None and saved_depth is not None and args.depth != saved_depth:
                raise ValueError("Requested depth does not match the CSV depth")
            if args.depth is None:
                args.depth = saved_depth
        except (OSError, ValueError, KeyError) as error:
            parser.error(f"Cannot resume {args.resume}: {error}")
    available = list(POLICY_CONFIG_KEYS)
    if args.depth is None:
        args.depth = 3
    if not available:
        parser.error("No public policy IDs found")
    policies = list(dict.fromkeys(available))
    unknown = set(policies) - POLICY_CONFIG_KEYS.keys()
    if unknown:
        parser.error("Unknown public policy IDs: " + ", ".join(sorted(unknown)))
    execution_order = policies
    testing_skips = set()
    if args.depth in (4, 5):
        try:
            schedule = read_testing_schedule(args.depth)
            execution_order = sorted(policies, key=lambda policy: schedule[policy][0])
            testing_skips = {
                policy for policy in policies if schedule[policy][1]
            }
        except (IndexError, ValueError, KeyError) as error:
            parser.error(f"Cannot read testing schedule from autopilot_testing.tmp: {error}")
    properties = yaml.safe_load((ROOT / "configs/properties_config.yml").read_text())
    formulas = yaml.safe_load((ROOT / "configs/verifications.yml").read_text())
    results_root = ROOT / "verifyDataBase/results"
    results_root.mkdir(parents=True, exist_ok=True)
    output = (args.resume or args.output or results_root / f"depth{args.depth}" /
              datetime.now().strftime("%Y%m%d_%H%M%S_%f")).resolve()
    config_path = ROOT / "configs/IR_config.yml"
    env = dict(os.environ, DTMC=str(ROOT), PYTHONUNBUFFERED="1")
    env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", "")
    for name in ("ALL_PROXY", "all_proxy", "HTTP_PROXY", "http_proxy", "HTTPS_PROXY", "https_proxy"):
        if env.get(name, "").startswith("socks://"):
            env[name] = "socks5://" + env[name][len("socks://"):]
    env["PYTHONPATH"] = os.pathsep.join(filter(None, [str(ROOT), str(ROOT / "pyscript/initializer"),
                                                    env.get("PYTHONPATH")]))
    with testing_lock(results_root) as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error("Another testing batch is already using the shared IR configuration")
        try:
            rows = read_results(output, args.depth) if args.resume else {}
        except (OSError, ValueError) as error:
            parser.error(f"Cannot read saved results: {error}")
        pending = [p for p in policies if p not in rows]
        skipped = len(policies) - len(pending)
        original = config_path.read_bytes() if config_path.exists() else None
        if not args.resume:
            output.mkdir(parents=True, exist_ok=False)
            write_results(output, rows)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        print("Output:", output, flush=True)
        print(f"Policies: {len(policies)} (sequential), depth: {args.depth}", flush=True)
        if args.resume:
            print(f"Resume: skip {skipped} recorded policies; run {len(pending)} policies", flush=True)
        print("Testing entry: ./run_verify.sh -u verify (original defaults)", flush=True)
        failed = False
        previous_sigterm = signal.signal(signal.SIGTERM, interrupt)
        try:
            api = ROOT / "verify-api.py"
            if api.is_file() and any(policy not in testing_skips for policy in pending):
                python = shutil.which("python3", path=env.get("PATH")) or sys.executable
                try:
                    preflight = subprocess.run(
                        [python, str(api), "--help"], stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, errors="replace",
                        env=env, cwd=ROOT, timeout=60)
                    detail = error_detail(preflight.stdout) if preflight.returncode != 0 else ""
                except subprocess.TimeoutExpired:
                    detail = "Startup check exceeded 60 seconds"
                if detail:
                    print(f"Original testing entry cannot start: {detail}", file=sys.stderr)
                    print(f"Python: {python}", file=sys.stderr)
                    return 1
            for index, policy in enumerate(execution_order, 1):
                if policy not in pending:
                    continue
                key = POLICY_CONFIG_KEYS[policy]
                name = policy.replace(".", "_") if list(POLICY_CONFIG_KEYS.values()).count(key) > 1 else key
                directory = output / name
                if directory.exists():
                    directory = directory / ("retry_" + stamp)
                directory.mkdir(parents=True)
                row = dict.fromkeys(FIELDS, "")
                row.update(policy_id=policy, depth=args.depth, result="Incomplete", status="Incomplete")
                print(f"[{index}/{len(policies)}] {policy}, depth={args.depth}", flush=True)
                interrupted = False
                try:
                    run_policy(row, properties.get(key), formulas.get(key), args, env, directory,
                               skip_testing=policy in testing_skips)
                except KeyboardInterrupt:
                    row.update(status="Interrupted", error="Stopped by user")
                    row["result"] = "Incomplete"
                    interrupted = True
                except Exception as error:
                    row["error"] = f"{type(error).__name__}: {error}"
                rows[policy] = row
                write_results(output, rows)
                print(f"  {row['result']} / {row['status']}: {row['error']}", flush=True)
                failed |= row["status"] != "Complete"
                if interrupted:
                    return 130
        finally:
            if original is None:
                config_path.unlink(missing_ok=True)
            else:
                config_path.write_bytes(original)
            signal.signal(signal.SIGTERM, previous_sigterm)
        print("Summary:", output / "results.csv", flush=True)
        total_testing_time = sum(float(row["testing_time_s"] or 0) for row in rows.values())
        print(f"Total testing time: {total_testing_time:.3f} seconds ({total_testing_time / 3600:.3f} hours)", flush=True)
        return 1 if failed else 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--model-worker":
        model_worker(Path(sys.argv[2]))
    else:
        sys.exit(main())
