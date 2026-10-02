#!/usr/bin/env python3
"""Classify Philips CD-i VMPEG FMA DSP attenuation evidence.

The script intentionally consumes extracted 24-bit words or disassembly text;
it never requires a copyrighted VMPEG ROM to be checked into the repository.

Examples:
  python3 scripts/cdi-fma-dsp-evidence.py --emit-q22
  python3 scripts/cdi-fma-dsp-evidence.py --words fma-y-words.txt
  python3 scripts/cdi-fma-dsp-evidence.py --disassembly fma-p-disassembly.txt
"""

from __future__ import annotations

import argparse
import math
import re
from pathlib import Path

Q22_SCALE = 1 << 22
Q22_WORDS = tuple(round(Q22_SCALE * 10 ** (-db / 20.0)) for db in range(128))
ARITHMETIC_MNEMONICS = ("mpy", "mpyr", "mac", "macr", "rnd")


def parse_words(path: Path) -> list[int]:
    words: list[int] = []
    for lineno, raw in enumerate(path.read_text(errors="strict").splitlines(), 1):
        line = raw.split("//", 1)[0].split("#", 1)[0].split(";", 1)[0].strip()
        if not line:
            continue
        for token in re.split(r"[\s,]+", line):
            if not token:
                continue
            value_text = token[2:] if token.lower().startswith("0x") else token
            if not re.fullmatch(r"[0-9a-fA-F]{1,6}", value_text):
                raise ValueError(f"{path}:{lineno}: unsupported word token {token!r}")
            value = int(value_text, 16)
            if value > 0xFFFFFF:
                raise ValueError(f"{path}:{lineno}: word exceeds 24 bits: {token!r}")
            words.append(value)
    return words


def find_q22_table(words: list[int]) -> list[int]:
    width = len(Q22_WORDS)
    return [
        offset
        for offset in range(0, len(words) - width + 1)
        if tuple(words[offset : offset + width]) == Q22_WORDS
    ]


def classify_disassembly(path: Path) -> int:
    lines = path.read_text(errors="strict").splitlines()
    counts = {mnemonic: 0 for mnemonic in ARITHMETIC_MNEMONICS}
    scaling_lines: list[tuple[int, str]] = []

    for lineno, raw in enumerate(lines, 1):
        line = raw.lower()
        for mnemonic in ARITHMETIC_MNEMONICS:
            if re.search(rf"\b{mnemonic}\b", line):
                counts[mnemonic] += 1
        if re.search(r"\b(?:omr|mr)\b", line) and re.search(
            r"\b(?:move|ori|andi|bset|bclr)\b", line
        ):
            scaling_lines.append((lineno, raw.strip()))

    print("DSP_ARITHMETIC_SUMMARY")
    for mnemonic in ARITHMETIC_MNEMONICS:
        print(f"{mnemonic.upper()}={counts[mnemonic]}")
    print(f"SCALING_CONTROL_CANDIDATES={len(scaling_lines)}")
    for lineno, line in scaling_lines:
        print(f"SCALING line={lineno} text={line}")

    rounded = counts["rnd"] + counts["mpyr"] + counts["macr"]
    unrounded = counts["mpy"] + counts["mac"]
    if rounded:
        print("ROUNDING_CLASS=CONVERGENT_CAPABLE_INSTRUCTION_PRESENT")
    elif unrounded:
        print("ROUNDING_CLASS=UNROUNDED_MULTIPLY_ACCUMULATE_ONLY_IN_INPUT")
    else:
        print("ROUNDING_CLASS=NO_MULTIPLY_ROUNDING_MNEMONIC_FOUND")
    return 0


def emit_q22() -> None:
    print("db,coefficient_hex,coefficient_decimal,gain")
    for db, coefficient in enumerate(Q22_WORDS):
        print(f"{db},0x{coefficient:06X},{coefficient},{coefficient / Q22_SCALE:.12g}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--words", type=Path, help="extracted 24-bit P/X/Y words to scan")
    parser.add_argument("--disassembly", type=Path, help="DSP56001 disassembly to classify")
    parser.add_argument("--emit-q22", action="store_true", help="emit the expected 128-entry Q22 curve")
    args = parser.parse_args()

    if not (args.words or args.disassembly or args.emit_q22):
        parser.error("select --words, --disassembly, and/or --emit-q22")

    if args.emit_q22:
        emit_q22()

    if args.words:
        words = parse_words(args.words)
        matches = find_q22_table(words)
        print(f"WORDS={len(words)}")
        print(f"Q22_TABLE_LENGTH={len(Q22_WORDS)}")
        print(f"Q22_TABLE_MATCHES={len(matches)}")
        for offset in matches:
            print(f"Q22_TABLE_MATCH_WORD_OFFSET={offset}")
        if not matches:
            return 2

    if args.disassembly:
        return classify_disassembly(args.disassembly)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
