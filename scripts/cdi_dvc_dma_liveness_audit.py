#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Dawidowicz
"""Static audit reproducer for the Mono-I DVC/SCC68070 DMA liveness bug.

This is deliberately an audit gate, not a substitute for a future runtime
regression test.  It examines the production source paths that currently own
DREQ/service/completion state and returns non-zero while the known held-DREQ
orphan condition is present.

The RED condition is:
  * the DVC service loop is active because VMPEG asserted DREQ;
  * SCC68070 DMA channel 1 becomes inactive before normal completion;
  * the service tick drops only the driver's service-active state and returns;
  * DVC dma_done() is not called, so DVC-side DMA/DREQ stays active;
  * an ASSERT callback received while service is already active is ignored,
    confirming the driver treats the request callback as a start event rather
    than maintaining a persistent level-sensitive service contract.

Once the production ownership model is corrected, replace this audit gate with
(or supplement it by) a behavioral mametests regression for abort/re-arm.
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


def require(text: str, pattern: str, description: str) -> None:
    if not re.search(pattern, text, flags=re.MULTILINE | re.DOTALL):
        raise RuntimeError(f"expected production behavior not found: {description}")


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

    # Establish the exact current ownership split before declaring RED.
    require(
        inactive,
        r"m_dvc_dma_service_active\s*=\s*false\s*;",
        "inactive SCC path drops driver service ownership",
    )
    require(
        inactive,
        r"\breturn\s*;",
        "inactive SCC path returns without further service",
    )
    require(
        request,
        r"if\s*\(\s*m_dvc_dma_service_active\s*\)\s*\{"
        r".*?DVC_DMA_SERVICE_REASSERT.*?\breturn\s*;\s*\}",
        "DREQ ASSERT while service-active is treated as a no-op reassert",
    )
    require(
        done,
        r"m_dma_active\s*=\s*false\s*;",
        "DVC completion clears DVC-side DMA state",
    )
    require(
        done,
        r"m_dma_req_callback\s*\(\s*CLEAR_LINE\s*\)\s*;",
        "DVC completion clears DREQ",
    )

    inactive_completes_dvc = bool(
        re.search(r"(?:m_dvc\s*->\s*)?dma_done\s*\(", inactive)
    )
    inactive_reschedules = bool(
        re.search(r"m_dvc_dma_timer\s*->\s*adjust\s*\(", inactive)
    )

    if not inactive_completes_dvc and not inactive_reschedules:
        print("RED: DVC DMA held-request liveness bug reproduced")
        print("  SCC channel inactive -> driver clears service-active and returns")
        print("  DVC dma_done() is not called -> DVC-side DMA/DREQ remains active")
        print("  no service timer is re-armed -> a held DREQ has no retry path")
        print("  DREQ reassert while already servicing is explicitly ignored")
        return 1

    print("GREEN: inactive SCC DMA no longer silently orphans a held DVC request")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
