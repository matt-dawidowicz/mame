# Philips CD-i code audit checkpoint — 2026-09-05

Branch: `audio/cdi-fidelity-100-campaign-20260905`

This checkpoint records the result of the full CD-i-owned source sweep plus the SCC68070 integration pass. It distinguishes fixes already landed from source changes that remain intentionally open because the available GitHub write interface cannot safely apply narrow hunks to very large files without replacing their complete contents.

## Scope completed

Reviewed CD-i-owned implementation and focused tests covering:

- machine glue and IRQ/DMA integration;
- Mono-I and Mono-II configuration;
- MCD212 display/presentation helpers and source;
- CDIC command, RAM, XA, CD-DA, buffering and IRQ paths;
- DVC MPEG parser/decode, audio/video presentation, clocks and save-state reconstruction;
- SLAVE HLE parser, transport, pointer and response state;
- LCD rendering;
- SCC68070 DMA/IRQ/I2C/UART/MMU integration behavior relevant to CD-i.

Generic DMADAC and third-party PL_MPEG internals are upstream dependencies rather than CD-i-owned code and were not treated as candidates for local cleanup.

## Fixed during this audit

### Critical / high

1. **Optional SLAVE-HLE LCD dereference** — fixed.
   Mono-II and Mini-MMC configurations use the LCD callback without the Mono-I SLAVE HLE. The callback now renders an explicitly unmodeled blank panel instead of dereferencing an absent optional device.

2. **MCD212 mosaic modulo-zero path** — fixed and regression-covered.
   Enabled mosaic with a zero hold count could evaluate `x % 0`. Source selection is now zero-safe. Zero means no hold only as a compatibility safety rule; this is not promoted to a silicon claim.

3. **Quizard port-3 save-state omission** — fixed.
   `m_mcu_p3` is now deterministically initialized and registered with save state.

### Portability / maintainability

4. **XA signed-shift portability** — fixed earlier in the campaign and included in this audit boundary.
   Production arithmetic uses explicit floor division instead of implementation-defined negative signed right shift.

5. **CDIC RAM byte/word ownership** — helper and exhaustive regression landed.
   `cdicdic_memory.h` defines the existing SCC-visible little-endian word view over byte-owned CDIC RAM without host-native aliasing assumptions. Production call sites still need conversion before this item is fully closed.

6. **LCD fabricated unknown labels and clip handling** — fixed.
   Unidentified panel bits are no longer presented as invented words; drawing paths honor `cliprect`.

7. **Obsolete CDIC and machine declarations** — removed.
   Stale helper declarations/constants, an unused include, and unused legacy SERVO bit declarations were deleted.

8. **DVC DMA trace spam** — fixed.
   Campaign DMA traces are now opt-in logging rather than unconditional `logerror()` output.

## Confirmed open source defects

### High — DVC packet scheduling uses a stale clock

The current `mpeg_schedule_packet()` compares PTS/DTS against `m_mpeg_clock90`, the most recently parsed SCR value. The device already has a DCLK-advanced `current_mpeg_clock90(target)` model.

A pure regression now demonstrates the error mechanism: with SCR=1000, DCLK anchor=100, current DCLK=145, PTS=1100 and DTS=1080, the stale comparison reports +100/+80 90-kHz ticks while the live anchored clock correctly reports +10/-10. The FMA play delta feeds scheduled audio silence, so stale comparison time can become false audio delay.

`measure_packet_schedule()` and the regression are committed. The large `cdidvc.cpp` source hunk remains open because an attempted whole-file replacement also removed unrelated explanatory comments; that commit was deliberately rolled back instead of accepting collateral edits.

### High/medium — CDIC byte RAM still uses host-native `uint16_t *` writes

The production `ram_w()` and sector-buffer delivery still write a `uint8_t[]` through `uint16_t *` casts. The new helper/test proves the intended byte representation and masked-write semantics independently. Production conversion remains open.

## Confirmed inefficiencies

1. **XA DMADAC output granularity** — one transfer call per sample per channel in the decoded XA loop. The already-decoded channel arrays can be mixed in place and submitted once per channel without changing the signal model.

2. **DVC diagnostic video hashing** — decoded Y/Cr/Cb planes are FNV-hashed for every frame even when `LOG_VIDEO` is disabled. This should be compile-time/log-mask gated.

3. **CDIC unconditional transport traces** — `CDIC_TRACE` / `CDIC_DBUF_TRACE` are campaign diagnostics on hot paths and should be converted to opt-in logging.

4. **PL_MPEG byte-at-a-time feed** — potentially inefficient, but parser/backend boundary semantics are sensitive. Profile before batching rather than changing behavior speculatively.

5. **DVC fixed save-state mirrors** — reserve substantial memory for deterministic decoder reconstruction. This is architectural debt, not dead storage; defer until an equally deterministic variable-size save representation exists.

## Long-session robustness finding

Several DVC audio counters are 32-bit, notably emitted/output and decoded-sample counters. At 48 kHz a continuously incremented 32-bit frame/sample counter wraps in about 24.9 hours; at 44.1 kHz it wraps in about 27.1 hours. Current playback is not driven solely by these counters, but future long-run A/V telemetry must use a 64-bit emitted-sample clock or explicitly handle wrap.

This is medium severity rather than an immediate compatibility bug because the campaign's current 30-minute evidence window is far below the wrap interval.

## SCC68070 pass

No new evidence-safe correctness patch was identified in the SCC68070 integration pass. The major remaining items are known incomplete hardware models rather than obsolete code:

- live MMU translation and segment exceptions;
- Timer 1/2 match/capture/event-counter routing;
- I2C arbitration, slave mode and multi-master behavior;
- UART bit-level/framing fidelity beyond the current timer model;
- DMA request/burst/cycle-steal/error timing and disputed zero-count/reset details;
- bus-level IACK and DREQ edge timing.

These should not be filled with compatibility guesses merely to reduce TODO count.

## Evidence-sensitive observations deliberately left unchanged

- CDIC attenuation reset coefficients;
- MCD212 zero mosaic-factor silicon interpretation beyond the crash-safe fallback;
- MCD212 mosaic transparency ordering under held source pixels;
- DVC host decode-ahead depth of 26 pictures;
- CDIC six-sector seek compatibility delay;
- synthesized TOC/Q behavior where no SERVO/subcode replacement exists;
- Mono-II DSP and asynchronous `/DTACK` holes.

## Remaining implementation order

1. Apply the narrow DVC live-clock scheduling hunk and gate diagnostic frame hashing without replacing unrelated source text.
2. Convert CDIC production RAM accesses to `cdicdic_memory.h` and retain exact existing word/Q layout.
3. Batch XA DMADAC output and prove PCM equivalence.
4. Gate CDIC hot-path traces.
5. Remove the remaining low-risk obsolete wrappers/comments.
6. Make the independent XA test oracle avoid negative signed right shift as well.
7. Run focused Philips tests, full native tests, validation, and platform CI before promoting the audit checkpoint.

The audit is therefore source-complete as a review pass, but remediation is not complete until the two large-source high-priority changes above are landed and validated.
