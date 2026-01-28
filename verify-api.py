import yaml
import click
import os, sys, re
import pexpect
import time
from pyscript.pyTools.utils import *
from pyscript.pyTools.parseConfig import *
# from pyscript.pyAutoFlightControl.UAV_simulation import run_collector
from pyscript.modelTrans.llvmtouppaal import *
from ctypes import cdll
# Global configuration for Raspberry Pi connection
RASPI_HOST = 'raspi.local'
RASPI_USER = 'pi'
RASPI_SSH_TARGET = f'{RASPI_USER}@{RASPI_HOST}'

@click.group()
def cli():
    """Flight Control Model Checking Tool"""
    pass

##################### prepare #######################
'''
Generate IR for verification.
'''
from pyscript.pyTools.flightControlBuilder import uav_ir, uav_build, ir_build
cli.add_command(uav_ir) 
cli.add_command(uav_build)
# cli.add_command(gen_ir)
# cli.add_command(ir_build)

'''
Generate template files.
'''
from pyscript.pyTools.createTemplates import tempsGen
cli.add_command(tempsGen)

###################### modeling #######################
'''
Create the IR_config.yml configuration file for the property to be verified.
'''
from pyscript.pyTools.createIRConfig import create_IR_config
cli.add_command(create_IR_config)

'''
Generating Function Level Slices with llvm-pdg —— 'property_name'_Slice_FS.yml
'''
from pyscript.pyTools.parseConfig import parse_IR_config
ir_config = parse_IR_config()
target_ir = os.path.join(DTMC, ir_config.LLVM_IR) if ir_config else None

@click.command(name="slicing")
@click.option('-i', '--ir', type=str, default=target_ir, help='Path to the LLVM IR file')
@click.option('-o', '--output', type=str, default=os.path.join(DTMC, 'verifyDataBase', 'func_slice'), help='Output directory for the function slices')
def slicing(ir, output):
    """
    Generate function level slices from the LLVM IR file.
    """
    if not os.path.exists(ir):
        ERROR(f"LLVM IR file {ir} does not exist.")
        return
    if not os.path.exists(output):
        os.makedirs(output)
    
    llvm_pdg_path = os.path.join(DTMC, "llvm-pdg", "build", "llvm-pdg")
    if not os.path.exists(llvm_pdg_path):
        ERROR(f"llvm-pdg tool not found at {llvm_pdg_path}")
        return
    
    cmd = f"{llvm_pdg_path} {ir} -o {output}"

    try:
        INFO(f"Running command: {cmd}")
        
        child = pexpect.spawn(cmd)
        child.timeout = None
        child.expect(pexpect.EOF)
        
        output_text = child.before.decode('utf-8')
        
        INFO(f"Command completed. Output:")
        if output_text:
            INFO(f"{output_text}")
        else:
            INFO("No output generated")
    except pexpect.exceptions.ExceptionPexpect as e:
        ERROR(f"Failed to execute command: {e}")
    except Exception as e:
        ERROR(f"Unexpected error: {e}")

cli.add_command(slicing)

'''
Generate the UPPAAL model from the LLVM IR file.
'''
import pyscript.modelTrans.llvmtouppaal as llvmtouppaal
from pyscript.initializer.parseTimeConstraint import *
def run_gen_uppaal_model():
    """
    Generate the UPPAAL model from the LLVM IR file.
    """
    output = os.path.join(DTMC, 'verifyDataBase', 'uppaal_models', ir_config.property)
    if not os.path.exists(output):
        os.makedirs(output)
    
    model_file = os.path.join(output, ir_config.property + '.xml') if ir_config and ir_config.property else None
    
    base_lib = os.path.join(DTMC, 'InterpreterR', 'build', 'libbase.so')

    isTime = ir_config.isTime
    if isTime:
        INFO("Parsing time constraints from logged data...")
        time_log_dir = os.path.join(DTMC, 'verifyDataBase', 'model_inputs', 'runtime_data', ir_config.property)
        if not os.path.exists(time_log_dir):
            ERROR(f"Time log directory {time_log_dir} does not exist.")
            return
        
        # Find files matching pattern: {property}_Time_{digits}.in
        import glob
        time_log_pattern = os.path.join(time_log_dir, f'{ir_config.property}_Time_*.in')
        time_log_files = glob.glob(time_log_pattern)
        
        if not time_log_files:
            ERROR(f"No time log files found matching pattern: {time_log_pattern}")
            return
        
        # Use the first matching file (or you could sort and use the latest)
        timeLogPath = time_log_files[0]
        INFO(f"Using time log file: {timeLogPath}")
        timeConstraints = getTimeBounds(timeLogPath, block_size=100, alpha=0.98)
    
    try:
        llvmtouppaal.run(base_lib, model_file, timeConstraints if isTime else None)
        INFO(f"UPPAAL model {model_file} generated successfully!")
    except Exception as e:
        ERROR(f"Failed to generate UPPAAL model: {e}")
@click.command(name="gen_uppaal_model")
def gen_uppaal_model():
    run_gen_uppaal_model()

cli.add_command(gen_uppaal_model)

