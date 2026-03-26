import argparse
import sqlite3
import warnings
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import scipy.stats
import seaborn as sns

plt.rcParams.update({
    "font.family":        "serif",
    "font.serif":         ["Times New Roman", "DejaVu Serif", "Liberation Serif", "serif"],
    "font.size":          13,
    "axes.labelsize":     15,
    "xtick.labelsize":    15,
    "ytick.labelsize":    15,
    "font.weight":        "bold",
    "axes.labelweight":   "bold",
    "mathtext.fontset":   "cm",
    "axes.unicode_minus": False,
})
sns.set_theme(
    style="ticks",
    font="serif",
    rc={"font.serif": ["Times New Roman", "DejaVu Serif", "serif"]},
)

ALPHA_LEVELS: List[float] = [0.99, 0.992, 0.995, 0.997, 0.999, 0.9995, 0.9999, 0.99999]
ALPHA_LABELS: List[str]   = ["0.99", "0.992", "0.995", "0.997", "0.999", "0.9995", "0.9999", "0.99999"]

TAIL_FRAC = 0.30

GEV_COLS = [
    "c_high", "loc_high", "scale_high",
    "c_low",  "loc_low",  "scale_low",
]


def _recompute_bounds(row: pd.Series, alpha: float) -> Tuple[float, float]:
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore")
        try:
            ub = float(scipy.stats.genextreme.ppf(
                alpha, row["c_high"],
                loc=row["loc_high"], scale=row["scale_high"],
            ))
        except Exception:
            ub = np.nan
        try:
            lb = float(-scipy.stats.genextreme.ppf(
                alpha, row["c_low"],
                loc=row["loc_low"], scale=row["scale_low"],
            ))
        except Exception:
            lb = np.nan
    return ub, lb


def load_tail_sequences(
    db_path: str,
    bb_ids: List[int],
    tail_frac: float = TAIL_FRAC,
) -> Dict[int, np.ndarray]:
    conn = sqlite3.connect(db_path)
    result: Dict[int, np.ndarray] = {}
    for bb_id in bb_ids:
        rows = conn.execute(
            "SELECT value FROM bb_records WHERE bb_id = ? ORDER BY id",
            (int(bb_id),),
        ).fetchall()
        if not rows:
            continue
        values = np.array([r[0] for r in rows], dtype=np.int64)
        n = len(values)
        tail_start = max(0, int(np.ceil(n * (1 - tail_frac))))
        result[bb_id] = values[tail_start:]
    conn.close()
    return result


def compute_coverage(
    gev_df: pd.DataFrame,
    tail_seqs: Dict[int, np.ndarray],
) -> pd.DataFrame:
    valid = gev_df[gev_df["status"].isin(["ok", "trivial"])].copy()
    valid = valid.dropna(subset=GEV_COLS)
    records = []
    for alpha, alpha_label in zip(ALPHA_LEVELS, ALPHA_LABELS):
        n_bb_fully_covered = 0
        n_bb_valid         = 0
        for _, row in valid.iterrows():
            bb_id = int(row["bb_id"])
            if bb_id not in tail_seqs:
                continue
            vals = tail_seqs[bb_id]
            if len(vals) == 0:
                continue
            ub, lb = _recompute_bounds(row, alpha)
            if not (np.isfinite(ub) and np.isfinite(lb)):
                continue
            n_bb_valid += 1
            if bool(np.all(vals <= ub)):
                n_bb_fully_covered += 1
        proportion = n_bb_fully_covered / n_bb_valid if n_bb_valid > 0 else np.nan
        records.append({
            "alpha_label":        alpha_label,
            "alpha":              alpha,
            "n_bb_fully_covered": n_bb_fully_covered,
            "n_bb_valid":         n_bb_valid,
            "proportion":         proportion,
        })
    return pd.DataFrame(records)


