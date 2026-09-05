# Philips CD-i code audit — 2026-09-05

Branch: `audio/cdi-fidelity-100-campaign-20260905`

Scope: CD-i / Mono-I / Mono-II / CDIC / DVC / SLAVE / MCD212 implementation under `src/mame/philips`, plus focused tests. Unrelated Philips systems are out of scope.

This audit separates mechanical defects from hardware-sensitive behavior. A cleanup is not considered valid merely because it looks simpler: where a behavior is a compatibility model or an unresolved hardware edge, the evidence boundary is retained explicitly.

## Severity legend

- **Critical** — plausible crash, undefined behavior, or corruption in a reachable path.
- **High** — correctness/state/timing defect likely to affect emulation results.
- **Medium** — avoidable performance cost, portability defect, save-state weakness, or misleading implementation artifact.
- **Low** — dead code, stale naming/comments, or maintainability debt without current runtime impact.

## Confirmed defects and cleanup targets

| Severity | Area | Finding | Status |
| --- | --- | --- | --- |
| Critical | LCD / machine glue | `screen_update_cdimono1_lcd()` dereferences optional `m_slave_hle` although Mono-II and CD-i 910 use the LCD callback without instantiating the Mono-I SLAVE HLE. Missing-HLE configurations must render an unmodeled/blank panel instead of dereferencing an absent device. | Open source fix |
| Critical | MCD212 | Mosaic mixing can evaluate `x % mosaic_count` with `mosaic_count == 0` when mosaic enable is set and the low-byte hold factor is zero. | Zero-safe helper + tests committed; production call site open |
| High | DVC A/V | Packet scheduling compares PTS/DTS to the last parsed SCR value (`m_mpeg_clock90`) rather than the DCLK-advanced current MPEG clock. The resulting FMA delta feeds `m_audio_wait_samples`, so elapsed packet/DMA time can become extra startup silence. | Open source fix |
| High | Quizard | `m_mcu_p3` is live serial/MCU state but is not registered for save state. | Header initialized; `save_item` source fix open |
| High | CDIC RAM | Byte-owned CDIC RAM is written through `uint16_t *` casts in `ram_w()` and sector-buffer delivery. This embeds host byte order in the storage model and is avoidable. | Open source fix + regression needed |
| Medium | CDIC XA output | XA playback calls each DMADAC once per decoded sample. A sector can therefore generate thousands of tiny transfer calls although output arrays already exist. | Open batching optimization |
| Medium | CDIC diagnostics | `CDIC_TRACE` / `CDIC_DBUF_TRACE` use unconditional `logerror()` in the 75 Hz transport and register paths. These are campaign traces, not production errors. | Open log-mask cleanup |
| Medium | DVC video | A full FNV pass over decoded Y/Cr/Cb data is performed for every frame even when `LOG_VIDEO` is disabled; the hash is used only by a diagnostic log line. | Open diagnostic gating |
| Medium | XA test oracle | The independent XA regression still uses negative signed `>>`, reintroducing the portability assumption removed from production. | Open test fix |
| Medium | DVC backend feed | PL_MPEG audio/video input is written one byte at a time. This follows the parser architecture and is not yet changed without profiling because batching can alter parser/decoder boundaries. | Profile before changing |
| Medium | DVC save memory | Fixed save-state mirrors reserve tens of megabytes per DVC instance. This is intentional deterministic reconstruction, but it is a future memory-architecture target. | Deferred |
| Low | CDIC naming | Register `0x3ffa` is AUDCTL, but the live member remains `m_z_buffer` and one write log still says Z-Buffer. | Comment corrected; rename/log cleanup open |
| Low | CDIC header | Obsolete `sample_trigger`, duplicate sector/submode constants, unused include, and stale “just enough” status remained from the older HLE. | Removed |
| Low | machine header | Unused legacy SERVO port-bit declarations remained in `cdi.h`. | Removed |
| Low | LCD UI | Unidentified bits were rendered as fabricated words such as `UNKNOWN`, `ONE`, `TWO`, etc.; drawing also bypassed clip bounds for several pixel paths. | Fixed |
| Low | MCD212 header | Top-of-file “just enough” / `TODO: Unknown yet` wording predates the modern documented MCD212 work. | Open documentation cleanup |
| Low | DVC comments | “Stage 4/Stage 5” and “TEMPORARY DVC A/B PROBE” comments describe old campaign staging rather than the current implementation/evidence boundary. | Open documentation cleanup |
| Low | SLAVE transport | `reset_pointer_input_enabled(bool)` ignores its argument and always returns false; the helper/test are obsolete scaffolding for a direct reset assignment. | Open cleanup |
| Low | CDIC wrapper | `get_sector_count_for_coding()` is a one-line wrapper over `cdic_hle::xa_sector_count()` used at one call site. | Open cleanup |

## Hardware-sensitive observations — do not auto-fix

### CDIC attenuation reset

`device_reset()` currently zeroes all four attenuation bytes. In LL/LR/RR/RL order that means four nominal 0 dB paths, whereas the shared audio module also defines distinct reset and straight-through matrices. The shared constants are not sufficient evidence that the physical CDIC reset state is wrong: they may describe a different control boundary or board path. Leave this behavior unchanged until firmware/hardware evidence establishes the CDIC reset coefficients.

### MCD212 zero mosaic factor

The zero-factor source helper deliberately treats zero as “no hold” solely to avoid undefined modulo-by-zero behavior. It is a compatibility safety rule, not a claim about the MCD212's physical zero encoding. A hardware/full-frame trace may justify a different decoded factor later.

### DVC decode-ahead depth

The 26-picture host decode-ahead queue is a backend adaptation that decouples PL_MPEG input consumption from guest-visible VMPEG FIFO depth. It should be renamed/documented rather than casually collapsed to the nominal three-picture VMPEG output FIFO. Any new value requires runtime calibration.

### CDIC seek delay and synthesized subcode

The six-sector seek delay and portions of TOC/Q synthesis remain explicit HLE compatibility models. Removing them as “old code” without a replacement SERVO model would reduce compatibility and fidelity rather than modernize the implementation.

## Safe cleanup commits already made during this audit

- LCD unidentified indicators removed; all LCD drawing paths now honor clip bounds.
- CDIC header stale declarations/constants and an unused include removed.
- MCD212 zero-safe mosaic index helper added with exhaustive zero/nonzero hold tests.
- Dead SERVO bit declarations removed and Quizard MCU port-3 state given deterministic construction state.

## Priority order for source fixes

1. Guard the LCD callback against absent `m_slave_hle`; register Quizard `m_mcu_p3` for save state.
2. Switch DVC packet scheduling to `current_mpeg_clock90(target)` and add delayed-scheduling regression coverage.
3. Switch MCD212 mixing to the zero-safe mosaic source helper.
4. Make CDIC RAM word accesses explicitly byte ordered and add masked-write/sector-layout tests.
5. Batch XA DMADAC output into one transfer per channel while preserving exact sample conversion.
6. Gate/remove unconditional CDIC campaign traces and disabled DVC video hashing.
7. Remove the remaining obsolete wrappers/staging comments and make the independent XA oracle implementation-independent.

## Completion criterion for this audit

The audit is complete when every source file in the scoped CD-i set has been inspected, all confirmed critical/high findings are either fixed or explicitly evidence-blocked, safe dead code has been removed, hot-path diagnostics/avoidable work are gated, focused regressions cover the changed helpers, and CI/build validation is green for the resulting branch.