'''
Instrument the IR for subsequent data collection.
'''
@click.command(name="instrument")
def instrument():
    """
    Instrument the LLVM IR for data collection.
    """
    isTime = ir_config.isTime if ir_config else False
    lib_path = os.path.join(DTMC, 'InterpreterR', 'build', 'libbase.so')
    if not os.path.exists(lib_path):
        ERROR(f"libbase.so not found at {lib_path}")
        return
    lib = cdll.LoadLibrary (lib_path)

    lib.buildCFAModels.restype = None
    lib.buildCFAModels.argtypes = None
    lib.printModule.restype = None
    lib.printModule.argtypes = None
    lib.buildCFAModels()
    lib.printModule()

    # TODO: Instrument the IR
    instrumenter_path = os.path.join(DTMC, 'instrumenter', 'build', 'instrumenter')
    # target_callsites = os.path.join(DTMC, 'verifyDataBase', 'uppaal_models', ir_config.property, ir_config.property + '_noInlineCalls.bin')
    if not os.path.exists(instrumenter_path):
        ERROR(f"Instrumenter tool not found at {instrumenter_path}")
        return
    # if not os.path.exists(target_callsites):
    #     ERROR(f"Target callsites file {target_callsites} does not exist.")
    #     # ERROR(f"Please run the 'gen_uppaal_model' command first to generate the callsites file.")
    #     return
    cmd = f"{instrumenter_path}"
    
    if isTime:
        # bb_to_timer = os.path.join(DTMC, 'verifyDataBase', 'uppaal_models', ir_config.property, ir_config.property + '_bbNums.bin')
        cmd2 = f"{instrumenter_path} -t"

    try:
        child = pexpect.spawn(cmd)
        child.timeout = None
        child.expect(pexpect.EOF)
        
        output_text = child.before.decode('utf-8')
        
        # INFO(f"Command completed. Output:")
        if output_text:
            print(f"{output_text}")
        else:
            INFO("No output generated")
                
        if isTime: 
            child2 = pexpect.spawn(cmd2)
            child2.timeout = None
            child2.expect(pexpect.EOF)
            output_text2 = child2.before.decode('utf-8')
            
            # INFO(f"Time instrumentation command completed. Output:")
            if output_text2:
                print(f"{output_text2}")
            # else:
            #     INFO("No output generated for time instrumentation")
    except pexpect.exceptions.ExceptionPexpect as e:
        ERROR(f"Failed to execute command: {e}")
    except Exception as e:
        ERROR(f"Unexpected error: {e}")

cli.add_command(instrument)

'''
Build the LLVM IR.
'''
@click.command(name="ir_build")
def ir_build_with_config():
    """
    Build the LLVM IR.
    """
    if not ir_config or not ir_config.property:
        ERROR("ir_config or property is not properly configured.")
        return
    
    property = ir_config.property
    ir_filename = ir_config.LLVM_IR.split('/')[-1] if '/' in ir_config.LLVM_IR else ir_config.LLVM_IR
    base_filename = ir_filename.rsplit('.', 1)[0] if '.' in ir_filename else ir_filename
    INFO(f"Building LLVM IR for property: {property}")
    target_ir = os.path.join(DTMC, 'verifyDataBase', 'ir_and_elf', property, f'{base_filename}_memdump.ll')
    if not os.path.exists(target_ir):
        ERROR(f"LLVM IR file {target_ir} does not exist.")
        return
    ir_build.callback(target_ir, property, False)
    if ir_config.isTime:
        INFO("Building LLVM IR with timer instrumentation.")
        target_ir = os.path.join(DTMC, 'verifyDataBase', 'ir_and_elf', property, f'{base_filename}_timer.ll')
        if not os.path.exists(target_ir):
            ERROR(f"LLVM IR file {target_ir} does not exist.")
            return
        ir_build.callback(target_ir, property, True)
cli.add_command(ir_build_with_config)

'''
Run the UAV simulation.
'''
# cli.add_command(run_collector)
@click.command(name="run_sim")
def run_sim():
    if not ir_config:
        ERROR("IR config not loaded. Please ensure IR_config.yml exists and is valid.")
        return False
    curr_flightControl = ir_config.flightControl
    if curr_flightControl not in ['ArduCopter', 'PX4']:
        ERROR(f"Unsupported flight control: {curr_flightControl}. Supported types are 'ArduCopter' and 'PX4'.")
        return False
    INFO(f"Running UAV simulation for flight control: {curr_flightControl}")
    if curr_flightControl == 'ArduCopter':
         # mavproxy.py --master tcp:raspi.local:5760 --out raspi.local:14550 --out "127.0.0.1:14551" --console --map
        mavproxy_cmd = "mavproxy.py \
                        --master tcp:raspi.local:5760 \
                        --out raspi.local:14550 \
                        --out '127.0.0.1:14551' \
                        --console \
                        --map"
        print(f"Starting MAVProxy...")
        child = pexpect.spawn('/bin/bash', ['-lc', mavproxy_cmd], encoding='utf-8')
        child.timeout = None
        child.interact()
    else:

        # jmavsim.launch
        # if need start jmavsim without GUI, run `export HEADLESS=1`
        jmavsim_cmd = f"{DTMC + '/flight-control/PX4-1.15.2/Tools/simulation/jmavsim/jmavsim_run.sh'} -u -p 14577 -q -r 250"
        print(f"Starting jMAVSim...")
        child = pexpect.spawn('/bin/bash', ['-lc', jmavsim_cmd], encoding='utf-8')
        child.timeout = None
        child.interact()

