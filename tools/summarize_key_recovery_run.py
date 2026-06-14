#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import statistics
from fractions import Fraction
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", required=True)
    return parser.parse_args()


def fraction_to_text(value: Fraction) -> str:
    den = value.denominator
    if den > 0 and den & (den - 1) == 0:
        return f"{value.numerator}/2^{den.bit_length() - 1}"
    return f"{value.numerator}/{value.denominator}"


def load_rows(summary_path: Path) -> list[dict[str, str]]:
    with summary_path.open(newline="") as f:
        return list(csv.DictReader(f))


def main() -> int:
    args = parse_args()
    run_dir = Path(args.run_dir)
    summary_path = run_dir / "summary.csv"
    aggregate_csv = run_dir / "aggregate_report.csv"
    aggregate_md = run_dir / "aggregate_report.md"

    rows = load_rows(summary_path)
    if not rows:
        raise SystemExit("summary.csv is empty")

    ranks = [int(r["TRUE_RANK"]) for r in rows]
    pair_counts = [int(r["PREFIX_MAX_PAIR_COUNT_TOL_2_NEG_15"]) for r in rows]
    sample_counts = [int(r["SAMPLE_COUNT"]) for r in rows]
    prefix_abs_values = [float(r["PREFIX_DELTA_ABS_VALUE"]) for r in rows]
    total_times = [float(r["TOTAL_TIME_SEC"]) for r in rows]
    delta_times = [float(r["DELTA_TIME_SEC"]) for r in rows]
    recovery_times = [float(r["RECOVERY_TIME_SEC"]) for r in rows]

    true_mean = sum(
        Fraction(int(r["TRUE_DELTA_SIGNED_NUMERATOR"]), int(r["TRUE_DELTA_SIGNED_DENOMINATOR"]))
        for r in rows
    ) / len(rows)
    false_mean = sum(
        Fraction(int(r["FALSE_DELTA_SIGNED_MEAN_NUMERATOR"]), int(r["FALSE_DELTA_SIGNED_MEAN_DENOMINATOR"]))
        for r in rows
    ) / len(rows)

    prefix_min_row = min(rows, key=lambda r: float(r["PREFIX_DELTA_ABS_VALUE"]))
    prefix_max_row = max(rows, key=lambda r: float(r["PREFIX_DELTA_ABS_VALUE"]))

    aggregate_row = {
        "RUN_ID": rows[0]["RUN_ID"],
        "SERIES_LABEL": rows[0]["SERIES_LABEL"],
        "SAMPLE_MODE": rows[0]["SAMPLE_MODE"],
        "SAMPLE_FACTOR_M": rows[0]["SAMPLE_FACTOR_M"],
        "SAMPLE_CAP": rows[0]["SAMPLE_CAP"],
        "THREAD_COUNT": rows[0]["THREAD_COUNT"],
        "SEED": rows[0]["SEED"],
        "FULL_MATERIAL": rows[0]["FULL_MATERIAL"],
        "ITERATION_COUNT": str(len(rows)),
        "TOP1_COUNT": str(sum(r <= 1 for r in ranks)),
        "TOP3_COUNT": str(sum(r <= 3 for r in ranks)),
        "TOP5_COUNT": str(sum(r <= 5 for r in ranks)),
        "TOP10_COUNT": str(sum(r <= 10 for r in ranks)),
        "TOP32_COUNT": str(sum(r <= 32 for r in ranks)),
        "TOP100_COUNT": str(sum(r <= 100 for r in ranks)),
        "TRUE_RANK_MEAN": f"{statistics.mean(ranks):.6f}",
        "TRUE_RANK_MEDIAN": f"{statistics.median(ranks):.6f}",
        "TRUE_RANK_MAX": str(max(ranks)),
        "SAMPLE_COUNT_MEAN": f"{statistics.mean(sample_counts):.6f}",
        "SAMPLE_COUNT_MIN": str(min(sample_counts)),
        "SAMPLE_COUNT_MAX": str(max(sample_counts)),
        "PREFIX_DELTA_ABS_MIN_VALUE": prefix_min_row["PREFIX_DELTA_ABS_VALUE"],
        "PREFIX_DELTA_ABS_MIN_FRACTION": prefix_min_row["PREFIX_DELTA_ABS_FRACTION"],
        "PREFIX_DELTA_ABS_MAX_VALUE": prefix_max_row["PREFIX_DELTA_ABS_VALUE"],
        "PREFIX_DELTA_ABS_MAX_FRACTION": prefix_max_row["PREFIX_DELTA_ABS_FRACTION"],
        "PREFIX_MAX_PAIR_COUNT_MEAN": f"{statistics.mean(pair_counts):.6f}",
        "PREFIX_MAX_PAIR_COUNT_MIN": str(min(pair_counts)),
        "PREFIX_MAX_PAIR_COUNT_MAX": str(max(pair_counts)),
        "MEAN_TRUE_DELTA_SIGNED_VALUE": f"{float(true_mean):.10f}",
        "MEAN_TRUE_DELTA_SIGNED_FRACTION": fraction_to_text(true_mean),
        "MEAN_FALSE_DELTA_SIGNED_VALUE": f"{float(false_mean):.10f}",
        "MEAN_FALSE_DELTA_SIGNED_FRACTION": fraction_to_text(false_mean),
        "TOTAL_TIME_SUM_SEC": f"{sum(total_times):.4f}",
        "TOTAL_TIME_MEAN_SEC": f"{statistics.mean(total_times):.4f}",
        "DELTA_TIME_MEAN_SEC": f"{statistics.mean(delta_times):.4f}",
        "RECOVERY_TIME_MEAN_SEC": f"{statistics.mean(recovery_times):.4f}",
    }

    with aggregate_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(aggregate_row.keys()))
        writer.writeheader()
        writer.writerow(aggregate_row)

    lines = [
        f"RUN_ID,{aggregate_row['RUN_ID']}",
        f"SERIES_LABEL,{aggregate_row['SERIES_LABEL']}",
        f"TOP1_COUNT,{aggregate_row['TOP1_COUNT']}",
        f"TOP3_COUNT,{aggregate_row['TOP3_COUNT']}",
        f"TOP32_COUNT,{aggregate_row['TOP32_COUNT']}",
        f"TRUE_RANK_MEAN,{aggregate_row['TRUE_RANK_MEAN']}",
        f"TRUE_RANK_MEDIAN,{aggregate_row['TRUE_RANK_MEDIAN']}",
        f"TRUE_RANK_MAX,{aggregate_row['TRUE_RANK_MAX']}",
        f"PREFIX_DELTA_ABS_MIN,{aggregate_row['PREFIX_DELTA_ABS_MIN_VALUE']} ({aggregate_row['PREFIX_DELTA_ABS_MIN_FRACTION']})",
        f"PREFIX_DELTA_ABS_MAX,{aggregate_row['PREFIX_DELTA_ABS_MAX_VALUE']} ({aggregate_row['PREFIX_DELTA_ABS_MAX_FRACTION']})",
        f"PREFIX_MAX_PAIR_COUNT_MEAN,{aggregate_row['PREFIX_MAX_PAIR_COUNT_MEAN']}",
        f"MEAN_TRUE_DELTA_SIGNED,{aggregate_row['MEAN_TRUE_DELTA_SIGNED_VALUE']} ({aggregate_row['MEAN_TRUE_DELTA_SIGNED_FRACTION']})",
        f"MEAN_FALSE_DELTA_SIGNED,{aggregate_row['MEAN_FALSE_DELTA_SIGNED_VALUE']} ({aggregate_row['MEAN_FALSE_DELTA_SIGNED_FRACTION']})",
        f"TOTAL_TIME_SUM_SEC,{aggregate_row['TOTAL_TIME_SUM_SEC']}",
    ]
    aggregate_md.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
