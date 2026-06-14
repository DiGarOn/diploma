#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import math
import shutil
import subprocess
import tempfile
from pathlib import Path
from statistics import mean, median, pstdev

from csv_to_xlsx import convert_tree


SERIES_LAYOUT = [
    ("adaptive_m10", "01_adaptive_m10", "Серия material / 10 по delta^2"),
    ("adaptive_m100", "02_adaptive_m100", "Серия material / 100 по delta^2"),
    ("full_material", "03_full_material", "Прогон по полному материалу"),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--suite-dir", required=True)
    return parser.parse_args()


def load_single_row_csv(path: Path) -> dict[str, str]:
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise ValueError(f"CSV is empty: {path}")
    return rows[0]


def write_csv(path: Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def percentile(sorted_values: list[float], p: float) -> float:
    if not sorted_values:
        return float("nan")
    if len(sorted_values) == 1:
        return sorted_values[0]
    pos = (len(sorted_values) - 1) * p
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return sorted_values[lo]
    frac = pos - lo
    return sorted_values[lo] * (1.0 - frac) + sorted_values[hi] * frac


def safe_float(text: str) -> float:
    return float(text)


def build_false_delta_distribution(summary_path: Path, series_label: str) -> dict[str, str]:
    values: list[float] = []
    with summary_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            values.append(float(row["FALSE_DELTA_SIGNED_MEAN_VALUE"]))

    if not values:
        raise ValueError(f"No rows found in {summary_path}")

    sorted_values = sorted(values)
    n = len(values)
    avg = mean(values)
    sigma = pstdev(values)
    positives = sum(1 for v in values if v > 0.0)
    negatives = sum(1 for v in values if v < 0.0)
    zeros = n - positives - negatives

    if sigma == 0.0:
        skewness = 0.0
        excess_kurtosis = 0.0
    else:
        centered3 = mean((v - avg) ** 3 for v in values)
        centered4 = mean((v - avg) ** 4 for v in values)
        skewness = centered3 / (sigma ** 3)
        excess_kurtosis = centered4 / (sigma ** 4) - 3.0

    return {
        "SERIES_LABEL": series_label,
        "ITERATION_COUNT": str(n),
        "FALSE_DELTA_MEAN": f"{avg:.12f}",
        "FALSE_DELTA_STDDEV": f"{sigma:.12f}",
        "FALSE_DELTA_MEDIAN": f"{median(values):.12f}",
        "FALSE_DELTA_MIN": f"{sorted_values[0]:.12f}",
        "FALSE_DELTA_MAX": f"{sorted_values[-1]:.12f}",
        "FALSE_DELTA_ABS_MEAN": f"{mean(abs(v) for v in values):.12f}",
        "FALSE_DELTA_P01": f"{percentile(sorted_values, 0.01):.12f}",
        "FALSE_DELTA_P05": f"{percentile(sorted_values, 0.05):.12f}",
        "FALSE_DELTA_P25": f"{percentile(sorted_values, 0.25):.12f}",
        "FALSE_DELTA_P50": f"{percentile(sorted_values, 0.50):.12f}",
        "FALSE_DELTA_P75": f"{percentile(sorted_values, 0.75):.12f}",
        "FALSE_DELTA_P95": f"{percentile(sorted_values, 0.95):.12f}",
        "FALSE_DELTA_P99": f"{percentile(sorted_values, 0.99):.12f}",
        "POSITIVE_SHARE": f"{positives / n:.6f}",
        "NEGATIVE_SHARE": f"{negatives / n:.6f}",
        "ZERO_COUNT": str(zeros),
        "SKEWNESS": f"{skewness:.6f}",
        "EXCESS_KURTOSIS": f"{excess_kurtosis:.6f}",
    }


def build_readable_overview_row(aggregate_row: dict[str, str]) -> dict[str, str]:
    return {
        "SERIES": aggregate_row["RUN_ID"],
        "RUNS": aggregate_row["ITERATION_COUNT"],
        "TOP1": aggregate_row["TOP1_COUNT"],
        "TOP3": aggregate_row["TOP3_COUNT"],
        "TOP5": aggregate_row["TOP5_COUNT"],
        "TOP32": aggregate_row["TOP32_COUNT"],
        "TOP100": aggregate_row["TOP100_COUNT"],
        "TRUE_RANK_MEAN": aggregate_row["TRUE_RANK_MEAN"],
        "TRUE_RANK_MEDIAN": aggregate_row["TRUE_RANK_MEDIAN"],
        "TRUE_RANK_MAX": aggregate_row["TRUE_RANK_MAX"],
        "SAMPLE_COUNT_MEAN": aggregate_row["SAMPLE_COUNT_MEAN"],
        "SAMPLE_COUNT_MIN": aggregate_row["SAMPLE_COUNT_MIN"],
        "SAMPLE_COUNT_MAX": aggregate_row["SAMPLE_COUNT_MAX"],
        "PREFIX_DELTA_ABS_MIN": aggregate_row["PREFIX_DELTA_ABS_MIN_FRACTION"],
        "PREFIX_DELTA_ABS_MAX": aggregate_row["PREFIX_DELTA_ABS_MAX_FRACTION"],
        "MAX_PAIR_COUNT_MEAN": aggregate_row["PREFIX_MAX_PAIR_COUNT_MEAN"],
        "MAX_PAIR_COUNT_MIN": aggregate_row["PREFIX_MAX_PAIR_COUNT_MIN"],
        "MAX_PAIR_COUNT_MAX": aggregate_row["PREFIX_MAX_PAIR_COUNT_MAX"],
        "MEAN_TRUE_DELTA_SIGNED": aggregate_row["MEAN_TRUE_DELTA_SIGNED_VALUE"],
        "MEAN_FALSE_DELTA_SIGNED": aggregate_row["MEAN_FALSE_DELTA_SIGNED_VALUE"],
        "TOTAL_TIME_MEAN_SEC": aggregate_row["TOTAL_TIME_MEAN_SEC"],
    }


def hypothesis_text(dist_row: dict[str, str]) -> str:
    mean_value = abs(float(dist_row["FALSE_DELTA_MEAN"]))
    stddev = float(dist_row["FALSE_DELTA_STDDEV"])
    positive_share = float(dist_row["POSITIVE_SHARE"])
    skewness = float(dist_row["SKEWNESS"])

    parts = []
    if stddev > 0.0 and mean_value <= 0.1 * stddev:
        parts.append("центр распределения близок к нулю")
    else:
        parts.append("центр распределения заметно смещен относительно нуля")

    if abs(positive_share - 0.5) <= 0.02:
        parts.append("доли положительных и отрицательных значений почти симметричны")
    else:
        parts.append("наблюдается заметная асимметрия по знаку")

    if abs(skewness) <= 0.2:
        parts.append("асимметрия мала")
    elif skewness > 0:
        parts.append("есть правосторонняя асимметрия")
    else:
        parts.append("есть левосторонняя асимметрия")

    return "; ".join(parts)


def render_pdf(markdown_path: Path, pdf_path: Path) -> str | None:
    pandoc = shutil.which("pandoc")
    xelatex = shutil.which("xelatex") or shutil.which("/Library/TeX/texbin/xelatex")
    if not pandoc or not xelatex:
        return "pandoc/xelatex not found"

    header_text = r"""
\usepackage[a4paper, left=1.8cm, right=1.8cm, top=2cm, bottom=2cm]{geometry}
\usepackage{polyglossia}
\setdefaultlanguage{russian}
\usepackage{fontspec}
\setmainfont{Times New Roman}
\setmonofont[Scale=0.8]{PT Mono}
\newfontfamily\cyrillicfonttt[Scale=0.8]{PT Mono}
\usepackage{longtable}
\usepackage{booktabs}
\usepackage{array}
\usepackage{hyperref}
\hypersetup{colorlinks=true, linkcolor=blue, urlcolor=blue}
\setlength{\parskip}{4pt}
\setlength{\parindent}{0pt}
"""

    with tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".tex", delete=False) as tmp:
        tmp.write(header_text)
        header_path = Path(tmp.name)

    try:
        subprocess.run(
            [
                pandoc,
                str(markdown_path),
                "-o",
                str(pdf_path),
                "--pdf-engine",
                xelatex,
                "--include-in-header",
                str(header_path),
                "--toc",
                "--toc-depth=2",
                "-V",
                "fontsize=10pt",
                "-V",
                "documentclass=article",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        return None
    except subprocess.CalledProcessError as exc:
        return exc.stderr.strip() or exc.stdout.strip() or "unknown pandoc error"
    finally:
        header_path.unlink(missing_ok=True)


def copy_if_exists(src: Path, dst: Path) -> None:
    if src.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def write_package_readme(package_dir: Path) -> None:
    (package_dir / "README.txt").write_text(
        "\n".join(
            [
                "Пакет результатов для отправки преподавателю",
                "",
                "В этой папке собраны материалы большого прогона key recovery.",
                "",
                "Структура:",
                "1. overview/",
                "   - 00_series_overview_readable.csv/.xlsx: краткая сводка по сериям.",
                "   - 01_series_overview.csv/.xlsx: полная машинная сводка по сериям.",
                "   - 02_false_delta_distribution.csv/.xlsx: статистика распределения",
                "     средних signed delta по ложным подключам.",
                "   - 03_th1_h2_t_verification.csv/.xlsx: проверка TH1H2T.",
                "   - 04_report.md и 05_report.pdf: отчет на русском языке.",
                "2. series_tables/: полные таблицы и конфигурации по сериям.",
                "",
                "top_candidates_*.csv в пакет не включены, чтобы он оставался компактным.",
                "Их можно приложить отдельно при необходимости.",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def write_assignment_mapping(package_dir: Path) -> None:
    (package_dir / "00_assignment_mapping.md").write_text(
        "\n".join(
            [
                "# Соответствие задачам и материалам",
                "",
                "- Полные данные по прогонам находятся в `series_tables/*/01_summary.csv` и `.xlsx`.",
                "- Конфигурации запусков находятся в `series_tables/*/03_run_config.txt`.",
                "- Сводка по сериям вынесена в `overview/00_series_overview_readable.csv` и `overview/01_series_overview.csv`.",
                "- Проверка корректности TH1H2T находится в `overview/03_th1_h2_t_verification.csv`.",
                "- Статистика по распределению средних signed delta ложных подключей находится в `overview/02_false_delta_distribution.csv`.",
                "- Основной отчет на русском языке находится в `overview/04_report.md` и `overview/05_report.pdf`.",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def build_report_markdown(
    suite_dir: Path,
    aggregate_rows: list[dict[str, str]],
    distribution_rows: list[dict[str, str]],
    verification_row: dict[str, str] | None,
) -> str:
    agg_by_series = {row["RUN_ID"]: row for row in aggregate_rows}
    dist_by_series = {row["SERIES_LABEL"]: row for row in distribution_rows}
    lines: list[str] = []

    lines.append("# Отчет по большому прогону key recovery")
    lines.append("")
    lines.append(f"Каталог прогона: `{suite_dir.name}`.")
    lines.append("")
    lines.append("## 1. Что запускалось")
    lines.append("")
    lines.append("Были подготовлены три серии вычислений:")
    lines.append("")
    lines.append("- `adaptive_m10`: режим material / 10 по `delta^2`.")
    lines.append("- `adaptive_m100`: режим material / 100 по `delta^2`.")
    lines.append("- `full_material`: прогон по полному материалу.")
    lines.append("")

    if verification_row:
        lines.append("## 2. Проверка TH1H2T")
        lines.append("")
        lines.append(f"- ключ: `{verification_row['KEY']}`")
        lines.append(f"- проверено открытых текстов: `{verification_row['TOTAL_PLAINTEXTS']}`")
        lines.append(f"- число несовпадений: `{verification_row['MISMATCH_COUNT']}`")
        lines.append(f"- статус: `{verification_row['STATUS']}`")
        lines.append("")

    lines.append("## 3. Основные результаты по сериям")
    lines.append("")
    for series_name, _, _ in SERIES_LAYOUT:
        aggregate_row = agg_by_series.get(series_name)
        dist_row = dist_by_series.get(series_name)
        if not aggregate_row:
            continue
        lines.append(f"### {series_name}")
        lines.append("")
        lines.append(f"- число прогонов: `{aggregate_row['ITERATION_COUNT']}`")
        lines.append(f"- `top1`: `{aggregate_row['TOP1_COUNT']} / {aggregate_row['ITERATION_COUNT']}`")
        lines.append(f"- `top3`: `{aggregate_row['TOP3_COUNT']} / {aggregate_row['ITERATION_COUNT']}`")
        lines.append(f"- `top5`: `{aggregate_row['TOP5_COUNT']} / {aggregate_row['ITERATION_COUNT']}`")
        lines.append(f"- `top32`: `{aggregate_row['TOP32_COUNT']} / {aggregate_row['ITERATION_COUNT']}`")
        lines.append(f"- `top100`: `{aggregate_row['TOP100_COUNT']} / {aggregate_row['ITERATION_COUNT']}`")
        lines.append(f"- средний ранг истинного ключа: `{aggregate_row['TRUE_RANK_MEAN']}`")
        lines.append(f"- медианный ранг истинного ключа: `{aggregate_row['TRUE_RANK_MEDIAN']}`")
        lines.append(f"- максимальный ранг истинного ключа: `{aggregate_row['TRUE_RANK_MAX']}`")
        lines.append(f"- среднее число максимизирующих пар `alpha/beta`: `{aggregate_row['PREFIX_MAX_PAIR_COUNT_MEAN']}`")
        lines.append(f"- минимум и максимум `|prefix delta|`: `{aggregate_row['PREFIX_DELTA_ABS_MIN_FRACTION']}` и `{aggregate_row['PREFIX_DELTA_ABS_MAX_FRACTION']}`")
        lines.append(f"- средняя signed delta на истинном ключе: `{aggregate_row['MEAN_TRUE_DELTA_SIGNED_VALUE']}`")
        lines.append(f"- средняя signed delta по ложным подключам: `{aggregate_row['MEAN_FALSE_DELTA_SIGNED_VALUE']}`")
        lines.append(f"- среднее время одного прогона, сек: `{aggregate_row['TOTAL_TIME_MEAN_SEC']}`")
        if dist_row:
            lines.append(f"- stddev средних ложных delta: `{dist_row['FALSE_DELTA_STDDEV']}`")
            lines.append(f"- медиана средних ложных delta: `{dist_row['FALSE_DELTA_MEDIAN']}`")
            lines.append(f"- 1% / 99% квантили: `{dist_row['FALSE_DELTA_P01']}` / `{dist_row['FALSE_DELTA_P99']}`")
            lines.append(f"- доля положительных значений: `{dist_row['POSITIVE_SHARE']}`")
            lines.append(f"- гипотеза по распределению: {hypothesis_text(dist_row)}.")
        lines.append("")

    if distribution_rows:
        lines.append("## 4. Наблюдения по распределению средних ложных delta")
        lines.append("")
        lines.append("По файлу `false_delta_distribution.csv` можно проверять гипотезы о распределении.")
        lines.append("На что имеет смысл смотреть в первую очередь:")
        lines.append("")
        lines.append("- насколько среднее близко к нулю относительно стандартного отклонения;")
        lines.append("- насколько близки доли положительных и отрицательных значений;")
        lines.append("- как меняется `stddev` при переходе от `adaptive_m10` к `adaptive_m100` и `full_material`;")
        lines.append("- есть ли выраженная асимметрия по `skewness` и тяжелые хвосты по `excess kurtosis`.")
        lines.append("")

    lines.append("## 5. Где лежат материалы для отправки")
    lines.append("")
    lines.append("- полные таблицы по сериям: `series_tables/*/01_summary.csv` и `.xlsx`;")
    lines.append("- конфигурации запусков: `series_tables/*/03_run_config.txt`;")
    lines.append("- сводка по сериям: `overview/00_series_overview_readable.csv` и `overview/01_series_overview.csv`;")
    lines.append("- распределение ложных delta: `overview/02_false_delta_distribution.csv`;")
    lines.append("- проверка TH1H2T: `overview/03_th1_h2_t_verification.csv`;")
    lines.append("- PDF-версия отчета: `overview/05_report.pdf`.")
    lines.append("")

    return "\n".join(lines) + "\n"


def build_package(
    suite_dir: Path,
    overview_rows: list[dict[str, str]],
    readable_rows: list[dict[str, str]],
    distribution_rows: list[dict[str, str]],
    report_md_path: Path,
    report_pdf_path: Path | None,
) -> Path:
    package_dir = suite_dir / f"for_teacher_key_recovery_results_{suite_dir.name}"
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(parents=True)

    write_package_readme(package_dir)
    write_assignment_mapping(package_dir)

    overview_dir = package_dir / "overview"
    overview_dir.mkdir(parents=True, exist_ok=True)

    series_overview_path = suite_dir / "series_overview.csv"
    readable_path = suite_dir / "series_overview_readable.csv"
    distribution_path = suite_dir / "false_delta_distribution.csv"
    verification_path = suite_dir / "th1h2t_verification.csv"

    copy_if_exists(readable_path, overview_dir / "00_series_overview_readable.csv")
    copy_if_exists(series_overview_path, overview_dir / "01_series_overview.csv")
    copy_if_exists(distribution_path, overview_dir / "02_false_delta_distribution.csv")
    copy_if_exists(verification_path, overview_dir / "03_th1_h2_t_verification.csv")
    copy_if_exists(report_md_path, overview_dir / "04_report.md")
    if report_pdf_path and report_pdf_path.exists():
        copy_if_exists(report_pdf_path, overview_dir / "05_report.pdf")

    series_root = package_dir / "series_tables"
    for series_name, folder_name, _ in SERIES_LAYOUT:
        src_dir = suite_dir / series_name
        if not src_dir.exists():
            continue
        dst_dir = series_root / folder_name
        dst_dir.mkdir(parents=True, exist_ok=True)
        copy_if_exists(src_dir / "summary.csv", dst_dir / "01_summary.csv")
        copy_if_exists(src_dir / "aggregate_report.csv", dst_dir / "02_aggregate_report.csv")
        copy_if_exists(src_dir / "run_config.txt", dst_dir / "03_run_config.txt")

    convert_tree(package_dir)
    return package_dir


def main() -> int:
    args = parse_args()
    suite_dir = Path(args.suite_dir).resolve()

    aggregate_rows: list[dict[str, str]] = []
    readable_rows: list[dict[str, str]] = []
    distribution_rows: list[dict[str, str]] = []

    for series_name, _, _ in SERIES_LAYOUT:
        series_dir = suite_dir / series_name
        aggregate_path = series_dir / "aggregate_report.csv"
        summary_path = series_dir / "summary.csv"
        if not aggregate_path.exists() or not summary_path.exists():
            continue
        aggregate_row = load_single_row_csv(aggregate_path)
        aggregate_rows.append(aggregate_row)
        readable_rows.append(build_readable_overview_row(aggregate_row))
        distribution_rows.append(build_false_delta_distribution(summary_path, series_name))

    if aggregate_rows:
        write_csv(suite_dir / "series_overview.csv", aggregate_rows, list(aggregate_rows[0].keys()))
        write_csv(suite_dir / "series_overview_readable.csv", readable_rows, list(readable_rows[0].keys()))

    if distribution_rows:
        write_csv(suite_dir / "false_delta_distribution.csv", distribution_rows, list(distribution_rows[0].keys()))

    verification_row = None
    verification_path = suite_dir / "th1h2t_verification.csv"
    if verification_path.exists():
        verification_row = load_single_row_csv(verification_path)

    report_md_path = suite_dir / "report.md"
    report_md_path.write_text(
        build_report_markdown(suite_dir, aggregate_rows, distribution_rows, verification_row),
        encoding="utf-8",
    )

    report_pdf_path = suite_dir / "report.pdf"
    pdf_error = render_pdf(report_md_path, report_pdf_path)
    if pdf_error:
        (suite_dir / "report_pdf_error.txt").write_text(pdf_error + "\n", encoding="utf-8")
        if report_pdf_path.exists():
            report_pdf_path.unlink()

    package_dir = build_package(
        suite_dir=suite_dir,
        overview_rows=aggregate_rows,
        readable_rows=readable_rows,
        distribution_rows=distribution_rows,
        report_md_path=report_md_path,
        report_pdf_path=report_pdf_path if report_pdf_path.exists() else None,
    )

    print(f"suite_dir={suite_dir}")
    print(f"package_dir={package_dir}")
    if pdf_error:
        print(f"pdf_status=warning:{pdf_error}")
    else:
        print(f"report_pdf={report_pdf_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
