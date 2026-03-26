"""
plotBlockSizeCDF.py
-------------------
Standalone script: reads the GEV-results CSV produced by fitGEVFromDB.py
(which already discards BB_IDs that fail chi-squared on every block size) and
generates a cumulative bar/line chart showing the distribution of the *chosen*
block size across all successfully fitted BB_IDs.

Because fitGEVFromDB.py now uses status='no_fit' and excludes those rows from
the output CSV, the plot reflects only successfully fitted BB_IDs — no fallback
sentinels or failure adjustments are needed.

Figure 1 – CDF of selected block sizes
    Shows what fraction of BB_IDs converge (first-pass chi-squared test)
    at each candidate block size.

Usage
-----
    python3 plotBlockSizeCDF.py results.csv
    python3 plotBlockSizeCDF.py results.csv --out-dir /tmp/plots --prefix myrun
    python3 -m pyscript.initializer.plotBlockSizeCDF results.csv
"""

import argparse
from pathlib import Path
from typing import List

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import seaborn as sns


# ---------------------------------------------------------------------------
# Global style
# ---------------------------------------------------------------------------
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

# Candidate block sizes – must match BLOCK_SIZES in fitGEVFromDB.py
CANDIDATE_BLOCKS: List[int] = [50, 75, 100, 125, 150, 175, 200, 225, 250]


# ---------------------------------------------------------------------------
# Plot function
# ---------------------------------------------------------------------------

