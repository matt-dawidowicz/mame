#!/usr/bin/env python3
"""Generate CD-i attenuation candidate discrimination vectors.

This is measurement tooling, not an emulator hardware model.  It projects the
Green Book nominal 1 dB curve onto candidate unsigned Q-format coefficient grids
so real-hardware captures can eliminate incompatible widths.
"""

from __future__ import annotations

import argparse
import csv
import math
import sys


def ideal_gain(db: int) -> float:
    return 10.0 ** (-db / 20.0)


def coefficient(db: int, fractional_bits: int) -> int:
    scale = 1 << fractional_bits
    return int(ideal_gain(db) * scale + 0.5)


def reconstructed_gain(db: int, fractional_bits: int) -> float:
    return coefficient(db, fractional_bits) / float(1 << fractional_bits)


def first_zero(fractional_bits: int) -> int | None:
    for db in range(128):
        if coefficient(db, fractional_bits) == 0:
            return db
    return None


def verify() -> None:
    expected = {
        12: 79,
        13: 85,
        14: 91,
        15: 97,
        16: 103,
        17: 109,
        18: 115,
        19: 121,
        20: 127,
        21: None,
        22: None,
        23: None,
        24: None,
    }
    observed = {bits: first_zero(bits) for bits in expected}
    if observed != expected:
        raise SystemExit(f"candidate-grid verification failed: {observed!r}")


def parse_widths(value: str) -> list[int]:
    widths = [int(item) for item in value.split(",") if item]
    if not widths or any(bits < 1 or bits > 52 for bits in widths):
        raise argparse.ArgumentTypeError("widths must be comma-separated integers from 1 through 52")
    return widths


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--start-db", type=int, default=72)
    parser.add_argument("--end-db", type=int, default=127)
    parser.add_argument("--widths", type=parse_widths, default=parse_widths("12,13,14,15,16,17,18,19,20,21,22,23,24,31"))
    parser.add_argument("--full-scale-pcm", type=float, default=32767.0)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()

    if not (0 <= args.start_db <= args.end_db <= 127):
        parser.error("attenuation range must satisfy 0 <= start <= end <= 127")

    if args.verify:
        verify()

    writer = csv.writer(sys.stdout, lineterminator="\n")
    header = ["attenuation_db", "ideal_gain", "ideal_pcm_peak"]
    for bits in args.widths:
        header.extend((f"q{bits}_coefficient", f"q{bits}_gain", f"q{bits}_pcm_peak"))
    writer.writerow(header)

    for db in range(args.start_db, args.end_db + 1):
        ideal = ideal_gain(db)
        row: list[object] = [db, f"{ideal:.17g}", f"{args.full_scale_pcm * ideal:.17g}"]
        for bits in args.widths:
            coeff = coefficient(db, bits)
            gain = reconstructed_gain(db, bits)
            row.extend((coeff, f"{gain:.17g}", f"{args.full_scale_pcm * gain:.17g}"))
        writer.writerow(row)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