cli.add_command(run_sim)

'''
Create model input files for verification.
'''
from pyscript.initializer.initModelInputSeed import *
cli.add_command(create_input_seeds)

'''
Instrument KLEE entry function.
'''
@click.command(name="instrument_klee_entry")
def instrument_klee_entry():
    start_time = time.time()
    if not ir_config or not ir_config.property:
        ERROR("ir_config or property is not properly configured.")
        return
    property = ir_config.property
    ir_filename = ir_config.LLVM_IR.split('/')[-1] if '/' in ir_config.LLVM_IR else ir_config.LLVM_IR
    base_filename = ir_filename.rsplit('.', 1)[0] if '.' in ir_filename else ir_filename
    INFO(f"Instrument KLEE entry function for property: {property}")
    model_inputs_dir = DTMC + '/verifyDataBase/model_inputs/' + property
    if not os.path.exists(model_inputs_dir):
        os.makedirs(model_inputs_dir)

    seed_files = [f for f in os.listdir(model_inputs_dir) if f.startswith(f'{property}_seed') and f.endswith('.mi')]
    if not seed_files:
        ERROR(f"No seed file found for property {property} in {model_inputs_dir}")
        return
    seed_file = os.path.join(model_inputs_dir, seed_files[0])
    INFO(f"Using seed file: {seed_file}")
    klee_helper_dir = DTMC + "/KLEE/build"
    if not os.path.exists(klee_helper_dir):
        ERROR(f"KLEE helper directory {klee_helper_dir} does not exist. Please build it first.")
        return
    klee_helper = os.path.join(klee_helper_dir, "klee_helper")
    if not os.path.exists(klee_helper):
        ERROR(f"KLEE helper {klee_helper} does not exist. Please build it first.")
        return
    if ir_config.flightControl == 'ArduCopter':
        elf_dir = DTMC + '/verifyDataBase/ir_and_elf/' + property + '/arducopter_' + property
    elif ir_config.flightControl == 'PX4':
        elf_dir = DTMC + '/verifyDataBase/ir_and_elf/' + property + '/px4_' + property

    if not os.path.exists(elf_dir):
        ERROR(f"ELF file {elf_dir} does not exist.")
        return
    INFO(f"Using ELF file: {elf_dir}")
    cmd = f"{klee_helper} --seed {seed_file} --elf {elf_dir}"
    try:
        child = pexpect.spawn(cmd)
        child.timeout = None
        child.interact()
        child.close()
    except pexpect.exceptions.ExceptionPexpect as e:
        ERROR(f"Failed to execute command: {e}")
    except Exception as e:
        ERROR(f"Unexpected error: {e}")
    end_time = time.time()
    INFO(f"Execution time: {end_time - start_time} seconds")

cli.add_command(instrument_klee_entry)

'''
Perform symbolic execution with KLEE.
'''
@click.command(name="klee")
@click.option("--max-memory", required=True, help="KLEE max memory usage.")
@click.option("--max-time", required=True, help="KLEE max time usage.")
def klee(max_memory, max_time):
    start_time = time.time()
    if not ir_config or not ir_config.entrypoints[0]:
        ERROR("ir_config or entrypoints is not properly configured.")
        return
    split_entry_point = ir_config.entrypoints[0]
    
    KLEE_dir = DTMC + "/dependencies/KLEE/klee/build/bin/"
    if not os.path.exists(KLEE_dir):
        ERROR(f"KLEE directory {KLEE_dir} does not exist. Please build KLEE first.")
        return
    klee_exe = os.path.join(KLEE_dir, "klee")

    property = ir_config.property
    INFO(f"Running KLEE for property: {property}")

    target_ir_dir = DTMC + '/verifyDataBase/ir_and_elf/' + property
    if not os.path.exists(target_ir_dir):
        ERROR(f"LLVM IR directory {target_ir_dir} does not exist.")
        return
    if ir_config.flightControl == 'ArduCopter':
        ir_base_name = "arducopter_sitl_pi64"
    elif ir_config.flightControl == 'PX4':
        ir_base_name = "px4_hitl_pi64"
    else:
        ERROR(f"Unsupported flight control: {ir_config.flightControl}")
        return
    target_ir = os.path.join(target_ir_dir, f'{ir_base_name}_klee.ll')
    if not os.path.exists(target_ir):
        ERROR(f"LLVM IR file {target_ir} does not exist.")
        return

    max_call_depth = ir_config.call_depth

    import datetime
    current_time = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    output_dir = DTMC + '/verifyDataBase/model_inputs/' + property + '/ktests-' + current_time + '/'

    cmd = (
        f"{klee_exe} --optimize --libc=klee --solver-backend=cvc5 --fp-runtime "
        f"--external-calls=all "
        f"--max-memory={max_memory} --max-time={max_time} --libc=uclibc --libcxx "
        f"--posix-runtime --entry-point=klee_entry --enable-split --max-call-depth={max_call_depth} "
        f"--output-dir={output_dir} --emit-all-errors --search=random-path "
        f"--delay-gen-test-case --split-entry-point={split_entry_point} {target_ir}"
    )

    try:
        child = pexpect.spawn(cmd)
        child.timeout = None
        child.interact()
        child.close()
    except pexpect.exceptions.ExceptionPexpect as e:
        ERROR(f"Failed to execute command: {e}")
    except Exception as e:
        ERROR(f"Unexpected error: {e}")
    end_time = time.time()
    INFO(f"Execution time: {end_time - start_time} seconds")