def plot_block_cdf(df: pd.DataFrame, out_path: str) -> None:
    """
    Cumulative line chart with two lines:
      - Block maxima (WCET): selected_block_high
      - Block minima (BCET): selected_block_low

    Only rows with status='ok' are counted.
    Style matches similarity_threshold_selection.py.
    """
    ok_df = df[df["status"] == "ok"].copy()
    n_ok  = len(ok_df)

    if n_ok == 0:
        print("  [WARN] No rows with status='ok' found; nothing to plot.")
        return

    def _cum_pcts(col: str) -> List[float]:
        blocks = pd.to_numeric(ok_df[col], errors="coerce").dropna().astype(int)
        counts = {bs: int((blocks == bs).sum()) for bs in CANDIDATE_BLOCKS}
        cum: List[float] = []
        running = 0.0
        for bs in CANDIDATE_BLOCKS:
            running += 100.0 * counts[bs] / n_ok
            cum.append(min(running, 100.0))
        return cum

    cum_high = _cum_pcts("selected_block_high")
    cum_low  = _cum_pcts("selected_block_low")

    def _truncate(xs: list, ys: List[float]):
        """Keep points up to and including the first one that reaches 100."""
        result_x, result_y = [], []
        for x, y in zip(xs, ys):
            result_x.append(x)
            result_y.append(y)
            if y >= 100.0:
                break
        return result_x, result_y

    x_pos = list(range(len(CANDIDATE_BLOCKS)))
    x_high, y_high = _truncate(x_pos, cum_high)   # WCET: stop at first 100%
    x_low,  y_low  = x_pos, cum_low                # BCET: show all points incl. 100% tail

    colors = sns.color_palette()
    color_high = colors[3]   # red  – WCET (block maxima)
    color_low  = colors[0]   # blue – BCET (block minima)

    fig, ax = plt.subplots(figsize=(8, 4.0))

    sns.lineplot(x=x_high, y=y_high, marker="o", linewidth=2.5, markersize=8,
                 color=color_high, linestyle="-",  label="WCET", ax=ax)
    sns.lineplot(x=x_low,  y=y_low,  marker="s", linewidth=2.5, markersize=8,
                 color=color_low,  linestyle="--", label="BCET", ax=ax)

    # Fill between the two curves over the shared x range to highlight the gap
    x_shared = list(range(min(len(x_high), len(x_low))))
    if x_shared:
        ax.fill_between(
            x_shared,
            [y_high[i] for i in x_shared],
            [y_low[i]  for i in x_shared],
            alpha=0.15,
            color="grey",
            zorder=0,
        )

    # Annotate: two stacked coloured labels per x position (WCET above, BCET below)
    LINE_GAP = 2.8   # vertical gap between the two text lines in data units
    TEXT_OFFSET = 5.5  # distance below the anchor point (marker) in data units
    # x positions that need a rightward nudge to avoid overlap
    NUDGE_RIGHT = {1, 2, 4}   # xi for block sizes 75, 100, 150
    NUDGE_AMOUNT = 0.15         # in x-axis units
    for xi in x_pos:
        has_high = xi < len(x_high)
        has_low  = xi < len(x_low)
        if not has_high and not has_low:
            continue

        y_anchor = y_low[xi] if has_low else y_high[xi]
        x_offset = NUDGE_AMOUNT if xi in NUDGE_RIGHT else 0.0

        if has_high and has_low:
            # BCET label (bottom)
            ax.text(xi + x_offset, y_anchor - TEXT_OFFSET, f"{y_low[xi]:.1f}%",
                    ha="center", va="top",
                    fontsize=10, fontweight="bold", color=color_low)
            # WCET label (directly above BCET label)
            ax.text(xi + x_offset, y_anchor - TEXT_OFFSET + LINE_GAP, f"{y_high[xi]:.1f}%",
                    ha="center", va="top",
                    fontsize=10, fontweight="bold", color=color_high)
        elif has_high:
            ax.text(xi + x_offset, y_anchor - TEXT_OFFSET, f"{y_high[xi]:.1f}%",
                    ha="center", va="top",
                    fontsize=10, fontweight="bold", color=color_high)
        else:
            ax.text(xi + x_offset, y_anchor - TEXT_OFFSET, f"{y_low[xi]:.1f}%",
                    ha="center", va="top",
                    fontsize=10, fontweight="bold", color=color_low)

    labels = [str(bs) for bs in CANDIDATE_BLOCKS]
    ax.set_xticks(x_pos)
    ax.set_xticklabels(labels)

    ax.set_xlabel("GEV Window Size",
                  fontsize=15, fontweight="bold", labelpad=10)
    ax.set_ylabel("Fitted Basic Blocks (%)",
                  fontsize=15, fontweight="bold")
    ax.yaxis.set_major_formatter(mticker.PercentFormatter())

    # Zoom y-axis into the data range to amplify the visual gap between lines
    y_all = y_high + y_low
    y_data_min = max(0.0, min(y_all) - 8.0)
    y_data_max = min(max(y_all) * 1.12, 104.0)
    ax.set_ylim(y_data_min, y_data_max)   # keeps headroom for annotations
    # Only draw ticks up to 100 – the extra space above is preserved but unlabelled
    tick_step = 10.0
    ax.set_yticks([t for t in np.arange(0, 101, tick_step) if t >= y_data_min])
    ax.tick_params(axis="both", which="major", labelsize=13)

    # Background and spines – match similarity_threshold_selection.py
    ax.set_facecolor(sns.color_palette("light:#e8ecf1", as_cmap=False)[0])
    fig.patch.set_facecolor("white")
    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(1.0)
        spine.set_color("black")
    sns.despine(ax=ax, top=False, right=False)

    ax.grid(True, linestyle=":", alpha=0.6)

    ax.legend(
        loc="lower right",
        frameon=False,
        fontsize=15,
        prop={"weight": "bold", "size": 15, "family": "serif"},
    )

    plt.tight_layout()
    plt.subplots_adjust(left=0.14)
    fig.savefig(out_path, format="pdf", dpi=600, bbox_inches="tight",
                facecolor="white")
    plt.close(fig)
    print(f"  Figure saved: {out_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a cumulative block-size CDF chart from a fitGEVFromDB "
            "CSV file.  BB_IDs that failed chi-squared on every block size are "
            "already excluded from the CSV (status='no_fit' rows are discarded "
            "by fitGEVFromDB.py)."
        )
    )
    parser.add_argument("csv", help="CSV file produced by fitGEVFromDB.py")
    parser.add_argument(
        "--out-dir",
        default=None,
        metavar="DIR",
        help="output directory for the PDF (default: same directory as the CSV)",
    )
    parser.add_argument(
        "--prefix",
        default=None,
        metavar="STR",
        help="filename prefix for the PDF (default: CSV stem)",
    )
    args = parser.parse_args()

    csv_path = Path(args.csv).resolve()
    out_dir  = Path(args.out_dir).resolve() if args.out_dir else csv_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    prefix   = args.prefix if args.prefix else csv_path.stem

    print(f"Loading CSV : {csv_path}")
    df = pd.read_csv(csv_path)
    print(f"  {len(df)} rows loaded")

    out_path = str(out_dir / f"{prefix}_block_size_cdf.pdf")
    plot_block_cdf(df, out_path)
    print("Done.")


if __name__ == "__main__":
    main()
