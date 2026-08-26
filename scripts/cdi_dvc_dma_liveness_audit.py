#!/usr/bin/env python3
# license:BSD-3-Clause
# copyright-holders:Matt Dawidowicz
"""Static audit gate for Mono-I DVC/SCC68070 DMA held-request liveness.

This is deliberately an audit gate, not a substitute for a runtime mametests
integration test.  It examines the production source paths that own
DREQ/service/completion state.

The original RED condition was a level-held VMPEG DREQ becoming orphaned when
SCC68070 DMA channel 2 stopped before normal completion: the driver dropped its
scheduled service state, DVC kept DREQ asserted, and no event could re-evaluate
the request after firmware repaired/re-armed the SCC channel.

A GREEN event-driven implementation must retain the DREQ level, receive an SCC
DMA-channel reconfiguration notification, and retry the existing validated
DREQ-start path only while the request is still asserted and service is idle.
The current two-SCC-clock service cadence and exactly-once normal completion
path must remain unchanged by this audit batch.
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
    cdi_h = (root / "src/mame/philips/cdi.h").read_text(encoding="utf-8")
    dvc_cpp = (root / "src/mame/philips/cdidvc.cpp").read_text(encoding="utf-8")
    scc_cpp = (root / "src/devices/machine/scc68070.cpp").read_text(encoding="utf-8")
    scc_h = (root / "src/devices/machine/scc68070.h").read_text(encoding="utf-8")

    request = extract_function(
        cdi_cpp,
        r"void\s+cdi_state::dvc_dma_req_w\s*\(\s*int\s+state\s*\)",
    )
    reconfigure = extract_function(
        cdi_cpp,
        r"void\s+cdi_state::dvc_dma_reconfigure_w\s*\(\s*uint8_t\s+channel\s*\)",
    )
    tick = extract_function(
        cdi_cpp,
        r"TIMER_CALLBACK_MEMBER\s*\(\s*cdi_state::dvc_dma_service_tick\s*\)",
    )
    done = extract_function(
        dvc_cpp,
        r"void\s+cdi_dvc_device::dma_done\s*\(\s*\)",
    )
    scc_dma_w = extract_function(
        scc_cpp,
        r"void\s+scc68070_device::dma_w\s*\(\s*offs_t\s+offset\s*,\s*uint16_t\s+data\s*,\s*uint16_t\s+mem_mask\s*\)",
    )
    inactive = extract_if_block(
        tick,
        r"if\s*\(\s*!m_maincpu->dma_channel_active\s*\(\s*1\s*\)\s*\)",
    )

    drops_service = present(
        inactive, r"m_dvc_dma_service_active\s*=\s*false\s*;"
    )
    returns = present(inactive, r"\breturn\s*;")
    completes_dvc_on_abort = present(
        inactive, r"(?:m_dvc\s*->\s*)?dma_done\s*\("
    )
    polls_after_abort = present(
        inactive, r"m_dvc_dma_timer\s*->\s*adjust\s*\("
    )

    req_level_member = present(
        cdi_h, r"bool\s+m_dvc_dma_req_state\s*=\s*false\s*;"
    )
    req_level_tracked = present(
        request, r"m_dvc_dma_req_state\s*=\s*state\s*!=\s*CLEAR_LINE\s*;"
    )
    req_level_saved = present(
        cdi_cpp, r"save_item\s*\(\s*NAME\s*\(\s*m_dvc_dma_req_state\s*\)\s*\)\s*;"
    )
    req_level_reset = present(
        cdi_cpp, r"m_dvc_dma_req_state\s*=\s*false\s*;"
    )

    reconfigure_declared = present(
        cdi_h, r"void\s+dvc_dma_reconfigure_w\s*\(\s*uint8_t\s+channel\s*\)\s*;"
    )
    reconfigure_channel_guard = present(
        reconfigure, r"channel\s*!=\s*1"
    )
    reconfigure_req_guard = present(
        reconfigure, r"!m_dvc_dma_req_state"
    )
    reconfigure_idle_guard = present(
        reconfigure, r"m_dvc_dma_service_active"
    )
    reconfigure_retries_request = present(
        reconfigure, r"dvc_dma_req_w\s*\(\s*ASSERT_LINE\s*\)\s*;"
    )

    scc_callback_accessor = present(
        scc_h,
        r"dma_reconfigure_callback\s*\(\s*\)\s*\{\s*return\s+m_dma_reconfigure_callback\.bind\s*\(\s*\)\s*;\s*\}",
    )
    scc_callback_member = present(
        scc_h, r"devcb_write8\s+m_dma_reconfigure_callback\s*;"
    )
    scc_callback_constructed = present(
        scc_cpp, r"m_dma_reconfigure_callback\s*\(\s*\*this\s*\)"
    )
    scc_channel2_notifies = present(
        scc_dma_w, r"m_dma_reconfigure_callback\s*\(\s*1\s*\)\s*;"
    )

    binding_pattern = (
        r"m_maincpu->dma_reconfigure_callback\s*\(\s*\)\.set\s*\(\s*"
        r"FUNC\s*\(\s*cdi_state::dvc_dma_reconfigure_w\s*\)\s*\)\s*;"
    )
    dvc_binding_count = len(re.findall(binding_pattern, cdi_cpp, flags=re.MULTILINE))

    cadence_constant_unchanged = present(
        cdi_cpp,
        r"static\s+constexpr\s+uint32_t\s+DVC_DMA_SERVICE_CLOCK_TICKS\s*=\s*2\s*;",
    )
    cadence_still_used = present(
        tick,
        r"attotime::from_ticks\s*\(\s*DVC_DMA_SERVICE_CLOCK_TICKS\s*,\s*m_maincpu->clock\s*\(\s*\)\s*\)",
    )

    normal_done_sites = len(
        re.findall(r"m_dvc\s*->\s*dma_done\s*\(\s*\)\s*;", tick)
    )
    done_clears_dma = present(done, r"m_dma_active\s*=\s*false\s*;")
    done_clears_dreq = present(
        done, r"m_dma_req_callback\s*\(\s*CLEAR_LINE\s*\)\s*;"
    )

    event_rearm = all(
        (
            req_level_member,
            req_level_tracked,
            req_level_saved,
            req_level_reset,
            reconfigure_declared,
            reconfigure_channel_guard,
            reconfigure_req_guard,
            reconfigure_idle_guard,
            reconfigure_retries_request,
            scc_callback_accessor,
            scc_callback_member,
            scc_callback_constructed,
            scc_channel2_notifies,
            dvc_binding_count == 2,
        )
    )
    cadence_preserved = cadence_constant_unchanged and cadence_still_used
    completion_preserved = (
        normal_done_sites == 1
        and done_clears_dma
        and done_clears_dreq
        and not completes_dvc_on_abort
    )
    no_abort_polling = not polls_after_abort

    orphan_without_rearm = (
        drops_service
        and returns
        and not completes_dvc_on_abort
        and not polls_after_abort
        and not event_rearm
    )

    if orphan_without_rearm:
        print("RED: DVC DMA held-request liveness bug reproduced")
        print("  SCC channel inactive -> driver clears service-active and returns")
        print("  DVC completion is not fabricated, but no re-arm event path exists")
        print("  held DREQ can therefore become orphaned")
        return 1

    if not event_rearm:
        print("RED: held-DREQ event-driven re-arm contract is incomplete")
        print(
            "  req(member/tracked/saved/reset)="
            f"{int(req_level_member)}/{int(req_level_tracked)}/"
            f"{int(req_level_saved)}/{int(req_level_reset)}"
        )
        print(
            "  reconfigure(declared/channel/req/idle/retry)="
            f"{int(reconfigure_declared)}/{int(reconfigure_channel_guard)}/"
            f"{int(reconfigure_req_guard)}/{int(reconfigure_idle_guard)}/"
            f"{int(reconfigure_retries_request)}"
        )
        print(
            "  scc(accessor/member/ctor/ch2notify)="
            f"{int(scc_callback_accessor)}/{int(scc_callback_member)}/"
            f"{int(scc_callback_constructed)}/{int(scc_channel2_notifies)}"
        )
        print(f"  DVC machine bindings={dvc_binding_count}/2")
        return 1

    if not cadence_preserved:
        print("RED: E1c2 unexpectedly changed the validated DVC DMA service cadence")
        return 1

    if not completion_preserved:
        print("RED: E1c2 disturbed the exactly-once normal DVC completion contract")
        print(
            f"  normal_done_sites={normal_done_sites} "
            f"done_clears_dma={int(done_clears_dma)} "
            f"done_clears_dreq={int(done_clears_dreq)} "
            f"abort_completes={int(completes_dvc_on_abort)}"
        )
        return 1

    if not no_abort_polling:
        print("RED: E1c2 introduced timer polling after SCC abort")
        return 1

    print("GREEN: held DVC DMA request has an event-driven SCC re-arm path")
    print("  DREQ level is tracked, saved, and reset")
    print(f"  DVC PAL/NTSC SCC reconfiguration bindings={dvc_binding_count}")
    print("  channel-2 register changes re-evaluate a held request only while service is idle")
    print("  abort does not fabricate DVC completion or start a retry polling loop")
    print("  two-SCC-clock service cadence is unchanged")
    print("  normal DVC dma_done() site remains exactly one")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