cli.add_command(klee)

'''
Convert .ktest files to model input .mi files.
'''
from pyscript.initializer.modelImputsBuilder import *
@click.command(name="create_inputs")
def create_inputs():
    start_time = time.time()
    if not ir_config or not ir_config.property:
        ERROR("ir_config or property is not properly configured.")
        return
    property = ir_config.property
    INFO(f"Creating model inputs for property: {property}")
    model_inputs_dir = DTMC + '/verifyDataBase/model_inputs/' + property
    if not os.path.exists(model_inputs_dir):
        os.makedirs(model_inputs_dir)

    seed_files = [f for f in os.listdir(model_inputs_dir) if f.startswith(f'{property}_seed') and f.endswith('.mi')]
    if not seed_files:
        ERROR(f"No seed file found for property {property} in {model_inputs_dir}")
        return
    seed_file = os.path.join(model_inputs_dir, seed_files[0])

    ktests_pdir = DTMC + '/verifyDataBase/model_inputs/' + property

    # Find the latest created directory starting with 'ktests-%Y%m%d_%H%M%S'
    subdirs = [d for d in os.listdir(ktests_pdir) if d.startswith('ktests-') and os.path.isdir(os.path.join(ktests_pdir, d))]
    if not subdirs:
        ERROR(f"No directories starting with 'ktests-' found in {ktests_pdir}")
        return
    latest_dir = max(subdirs, key=lambda d: os.path.getctime(os.path.join(ktests_pdir, d)))
    ktests_dir = os.path.join(ktests_pdir, latest_dir)

    if not os.path.exists(ktests_dir):
        ERROR(f"ktests directory {ktests_dir} does not exist.")
        return
    import random
    # Collect .ktest files and categorize them by marker files
    # Priority: unmarked > .early marked > .err marked (excluded)
    all_files = [f for f in os.listdir(ktests_dir) if os.path.isfile(os.path.join(ktests_dir, f))]
    
    # Extract basename (before first dot) for marker files
    err_basenames = {f.split('.')[0] for f in all_files if '.err' in f}
    early_basenames = {f.split('.')[0] for f in all_files if '.early' in f}
    
    unmarked_ktests = []
    early_ktests = []
    
    for f in all_files:
        if not f.endswith('.ktest'):
            continue
        basename = f.split('.')[0]  # Get basename before first dot
        
        if basename in err_basenames:
            continue  # Skip .err marked files
        elif basename in early_basenames:
            early_ktests.append(f)
        else:
            unmarked_ktests.append(f)
    
    if not unmarked_ktests and not early_ktests:
        ERROR(f"No valid .ktest files found in {ktests_dir} (excluding .err marked files)")
        return

    # Randomly select 87-97 files, prioritizing unmarked over early
    max_count = random.randint(87, 97)
    selected_files = []
    
    # First, select from unmarked files
    if unmarked_ktests:
        unmarked_count = min(max_count, len(unmarked_ktests))
        selected_files.extend(random.sample(unmarked_ktests, unmarked_count))
    
    # If we need more, select from early marked files
    remaining = max_count - len(selected_files)
    if remaining > 0 and early_ktests:
        early_count = min(remaining, len(early_ktests))
        selected_files.extend(random.sample(early_ktests, early_count))
    
    total_available = len(unmarked_ktests) + len(early_ktests)
    INFO(f"Selected {len(selected_files)} files out of {total_available} available files: {len([f for f in selected_files if f in unmarked_ktests])} unmarked, {len([f for f in selected_files if f in early_ktests])} early-marked")

    ktests = [os.path.join(ktests_dir, f) for f in selected_files]
    
    import multiprocessing
    from concurrent.futures import ProcessPoolExecutor, as_completed
    from pyscript.initializer.modelImputsBuilder import process_single_ktest
    
    # Read seed file once and pass bytes to all processes
    with open(seed_file, 'rb') as f:
        seed_bytes = f.read()
    
    input_dir = os.path.dirname(seed_file)
    flight_control = ir_config.flightControl
    
    cpu_cores = multiprocessing.cpu_count()
    max_workers = min(cpu_cores, 16)
    INFO(f"Using {max_workers} processes to handle {len(ktests)} ktest files...")
    
    failures = []
    completed = 0
    
    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        # Submit all tasks
        future_to_ktest = {
            executor.submit(process_single_ktest, seed_bytes, ktest, input_dir, flight_control): ktest 
            for ktest in ktests
        }
        
        # Process results as they complete
        for future in as_completed(future_to_ktest):
            ktest_path, error = future.result()
            completed += 1
            
            if error:
                ERROR(f"Failed {os.path.basename(ktest_path)}: {error}")
                failures.append((ktest_path, error))
            
            if completed % 10 == 0:
                INFO(f"Progress: {completed}/{len(ktests)} completed")

    if failures:
        WARNING(f"Completed with {len(failures)} failures out of {len(ktests)} ktests")
    else:
        INFO(f"Successfully processed all {len(ktests)} ktests")
    end_time = time.time()
    INFO(f"Execution time: {end_time - start_time} seconds")

