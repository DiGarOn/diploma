#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--overview-csv", required=True)
    parser.add_argument("--target-count", type=int, required=True)
    return parser.parse_args()


def format_duration(seconds: float) -> str:
    minutes = seconds / 60.0
    hours = minutes / 60.0
    days = hours / 24.0
    return f"{seconds:.0f} sec | {hours:.2f} h | {days:.2f} d"


def main() -> int:
    args = parse_args()
    overview_path = Path(args.overview_csv)
    with overview_path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    legacy_total_seconds = 0.0
    shared_delta_mean = 0.0
    recovery_sum_mean = 0.0
    print(f"reference_overview={overview_path}")
    print(f"target_count={args.target_count}")

    for row in rows:
        mean_sec = float(row["TOTAL_TIME_MEAN_SEC"])
        estimate = mean_sec * args.target_count
        legacy_total_seconds += estimate
        shared_delta_mean += float(row["DELTA_TIME_MEAN_SEC"])
        recovery_sum_mean += float(row["RECOVERY_TIME_MEAN_SEC"])
        print(f"{row['RUN_ID']}={format_duration(estimate)}")

    shared_delta_mean /= len(rows)
    optimized_per_iteration = shared_delta_mean + recovery_sum_mean
    optimized_total_seconds = optimized_per_iteration * args.target_count

    print(f"legacy_total={format_duration(legacy_total_seconds)}")
    print(
        "optimized_shared_prefix="
        f"{format_duration(optimized_total_seconds)} "
        f"(shared_delta_mean_sec={shared_delta_mean:.4f}, "
        f"recovery_sum_mean_sec={recovery_sum_mean:.4f}, "
        f"per_iteration_sec={optimized_per_iteration:.4f})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
