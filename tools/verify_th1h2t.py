#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from minigost import MiniGost


FULL_SCHEDULE = [3, 2, 1, 0, 3, 2, 1, 0, 0, 1, 2, 3]
PREFIX10_SCHEDULE = [3, 2, 1, 0, 3, 2, 1, 0, 0, 1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--key", default="0xDEADBEEF")
    parser.add_argument("--output", required=True)
    return parser.parse_args()


def split_key_be(key: int) -> list[int]:
    return [(key >> 24) & 0xFF, (key >> 16) & 0xFF, (key >> 8) & 0xFF, key & 0xFF]


def encrypt_prefix10(cipher: MiniGost, block: int, key: int) -> int:
    left = (block >> 8) & 0xFF
    right = block & 0xFF
    round_keys = split_key_be(key)
    for idx in PREFIX10_SCHEDULE:
        left, right = right ^ cipher.g(round_keys[idx], left), left
    return ((left & 0xFF) << 8) | (right & 0xFF)


def peel_th1h2t(cipher: MiniGost, ciphertext: int, key: int) -> int:
    round_keys = split_key_be(key)
    k1 = round_keys[3]
    k2 = round_keys[2]
    cipher_hi = (ciphertext >> 8) & 0xFF
    cipher_lo = ciphertext & 0xFF
    u = cipher_lo ^ cipher.g(k1, cipher_hi)
    v = cipher_hi ^ cipher.g(k2, u)
    return ((u & 0xFF) << 8) | (v & 0xFF)


def main() -> int:
    args = parse_args()
    key = int(args.key, 0)
    output_path = Path(args.output)
    cipher = MiniGost()

    mismatch_count = 0
    first_mismatch_plain = ""
    first_expected = ""
    first_actual = ""

    for plaintext in range(0x10000):
        ciphertext = cipher.encrypt_block(plaintext, key)
        expected = encrypt_prefix10(cipher, plaintext, key)
        actual = peel_th1h2t(cipher, ciphertext, key)
        if expected != actual:
            mismatch_count += 1
            if not first_mismatch_plain:
                first_mismatch_plain = f"0x{plaintext:04X}"
                first_expected = f"0x{expected:04X}"
                first_actual = f"0x{actual:04X}"

    with output_path.open("w", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "KEY",
                "TOTAL_PLAINTEXTS",
                "MISMATCH_COUNT",
                "FIRST_MISMATCH_PLAINTEXT",
                "FIRST_EXPECTED_PREFIX10",
                "FIRST_ACTUAL_TH1H2T",
                "STATUS",
            ],
        )
        writer.writeheader()
        writer.writerow(
            {
                "KEY": f"0x{key:08X}",
                "TOTAL_PLAINTEXTS": "65536",
                "MISMATCH_COUNT": str(mismatch_count),
                "FIRST_MISMATCH_PLAINTEXT": first_mismatch_plain,
                "FIRST_EXPECTED_PREFIX10": first_expected,
                "FIRST_ACTUAL_TH1H2T": first_actual,
                "STATUS": "OK" if mismatch_count == 0 else "FAIL",
            }
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