cli.add_command(create_inputs)

'''
Parse flight control doc.
'''
from pyscript.LLM.property_extraction.UAVDocPropertyUtils import *
cli.add_command(uav_doc_extract)

'''
Reorganize the csv file.
'''
cli.add_command(reorganize_csv)

'''
Call LLM API generate property.
'''
from pyscript.LLM.property_extraction.LLMClient import *
cli.add_command(extract_property_specifications)

'''
Get property specification embedding.
'''
from pyscript.LLM.property_extraction.UAVDocPropertyUtils import *
cli.add_command(text_embedding)

'''
Threshold evaluation.
'''
@click.command(name="similarity_threshold")
@click.option('--fc-type', '-t', required=True, type=click.Choice(['ardupilot', 'px4'], case_sensitive=False), help='Flight control type: ArduCopter or PX4')
def similarity_threshold(fc_type: str):
    """Threshold evaluation."""
    INFO(f"Threshold evaluation...")
    threshold_evaluation(fc_type)
cli.add_command(similarity_threshold)

'''
Clustering property specification embedding.
'''
@click.command(name="clustering")
@click.option('--fc-type', '-t', required=True, type=click.Choice(['ardupilot', 'px4'], case_sensitive=False), help='Flight control type: ArduCopter or PX4')
@click.option('--threshold', '-th', default=0.9, help='Similarity threshold for clustering')
def clustering(fc_type: str, threshold: float):
    """Clustering property specification embedding."""
    INFO(f"Clustering property specification embedding...")
    clustering_to_json(fc_type, threshold=threshold)
cli.add_command(clustering)

'''
Extract property using the clustering results.
'''
cli.add_command(extract_useit)

'''
Vectorize the flight control code.
'''
from pyscript.LLM.slice_criteria_rag.main import *
@click.command(name="vectorize")
@click.option('--type', '-t', required=True, type=click.Choice(['ArduCopter', 'PX4'], case_sensitive=False), help='Flight control type: ArduCopter or PX4')
def vectorize(type):
    """Vectorize the flight control code."""
    INFO(f"Vectorizing {type} flight control code...")
    if type == 'ArduCopter':
        target_dirs = [
            DTMC + '/flight-control/arducopter-4.4/ArduCopter',
            DTMC + '/flight-control/arducopter-4.4/libraries'
        ]
        vectorization(target_dirs, 'ArduCopter')
    elif type == 'PX4':
        target_dirs = [
            DTMC + '/flight-control/PX4-1.15.2/src/include',
            DTMC + '/flight-control/PX4-1.15.2/src/lib',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/airspeed_selector',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/attitude_estimator_q',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/battery_status',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/camera_feedback',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/commander',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/control_allocator',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/dataman',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/differential_drive',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/ekf2',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/esc_battery',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/events',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/flight_mode_manager',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/gimbal',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/gyro_calibration',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/gyro_fft',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/land_detector',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/landing_target_estimator',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/load_mon',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/local_position_estimator',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/logger',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mag_bias_estimator',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/manual_control',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mavlink',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mc_att_control',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mc_autotune_attitude_control',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mc_hover_thrust_estimator',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mc_pos_control',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/mc_rate_control',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/muorb',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/navigator',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/payload_deliverer',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/px4iofirmware',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/rc_update',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/replay',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/sensors',
            DTMC + '/flight-control/PX4-1.15.2/src/modules/temperature_compensation',
        ]
        vectorization(target_dirs, 'PX4')

cli.add_command(vectorize)

'''
Get slice criterion.
'''
from pyscript.LLM.slice_criteria_rag.main import *
@click.command(name="find_slice_criterion")
def find_slice_criterion():
    """Find slice criterion."""
    find_slice_criterias()

cli.add_command(find_slice_criterion)

def handle_ssh_connection(child, host_description="remote host"):
    """
    Handle SSH connection interactions including password prompts, host key verification, etc.
    
    Args:
        child: pexpect spawn object for the SSH command
        host_description: Description of the target host for user-friendly messages
        
    Returns:
        bool: True if connection successful, False if failed
    """
    import getpass
    
    while True:
        index = child.expect([
            pexpect.TIMEOUT,
            pexpect.EOF,
            r'[Pp]assword:',  
            r'Are you sure you want to continue connecting \(yes/no.*\)\?',  
            r'Host key verification failed',  
            r'Permission denied',  
            r'No route to host',  
            r'Connection refused',
            r'.*@.*:.*\$',  # Shell prompt - connection successful
            r'.*@.*:.*#'    # Root shell prompt - connection successful
        ], timeout=None)
        
        if index == 0:  # TIMEOUT (should not occur with timeout=None)
            WARNING("Unexpected timeout occurred")
            child.close()
            return False
            
        elif index == 1:  # EOF 
            child.close()
            if child.exitstatus == 0:
                INFO("Command completed successfully!")
                return True
            else:
                ERROR(f"Command failed with exit code: {child.exitstatus}")
                if child.exitstatus == 23:
                    ERROR("rsync exit code 23: Partial transfer due to error")
                    ERROR("Common causes:")
                    ERROR("  - Source files don't exist")
                    ERROR("  - Permission issues")
                    ERROR("  - Network problems")
                    ERROR("  - Destination path issues")
                    ERROR("Try using --dry-run flag to debug")
                return False
                
        elif index == 2:  
            INFO(f"SSH password required for {host_description}. Please enter password:")
            password = getpass.getpass(f"Enter password for {host_description}: ")
            child.sendline(password)
            
        elif index == 3:  
            INFO("First time connecting to this host. Accepting host key...")
            child.sendline('yes')
            
        elif index == 4: 
            ERROR("SSH host key verification failed. Please check your SSH configuration.")
            child.close()
            return False
            
        elif index == 5:  
            ERROR("Permission denied. Please check your SSH credentials.")
            child.close()
            return False
            
        elif index == 6:  
            ERROR("No route to host. Please check network connectivity.")
            child.close()
            return False
            
        elif index == 7:  
            ERROR("Connection refused. Please check if SSH service is running on the target host.")
            child.close()
            return False
            
        elif index in [8, 9]:  # Shell prompt - connection successful
            INFO(f"Successfully connected to {host_description}!")
            return True

