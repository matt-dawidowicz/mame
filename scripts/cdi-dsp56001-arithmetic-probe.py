#!/usr/bin/env python3
"""Generate DSP56001 arithmetic discrimination vectors for the CD-i campaign.

The DSP56001 architecture is documented; the Philips VMPEG firmware instruction
sequence is not.  This tool therefore emits candidate boundary behavior rather
than asserting that any one model is the CD-i attenuation implementation.
"""

from __future__ import annotations

import argparse
import csv
import sys

WORD_MIN = -(1 << 23)
WORD_MAX = (1 << 23) - 1


def round_nearest_even(value: int, shift: int) -> int:
    if shift == 0:
        return value
    magnitude = abs(value)
    quotient, remainder = divmod(magnitude, 1 << shift)
    half = 1 << (shift - 1)
    if remainder > half or (remainder == half and (quotient & 1)):
        quotient += 1
    return -quotient if value < 0 else quotient


def round_nearest_away(value: int, shift: int) -> int:
    if shift == 0:
        return value
    magnitude = abs(value)
    quotient, remainder = divmod(magnitude, 1 << shift)
    if remainder >= (1 << (shift - 1)):
        quotient += 1
    return -quotient if value < 0 else quotient


def truncate_toward_zero(value: int, shift: int) -> int:
    if shift == 0:
        return value
    magnitude = abs(value) >> shift
    return -magnitude if value < 0 else magnitude


def limit_word(value: int) -> int:
    return max(WORD_MIN, min(WORD_MAX, value))


def verify() -> None:
    ties = {
        1: (0, 1, 0),
        3: (2, 2, 1),
        5: (2, 3, 2),
        7: (4, 4, 3),
        -1: (0, -1, 0),
        -3: (-2, -2, -1),
        -5: (-2, -3, -2),
        -7: (-4, -4, -3),
    }
    for value, expected in ties.items():
        observed = (
            round_nearest_even(value, 1),
            round_nearest_away(value, 1),
            truncate_toward_zero(value, 1),
        )
        if observed != expected:
            raise SystemExit(f"rounding verification failed for {value}: {observed!r}")

    limiter = {
        WORD_MIN - 1: WORD_MIN,
        WORD_MIN: WORD_MIN,
        WORD_MAX: WORD_MAX,
        WORD_MAX + 1: WORD_MAX,
    }
    for value, expected in limiter.items():
        if limit_word(value) != expected:
            raise SystemExit(f"limiter verification failed for {value}")


def vectors(shift: int) -> list[int]:
    unit = 1 << shift
    half = 1 << (shift - 1) if shift else 0
    values: set[int] = {0, WORD_MIN - 1, WORD_MIN, WORD_MIN + 1, WORD_MAX - 1, WORD_MAX, WORD_MAX + 1}
    if shift:
        # Four parity-distinguishing half-way boundaries on both signs, plus
        # one LSB below and above each tie.  These are the smallest captures
        # that separate convergent, nearest-away, and truncation candidates.
        for quotient in range(4):
            tie = quotient * unit + half
            for delta in (-1, 0, 1):
                values.add(tie + delta)
                values.add(-(tie + delta))
    return sorted(values)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shift", type=int, default=24)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if not 0 <= args.shift <= 62:
        parser.error("shift must be between 0 and 62")
    if args.verify:
        verify()

    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow((
        "input_integer",
        "shift",
        "nearest_even",
        "nearest_away",
        "truncate_toward_zero",
        "nearest_even_limited24",
    ))
    for value in vectors(args.shift):
        even = round_nearest_even(value, args.shift)
        writer.writerow((
            value,
            args.shift,
            even,
            round_nearest_away(value, args.shift),
            truncate_toward_zero(value, args.shift),
            limit_word(even),
        ))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
