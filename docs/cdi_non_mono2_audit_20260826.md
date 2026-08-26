# Philips CD-i non-Mono-II audit ledger

Baseline: `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`

Review branch: `cdi-audit-e1b-vdsc-berr-status`

Mono-II DRVDSP/DSP architecture work is intentionally parked. Findings here must keep functional correctness separate from hardware fidelity and must not introduce title-specific behavior.

## Batch map

| Batch | Scope | Initial risk | Notes |
| --- | --- | --- | --- |
| E1 | Mono-I platform integration: reset vector decode, VDSC bus error, board memory decode/maps | Critical | Highest-value boundary between SCC68070, MCD212 and board glue. |
| E2 | MCD212 remaining rendering/control: matte, cursor, mosaic, transparency, DCA/ICA edge cases, wait states | High | Phase B improved timing/QHY but did not prove all legacy display behavior. |
| E3 | DVC/VMPEG hardware boundary: register constants, MPEG RAM gate, DMA pacing, presentation policy, IRQ | Critical | Current source explicitly contains compatibility/provisional behavior. |
| E4 | Mono-I CDIC/audio: AUDCTL, TOC/subcode, CDDA/XA transitions, attenuation, error/status behavior | High | Phase C intentionally retained several compatibility/unknown behaviors. |
| E5 | Cross-device IRQ/DMA integration: IRQ4 arbitration, SCC channel-1 handoff, acknowledge timing | High | Device-local tests do not prove board-level arbitration/timing. |
| E6 | Save/reset ownership and postload reconstruction | High | DVC decoder reconstruction is explicitly incomplete; cross-device line/timer state needs review. |
| E7 | Input/SLAVE/Quizard integration where hardware-relevant | Medium | Preserve HLE evidence boundaries; avoid expanding into Mono-II SLAVE architecture. |
| E8 | Cleanup: dead/duplicated state, outdated MAME idioms, stale logging/constants/comments | Medium | Do after correctness-sensitive batches so cleanup does not obscure behavior changes. |

## E1a - reset-vector / temporary ROM decode

### Finding E1a-1 - inherited reset-vector fabrication

**Classification:** confirmed hardware-fidelity bug; medium functional risk; possible compatibility dependency.

Current `cdi_state::machine_reset()` copies the first eight bytes of the BIOS into plane-A DRAM. The MCD212 specification instead describes temporary ROM decoding for the first four 680x0 word accesses after reset (SSP and PC fetch), after which addresses `0x000000-0x000007` return to normal DRAM decoding. Reset therefore must not overwrite DRAM bytes 0-7.

Secondary evidence: the independent MiSTer CD-i implementation models boot selection as bus/decode behavior rather than by modifying guest DRAM.

**Required fix shape:** model a reset-armed four-word ROM decode window; do not count debugger reads; preserve/save the remaining access count; remove the reset-time DRAM copy only after validation.

**Required regression coverage:** first four accesses source BIOS; fifth sources DRAM; reset does not mutate DRAM bytes 0-7; reset rearms the window; partial-window save/load restores the remaining count.

## E1b - VDSC bus error

Primary MCD212 behavior: BERR is generated only when CSR1W.BE is enabled and a selected transfer remains unacknowledged for at least one whole video line (approximately 64 microseconds). The VDSC then sets CSR2R.BE. BERR is released when UDS/LDS are released, and reading CSR2R clears BE. Reading CSR2R also clears IT1/IT2.

### Finding E1b-1 - immediate unconditional board-level BERR

**Classification:** confirmed hardware-fidelity bug; potentially functional; compatibility-sensitive.

Mono-I currently overlays a catch-all `bus_error_r/w` across the 24-bit program map. Any unmapped access reaching it immediately pulses the 68k BERR input regardless of CSR1W.BE and without the documented watchdog delay. This bypasses the MCD212 system-controller behavior.

**Do not change yet:** replacing the immediate exception with an asynchronous ~64 us wait requires careful CPU/bus-cycle integration. Preserve current validated behavior until that mechanism is designed and tested.

### Finding E1b-2 - CSR2R.BE is dead state

**Classification:** confirmed correctness bug with high-confidence primary evidence; low compatibility risk to fix separately.

`CSR2R_BE` is defined but current source never sets it. `csr2_r()` clears only IT1/IT2, although the specification requires a status read to clear BE as well. Thus software cannot observe the documented VDSC bus-error status even when MAME generates a bus error through the compatibility handler.

**Safe partial-fix direction:** while retaining current immediate CPU BERR behavior, latch CSR2R.BE only when CSR1W.BE is enabled and the compatibility bus-error path fires; clear BE together with IT1/IT2 on a side-effecting CSR2R read. Explicitly document that CPU exception timing remains compatibility behavior.

### Finding E1b-3 - bus-error fault address loses byte-lane information

**Classification:** likely correctness bug; needs a focused CPU exception-frame test before code change.

`bus_error_r/w` reports `offset * 2` to `set_buserror_details`. The callbacks do not currently accept `mem_mask`, so an unmapped odd-byte access can only be reported as the preceding even word address. Current MAME examples commonly use the same legacy pattern, but the Musashi API accepts an exact fault address and the CD-i handler should preserve UDS/LDS lane information if the SCC68070 exception frame exposes it.

**Next check:** add a tiny 68070-side regression proving the stacked fault address for even byte, odd byte and word unmapped accesses before changing callback signatures.

## Current stop point

No validated behavior has been promoted. The authoritative branch remains untouched. E1b source changes are intentionally not bundled with E1a or memory-map corrections.