#!/usr/bin/env python3

import argparse
import csv
import math
import random
import statistics
from collections import defaultdict
from pathlib import Path


DEFAULT_BOOTSTRAP_SAMPLES = 10000
BOOTSTRAP_SEED = 20260825


def parse_metadata(path):
    metadata = {}

    for line in path.read_text().splitlines():
        if "=" not in line:
            continue

        key, value = line.split("=", 1)
        metadata[key] = value

    return metadata


def mean(values):
    return statistics.fmean(values)


def median(values):
    return statistics.median(values)


def sample_stdev(values):
    if len(values) < 2:
        return 0.0

    return statistics.stdev(values)


def percentile(sorted_values, probability):
    if not sorted_values:
        raise ValueError("empty percentile input")

    if len(sorted_values) == 1:
        return sorted_values[0]

    position = (
        probability *
        (len(sorted_values) - 1)
    )

    lower = math.floor(position)
    upper = math.ceil(position)

    if lower == upper:
        return sorted_values[lower]

    fraction = position - lower

    return (
        sorted_values[lower] *
        (1.0 - fraction) +
        sorted_values[upper] *
        fraction
    )


def bootstrap_improvement_ci(
    full_values,
    asid_values,
    samples,
):
    rng = random.Random(
        BOOTSTRAP_SEED
    )

    improvements = []

    for _ in range(samples):
        full_sample = [
            full_values[
                rng.randrange(
                    len(full_values)
                )
            ]
            for _ in full_values
        ]

        asid_sample = [
            asid_values[
                rng.randrange(
                    len(asid_values)
                )
            ]
            for _ in asid_values
        ]

        full_mean = mean(
            full_sample
        )

        asid_mean = mean(
            asid_sample
        )

        if full_mean == 0:
            raise ValueError(
                "FULL_FLUSH bootstrap mean is zero"
            )

        improvement = (
            (
                asid_mean /
                full_mean
            ) -
            1.0
        ) * 100.0

        improvements.append(
            improvement
        )

    improvements.sort()

    return (
        percentile(
            improvements,
            0.025,
        ),
        percentile(
            improvements,
            0.975,
        ),
    )


def load_rows(
    results_path,
    timer_hz,
):
    rows = []

    with results_path.open(
        newline=""
    ) as file:

        reader = csv.DictReader(
            file
        )

        for raw in reader:
            elapsed_ticks = int(
                raw["elapsed_ticks"]
            )

            context_switches = int(
                raw["context_switches"]
            )

            switch_ticks_total = int(
                raw[
                    "address_space_switch_ticks_total"
                ]
            )

            total_work_units = int(
                raw["total_work_units"]
            )

            p1_work_units = int(
                raw["p1_work_units"]
            )

            p2_work_units = int(
                raw["p2_work_units"]
            )

            throughput = (
                total_work_units *
                timer_hz /
                elapsed_ticks
            )

            switch_ticks_per_switch = (
                switch_ticks_total /
                context_switches
            )

            switch_fraction_percent = (
                switch_ticks_total /
                elapsed_ticks *
                100.0
            )

            larger_process_work = max(
                p1_work_units,
                p2_work_units,
            )

            if larger_process_work == 0:
                fairness_ratio = 0.0
            else:
                fairness_ratio = (
                    min(
                        p1_work_units,
                        p2_work_units,
                    ) /
                    larger_process_work
                )

            rows.append({
                **raw,
                "quantum_ticks":
                    int(raw["quantum_ticks"]),
                "working_set_pages":
                    int(raw["working_set_pages"]),
                "elapsed_ticks":
                    elapsed_ticks,
                "context_switches":
                    context_switches,
                "sfence_count":
                    int(raw["sfence_count"]),
                "address_space_switch_ticks_total":
                    switch_ticks_total,
                "address_space_switch_ticks_max":
                    int(
                        raw[
                            "address_space_switch_ticks_max"
                        ]
                    ),
                "p1_work_units":
                    p1_work_units,
                "p2_work_units":
                    p2_work_units,
                "total_work_units":
                    total_work_units,
                "throughput_work_units_per_second":
                    throughput,
                "switch_ticks_per_switch":
                    switch_ticks_per_switch,
                "switch_fraction_percent":
                    switch_fraction_percent,
                "fairness_ratio":
                    fairness_ratio,
            })

    return rows


