# CD-i upstream validation

Baseline: `mamedev/mame` master at `6743eb674d0754dbbb0726c153d5ef49d84ef67c` (2026-10-01 UTC).

This branch is deliberately based on current upstream MAME, not on the historical
`cdi-unified` implementation.  Its purpose is to preserve reusable validation from
that research and apply it to the implementation that actually shipped upstream.

## Upstream state

Upstream merged Digital Video Cartridge / VMPEG support in
`f43983b62edf2b7d1dc8911ee4a2c08dba8a98de` (#16181) on 2026-09-23.  That work adds:

- a reusable CD-i DVC slot and VMPEG cartridge;
- Motorola MCD251 full-motion video emulation;
- Motorola GSC38GG307 full-motion audio emulation;
- an MPEG-1 system-stream demultiplexer;
- SCC68070 channel-1 DMA ingress and shared interrupt handling;
- external-video composition through the MCD212;
- PAL/NTSC handling for Mono-I.

The upstream PR reports manual/AI testing across many retail DVC titles plus ten
CDi_MiSTer VMPEG test ROMs, sample-by-sample audio comparisons, decoded-picture
comparison, and snapshot checks.

## Why the old DVC implementation is not being ported wholesale

The historical `cdi-unified` branch is now hundreds of commits behind current MAME
and implements the same central product feature with a different architecture,
including a bundled PL_MPEG path.  MAME review explicitly converged on reusable
`mpeg_video`, `mpeg_audio`, MCD251 and GSC38GG307 devices instead.  Replacing the
newly merged architecture would create duplicate decoder infrastructure and would
not be a useful upstream contribution.

The old branch should therefore be treated as a research/validation corpus.

## Validation matrix

| Area | Upstream status | Historical branch value | Action |
| --- | --- | --- | --- |
| Basic VMPEG/DVC playback | Implemented and tested | Duplicate functionality | Keep upstream |
| MPEG video decode | Native MAME `mpeg_video` | PL_MPEG-backed implementation and fixtures | Reuse fixtures where licensing permits |
| MPEG Layer II audio | Native MAME `mpeg_audio` | Long-run/reference testing | Port behavior tests, not decoder |
| MPEG system demux | New `mpeg_demux` | Exhaustive PES/routing tests | Port tests against upstream semantics |
| 33-bit timestamp wrap | Helper exists upstream | Explicit boundary/property tests | **Ported first** |
| DVC DMA / IRQ | Implemented | DMA-liveness and integration coverage | Re-express against upstream devices |
| Save/load during FMV | Upstream devices register state | Snapshot/replay campaign | Add targeted upstream regression |
| Long-run A/V drift | Runtime pacing implemented | 30-minute arithmetic and decoded A/V fixtures | Re-run against upstream implementation |
| Stream changes / starvation | Implemented in device state machines | Exhaustive transition tests | Port highest-risk cases |
| VMPEG attenuation / DSP56001 | Explicitly not fully emulated in #16181 | Recovered FMA attenuation evidence and tests | Highest-value fidelity follow-up |
| CDIC audio / attenuation | Changed upstream after DVC merge | Hardware-evidence campaign | Compare before proposing fixes |
| De-emphasis | Not part of the DVC merge scope | Standards-driven implementation/tests | Audit current upstream separately |
| Physical DAC transition edges | Evidence-limited | Marked hardware-evidence blocked | Do not guess; wait for captures |

## Known upstream caveats worth validating

The #16181 review explicitly documents that the VMPEG DSP56001 attenuation ramp was
not emulated at merge time, and that the analogue mixing/attenuation model was not
complete in that change.  Subsequent CD-i audio commits must be checked before any
new patch is proposed.

Do not infer a defect merely because the historical branch made a different choice.
Where the old branch and upstream disagree, evidence priority remains:

1. authoritative Philips/Motorola/MPEG/Red Book/Green Book documentation;
2. reproducible physical CD-i measurements;
3. independent implementations;
4. controlled synthetic tests;
5. compatibility inference, explicitly labeled.

## Port sequence

1. Timestamp-wrap and pure timing arithmetic.
2. MPEG demux/PES routing behavior.
3. Save-state continuity around DVC playback.
4. Starvation/refill and stream-transition behavior.
5. 30-minute A/V drift/presentation fixtures.
6. DMA/IRQ integration.
7. Attenuation/DSP fidelity, only where evidence exceeds current upstream behavior.
8. CDIC/de-emphasis follow-ups after reconciling September upstream audio changes.

A test that upstream already passes is still useful if it protects a subtle hardware
contract.  A historical implementation detail that has no hardware-facing contract
should not be preserved merely because it existed in `cdi-unified`.
