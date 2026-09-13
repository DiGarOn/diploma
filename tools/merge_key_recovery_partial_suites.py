#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


SERIES_NAMES = ("adaptive_m1", "adaptive_m5", "adaptive_m10", "full_material")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Merge partial key-recovery suite directories into one reportable suite."
    )
    parser.add_argument("--output-suite-dir", required=True)
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("partial_suite_dirs", nargs="+")
    return parser.parse_args()


def copy_file_if_missing(src: Path, dst: Path) -> None:
    if src.exists() and not dst.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_suite_dir).resolve()
    partial_dirs = [Path(p).resolve() for p in args.partial_suite_dirs]

    output_dir.mkdir(parents=True, exist_ok=True)

    copied_series: list[str] = []
    for partial_dir in partial_dirs:
        if not partial_dir.exists():
            raise SystemExit(f"partial suite dir does not exist: {partial_dir}")

        copy_file_if_missing(partial_dir / "th1h2t_verification.csv", output_dir / "th1h2t_verification.csv")
        copy_file_if_missing(partial_dir / "build_backend.txt", output_dir / "build_backend.txt")

        for series_name in SERIES_NAMES:
            src = partial_dir / series_name
            if not src.exists():
                continue
            dst = output_dir / series_name
            if dst.exists():
                if not args.overwrite:
                    raise SystemExit(
                        f"series already exists in output: {dst}; use --overwrite to replace it"
                    )
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
            copied_series.append(series_name)

    if not copied_series:
        raise SystemExit("no series directories were copied")

    script = Path(__file__).resolve().parent / "build_key_recovery_suite_artifacts.py"
    subprocess.run(
        [sys.executable, str(script), "--suite-dir", str(output_dir)],
        check=True,
    )

    print(f"output_suite_dir={output_dir}")
    print(f"copied_series={','.join(copied_series)}")
    print(f"report={output_dir / 'report.md'}")
    print(f"package={output_dir / ('for_teacher_key_recovery_results_' + output_dir.name)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

