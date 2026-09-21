#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import math
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
    with summary_path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def mean_fraction(rows: list[dict[str, str]], numerator_key: str, denominator_key: str) -> Fraction:
    total = sum(Fraction(int(row[numerator_key]), int(row[denominator_key])) for row in rows)
    return total / len(rows)


def round_abs_to_1e7(value: float) -> int:
    return int(math.floor(abs(value) * 10_000_000.0 + 0.5))


def max_value_and_count_tol_1e7(rows: list[dict[str, str]], value_key: str, fraction_key: str) -> tuple[float, str, int]:
    value_rows = [(float(row[value_key]), row[fraction_key]) for row in rows]
    max_value, max_fraction = max(value_rows, key=lambda item: item[0])
    rounded_max = round_abs_to_1e7(max_value)
    count = sum(1 for value, _ in value_rows if round_abs_to_1e7(value) == rounded_max)
    return max_value, max_fraction, count


def main() -> int:
    args = parse_args()
    run_dir = Path(args.run_dir)
    summary_path = run_dir / "summary.csv"
    aggregate_csv = run_dir / "aggregate_report.csv"
    aggregate_md = run_dir / "aggregate_report.md"

    rows = load_rows(summary_path)
    if not rows:
        raise SystemExit("summary.csv is empty")

    ranks = [int(row["TRUE_RANK"]) for row in rows]
    sample_counts = [int(row["SAMPLE_COUNT"]) for row in rows]
    total_times = [float(row["TOTAL_TIME_SEC"]) for row in rows]
    delta_times = [float(row["DELTA_TIME_SEC"]) for row in rows]
    recovery_times = [float(row["RECOVERY_TIME_SEC"]) for row in rows]
    true_false_abs_ratio_values = [float(row["TRUE_FALSE_ABS_RATIO_VALUE"]) for row in rows]
    false_abs_max_key_counts = [int(row["FALSE_DELTA_ABS_MAX_KEY_COUNT_TOL_1E_7"]) for row in rows]

    true_mean_signed = mean_fraction(rows, "TRUE_DELTA_SIGNED_NUMERATOR", "TRUE_DELTA_SIGNED_DENOMINATOR")
    true_mean_abs = mean_fraction(rows, "TRUE_DELTA_ABS_NUMERATOR", "TRUE_DELTA_ABS_DENOMINATOR")
    false_mean_signed = mean_fraction(rows, "FALSE_DELTA_SIGNED_MEAN_NUMERATOR", "FALSE_DELTA_SIGNED_MEAN_DENOMINATOR")
    false_mean_abs = mean_fraction(rows, "FALSE_DELTA_ABS_MEAN_NUMERATOR", "FALSE_DELTA_ABS_MEAN_DENOMINATOR")
    true_false_abs_diff_mean = mean_fraction(
        rows,
        "TRUE_FALSE_ABS_DIFF_NUMERATOR",
        "TRUE_FALSE_ABS_DIFF_DENOMINATOR",
    )

    true_abs_max_value, true_abs_max_fraction, true_abs_max_count = max_value_and_count_tol_1e7(
        rows,
        "TRUE_DELTA_ABS_VALUE",
        "TRUE_DELTA_ABS_FRACTION",
    )
    false_abs_max_value, false_abs_max_fraction, false_abs_max_count = max_value_and_count_tol_1e7(
        rows,
        "FALSE_DELTA_ABS_MAX_VALUE",
        "FALSE_DELTA_ABS_MAX_FRACTION",
    )

    aggregate_row = {
        "RUN_ID": rows[0]["RUN_ID"],
        "SERIES_LABEL": rows[0]["SERIES_LABEL"],
        "SAMPLE_MODE": rows[0]["SAMPLE_MODE"],
        "SAMPLE_FACTOR_M": rows[0]["SAMPLE_FACTOR_M"],
        "SAMPLE_CAP": rows[0]["SAMPLE_CAP"],
        "THREAD_COUNT": rows[0]["THREAD_COUNT"],
        "SEED": rows[0]["SEED"],
        "FULL_MATERIAL": rows[0]["FULL_MATERIAL"],
        "KEY_MIX": rows[0].get("KEY_MIX", "modadd"),
        "ITERATION_COUNT": str(len(rows)),
        "TOP1_COUNT": str(sum(rank <= 1 for rank in ranks)),
        "TOP3_COUNT": str(sum(rank <= 3 for rank in ranks)),
        "TOP32_COUNT": str(sum(rank <= 32 for rank in ranks)),
        "TRUE_RANK_MEAN": f"{statistics.mean(ranks):.6f}",
        "TRUE_RANK_MAX": str(max(ranks)),
        "SAMPLE_COUNT_MEAN": f"{statistics.mean(sample_counts):.6f}",
        "SAMPLE_COUNT_MIN": str(min(sample_counts)),
        "SAMPLE_COUNT_MAX": str(max(sample_counts)),
        "MEAN_TRUE_DELTA_SIGNED_VALUE": f"{float(true_mean_signed):.10f}",
        "MEAN_TRUE_DELTA_SIGNED_FRACTION": fraction_to_text(true_mean_signed),
        "MEAN_TRUE_DELTA_ABS_VALUE": f"{float(true_mean_abs):.10f}",
        "MEAN_TRUE_DELTA_ABS_FRACTION": fraction_to_text(true_mean_abs),
        "MEAN_FALSE_DELTA_SIGNED_VALUE": f"{float(false_mean_signed):.10f}",
        "MEAN_FALSE_DELTA_SIGNED_FRACTION": fraction_to_text(false_mean_signed),
        "MEAN_FALSE_DELTA_ABS_VALUE": f"{float(false_mean_abs):.10f}",
        "MEAN_FALSE_DELTA_ABS_FRACTION": fraction_to_text(false_mean_abs),
        "MEAN_TRUE_FALSE_ABS_RATIO_VALUE": f"{statistics.mean(true_false_abs_ratio_values):.10f}",
        "MEAN_TRUE_FALSE_ABS_DIFF_VALUE": f"{float(true_false_abs_diff_mean):.10f}",
        "MEAN_TRUE_FALSE_ABS_DIFF_FRACTION": fraction_to_text(true_false_abs_diff_mean),
        "TRUE_DELTA_ABS_MAX_VALUE": f"{true_abs_max_value:.10f}",
        "TRUE_DELTA_ABS_MAX_FRACTION": true_abs_max_fraction,
        "TRUE_DELTA_ABS_MAX_EXPERIMENT_COUNT_TOL_1E_7": str(true_abs_max_count),
        "FALSE_DELTA_ABS_MAX_VALUE": f"{false_abs_max_value:.10f}",
        "FALSE_DELTA_ABS_MAX_FRACTION": false_abs_max_fraction,
        "FALSE_DELTA_ABS_MAX_EXPERIMENT_COUNT_TOL_1E_7": str(false_abs_max_count),
        "FALSE_DELTA_ABS_MAX_KEY_COUNT_MEAN_TOL_1E_7": f"{statistics.mean(false_abs_max_key_counts):.6f}",
        "FALSE_DELTA_ABS_MAX_KEY_COUNT_MAX_TOL_1E_7": str(max(false_abs_max_key_counts)),
        "TOTAL_TIME_SUM_SEC": f"{sum(total_times):.4f}",
        "TOTAL_TIME_MEAN_SEC": f"{statistics.mean(total_times):.4f}",
        "DELTA_TIME_MEAN_SEC": f"{statistics.mean(delta_times):.4f}",
        "RECOVERY_TIME_MEAN_SEC": f"{statistics.mean(recovery_times):.4f}",
    }

    if "LAST4_TRUE_PRODUCT_VALUE" in rows[0]:
        last4_true_products = [float(row["LAST4_TRUE_PRODUCT_VALUE"]) for row in rows]
        last4_false_max_products = [float(row["LAST4_FALSE_PRODUCT_MAX_VALUE"]) for row in rows]
        last4_false_mean_products = [float(row["LAST4_FALSE_PRODUCT_MEAN_VALUE"]) for row in rows]
        last4_false_true_ratios = [float(row["LAST4_FALSE_TRUE_PRODUCT_RATIO"]) for row in rows]
        last4_false_mean_true_ratios = [float(row["LAST4_FALSE_MEAN_TRUE_PRODUCT_RATIO"]) for row in rows]
        last4_times = [float(row["LAST4_TIME_SEC"]) for row in rows]
        aggregate_row.update(
            {
                "MEAN_LAST4_TRUE_PRODUCT_VALUE": f"{statistics.mean(last4_true_products):.12f}",
                "MEAN_LAST4_FALSE_PRODUCT_MAX_VALUE": f"{statistics.mean(last4_false_max_products):.12f}",
                "MEAN_LAST4_FALSE_PRODUCT_MEAN_VALUE": f"{statistics.mean(last4_false_mean_products):.12f}",
                "MAX_LAST4_FALSE_PRODUCT_MAX_VALUE": f"{max(last4_false_max_products):.12f}",
                "MEAN_LAST4_FALSE_TRUE_PRODUCT_RATIO": f"{statistics.mean(last4_false_true_ratios):.12f}",
                "MEAN_LAST4_FALSE_MEAN_TRUE_PRODUCT_RATIO": f"{statistics.mean(last4_false_mean_true_ratios):.12f}",
                "LAST4_TIME_MEAN_SEC": f"{statistics.mean(last4_times):.4f}",
            }
        )

    with aggregate_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(aggregate_row.keys()))
        writer.writeheader()
        writer.writerow(aggregate_row)

    lines = [
        f"RUN_ID,{aggregate_row['RUN_ID']}",
        f"SERIES_LABEL,{aggregate_row['SERIES_LABEL']}",
        f"KEY_MIX,{aggregate_row['KEY_MIX']}",
        f"ITERATION_COUNT,{aggregate_row['ITERATION_COUNT']}",
        f"TOP1_COUNT,{aggregate_row['TOP1_COUNT']}",
        f"TOP3_COUNT,{aggregate_row['TOP3_COUNT']}",
        f"TOP32_COUNT,{aggregate_row['TOP32_COUNT']}",
        f"TRUE_RANK_MEAN,{aggregate_row['TRUE_RANK_MEAN']}",
        f"TRUE_RANK_MAX,{aggregate_row['TRUE_RANK_MAX']}",
        f"MEAN_TRUE_DELTA_SIGNED,{aggregate_row['MEAN_TRUE_DELTA_SIGNED_VALUE']} ({aggregate_row['MEAN_TRUE_DELTA_SIGNED_FRACTION']})",
        f"MEAN_TRUE_DELTA_ABS,{aggregate_row['MEAN_TRUE_DELTA_ABS_VALUE']} ({aggregate_row['MEAN_TRUE_DELTA_ABS_FRACTION']})",
        f"MEAN_FALSE_DELTA_SIGNED,{aggregate_row['MEAN_FALSE_DELTA_SIGNED_VALUE']} ({aggregate_row['MEAN_FALSE_DELTA_SIGNED_FRACTION']})",
        f"MEAN_FALSE_DELTA_ABS,{aggregate_row['MEAN_FALSE_DELTA_ABS_VALUE']} ({aggregate_row['MEAN_FALSE_DELTA_ABS_FRACTION']})",
        f"MEAN_TRUE_FALSE_ABS_RATIO,{aggregate_row['MEAN_TRUE_FALSE_ABS_RATIO_VALUE']}",
        f"MEAN_TRUE_FALSE_ABS_DIFF,{aggregate_row['MEAN_TRUE_FALSE_ABS_DIFF_VALUE']} ({aggregate_row['MEAN_TRUE_FALSE_ABS_DIFF_FRACTION']})",
        f"TRUE_DELTA_ABS_MAX,{aggregate_row['TRUE_DELTA_ABS_MAX_VALUE']} ({aggregate_row['TRUE_DELTA_ABS_MAX_FRACTION']})",
        f"TRUE_DELTA_ABS_MAX_EXPERIMENT_COUNT_TOL_1E_7,{aggregate_row['TRUE_DELTA_ABS_MAX_EXPERIMENT_COUNT_TOL_1E_7']}",
        f"FALSE_DELTA_ABS_MAX,{aggregate_row['FALSE_DELTA_ABS_MAX_VALUE']} ({aggregate_row['FALSE_DELTA_ABS_MAX_FRACTION']})",
        f"FALSE_DELTA_ABS_MAX_EXPERIMENT_COUNT_TOL_1E_7,{aggregate_row['FALSE_DELTA_ABS_MAX_EXPERIMENT_COUNT_TOL_1E_7']}",
        f"FALSE_DELTA_ABS_MAX_KEY_COUNT_MEAN_TOL_1E_7,{aggregate_row['FALSE_DELTA_ABS_MAX_KEY_COUNT_MEAN_TOL_1E_7']}",
        f"FALSE_DELTA_ABS_MAX_KEY_COUNT_MAX_TOL_1E_7,{aggregate_row['FALSE_DELTA_ABS_MAX_KEY_COUNT_MAX_TOL_1E_7']}",
        f"TOTAL_TIME_SUM_SEC,{aggregate_row['TOTAL_TIME_SUM_SEC']}",
        f"TOTAL_TIME_MEAN_SEC,{aggregate_row['TOTAL_TIME_MEAN_SEC']}",
    ]
    if "MEAN_LAST4_TRUE_PRODUCT_VALUE" in aggregate_row:
        lines.extend(
            [
                f"MEAN_LAST4_TRUE_PRODUCT,{aggregate_row['MEAN_LAST4_TRUE_PRODUCT_VALUE']}",
                f"MEAN_LAST4_FALSE_PRODUCT_MAX,{aggregate_row['MEAN_LAST4_FALSE_PRODUCT_MAX_VALUE']}",
                f"MEAN_LAST4_FALSE_PRODUCT_MEAN,{aggregate_row['MEAN_LAST4_FALSE_PRODUCT_MEAN_VALUE']}",
                f"MAX_LAST4_FALSE_PRODUCT_MAX,{aggregate_row['MAX_LAST4_FALSE_PRODUCT_MAX_VALUE']}",
                f"MEAN_LAST4_FALSE_TRUE_PRODUCT_RATIO,{aggregate_row['MEAN_LAST4_FALSE_TRUE_PRODUCT_RATIO']}",
                f"MEAN_LAST4_FALSE_MEAN_TRUE_PRODUCT_RATIO,{aggregate_row['MEAN_LAST4_FALSE_MEAN_TRUE_PRODUCT_RATIO']}",
                f"LAST4_TIME_MEAN_SEC,{aggregate_row['LAST4_TIME_MEAN_SEC']}",
            ]
        )
    aggregate_md.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
