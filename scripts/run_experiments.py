#!/usr/bin/env python3

import argparse
import csv
import random
import select
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


TIMER_HZ = 10_000_000

MODES = (
    {
        "use_asid": 0,
        "csv_name": "FULL_FLUSH",
        "build_name": "full",
    },
    {
        "use_asid": 1,
        "csv_name": "ASID",
        "build_name": "asid",
    },
)


def run_checked(
    command,
    cwd,
):
    result = subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    if result.returncode != 0:
        print(result.stdout)
        raise RuntimeError(
            "command failed: " +
            " ".join(command)
        )

    return result.stdout.strip()


def git_output(
    repo,
    *arguments,
):
    return run_checked(
        ["git", *arguments],
        repo,
    )


def command_first_line(
    command,
    repo,
):
    output = run_checked(
        command,
        repo,
    )

    if not output:
        return ""

    return output.splitlines()[0]


def create_output_directory(
    repo,
    requested,
):
    if requested is None:
        timestamp = datetime.now().strftime(
            "%Y%m%d-%H%M%S"
        )

        output = (
            repo /
            "research-results" /
            f"asid-memory-{timestamp}"
        )
    else:
        output = Path(requested)

        if not output.is_absolute():
            output = repo / output

    if output.exists():
        raise RuntimeError(
            f"output directory already exists: {output}"
        )

    output.mkdir(
        parents=True
    )

    (output / "logs").mkdir()

    return output


def write_metadata(
    path,
    repo,
    args,
    dirty,
):
    lines = [
        "Mini-RVOS ASID memory experiment",
        "",
        f"created={datetime.now().astimezone().isoformat()}",
        f"git_commit={git_output(repo, 'rev-parse', 'HEAD')}",
        f"git_branch={git_output(repo, 'branch', '--show-current')}",
        f"working_tree_dirty={'yes' if dirty else 'no'}",
        f"runs_per_condition={args.runs}",
        f"switches={args.switches}",
        "quanta=" + ",".join(
            str(value)
            for value in args.quanta
        ),
        "working_sets=" + ",".join(
            str(value)
            for value in args.working_sets
        ),
        f"random_seed={args.seed}",
        f"timer_hz={TIMER_HZ}",
        "qemu=" + command_first_line(
            ["qemu-system-riscv64", "--version"],
            repo,
        ),
        "compiler=" + command_first_line(
            ["riscv64-unknown-elf-gcc", "--version"],
            repo,
        ),
    ]

    path.write_text(
        "\n".join(lines) + "\n"
    )


def build_condition(
    repo,
    mode,
    quantum,
    working_set,
    switches,
):
    print(
        "[BUILD] "
        f"{mode['csv_name']} "
        f"q={quantum} "
        f"ws={working_set}"
    )

    command = [
        "make",
        "benchmark",
        f"BENCHMARK_USE_ASID={mode['use_asid']}",
        "BENCHMARK_WORKLOAD=memory",
        f"BENCHMARK_WORKING_SET_PAGES={working_set}",
        f"BENCHMARK_QUANTUM_TICKS={quantum}",
        f"BENCHMARK_SWITCHES={switches}",
    ]

    run_checked(
        command,
        repo,
    )


def kernel_path(
    repo,
    mode,
    quantum,
    working_set,
    switches,
):
    directory = (
        f"build-benchmark-"
        f"{mode['build_name']}-"
        f"memory-"
        f"ws{working_set}-"
        f"q{quantum}-"
        f"s{switches}"
    )

    return (
        repo /
        directory /
        "kernel.elf"
    )


