#!/usr/bin/env python3
"""Guard the MCD212 field-parity update against renderer-coupled regressions."""

from pathlib import Path
import re
import sys

SOURCE = Path("src/mame/philips/mcd212.cpp")
PARITY_TOGGLE = re.compile(r"m_csrr\[0\]\s*\^=\s*CSR1R_PA\s*;")
DA_CLEAR = re.compile(r"m_csrr\[0\]\s*&=\s*~CSR1R_DA\s*;")


def extract_function_body(text: str, marker: str) -> str:
    start = text.find(marker)
    if start < 0:
        raise RuntimeError(f"missing function marker: {marker}")

    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"missing opening brace after: {marker}")

    depth = 0
    for pos in range(brace, len(text)):
        char = text[pos]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1 : pos]

    raise RuntimeError(f"unterminated function body: {marker}")


def fail(message: str) -> None:
    print(f"MCD212 field-parity audit: RED: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")
    ica = extract_function_body(text, "TIMER_CALLBACK_MEMBER(mcd212_device::ica_tick)")
    screen = extract_function_body(text, "uint32_t mcd212_device::screen_update")

    parity = PARITY_TOGGLE.search(ica)
    if not parity:
        fail("ica_tick no longer advances CSR1R_PA")

    da_clear = DA_CLEAR.search(ica)
    if not da_clear:
        fail("ica_tick no longer clears CSR1R_DA")

    if parity.start() > da_clear.start():
        fail("CSR1R_PA must advance at the field boundary before CSR1R_DA is cleared")

    if PARITY_TOGGLE.search(screen):
        fail("CSR1R_PA advancement regressed into screen_update/rendering")

    print("MCD212 field-parity audit: GREEN")
    print("  CSR1R_PA advances in ica_tick before CSR1R_DA clear")
    print("  screen_update does not own field-parity advancement")


if __name__ == "__main__":
    main()