def write_condition_summary(
    rows,
    output_path,
):
    groups = defaultdict(
        list
    )

    for row in rows:
        key = (
            row["mode"],
            row["quantum_ticks"],
            row["working_set_pages"],
        )

        groups[key].append(
            row
        )

    fieldnames = [
        "mode",
        "quantum_ticks",
        "working_set_pages",
        "n",
        "mean_elapsed_ticks",
        "mean_throughput_work_units_per_second",
        "median_throughput_work_units_per_second",
        "stdev_throughput_work_units_per_second",
        "cv_throughput_percent",
        "min_throughput_work_units_per_second",
        "max_throughput_work_units_per_second",
        "mean_switch_ticks_per_switch",
        "median_switch_ticks_per_switch",
        "mean_switch_fraction_percent",
        "mean_sfence_count",
        "mean_fairness_ratio",
    ]

    with output_path.open(
        "w",
        newline="",
    ) as file:

        writer = csv.DictWriter(
            file,
            fieldnames=fieldnames,
        )

        writer.writeheader()

        for key in sorted(
            groups,
            key=lambda item: (
                item[1],
                item[2],
                item[0],
            ),
        ):
            mode, quantum, ws = key
            group = groups[key]

            throughput = [
                row[
                    "throughput_work_units_per_second"
                ]
                for row in group
            ]

            switch_ticks = [
                row[
                    "switch_ticks_per_switch"
                ]
                for row in group
            ]

            switch_fraction = [
                row[
                    "switch_fraction_percent"
                ]
                for row in group
            ]

            elapsed = [
                row["elapsed_ticks"]
                for row in group
            ]

            fences = [
                row["sfence_count"]
                for row in group
            ]

            fairness = [
                row["fairness_ratio"]
                for row in group
            ]

            throughput_mean = mean(
                throughput
            )

            throughput_stdev = sample_stdev(
                throughput
            )

            if throughput_mean == 0:
                cv = 0.0
            else:
                cv = (
                    throughput_stdev /
                    throughput_mean *
                    100.0
                )

            writer.writerow({
                "mode":
                    mode,
                "quantum_ticks":
                    quantum,
                "working_set_pages":
                    ws,
                "n":
                    len(group),
                "mean_elapsed_ticks":
                    f"{mean(elapsed):.3f}",
                "mean_throughput_work_units_per_second":
                    f"{throughput_mean:.6f}",
                "median_throughput_work_units_per_second":
                    f"{median(throughput):.6f}",
                "stdev_throughput_work_units_per_second":
                    f"{throughput_stdev:.6f}",
                "cv_throughput_percent":
                    f"{cv:.6f}",
                "min_throughput_work_units_per_second":
                    f"{min(throughput):.6f}",
                "max_throughput_work_units_per_second":
                    f"{max(throughput):.6f}",
                "mean_switch_ticks_per_switch":
                    f"{mean(switch_ticks):.6f}",
                "median_switch_ticks_per_switch":
                    f"{median(switch_ticks):.6f}",
                "mean_switch_fraction_percent":
                    f"{mean(switch_fraction):.9f}",
                "mean_sfence_count":
                    f"{mean(fences):.3f}",
                "mean_fairness_ratio":
                    f"{mean(fairness):.6f}",
            })