def terminate_process(
    process,
):
    if process.poll() is not None:
        return

    process.terminate()

    try:
        process.wait(
            timeout=2
        )
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def execute_kernel(
    kernel,
    timeout_seconds,
):
    command = [
        "qemu-system-riscv64",
        "-machine",
        "virt",
        "-m",
        "128M",
        "-nographic",
        "-bios",
        "default",
        "-kernel",
        str(kernel),
    ]

    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    if process.stdout is None:
        terminate_process(
            process
        )

        raise RuntimeError(
            "failed to capture QEMU output"
        )

    deadline = (
        time.monotonic() +
        timeout_seconds
    )

    lines = []
    csv_header = None
    csv_row = None

    try:
        while time.monotonic() < deadline:
            ready, _, _ = select.select(
                [process.stdout],
                [],
                [],
                0.2,
            )

            if ready:
                line = process.stdout.readline()

                if line:
                    line = line.rstrip(
                        "\r\n"
                    )

                    lines.append(
                        line
                    )

                    if line.startswith(
                        "[CSV_HEADER] "
                    ):
                        csv_header = line[
                            len("[CSV_HEADER] "):
                        ]

                    elif line.startswith(
                        "[CSV] "
                    ):
                        csv_row = line[
                            len("[CSV] "):
                        ]

                        break

            if process.poll() is not None:
                break

    finally:
        terminate_process(
            process
        )

    return (
        lines,
        csv_header,
        csv_row,
    )


def parse_result(
    csv_header,
    csv_row,
):
    if csv_header is None:
        raise RuntimeError(
            "benchmark did not emit CSV header"
        )

    if csv_row is None:
        raise RuntimeError(
            "benchmark did not emit CSV row"
        )

    header = next(
        csv.reader(
            [csv_header]
        )
    )

    values = next(
        csv.reader(
            [csv_row]
        )
    )

    if len(header) != len(values):
        raise RuntimeError(
            "CSV column count mismatch"
        )

    return (
        header,
        dict(
            zip(
                header,
                values,
            )
        ),
    )


def require_integer(
    result,
    key,
):
    try:
        return int(
            result[key]
        )
    except (
        KeyError,
        ValueError,
    ) as error:
        raise RuntimeError(
            f"invalid integer field: {key}"
        ) from error


def validate_result(
    result,
    mode,
    quantum,
    working_set,
    switches,
):
    if result.get("mode") != mode["csv_name"]:
        raise RuntimeError(
            "mode mismatch"
        )

    if result.get("workload") != "memory":
        raise RuntimeError(
            "workload mismatch"
        )

    if require_integer(
            result,
            "working_set_pages") != working_set:

        raise RuntimeError(
            "working-set mismatch"
        )

    if require_integer(
            result,
            "quantum_ticks") != quantum:

        raise RuntimeError(
            "quantum mismatch"
        )

    if require_integer(
            result,
            "target_switches") != switches:

        raise RuntimeError(
            "target-switch mismatch"
        )

    if require_integer(
            result,
            "context_switches") != switches:

        raise RuntimeError(
            "context-switch count mismatch"
        )

    expected_fences = (
        0
        if mode["use_asid"]
        else switches * 2
    )

    if require_integer(
            result,
            "sfence_count") != expected_fences:

        raise RuntimeError(
            "sfence count mismatch"
        )

    elapsed = require_integer(
        result,
        "elapsed_ticks"
    )

    p1 = require_integer(
        result,
        "p1_work_units"
    )

    p2 = require_integer(
        result,
        "p2_work_units"
    )

    total = require_integer(
        result,
        "total_work_units"
    )

    if elapsed <= 0:
        raise RuntimeError(
            "elapsed_ticks must be positive"
        )

    if p1 <= 0 or p2 <= 0:
        raise RuntimeError(
            "both processes must complete work"
        )

    if total != p1 + p2:
        raise RuntimeError(
            "total work-unit mismatch"
        )


