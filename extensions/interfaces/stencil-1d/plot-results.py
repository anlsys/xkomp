#!/usr/bin/env python3
"""
Plot stencil-1d performance results: one figure per ngpu value,
with all bins overlaid. X-axis is log2(n), Y-axis is log10(perf).
Repeated runs are aggregated as median with min/max error bars.
"""

import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

# ── Global font sizes ──────────────────────────────────────────────────────────
# Sized so text remains readable when three figures are placed side-by-side
# on a paper column (~7 in wide → each panel ≈ 2.3 in → shrink factor ≈ 3×).
plt.rcParams.update({
    'font.size':        22,
    'axes.titlesize':   26,
    'axes.labelsize':   24,
    'xtick.labelsize':  20,
    'ytick.labelsize':  20,
    'legend.fontsize':  18,
    'lines.linewidth':  2.5,
    'lines.markersize': 10,
})

# ── Visual palettes ───────────────────────────────────────────────────────────
LINESTYLES = ["-", "--", "-."]
MARKERS    = ["o", "s", "^", "D", "v", "p"]
COLORS     = list(plt.cm.tab10.colors)


def parse_args():
    p = argparse.ArgumentParser(
        description="Plot stencil-1d perf results (one figure per ngpu).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("csv", nargs="?", default="results.csv",
                   help="Path to the colon-delimited CSV file")
    p.add_argument("-o", "--output", default=None, metavar="PREFIX",
                   help="Save figures as PREFIX-{ngpu}gpu.{svg,pdf}")
    return p.parse_args()


def load_and_aggregate(path: str) -> pd.DataFrame:
    df = pd.read_csv(path, sep=":")

    # Normalize bin names: strip leading "./" so ./xkomp-access == xkomp-access
    df["bin"] = df["bin"].str.lstrip("./")

    # Trim small problem sizes (n < 2^10)
    df = df[df["n"] >= 2**10]

    group_cols = ["bin", "n", "ngpu"]
    agg = (
        df.groupby(group_cols)["perf"]
          .agg(median="median", lo="min", hi="max")
          .reset_index()
    )
    return agg


def make_figures(agg: pd.DataFrame, output_prefix):
    gpu_values = sorted(agg["ngpu"].unique())

    # Compute shared Y-axis limits across all ngpu figures
    global_lo = agg["lo"].min()
    global_hi = agg["hi"].max()
    # Tight bounds: half a decade of padding around the actual data range
    ymin = float(10 ** (np.floor(np.log10(global_lo) * 2) / 2))
    ymax = float(10 ** (np.ceil(np.log10(global_hi) * 2) / 2))

    for ngpu in gpu_values:
        sub = agg[agg["ngpu"] == ngpu]
        bins = sorted(sub["bin"].unique())

        # Aspect ratio ≈ 4:5 (width:height) so three panels side-by-side
        # fill a landscape row while keeping the plot area tall.
        fig, ax = plt.subplots(figsize=(10, 7))

        for i, bin_name in enumerate(bins):
            d = sub[sub["bin"] == bin_name].sort_values("n")
            if d.empty:
                continue

            yerr = [
                (d["median"] - d["lo"]).clip(lower=0).to_numpy(),
                (d["hi"] - d["median"]).clip(lower=0).to_numpy(),
            ]

            color = COLORS[i % len(COLORS)]

            # Plot error bars as a separate layer so they stand out
            ax.errorbar(
                d["n"].to_numpy(),
                d["median"].to_numpy(),
                yerr=yerr,
                fmt='none',
                ecolor=color, elinewidth=2, capsize=6, capthick=2.5,
                alpha=0.6,
                zorder=2,
            )

            # Plot the line + markers on top
            ax.plot(
                d["n"].to_numpy(),
                d["median"].to_numpy(),
                label=bin_name,
                color=color,
                linestyle=LINESTYLES[i % len(LINESTYLES)],
                marker=MARKERS[i % len(MARKERS)],
                linewidth=2.5, markersize=10,
                zorder=3,
            )

        # X-axis: log base 2, rotated labels to avoid overlap at this width
        ax.set_xscale("log", base=2)
        ax.xaxis.set_major_formatter(mticker.FuncFormatter(
            lambda x, _: f"$2^{{{int(np.log2(x))}}}$" if x > 0 and np.log2(x) == int(np.log2(x)) else ""
        ))
        ax.xaxis.set_minor_formatter(mticker.NullFormatter())
        ax.tick_params(axis='x', rotation=60)

        # Y-axis: log base 10 with 10^k labels
        ax.set_yscale("log", base=10)
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(
            lambda y, _: f"$10^{{{int(np.log10(y))}}}$" if y > 0 and np.isclose(np.log10(y), round(np.log10(y))) else ""
        ))
        ax.yaxis.set_minor_formatter(mticker.NullFormatter())

        # Shared Y-axis range across all figures
        ax.set_ylim(ymin, ymax)

        ax.set_xlabel("n (problem size)")
        ax.set_ylabel("Performance (GFlop/s)")
        ax.set_title(f"Stencil-1D — {ngpu} GPU{'s' if ngpu > 1 else ''}")
        ax.grid(True, alpha=0.3, which="both")
        ax.legend(loc="upper left")

        plt.tight_layout()

        if output_prefix:
            for ext in ("svg", "pdf"):
                fname = f"{output_prefix}-{ngpu}gpu.{ext}"
                plt.savefig(fname, bbox_inches="tight")
                print(f"Saved → {fname}")
        else:
            plt.show()

        plt.close(fig)


def main():
    args = parse_args()
    agg = load_and_aggregate(args.csv)
    make_figures(agg, args.output)


if __name__ == "__main__":
    main()