'''
Upload flight control binary to raspberry pi.
'''
@click.command(name="upload_pi")
def upload_pi():
    """Upload flight control binary to raspberry pi."""
    if not ir_config:
        ERROR("IR config not loaded. Please ensure IR_config.yml exists and is valid.")
        return False
    
    fc_type = ir_config.flightControl
    INFO(f"Uploading {fc_type} flight control binary to raspberry pi...")
    if fc_type == 'ArduCopter':
        target_property = ir_config.property
        import glob
        source_pattern = DTMC + f"/verifyDataBase/ir_and_elf/{target_property}/arducopter_{target_property}*"
        ardupilot_bins = glob.glob(source_pattern)
        if not ardupilot_bins:
            ERROR(f"Not found any file match pattern: {source_pattern}")
            return False
            
        INFO(f"Find {len(ardupilot_bins)} files to upload:")
        for bin_file in ardupilot_bins:
            INFO(f"  - {bin_file}")
        files_str = " ".join(ardupilot_bins)
        cmd = f"rsync -avz {files_str} {RASPI_SSH_TARGET}:~/ardupilot/{target_property}/"
        try:
            INFO(f"Executing: {cmd}")
            child = pexpect.spawn(cmd, timeout=300)
            # Use the reusable SSH connection handler
            return handle_ssh_connection(child, RASPI_SSH_TARGET)
                    
        except pexpect.exceptions.ExceptionPexpect as e:
            ERROR(f"Pexpect error: {e}")
            return False
        except Exception as e:
            ERROR(f"Unexpected error: {e}")
            return False
            
    elif fc_type == 'PX4':
        target_property = ir_config.property
        import glob
        source_pattern = DTMC + f"/verifyDataBase/ir_and_elf/{target_property}/px4_{target_property}*"
        px4_bins = glob.glob(source_pattern)
        if not px4_bins:
            ERROR(f"Not found any file match pattern: {source_pattern}")
            return False
        INFO(f"Find {len(px4_bins)} files to upload:")
        for bin_file in px4_bins:
            INFO(f"  - {bin_file}")
        files_str = " ".join(px4_bins)
        cmd = f"rsync -avz {files_str} {RASPI_SSH_TARGET}:~/px4/{target_property}/"
        try:
            INFO(f"Executing: {cmd}")
            child = pexpect.spawn(cmd, timeout=300)
            # Use the reusable SSH connection handler
            return handle_ssh_connection(child, RASPI_SSH_TARGET)
        except pexpect.exceptions.ExceptionPexpect as e:
            ERROR(f"Pexpect error: {e}")
            return False
        except Exception as e:
            ERROR(f"Unexpected error: {e}")
            return False

    else:
        raise click.ClickException(f"Unknown flight control type: {fc_type}")

cli.add_command(upload_pi)