def save_log(
    path,
    lines,
):
    path.write_text(
        "\n".join(lines) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Run the Mini-RVOS ASID memory "
            "experiment matrix."
        )
    )

    parser.add_argument(
        "--runs",
        type=int,
        default=20,
        help="replicates per condition",
    )

    parser.add_argument(
        "--switches",
        type=int,
        default=100,
        help="context switches per run",
    )

    parser.add_argument(
        "--quanta",
        type=int,
        nargs="+",
        default=[
            10000,
            100000,
            1000000,
        ],
    )

    parser.add_argument(
        "--working-sets",
        type=int,
        nargs="+",
        default=[
            1,
            8,
            32,
            128,
        ],
    )

    parser.add_argument(
        "--seed",
        type=int,
        default=20260825,
    )

    parser.add_argument(
        "--output",
        default=None,
    )

    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help=(
            "allow experiment execution "
            "from a modified working tree"
        ),
    )

    args = parser.parse_args()

    if args.runs <= 0:
        parser.error(
            "--runs must be positive"
        )

    if args.switches < 2:
        parser.error(
            "--switches must be at least 2"
        )

    if args.switches % 2 != 0:
        parser.error(
            "--switches must be even"
        )

    allowed_working_sets = {
        1,
        8,
        32,
        128,
    }

    for working_set in args.working_sets:
        if working_set not in allowed_working_sets:
            parser.error(
                "working sets must be "
                "1, 8, 32, or 128"
            )

    repo = Path(
        __file__
    ).resolve().parents[1]

    dirty_output = git_output(
        repo,
        "status",
        "--porcelain",
    )

    dirty = bool(
        dirty_output
    )

    if dirty and not args.allow_dirty:
        raise RuntimeError(
            "working tree is not clean; "
            "commit changes before the full experiment"
        )

    output = create_output_directory(
        repo,
        args.output,
    )

    write_metadata(
        output / "metadata.txt",
        repo,
        args,
        dirty,
    )

    conditions = []

    for mode in MODES:
        for quantum in args.quanta:
            for working_set in args.working_sets:
                conditions.append(
                    (
                        mode,
                        quantum,
                        working_set,
                    )
                )

    print(
        "[INFO] "
        f"conditions={len(conditions)} "
        f"runs_per_condition={args.runs}"
    )

    for (
        mode,
        quantum,
        working_set,
    ) in conditions:

        build_condition(
            repo,
            mode,
            quantum,
            working_set,
            args.switches,
        )

    tasks = []

    for run_index in range(
        1,
        args.runs + 1,
    ):
        for condition in conditions:
            tasks.append(
                (
                    run_index,
                    *condition,
                )
            )

    random_generator = random.Random(
        args.seed
    )

    random_generator.shuffle(
        tasks
    )

    result_path = (
        output /
        "results.csv"
    )

    writer = None
    result_file = result_path.open(
        "w",
        newline="",
    )

    try:
        for execution_order, task in enumerate(
            tasks,
            start=1,
        ):
            (
                run_index,
                mode,
                quantum,
                working_set,
            ) = task

            print(
                "[RUN] "
                f"{execution_order}/{len(tasks)} "
                f"{mode['csv_name']} "
                f"q={quantum} "
                f"ws={working_set} "
                f"rep={run_index}"
            )

            kernel = kernel_path(
                repo,
                mode,
                quantum,
                working_set,
                args.switches,
            )

            if not kernel.exists():
                raise RuntimeError(
                    f"kernel not found: {kernel}"
                )

            expected_seconds = (
                quantum *
                args.switches /
                TIMER_HZ
            )

            timeout_seconds = max(
                8.0,
                expected_seconds * 3.0 +
                5.0,
            )

            lines, header, row = execute_kernel(
                kernel,
                timeout_seconds,
            )

            log_name = (
                f"{execution_order:04d}_"
                f"{mode['build_name']}_"
                f"q{quantum}_"
                f"ws{working_set}_"
                f"rep{run_index:02d}.log"
            )

            save_log(
                output /
                "logs" /
                log_name,
                lines,
            )

            try:
                parsed_header, result = parse_result(
                    header,
                    row,
                )

                validate_result(
                    result,
                    mode,
                    quantum,
                    working_set,
                    args.switches,
                )

            except Exception:
                print(
                    "\n".join(lines)
                )
                raise

            if writer is None:
                fieldnames = [
                    "execution_order",
                    "run_index",
                    *parsed_header,
                ]

                writer = csv.DictWriter(
                    result_file,
                    fieldnames=fieldnames,
                )

                writer.writeheader()

            output_row = {
                "execution_order":
                    execution_order,
                "run_index":
                    run_index,
                **result,
            }

            writer.writerow(
                output_row
            )

            result_file.flush()

    finally:
        result_file.close()

    print(
        "[OK] experiment complete"
    )

    print(
        f"[OK] results={result_path}"
    )

    print(
        f"[OK] metadata={output / 'metadata.txt'}"
    )


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(
            "\n[FAIL] interrupted",
            file=sys.stderr,
        )
        sys.exit(130)
    except Exception as error:
        print(
            f"[FAIL] {error}",
            file=sys.stderr,
        )
        sys.exit(1)
