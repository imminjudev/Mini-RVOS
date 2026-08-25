#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


QUANTA = [
    10000,
    100000,
    1000000,
]

WORKING_SETS = [
    1,
    8,
    32,
    128,
]


def load_csv(path):
    with path.open(
        newline=""
    ) as file:
        return list(
            csv.DictReader(file)
        )


def quantum_label(
    quantum,
):
    labels = {
        10000: "10k ticks",
        100000: "100k ticks",
        1000000: "1M ticks",
    }

    return labels.get(
        quantum,
        str(quantum),
    )


def save_figure(
    figure,
    directory,
    stem,
):
    png = (
        directory /
        f"{stem}.png"
    )

    svg = (
        directory /
        f"{stem}.svg"
    )

    figure.savefig(
        png,
        dpi=200,
        bbox_inches="tight",
    )

    figure.savefig(
        svg,
        bbox_inches="tight",
    )

    print(
        f"[OK] {png}"
    )

    print(
        f"[OK] {svg}"
    )

    plt.close(
        figure
    )


def plot_throughput_change(
    effects,
    output,
):
    indexed = {
        (
            int(row["quantum_ticks"]),
            int(row["working_set_pages"]),
        ): row
        for row in effects
    }

    figure, axis = plt.subplots(
        figsize=(8, 5)
    )

    x = list(
        range(
            len(WORKING_SETS)
        )
    )

    for quantum in QUANTA:
        changes = []
        lower_errors = []
        upper_errors = []

        for ws in WORKING_SETS:
            row = indexed[
                (
                    quantum,
                    ws,
                )
            ]

            change = float(
                row[
                    "asid_throughput_change_percent"
                ]
            )

            low = float(
                row[
                    "throughput_change_ci95_low_percent"
                ]
            )

            high = float(
                row[
                    "throughput_change_ci95_high_percent"
                ]
            )

            changes.append(
                change
            )

            lower_errors.append(
                change - low
            )

            upper_errors.append(
                high - change
            )

        axis.errorbar(
            x,
            changes,
            yerr=[
                lower_errors,
                upper_errors,
            ],
            marker="o",
            capsize=4,
            label=quantum_label(
                quantum
            ),
        )

    axis.axhline(
        0,
        linewidth=1,
    )

    axis.set_xticks(
        x,
        [
            str(ws)
            for ws in WORKING_SETS
        ],
    )

    axis.set_xlabel(
        "Working-set size (pages)"
    )

    axis.set_ylabel(
        "ASID throughput change (%)"
    )

    axis.set_title(
        "ASID Throughput Change Relative to FULL_FLUSH"
    )

    axis.legend(
        title="Timer quantum"
    )

    axis.grid(
        axis="y",
        alpha=0.25,
    )

    figure.tight_layout()

    save_figure(
        figure,
        output,
        "01-throughput-change",
    )


def plot_switch_cost(
    summary,
    output,
):
    indexed = {
        (
            row["mode"],
            int(row["quantum_ticks"]),
            int(row["working_set_pages"]),
        ): row
        for row in summary
    }

    labels = []

    full_values = []
    asid_values = []

    for quantum in QUANTA:
        for ws in WORKING_SETS:
            labels.append(
                f"{quantum_label(quantum)}\n{ws}p"
            )

            full_values.append(
                float(
                    indexed[
                        (
                            "FULL_FLUSH",
                            quantum,
                            ws,
                        )
                    ][
                        "mean_switch_ticks_per_switch"
                    ]
                )
            )

            asid_values.append(
                float(
                    indexed[
                        (
                            "ASID",
                            quantum,
                            ws,
                        )
                    ][
                        "mean_switch_ticks_per_switch"
                    ]
                )
            )

    figure, axis = plt.subplots(
        figsize=(12, 5.5)
    )

    x = list(
        range(
            len(labels)
        )
    )

    width = 0.38

    axis.bar(
        [
            value - width / 2
            for value in x
        ],
        full_values,
        width=width,
        label="FULL_FLUSH",
    )

    axis.bar(
        [
            value + width / 2
            for value in x
        ],
        asid_values,
        width=width,
        label="ASID",
    )

    axis.set_xticks(
        x,
        labels,
    )

    axis.set_xlabel(
        "Timer quantum / working-set pages"
    )

    axis.set_ylabel(
        "Mean address-space switch cost (ticks/switch)"
    )

    axis.set_title(
        "Measured Address-Space Switch Cost"
    )

    axis.legend()

    axis.grid(
        axis="y",
        alpha=0.25,
    )

    figure.tight_layout()

    save_figure(
        figure,
        output,
        "02-switch-cost",
    )


