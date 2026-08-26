# CD-i audit E1b: MCD212/VDSC bus-error integration

Date: 2026-08-26
Baseline: `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`
Review branch: `cdi-audit-e1b-vdsc-berr-status-2`

## Scope

This micro-batch audits only MCD212/VDSC bus-error ownership and status behavior. It does not change reset-vector handling, DVC DMA, CDIC, or Mono-II/DRVDSP behavior.

## Evidence boundary

Primary evidence is the Philips MCD212 system-controller documentation. Secondary evidence is current MAME source/history and the MiSTer CD-i implementation. MiSTer is not treated as a hardware specification.

## Current MAME behavior

Mono-I installs a blanket `0x000000-0xffffff` fallback that immediately pulses `M68K_LINE_BUSERROR` for any otherwise unmapped read/write. This path is driver-owned in `cdi_state::bus_error_r/w`.

The MCD212 device already defines both `CSR1W_BE` (bus-error enable) and `CSR2R_BE` (bus-error status), but the current driver fallback does not consult `CSR1W_BE`, does not report a bus-error event to the MCD212, and `csr2_r()` clears only IT1/IT2 rather than the BE status bit.

## Hardware model

The MCD212 is the CD-i system controller for external address decoding and /DTACK generation. Its documented bus-error mechanism is conditional on the CSR1W BE control bit and is associated with failure to obtain an acknowledge within the VDSC timeout interval (approximately one video line), after which BERR is generated and the CSR2R BE status is latched. Reading CSR2R clears the latched status.

Therefore the current immediate, unconditional driver fallback is not a faithful model of VDSC bus-error generation.

## Findings

### E1b-1 — bus-error ownership is architecturally misplaced

**Classification:** confirmed architectural fidelity bug; possible functional edge-case bug.

The machine driver currently synthesizes BERR directly instead of having MCD212 system-control state participate in the decision. This hides documented VDSC ownership and makes `CSR1W_BE` ineffective.

### E1b-2 — CSR2R BE status is effectively dead

**Classification:** confirmed correctness bug.

`CSR2R_BE` is declared but no current path sets it. `csr2_r()` also does not clear it. Software probing the documented status cannot observe the hardware state transition.

### E1b-3 — timeout behavior is missing

**Classification:** known fidelity gap, not safe to guess.

The current handler asserts and clears BERR immediately. The documented hardware waits for an unacknowledged bus cycle to exceed the VDSC timeout. Exact integration with the current 68k core and asynchronous /DTACK limitations requires a separate design pass; this audit does not invent timing HLE.

### E1b-4 — current address classification should not automatically be rewritten

**Classification:** uncertain / requires board-level confirmation.

The existence of unmapped/bus-error regions is independently supported, including observed Mono-I gaps. The first correction should therefore separate *which accesses are invalid* from *how MCD212 turns an invalid/unacknowledged cycle into BERR*. Do not expand or collapse the address map merely to make this architectural change.

## Recommended implementation order

1. Add a focused MCD212 status helper/path that can latch `CSR2R_BE` only when the documented BE control permits it.
2. Make CSR2R reads clear BE alongside the documented read-to-clear status bits, while debugger reads remain side-effect-free.
3. Route the existing compatibility bus-error fallback through that MCD212 status path without changing the currently validated CPU exception timing yet.
4. Add regression tests for BE enable/disable, status latch, CSR2R read-to-clear, and debugger-read suppression.
5. Only after functional validation, audit replacement of the immediate BERR pulse with a hardware-backed timeout model.

## Validation boundary

A status-only correction can be tested without claiming cycle-level fidelity. The existing immediate CPU BERR behavior should remain explicitly marked compatibility behavior until asynchronous bus timeout semantics are implemented and validated.

No game-specific exception is justified by this audit.