def write_policy_effects(
    rows,
    output_path,
    bootstrap_samples,
):
    groups = defaultdict(
        lambda: defaultdict(list)
    )

    for row in rows:
        key = (
            row["quantum_ticks"],
            row["working_set_pages"],
        )

        groups[key][
            row["mode"]
        ].append(
            row
        )

    fieldnames = [
        "quantum_ticks",
        "working_set_pages",
        "n_full_flush",
        "n_asid",
        "full_mean_throughput",
        "asid_mean_throughput",
        "asid_throughput_change_percent",
        "throughput_change_ci95_low_percent",
        "throughput_change_ci95_high_percent",
        "full_mean_switch_ticks_per_switch",
        "asid_mean_switch_ticks_per_switch",
        "switch_ticks_reduction_percent",
        "full_mean_switch_fraction_percent",
        "asid_mean_switch_fraction_percent",
        "full_mean_sfence_count",
        "asid_mean_sfence_count",
        "full_mean_fairness_ratio",
        "asid_mean_fairness_ratio",
    ]

    with output_path.open(
        "w",
        newline="",
    ) as file:

        writer = csv.DictWriter(
            file,
            fieldnames=fieldnames,
        )

        writer.writeheader()

        for key in sorted(
            groups
        ):
            quantum, ws = key

            full = groups[key][
                "FULL_FLUSH"
            ]

            asid = groups[key][
                "ASID"
            ]

            if not full or not asid:
                raise RuntimeError(
                    f"missing policy group: {key}"
                )

            full_throughput = [
                row[
                    "throughput_work_units_per_second"
                ]
                for row in full
            ]

            asid_throughput = [
                row[
                    "throughput_work_units_per_second"
                ]
                for row in asid
            ]

            full_throughput_mean = mean(
                full_throughput
            )

            asid_throughput_mean = mean(
                asid_throughput
            )

            throughput_change = (
                (
                    asid_throughput_mean /
                    full_throughput_mean
                ) -
                1.0
            ) * 100.0

            ci_low, ci_high = (
                bootstrap_improvement_ci(
                    full_throughput,
                    asid_throughput,
                    bootstrap_samples,
                )
            )

            full_switch = mean([
                row[
                    "switch_ticks_per_switch"
                ]
                for row in full
            ])

            asid_switch = mean([
                row[
                    "switch_ticks_per_switch"
                ]
                for row in asid
            ])

            if full_switch == 0:
                switch_reduction = 0.0
            else:
                switch_reduction = (
                    1.0 -
                    (
                        asid_switch /
                        full_switch
                    )
                ) * 100.0

            writer.writerow({
                "quantum_ticks":
                    quantum,
                "working_set_pages":
                    ws,
                "n_full_flush":
                    len(full),
                "n_asid":
                    len(asid),
                "full_mean_throughput":
                    f"{full_throughput_mean:.6f}",
                "asid_mean_throughput":
                    f"{asid_throughput_mean:.6f}",
                "asid_throughput_change_percent":
                    f"{throughput_change:.6f}",
                "throughput_change_ci95_low_percent":
                    f"{ci_low:.6f}",
                "throughput_change_ci95_high_percent":
                    f"{ci_high:.6f}",
                "full_mean_switch_ticks_per_switch":
                    f"{full_switch:.6f}",
                "asid_mean_switch_ticks_per_switch":
                    f"{asid_switch:.6f}",
                "switch_ticks_reduction_percent":
                    f"{switch_reduction:.6f}",
                "full_mean_switch_fraction_percent":
                    f"{mean([row['switch_fraction_percent'] for row in full]):.9f}",
                "asid_mean_switch_fraction_percent":
                    f"{mean([row['switch_fraction_percent'] for row in asid]):.9f}",
                "full_mean_sfence_count":
                    f"{mean([row['sfence_count'] for row in full]):.3f}",
                "asid_mean_sfence_count":
                    f"{mean([row['sfence_count'] for row in asid]):.3f}",
                "full_mean_fairness_ratio":
                    f"{mean([row['fairness_ratio'] for row in full]):.6f}",
                "asid_mean_fairness_ratio":
                    f"{mean([row['fairness_ratio'] for row in asid]):.6f}",
            })


def print_effect_table(
    path,
):
    print()
    print(
        "=== ASID policy effects ==="
    )

    with path.open(
        newline=""
    ) as file:

        rows = list(
            csv.DictReader(file)
        )

    print(
        "quantum  ws    throughput_change"
        "        95% CI"
        "              switch_reduction"
    )

    for row in rows:
        quantum = int(
            row["quantum_ticks"]
        )

        ws = int(
            row["working_set_pages"]
        )

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

        switch_reduction = float(
            row[
                "switch_ticks_reduction_percent"
            ]
        )

        print(
            f"{quantum:7d} "
            f"{ws:3d} "
            f"{change:+10.3f}% "
            f"[{low:+8.3f}%, {high:+8.3f}%] "
            f"{switch_reduction:+10.3f}%"
        )


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "dataset",
        help=(
            "experiment directory containing "
            "results.csv and metadata.txt"
        ),
    )

    parser.add_argument(
        "--bootstrap-samples",
        type=int,
        default=DEFAULT_BOOTSTRAP_SAMPLES,
    )

    args = parser.parse_args()

    dataset = Path(
        args.dataset
    ).resolve()

    results_path = (
        dataset /
        "results.csv"
    )

    metadata_path = (
        dataset /
        "metadata.txt"
    )

    if not results_path.exists():
        raise SystemExit(
            f"results not found: {results_path}"
        )

    if not metadata_path.exists():
        raise SystemExit(
            f"metadata not found: {metadata_path}"
        )

    metadata = parse_metadata(
        metadata_path
    )

    if metadata.get(
        "working_tree_dirty"
    ) != "no":
        raise SystemExit(
            "dataset was not collected "
            "from a clean working tree"
        )

    timer_hz = int(
        metadata["timer_hz"]
    )

    rows = load_rows(
        results_path,
        timer_hz,
    )

    if len(rows) != 480:
        raise SystemExit(
            f"expected 480 rows, got {len(rows)}"
        )

    analysis_directory = (
        dataset /
        "analysis"
    )

    analysis_directory.mkdir(
        exist_ok=True
    )

    condition_summary = (
        analysis_directory /
        "condition_summary.csv"
    )

    policy_effects = (
        analysis_directory /
        "policy_effects.csv"
    )

    write_condition_summary(
        rows,
        condition_summary,
    )

    write_policy_effects(
        rows,
        policy_effects,
        args.bootstrap_samples,
    )

    print(
        f"[OK] rows={len(rows)}"
    )

    print(
        f"[OK] condition_summary={condition_summary}"
    )

    print(
        f"[OK] policy_effects={policy_effects}"
    )

    print_effect_table(
        policy_effects
    )


if __name__ == "__main__":
    main()
