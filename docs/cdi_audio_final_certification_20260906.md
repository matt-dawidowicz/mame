# CD-i Audio Fidelity Campaign — Final Certification Report

> Verification update: the [fresh audit](cdi_verified_status_20260906.md) supersedes broad MMU closure and decoded-A/V coverage claims. It records CDIC DMA/Q defects and the actual tolerant PCM-reference scope. Historical passing test results below remain valid within their exercised boundaries; current estimates are in [CDI_MASTER_STATUS.md](CDI_MASTER_STATUS.md).

Date: 2026-09-06

## Certification baseline

- Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`
- Last code/test commit before this documentation fold:
  `0416e3067975236d3ca4a90c0cb461d3a153398d`
- Maintained CD-i fast run: `34055417260`
- Helper result: **16,862,832 assertions / 187 cases — all passed**
- Full-machine integration result: **21 assertions / 6 cases — all passed**
- MMU architectural certification remains independently anchored at the prior green
  SCC68070 milestone and was not modified by the post-MMU audio work.

The green integration gate includes the live SCC68070 channel-2 DMA-to-DVC Layer II
path and simultaneous DVC audio/video save-load reconstruction.  This report does
not turn a green synthetic/integration gate into an undocumented hardware claim.

## Executive verdict

### Software/reference certification: PASS

The campaign has no known unresolved defect in the following software-visible
areas at the certified baseline:

- DVC MPEG-1 Layer II syntax/profile parsing;
- MPEG program-stream and direct-frame FMA ingress;
- all 32 legal FMA stream selectors;
- Layer II decode for the covered legal profile and independent PCM references;
- DVC starvation/refill, ISO termination, stream switching and save reconstruction;
- live SCC68070 DMA ingress to the DVC audio decoder;
- XA Mode-2 routing/coding validation;
- XA ADPCM decode at the documented/reference-model boundary;
- CDIC double-buffer/cadence behavior covered by retained captures;
- software A/V clock arithmetic and 30-minute drift bounds;
- VMPEG FMA attenuation register semantics and recovered Q22 coefficient table;
- AUDCTL behavior covered by the current hardware/service evidence;
- standards-defined de-emphasis response;
- generic CD-DA sector cadence, PCM handoff, pre-start gating and ADR/control
  preservation already exercised by the maintained tests.

### Physical-silicon certification: EXPLICITLY LIMITED

Several remaining matrix rows ask questions that cannot be answered responsibly by
more HLE inference.  They are recorded below as hardware/firmware evidence blockers.
The current deterministic model is retained until evidence distinguishes it from
plausible alternatives.

### Strict campaign-wide 100% verdict: WITHHELD

A binary "everything remaining is either validated or hardware-blocked" claim is
**not yet defensible** because a small set of unchecked CD-DA and interactive-FMV
rows are still ordinary software/runtime/reference-fixture validation gaps, not
pure silicon unknowns.  This report therefore does not hide those rows under the
hardware-evidence label.

In practical terms: the core audio implementation is strongly regression-certified,
all known silicon-only questions are isolated, but the campaign's own strict 100%
definition is not satisfied until the non-hardware gaps identified below are closed
or new evidence demonstrates that they truly are hardware-only.

## Classification of every remaining unchecked matrix item

Status vocabulary:

- **MODEL VALIDATED / HARDWARE BLOCKED** — software-visible deterministic behavior
  is covered, but the exact physical edge/mechanism is unavailable.
- **HARDWARE EVIDENCE BLOCKED** — answering the row requires physical silicon,
  firmware, analogue, servo, or equivalent retained reference evidence.
- **SOFTWARE/RUNTIME GAP** — additional deterministic/reference testing is still
  possible without first obtaining physical hardware; this prevents strict final
  certification.

| Campaign item | Classification | Certification rationale |
| --- | --- | --- |
| §3 — exact response to prohibited in-stream 48/32 kHz DVC output-rate changes | **HARDWARE EVIDENCE BLOCKED** | The Full Motion profile forbids the transition, but VMPEG's actual error/concealment/abort behavior is undocumented. PL_MPEG behavior is not a valid hardware oracle. |
| §3 — DVC mute/unmute, flush and decoder-reset transitions | **MODEL VALIDATED / HARDWARE BLOCKED** | Queue starvation, reset, stream replacement and save/load are deterministic; the physical DAC hold/zero/ramp/flush edge is not established. |
| §7 — exact XA rounding/saturation where observable | **HARDWARE EVIDENCE BLOCKED** | The Green Book and FFmpeg corroborate the software/reference result, but IMS66490/CDIC internal accumulator and silicon rounding details are unavailable. |
| §8 — stop/reset command timing through hidden DAC and predictor boundaries | **HARDWARE EVIDENCE BLOCKED** | The buffer/control state machine is tested; the physical output/predictor reset edge is not observable from current documentation. |
| §8 — real-media, host-output and physical-hardware long-run drift | **HARDWARE EVIDENCE BLOCKED** | Software arithmetic drift is already bounded; oscillator, drive, host resampler and presentation latency require physical/runtime measurement. |
| §10 — physical attenuation transitions and Mono-I CDIC coefficient quantization | **HARDWARE EVIDENCE BLOCKED** | VMPEG's digital Q22 table is recovered and low-range physical attenuation is measured, but the CDIC quantizer and full transition waveform are not. |
| §12 — sample/frame-boundary semantics for mute/pause/stop/reset/stream replacement/underrun | **HARDWARE EVIDENCE BLOCKED** | These are explicitly analogue/output-edge questions after the deterministic queue/control model has already been fixed. |
| §12 — whether the final sample is held, zeroed, ramped or flushed | **HARDWARE EVIDENCE BLOCKED** | No reviewed source distinguishes the candidate physical behaviors. |
| §12 — transition captures/regressions for those physical edges | **HARDWARE EVIDENCE BLOCKED** | A meaningful regression requires a retained hardware capture first; otherwise the test would only canonize a guess. |
| §13 — exact Philips FMA rounded/unrounded DSP instruction sequence, scaling and limiter use | **HARDWARE/FIRMWARE EVIDENCE BLOCKED** | DSP56001 architecture and recovered coefficients constrain the model, but the exact Philips FMA routine has not been positively identified to the point required for a silicon-attribution claim. |
| §13 — Mono-I CDIC silicon rounding/saturation | **HARDWARE EVIDENCE BLOCKED** | Internal CDIC arithmetic remains unobservable from current public documentation/reference data. |
| §14 — de-emphasis enable/disable transition behavior | **MODEL VALIDATED / HARDWARE BLOCKED** | The standards response and deterministic continuously-primed implementation are tested; physical switching/history/ramp behavior is not. |
| §15 — CD-DA play/pause/resume/stop/seek behavior | **SOFTWARE/RUNTIME GAP + physical timing blocker** | The generic command/cadence model exists, but retained reference-disc/title observations have not yet validated the complete state transitions. Physical command-to-audio latency is separately hardware-dependent. |
| §15 — track/index transitions and lead-in/lead-out handling | **SOFTWARE/RUNTIME GAP + physical timing blocker** | Track/index/lead packets remain synthesized and lack a retained synthetic/reference-disc validation campaign. |
| §15 — exact sample start/stop alignment and seek-latency model | **HARDWARE/REFERENCE-MEDIA EVIDENCE BLOCKED** | Exact physical seek/servo latency cannot be derived from HLE. A synthetic disc can validate packet bookkeeping, but not the real transport delay. |
| §15 — mixed-mode discs | **SOFTWARE/RUNTIME GAP** | A synthetic or retained mixed-mode image can exercise data/audio boundaries without new silicon evidence. This row therefore cannot honestly be called hardware-blocked. |
| §16 — Q-channel position/status behavior | **SOFTWARE/RUNTIME GAP** | ADR/control preservation and 75 Hz delivery are tested, but the synthesized position/status packets themselves still need a reference oracle/fixture. |
| §16 — track/index/time updates during play, pause, seek, lead-in and lead-out | **SOFTWARE/RUNTIME GAP + physical timing blocker** | The packet update logic can be tested synthetically; the exact command/transport timing around some edges remains physical. |
| §16 — synthetic subcode/reference fixtures where feasible | **SOFTWARE/RUNTIME GAP** | This is explicitly an implementable test deliverable and does not require hardware merely to exist. |
| §17 — interactive-FMV branch changes | **SOFTWARE/RUNTIME GAP** | Generic pause/continue/stop/play and clock re-anchoring are covered, but a real branching MPEG fixture with meaningful timestamps has not yet been retained and executed. |

## Hardware-evidence blocker ledger

The rows classified above as hardware/firmware blocked map directly to
`docs/cdi_audio_hardware_evidence_blockers_20260906.md`:

1. prohibited DVC in-stream rate changes;
2. DAC mute/flush/hold/ramp edges;
3. exact VMPEG DSP56001 FMA arithmetic path;
4. Mono-I CDIC silicon rounding/saturation;
5. de-emphasis transition edge;
6. CD-DA physical transport timing and Q-subcode observations;
7. physical long-run A/V drift;
8. interactive FMV branch evidence acquisition procedure;
9. attenuation transition and Mono-I CDIC quantizer.

The blocker ledger intentionally describes acquisition procedures instead of coding
changes.  A future implementation change against those rows should be tied to raw
or reproducible evidence that distinguishes the selected behavior from alternatives.

## Compatibility-matrix result

`docs/cdi_audio_compatibility_matrix_20260906.md` is the title-facing ledger.
It records only media properties supported by repository evidence and deliberately
does not claim a successful retail-title playthrough where none is retained.

Two particularly useful reference targets are already unambiguous from the software
list:

- **The 7th Guest (Europe)** — explicitly DVC and accompanied by a seven-track
  CD-Audio music disc; the software-list entry is still marked unsupported, so this
  campaign does not override that fact by assertion.
- **BURN-CYCLE - The Music (Europe)** — an explicit eleven-track CD-Audio disc,
  appropriate for future multi-track transport and Q-subcode validation.

For ordinary single-disc CD-i titles such as Hotel Mario and Link - The Faces of
Evil, the software-list metadata alone does not prove that a specific scene uses XA
ADPCM.  They are therefore listed as candidate non-DVC baselines rather than falsely
labelled XA compatibility passes.  The XA implementation itself is already covered
by exhaustive/reference/hardware-backed subsystem tests; what is absent is the
retail-title media audit and retained runtime trace.

## What the green CI run proves

Run `34055417260` on `0416e3067975236d3ca4a90c0cb461d3a153398d`
proved, on the exact code baseline:

- all maintained CD-i helper tests passed;
- the complete integration binary built;
- all six full-machine integration cases passed;
- the live DVC Layer II DMA path passed;
- the synchronized SCC completion IRQ checks passed;
- simultaneous DVC A/V save-load reconstruction passed;
- the synthetic 25 Hz video header and post-load continuation fixture passed.

This is the correct certification boundary for that commit.  It does **not** prove
physical DAC edges, physical drive seek latency, title-specific branch latency or
undocumented silicon rounding.

## Remaining work required for strict 100%

Only the following non-hardware items still prevent the campaign from being stamped
strictly complete under its own definition:

1. add a deterministic mixed-mode CD-DA/data reference-disc fixture;
2. add Q-channel position/index/time reference fixtures, including pause/seek and
   lead transitions to the extent a synthetic disc can define them;
3. execute and retain complete CD-DA play/pause/resume/stop/seek reference behavior
   at the software/model boundary, keeping physical servo latency separately labelled;
4. add a controlled branching Full Motion MPEG fixture with meaningful SCR/PTS/DCLK
   discontinuities and prove repeated branches do not accumulate timing error;
5. media-audit and retain at least one retail XA title path if the compatibility
   matrix is intended to contain a title-level XA pass rather than only subsystem
   certification.

Once those are green, every remaining unchecked engineering question can be
legitimately described as an explicit hardware/firmware evidence blocker rather
than a missing deterministic test.

## Certification conclusion

The campaign has reached a defensible **software/reference-certified audio
implementation milestone**.  The post-MMU DVC DMA/save-state/A-V batch is green,
XA/CDIC core behavior has extensive deterministic/reference coverage, and the
remaining silicon-specific unknowns are explicitly isolated.

The report deliberately **withholds the final campaign-wide 100% stamp** because
sections 15, 16 and 17 still contain testable software/runtime gaps.  That is a
stronger certification posture than relabelling those gaps as hardware limitations.
When those finite fixtures are added, the remaining open rows can be cleanly reduced
to the hardware-evidence ledger.