'''
Run flight control.
'''
@click.command(name="run_uav")
@click.option('--isTime', is_flag=True, help='Whether to run the flight control with time instrumentation')
def run_uav(istime: bool):
    """Run flight control."""
    if not ir_config:
        ERROR("IR config not loaded. Please ensure IR_config.yml exists and is valid.")
        return False
    
    fc_type = ir_config.flightControl
    INFO(f"Connecting {RASPI_SSH_TARGET}...")
    target_property = ir_config.property
    if istime:
        suffix = '_timer'
    else:
        suffix = ''
    try:
        ssh_command = f"ssh {RASPI_SSH_TARGET}"
        
        child = pexpect.spawn(ssh_command, encoding='utf-8')
        
        # Use handle_ssh_connection to establish connection
        if handle_ssh_connection(child, RASPI_SSH_TARGET):
            INFO("SSH connection established successfully!")
            
            # Send the flight control command automatically
            if fc_type.lower() == 'arducopter':
                command = f"cd ~/ardupilot/{target_property} && ./arducopter_{target_property}{suffix} -S --model + --speedup 1 --defaults ../copter.parm -I0"
                INFO(f"Sending ArduCopter command: {command}")
                child.sendline(command)
                
                # Wait a moment for the command to start
                time.sleep(2)
            elif fc_type.lower() == 'px4':
                symlink_command = (
                    f"cd ~/px4/{target_property} && "
                    f"ln -sf ../bin bin && "
                    f"ln -sf ../px4_hil_complated.config px4_hil_complated.config && "
                    f"ln -sf ../etc etc && "
                    f"mv -f px4_{target_property}{suffix} bin/px4_{target_property}{suffix}"
                )
                INFO(f"Setting up PX4 symlinks: {symlink_command}")
                child.sendline(symlink_command)
                time.sleep(1)  # Wait a moment for the symlink command to complete
                cmd = f"cd ~/px4/{target_property} && ./bin/px4_{target_property}{suffix} -s px4_hil_complated.config"
                INFO(f"Sending PX4 command: {cmd}")
                child.sendline(cmd)
            else:
                ERROR(f"Unknown flight control type: {fc_type}")
                return False
            
            INFO("You are now in interactive mode.")
            INFO("Input 'exit' or press Ctrl+C to close connection.")
            INFO("-" * 50)
            
            # Enter interact mode, keep terminal until user close
            try:
                child.interact()
            except KeyboardInterrupt:
                INFO("\nConnection interrupted by user.")
            except Exception as e:
                WARNING(f"Interact mode exception: {str(e)}")
            finally:
                child.close()
        else:
            ERROR(f"Failed to connect to {RASPI_SSH_TARGET}")
            
    except Exception as e:
        ERROR(f"SSH connection error: {str(e)}")

cli.add_command(run_uav)

'''
Download dumped data from raspberry pi.
'''
@click.command(name="download_pi")
def download_pi():
    """Download dumped data from raspberry pi."""
    if not ir_config:
        ERROR("IR config not loaded. Please ensure IR_config.yml exists and is valid.")
        return False
    
    fc_type = ir_config.flightControl
    INFO(f"Downloading {fc_type} dumped data from raspberry pi...")
    if fc_type == 'ArduCopter':
        target_property = ir_config.property
        # rsync -avz pi@raspi.local:~/ardupilot/AP_ACRO_P1/AP_ACRO_P1* $DTMC/verifyDataBase/model_inputs/runtime_data/<property>
        import glob
        source_pattern = f"{RASPI_SSH_TARGET}:~/ardupilot/{target_property}/{target_property}*.in"
        target_dir = DTMC + f"/verifyDataBase/model_inputs/runtime_data/{target_property}/"
        cmd = f"rsync -avz {source_pattern} {target_dir}"
        try:
            INFO(f"Executing: {cmd}")
            child = pexpect.spawn(cmd, timeout=300)
            # Use the reusable SSH connection handler
            return handle_ssh_connection(child, RASPI_SSH_TARGET)
        except pexpect.exceptions.ExceptionPexpect as e:
            ERROR(f"Pexpect error: {e}")
            return False
        except Exception as e:
            ERROR(f"Unexpected error: {e}")
            return False
    elif fc_type == 'PX4':
        target_property = ir_config.property
        import glob
        source_pattern = f"{RASPI_SSH_TARGET}:~/px4/{target_property}/{target_property}*.in"
        target_dir = DTMC + f"/verifyDataBase/model_inputs/runtime_data/{target_property}"
        cmd = f"rsync -avz {source_pattern} {target_dir}"
        try:
            INFO(f"Executing: {cmd}")
            child = pexpect.spawn(cmd, timeout=300)
            # Use the reusable SSH connection handler
            return handle_ssh_connection(child, RASPI_SSH_TARGET)
        except pexpect.exceptions.ExceptionPexpect as e:
            ERROR(f"Pexpect error: {e}")
            return False
        except Exception as e:
            ERROR(f"Unexpected error: {e}")
            return False
    else:
        raise click.ClickException(f"Unknown flight control type: {fc_type}")

cli.add_command(download_pi)

'''
Verify the property using UPPAAL.
'''
import threading
from queue import Queue
from threading import Lock, Event

def verify_single_input(verifyta, model, mi_file, result_queue, progress_lock, progress_counter, total_inputs, stop_event):
    """Verify a single model input file."""
    try:
        # Check if we should stop
        if stop_event.is_set():
            return
            
        # Create a new environment dict for this thread
        env = os.environ.copy()
        env['MODEL_INPUT_PATH'] = mi_file
        
        cmd = f"{verifyta} -q {model}"
        
        # Use subprocess instead of pexpect for better thread safety
        import subprocess
        import tempfile
        
        # Use temporary directory to prevent file creation in current directory
        with tempfile.TemporaryDirectory() as tmpdir:
            process = subprocess.Popen(
                cmd,
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
                cwd=tmpdir  # Execute in temporary directory
            )
            stdout, stderr = process.communicate()
        output_text = stdout.decode('utf-8')
        error_text = stderr.decode('utf-8')
        
        # Check for errors in output
        if "[ERROR]" in output_text or "[ERROR]" in error_text:
            from colorama import Fore
            with progress_lock:
                ERROR(f"Critical error detected in verification output for {os.path.basename(mi_file)}")
                if error_text:
                    print(f"{Fore.RED}{error_text}{Fore.RESET}")
                if output_text:
                    print(f"{output_text}")
            stop_event.set()  # Signal all threads to stop
            result_queue.put((mi_file, False))
            return
        
        is_satisfied = True
        if "Formula is NOT satisfied" in output_text:
            is_satisfied = False
        
        # Thread-safe progress update and result output
        from colorama import Fore, Style
        with progress_lock:
            progress_counter[0] += 1
            current_progress = progress_counter[0]
            
            # Output result immediately
            result_status = f"{Fore.GREEN}✓ Satisfied{Fore.RESET}" if is_satisfied else f"{Fore.RED}✗ NOT Satisfied{Fore.RESET}"
            print(f"[{current_progress}/{total_inputs}] {result_status} - {os.path.basename(mi_file)}")
            
            # Optionally show detailed output if verbose
            # if output_text:
            #     print(f"{output_text}")
        
        result_queue.put((mi_file, is_satisfied))
        
    except Exception as e:
        with progress_lock:
            ERROR(f"Error verifying {mi_file}: {e}")
        result_queue.put((mi_file, False))

