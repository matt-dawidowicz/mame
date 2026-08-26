#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Dawidowicz
"""Static RED audit gate for the Mono-I DVC/SCC68070 DMA liveness bug.

This is deliberately an audit reproducer, not a substitute for the future
runtime regression test.  It examines the production source paths that own
DREQ/service/completion state and returns non-zero while the known held-DREQ
orphan pattern is present.

RED means the SCC channel-inactive service path simultaneously:
  * drops the driver's service-active state;
  * returns without DVC completion; and
  * does not re-arm service.

That combination can orphan a level-held VMPEG DREQ after SCC abort/error.
Once the production ownership model is corrected, replace or supplement this
source audit with a behavioral mametests abort/re-arm regression.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


def extract_function(source: str, signature_pattern: str) -> str:
    match = re.search(signature_pattern, source, flags=re.MULTILINE)
    if not match:
        raise RuntimeError(f"function not found: {signature_pattern}")

    brace = source.find("{", match.end())
    if brace < 0:
        raise RuntimeError(f"opening brace not found: {signature_pattern}")

    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.start(): index + 1]

    raise RuntimeError(f"unterminated function: {signature_pattern}")


def extract_if_block(function: str, condition_pattern: str) -> str:
    match = re.search(condition_pattern, function, flags=re.MULTILINE)
    if not match:
        raise RuntimeError(f"condition not found: {condition_pattern}")

    brace = function.find("{", match.end())
    if brace < 0:
        raise RuntimeError(f"opening brace not found: {condition_pattern}")

    depth = 0
    for index in range(brace, len(function)):
        char = function[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return function[match.start(): index + 1]

    raise RuntimeError(f"unterminated condition: {condition_pattern}")


def present(text: str, pattern: str) -> bool:
    return bool(re.search(pattern, text, flags=re.MULTILINE | re.DOTALL))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="MAME source root (default: repository root)",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    cdi_cpp = (root / "src/mame/philips/cdi.cpp").read_text(encoding="utf-8")
    dvc_cpp = (root / "src/mame/philips/cdidvc.cpp").read_text(encoding="utf-8")

    request = extract_function(
        cdi_cpp,
        r"void\s+cdi_state::dvc_dma_req_w\s*\(\s*int\s+state\s*\)",
    )
    tick = extract_function(
        cdi_cpp,
        r"TIMER_CALLBACK_MEMBER\s*\(\s*cdi_state::dvc_dma_service_tick\s*\)",
    )
    done = extract_function(
        dvc_cpp,
        r"void\s+cdi_dvc_device::dma_done\s*\(\s*\)",
    )
    inactive = extract_if_block(
        tick,
        r"if\s*\(\s*!m_maincpu->dma_channel_active\s*\(\s*1\s*\)\s*\)",
    )

    drops_service = present(
        inactive, r"m_dvc_dma_service_active\s*=\s*false\s*;"
    )
    returns = present(inactive, r"\breturn\s*;")
    completes_dvc = present(
        inactive, r"(?:m_dvc\s*->\s*)?dma_done\s*\("
    )
    reschedules = present(
        inactive, r"m_dvc_dma_timer\s*->\s*adjust\s*\("
    )
    reassert_ignored = present(
        request,
        r"if\s*\(\s*m_dvc_dma_service_active\s*\)\s*\{"
        r".*?DVC_DMA_SERVICE_REASSERT.*?\breturn\s*;\s*\}",
    )
    done_clears_dma = present(done, r"m_dma_active\s*=\s*false\s*;")
    done_clears_dreq = present(
        done, r"m_dma_req_callback\s*\(\s*CLEAR_LINE\s*\)\s*;"
    )

    if drops_service and returns and not completes_dvc and not reschedules:
        print("RED: DVC DMA held-request liveness bug reproduced")
        print("  SCC channel inactive -> driver clears service-active and returns")
        print("  DVC dma_done() is not called -> normal DVC completion is skipped")
        print("  no service timer is re-armed -> the request has no retry path")
        print(f"  DREQ reassert while servicing ignored: {int(reassert_ignored)}")
        print(f"  dma_done clears DVC active/DREQ: {int(done_clears_dma)}/{int(done_clears_dreq)}")
        return 1

    print("GREEN: held DVC request is no longer silently orphaned by the SCC-inactive path")
    print(f"  drops_service={int(drops_service)} returns={int(returns)}")
    print(f"  completes_dvc={int(completes_dvc)} reschedules={int(reschedules)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