def plot_switch_fraction(
    summary,
    output,
):
    groups = {}

    for row in summary:
        key = (
            row["mode"],
            int(
                row[
                    "quantum_ticks"
                ]
            ),
        )

        groups.setdefault(
            key,
            [],
        ).append(
            float(
                row[
                    "mean_switch_fraction_percent"
                ]
            )
        )

    figure, axis = plt.subplots(
        figsize=(7.5, 5)
    )

    x = list(
        range(
            len(QUANTA)
        )
    )

    width = 0.36

    full_values = [
        sum(
            groups[
                (
                    "FULL_FLUSH",
                    quantum,
                )
            ]
        ) /
        len(
            groups[
                (
                    "FULL_FLUSH",
                    quantum,
                )
            ]
        )
        for quantum in QUANTA
    ]

    asid_values = [
        sum(
            groups[
                (
                    "ASID",
                    quantum,
                )
            ]
        ) /
        len(
            groups[
                (
                    "ASID",
                    quantum,
                )
            ]
        )
        for quantum in QUANTA
    ]

    axis.bar(
        [
            value - width / 2
            for value in x
        ],
        full_values,
        width=width,
        label="FULL_FLUSH",
    )

    axis.bar(
        [
            value + width / 2
            for value in x
        ],
        asid_values,
        width=width,
        label="ASID",
    )

    axis.set_xticks(
        x,
        [
            quantum_label(
                quantum
            )
            for quantum in QUANTA
        ],
    )

    axis.set_xlabel(
        "Timer quantum"
    )

    axis.set_ylabel(
        "Address-space switching / elapsed time (%)"
    )

    axis.set_title(
        "Address-Space Switching Share of Benchmark Time"
    )

    axis.legend()

    axis.grid(
        axis="y",
        alpha=0.25,
    )

    figure.tight_layout()

    save_figure(
        figure,
        output,
        "03-switch-time-share",
    )


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "dataset",
        help=(
            "experiment directory containing "
            "analysis results"
        ),
    )

    args = parser.parse_args()

    dataset = Path(
        args.dataset
    ).resolve()

    analysis = (
        dataset /
        "analysis"
    )

    summary_path = (
        analysis /
        "condition_summary.csv"
    )

    effects_path = (
        analysis /
        "policy_effects.csv"
    )

    if not summary_path.exists():
        raise SystemExit(
            f"missing analysis file: {summary_path}"
        )

    if not effects_path.exists():
        raise SystemExit(
            f"missing analysis file: {effects_path}"
        )

    output = (
        analysis /
        "figures"
    )

    output.mkdir(
        exist_ok=True
    )

    summary = load_csv(
        summary_path
    )

    effects = load_csv(
        effects_path
    )

    if len(summary) != 24:
        raise SystemExit(
            f"expected 24 summary rows, got {len(summary)}"
        )

    if len(effects) != 12:
        raise SystemExit(
            f"expected 12 policy-effect rows, got {len(effects)}"
        )

    plot_throughput_change(
        effects,
        output,
    )

    plot_switch_cost(
        summary,
        output,
    )

    plot_switch_fraction(
        summary,
        output,
    )

    print(
        "[OK] all research figures generated"
    )


if __name__ == "__main__":
    main()
