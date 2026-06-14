#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from minigost import MiniGost  # noqa: E402


def parse_u32(value: str) -> int:
    parsed = int(value, 0)
    if not 0 <= parsed <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("32-bit key must be in range 0..0xFFFFFFFF")
    return parsed


def build_runner() -> Path:
    runner = ROOT / "build" / "minigost_c_runner"
    subprocess.run(
        ["make", str(runner.relative_to(ROOT))],
        cwd=ROOT,
        check=True,
    )
    return runner


def run_c_batch(runner: Path, mode: str, c_key: int, blocks: range) -> list[int]:
    stdin_payload = "".join(f"{block:04X}\n" for block in blocks)
    completed = subprocess.run(
        [str(runner), mode, f"0x{c_key:08X}"],
        cwd=ROOT,
        input=stdin_payload,
        text=True,
        capture_output=True,
        check=False,
    )

    if completed.returncode != 0:
        raise RuntimeError(
            f"C runner failed for mode={mode} with exit code {completed.returncode}:\n"
            f"{completed.stderr.strip()}"
        )

    lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
    if len(lines) != len(blocks):
        raise RuntimeError(
            f"C runner returned {len(lines)} results for {len(blocks)} blocks in {mode} mode"
        )

    return [int(line, 16) for line in lines]


def run_python_batch(cipher: MiniGost, mode: str, py_key: int, blocks: range) -> list[int]:
    fn = cipher.encrypt_block if mode == "encrypt" else cipher.decrypt_block
    return [fn(block, py_key) for block in blocks]


def compare_mode(
    cipher: MiniGost,
    runner: Path,
    mode: str,
    py_key: int,
    c_key: int,
    max_mismatches: int,
) -> tuple[int, list[str]]:
    blocks = range(0x10000)
    py_results = run_python_batch(cipher, mode, py_key, blocks)
    c_results = run_c_batch(runner, mode, c_key, blocks)

    mismatches: list[str] = []
    mismatch_count = 0

    for block, (py_value, c_value) in enumerate(zip(py_results, c_results)):
        if py_value == c_value:
            continue

        mismatch_count += 1
        if len(mismatches) < max_mismatches:
            mismatches.append(
                f"block=0x{block:04X}: python=0x{py_value:04X}, c=0x{c_value:04X}"
            )

    return mismatch_count, mismatches


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Сравнивает pure-Python minigost.py и C-реализацию src/minigost.c "
            "на всех 65536 значениях 16-битного блока."
        )
    )
    parser.add_argument(
        "--py-key",
        type=parse_u32,
        required=True,
        help="32-битный ключ для Python-реализации, например 0xA1B2C3D4.",
    )
    parser.add_argument(
        "--c-key",
        type=parse_u32,
        help=(
            "32-битный ключ для C-реализации. Если не задан, будет использован тот же ключ, "
            "что и для Python (--py-key)."
        ),
    )
    parser.add_argument(
        "--mode",
        choices=["encrypt", "decrypt", "both"],
        default="both",
        help="Что сравнивать: шифрование, расшифрование или оба режима.",
    )
    parser.add_argument(
        "--max-mismatches",
        type=int,
        default=10,
        help="Сколько первых несовпадений печатать в отчете.",
    )
    args = parser.parse_args()

    if args.max_mismatches < 1:
        parser.error("--max-mismatches must be >= 1")

    py_key = args.py_key
    c_key = args.c_key if args.c_key is not None else py_key
    runner = build_runner()
    cipher = MiniGost()

    print(f"Python key : 0x{py_key:08X}")
    print(f"C key      : 0x{c_key:08X}")
    print(f"Key mode   : {'explicit' if args.c_key is not None else 'same as Python'}")
    print("Block space: 65536 values (0x0000..0xFFFF)")

    modes = ["encrypt", "decrypt"] if args.mode == "both" else [args.mode]
    overall_ok = True

    for mode in modes:
        mismatch_count, mismatches = compare_mode(
            cipher=cipher,
            runner=runner,
            mode=mode,
            py_key=py_key,
            c_key=c_key,
            max_mismatches=args.max_mismatches,
        )

        if mismatch_count == 0:
            print(f"[OK] {mode}: all 65536 blocks matched")
            continue

        overall_ok = False
        print(f"[FAIL] {mode}: {mismatch_count} mismatches")
        for mismatch in mismatches:
            print(f"  {mismatch}")

    if not overall_ok:
        print(
            "\nNote: full exhaustive checking over (block, key) is not practical here, "
            "because the combined input space is vastly larger than 65536."
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
