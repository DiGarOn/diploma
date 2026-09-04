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


def maybe_add_adaptive_m5_estimate(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    if any(row.get("RUN_ID") == "adaptive_m5" for row in rows):
        return rows

    by_run_id = {row.get("RUN_ID"): row for row in rows}
    m1 = by_run_id.get("adaptive_m1")
    m10 = by_run_id.get("adaptive_m10")
    if not m1 or not m10:
        return rows

    recovery_m1 = float(m1["RECOVERY_TIME_MEAN_SEC"])
    recovery_m10 = float(m10["RECOVERY_TIME_MEAN_SEC"])
    factor_m1 = float(m1["SAMPLE_FACTOR_M"])
    factor_m10 = float(m10["SAMPLE_FACTOR_M"])
    target_factor = 5.0
    if factor_m10 == factor_m1:
        return rows

    t = (target_factor - factor_m1) / (factor_m10 - factor_m1)
    recovery_m5 = recovery_m1 + t * (recovery_m10 - recovery_m1)

    synthetic = dict(m10)
    synthetic["RUN_ID"] = "adaptive_m5"
    synthetic["SERIES_LABEL"] = "adaptive_m5"
    synthetic["SAMPLE_FACTOR_M"] = f"{target_factor:.6f}"
    synthetic["TOTAL_TIME_SUM_SEC"] = "0"
    synthetic["TOTAL_TIME_MEAN_SEC"] = f"{recovery_m5:.4f}"
    synthetic["DELTA_TIME_MEAN_SEC"] = "0.0000"
    synthetic["RECOVERY_TIME_MEAN_SEC"] = f"{recovery_m5:.4f}"

    out: list[dict[str, str]] = []
    inserted = False
    for row in rows:
        out.append(row)
        if row.get("RUN_ID") == "adaptive_m1":
            out.append(synthetic)
            inserted = True
    if not inserted:
        out.append(synthetic)
    return out


def main() -> int:
    args = parse_args()
    overview_path = Path(args.overview_csv)
    with overview_path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    rows = maybe_add_adaptive_m5_estimate(rows)

    total_seconds = 0.0
    first_delta_mean = 0.0
    recovery_sum_mean = 0.0
    print(f"reference_overview={overview_path}")
    print(f"target_count={args.target_count}")

    for row in rows:
        mean_sec = float(row["TOTAL_TIME_MEAN_SEC"])
        estimate = mean_sec * args.target_count
        total_seconds += estimate
        if row.get("RUN_ID") == "adaptive_m1":
            first_delta_mean = float(row["DELTA_TIME_MEAN_SEC"])
        recovery_sum_mean += float(row["RECOVERY_TIME_MEAN_SEC"])
        print(f"{row['RUN_ID']}={format_duration(estimate)}")

    shared_prefix_per_iteration = first_delta_mean + recovery_sum_mean
    shared_prefix_total_seconds = shared_prefix_per_iteration * args.target_count

    print(f"total={format_duration(total_seconds)}")
    print(
        "shared_prefix_model="
        f"{format_duration(shared_prefix_total_seconds)} "
        f"(first_delta_mean_sec={first_delta_mean:.4f}, "
        f"recovery_sum_mean_sec={recovery_sum_mean:.4f}, "
        f"per_iteration_sec={shared_prefix_per_iteration:.4f})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