@click.command(name="verify")
@click.option('--threads', '-t', default=None, type=int, help='Number of parallel verification threads (default: CPU core count)')
def verify(threads):
    """Verify the property using UPPAAL with multi-threading support."""
    import multiprocessing
    
    # Set default thread count to CPU core count if not specified
    if threads is None:
        threads = multiprocessing.cpu_count()
    
    start_time = time.time()
    if not ir_config or not ir_config.property:
        ERROR("ir_config or property is not properly configured.")
        return
    
    INFO(f"Verifying property: {ir_config.property}")
    verifyta = os.path.join(DTMC, 'dependencies', 'uppaal-5.0.0-linux64', 'bin', 'verifyta')
    if not os.path.exists(verifyta):
        ERROR(f"UPPAAL verifyta tool not found at {verifyta}")
        return
    
    model = os.path.join(DTMC, 'verifyDataBase', 'uppaal_models', ir_config.property, ir_config.property + '.xml')
    if not os.path.exists(model):
        ERROR(f"UPPAAL model file {model} does not exist.")
        return

    model_inputs_dir = os.path.join(DTMC, 'verifyDataBase', 'model_inputs', ir_config.property) if ir_config and ir_config.property else None
    if not model_inputs_dir or not os.path.exists(model_inputs_dir):
        ERROR(f"Model inputs directory {model_inputs_dir} does not exist.")
        return
    
    # Find all .mi files in the model_inputs_dir
    model_inputs = [os.path.join(model_inputs_dir, f) for f in os.listdir(model_inputs_dir) if f.endswith('.mi')]
    if not model_inputs:
        ERROR(f"No model input (.mi) files found in {model_inputs_dir}")
        return

    total_inputs = len(model_inputs)
    INFO(f"Found {total_inputs} model input files to verify")
    INFO(f"Using {threads} threads for parallel verification")
    INFO("--------------------------------------------------")

    result_queue = Queue()
    progress_lock = Lock()
    progress_counter = [0]  # Use list for mutability in closure
    stop_event = Event()  # Event to signal stop on error
    
    # Create thread pool
    thread_pool = []
    input_index = 0
    
    # Process all inputs with dynamic thread management
    while input_index < total_inputs or thread_pool:
        # Check if we should stop due to error
        if stop_event.is_set():
            ERROR("Verification stopped due to critical error")
            # Wait for all threads to complete
            for t in thread_pool:
                t.join(timeout=5.0)
            break
            
        # Remove completed threads
        thread_pool = [t for t in thread_pool if t.is_alive()]
        
        # Start a new thread if slots available and there are remaining inputs
        if len(thread_pool) < threads and input_index < total_inputs:
            mi_file = model_inputs[input_index]
            thread = threading.Thread(
                target=verify_single_input,
                args=(verifyta, model, mi_file, result_queue, progress_lock, progress_counter, total_inputs, stop_event)
            )
            thread.start()
            thread_pool.append(thread)
            input_index += 1
        else:
            # Small delay to prevent busy waiting when pool is full
            time.sleep(0.05)
    
    # Collect results
    failed_inputs = []
    while not result_queue.empty():
        mi_file, is_satisfied = result_queue.get()
        if not is_satisfied:
            failed_inputs.append(mi_file)
    
    result = len(failed_inputs) == 0

    end_time = time.time() 
    INFO("")
    INFO("=========================================================")
    INFO("================== Verification Result ==================")
    INFO("=========================================================")
    INFO("")
    from colorama import Fore, Style, init
    if result:
        INFO(f"Verification result: Property {ir_config.property} is {Fore.GREEN}Satisfied{Fore.RESET}.")
    else:
        INFO(f"Verification result: Property {ir_config.property} is {Fore.RED}NOT Satisfied{Fore.RESET}.")
        INFO(f"Failed inputs: {len(failed_inputs)}/{total_inputs}")
        for failed_input in failed_inputs:
            INFO(f"  {Fore.RED}Unsatisfied{Fore.RESET} - {failed_input}")
    INFO("")
    INFO(f"Verification cost time: {Fore.YELLOW}{end_time - start_time:.2f}{Fore.RESET} seconds.")
    INFO("")
    INFO("=========================================================")

cli.add_command(verify)

if __name__ == "__main__":
    cli()