def plot_coverage_linechart(cov_df: pd.DataFrame, out_path: str) -> None:
    prop_pct = (cov_df["proportion"] * 100).tolist()
    x_pos    = list(range(len(ALPHA_LABELS)))

    color = "#1f77b4"   # blue

    fig, ax = plt.subplots(figsize=(8, 4.0))

    sns.lineplot(x=x_pos, y=prop_pct, marker="o", linewidth=2.5, markersize=8,
                 color=color, linestyle="-", ax=ax)

    for xi, pct in enumerate(prop_pct):
        if xi in {1, 2}:
            ax.text(xi + 0.08, pct - 0.8, f"{pct:.2f}%",
                    ha="left", va="top",
                    fontsize=10, fontweight="bold", color=color)
        elif xi == 4:
            ax.text(xi + 0.02, pct - 1.5, f"{pct:.2f}%",
                    ha="left", va="top",
                    fontsize=10, fontweight="bold", color=color)
        elif xi == 3:
            ax.text(xi + 0.08, pct - 0.8, f"{pct:.2f}%",
                    ha="left", va="top",
                    fontsize=10, fontweight="bold", color=color)
        elif xi == 5:
            ax.text(xi + 0.02, pct - 0.8, f"{pct:.2f}%",
                    ha="center", va="top",
                    fontsize=10, fontweight="bold", color=color)
        else:
            ax.text(xi, pct - 0.8, f"{pct:.2f}%",
                    ha="center", va="top",
                    fontsize=10, fontweight="bold", color=color)

    ax.set_xticks(x_pos)
    ax.set_xticklabels(ALPHA_LABELS, rotation=0, ha="center")
    ax.set_xlim(-0.5, len(ALPHA_LABELS) - 0.5)
    ax.set_xlabel("Confidence Level",
                  fontsize=15, fontweight="bold", labelpad=10)
    ax.set_ylabel("Basic Blocks within\nEstimated Interval (%)",
                  fontsize=15, fontweight="bold")
    ax.yaxis.set_major_formatter(mticker.PercentFormatter())

    y_min = max(0.0, min(prop_pct) - 5.0)
    y_max = min(102.0, max(prop_pct) + 5.0)
    ax.set_ylim(y_min, y_max)
    ax.yaxis.set_major_locator(mticker.MaxNLocator(nbins=6, steps=[1,2,5,10]))
    ax.tick_params(axis="both", which="major", labelsize=13)

    ax.set_facecolor(sns.color_palette("light:#e8ecf1", as_cmap=False)[0])
    fig.patch.set_facecolor("white")
    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(1.0)
        spine.set_color("black")
    sns.despine(ax=ax, top=False, right=False)

    ax.grid(True, linestyle=":", alpha=0.6)

    ax.get_legend().remove() if ax.get_legend() else None

    plt.tight_layout()
    plt.subplots_adjust(left=0.14)
    fig.savefig(out_path, format="pdf", dpi=600, bbox_inches="tight",
                facecolor="white")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("db", nargs="?", default=None)
    parser.add_argument("--out-dir", default=None, metavar="DIR")
    parser.add_argument("--prefix", default=None, metavar="STR")
    parser.add_argument("--tail-frac", type=float, default=TAIL_FRAC, metavar="FRAC")
    args = parser.parse_args()

    csv_path = Path(args.csv).resolve()
    if args.db:
        db_path = Path(args.db).resolve()
    else:
        db_path = csv_path.with_suffix(".db")
        if not db_path.exists():
            stem    = csv_path.stem.replace("_gev_results", "")
            db_path = csv_path.parent / f"{stem}.db"

    out_dir = Path(args.out_dir).resolve() if args.out_dir else csv_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    prefix = args.prefix if args.prefix else csv_path.stem

    gev_df    = pd.read_csv(csv_path)
    valid_ids = gev_df[gev_df["status"].isin(["ok", "trivial"])]["bb_id"].tolist()
    tail_seqs = load_tail_sequences(str(db_path), valid_ids, args.tail_frac)
    cov_df    = compute_coverage(gev_df, tail_seqs)

    plot_coverage_linechart(cov_df, str(out_dir / f"{prefix}_coverage_linechart.pdf"))
    cov_df.to_csv(str(out_dir / f"{prefix}_coverage_stats.csv"), index=False)


if __name__ == "__main__":
    main()

