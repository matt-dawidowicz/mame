# Philips CD-i / DVC master status

**Active and default branch: `cdi-unified`. Current unified assessment: 2026-09-07.**
All 32 available project branches are consolidated and the 60 missing upstream
commits through `1fb001f9bfab5cf0148fdfe8755659c4a869831b` are merged. Original
branches and fork `master` are retained. See the [consolidation record](cdi_branch_consolidation_20260906.md)
and [upstream synchronization](cdi_upstream_sync_20260906.md).

This is the canonical high-level index. Routine status reads this document;
verified status updates affected worksheets; development resumes the next action.
[AGENTS.md](../AGENTS.md) defines those modes. The audio campaign remains the
detailed audio ledger on this unified branch.

## CD-DA/Q transport update — 2026-09-07

Following the MMU certification, the real CDIC path now derives track number,
INDEX 00/01, relative MSF and control bits from disc metadata. A generated
twelve-track mixed-mode fixture passes 435 assertions; the complete integration
gate passes 459 assertions / 8 cases. It checks 33 sector deliveries, sequential
track/gap transitions, backward repositioning and no fabricated lead-out Q.
The independent CRC oracle passes without changing the production CRC algorithm.
See [the Q checkpoint](cdi_q_checkpoint_20260907.md) for scope and remaining gaps.
Same weighted rubric: CD-DA 48.75→65 raw (50→65%), Q 38.75→61.25 (40→60%),
CDIC 63.75→67.5 (65→70%), disc handling 50→56.25 (50→55%).
Physical fidelity remains unestimated. MMU CI run
[34074599579](https://github.com/matt-dawidowicz/mame/actions/runs/34074599579)
is now successful: 219 helpers / 17,393,781 assertions, 7 integration cases / 24 assertions.

## Publication certification — 2026-09-07

Verified production source: `6785ea148341de1bf54fdd8e70cec0ab4115d24e`. Local regenerated-source gate passes
17,393,781 assertions / 219 helper cases and 24 top-level assertions / 7
emulator-linked cases; DVC DMA liveness GREEN. Exact commands and limitations:
[MMU checkpoint](cdi_scc68070_mmu_checkpoint_20260906.md).
CDIC bounds were already published at `f330c0901c7d1aaf5581f50735ee03c6b8aeaa1f`,
supported by CI run [34070663002](https://github.com/matt-dawidowicz/mame/actions/runs/34070663002)
and rerun in this local integration gate. MMU staging run 34073320088 failed;
there was no green candidate to fast-forward. Explicit regeneration of tracked
Musashi output supplied the missing RTE branch, now committed with a CI freshness gate.

Weighted changes (same worksheet, no added denominator): SCC 63.75→72.5 raw
(65→75%), MMU 62.5→75 (65→75%), CDIC 56.25→63.75 (55→65%),
DMA 61.25→71.25 (60→70%), interrupts 66.25→68.75 (65→70%).
SCC CPU/MMU packages rise 2→3, MMU exception/restart 1→3,
CDIC SRAM and DMA channel-1 packages 1→3, all fault/error sources 1→2.
Remaining rows keep their historical grades, evidence dates and gaps.
Emulator-safe CDIC clipping/error behavior is not a silicon claim. MMU recovery
is whole-instruction retry; exact internal-cycle behavior, complete SSW/lane forms,
RR=1, ambiguous CAM and timing remain open. No hardware-fidelity or compatibility
percentage is invented. Next substantive task: synthetic multi-track CD-DA/Q.

## Meaning of the percentages

These estimate **engineering completion including verification obligations**.
The existing rows retain the previous A2 audit's weights and 0–4 grades, rounded
to five points. They are not percentages of games working, code coverage, measured
hardware fidelity, instructions implemented or effort remaining. Medium confidence
is approximately ±5 points of judgment uncertainty; Low can be ±10 or more.
No overall percentage is calculated from overlapping rows. Most consolidated
branches overlap or contain earlier stages already credited in A2; their scores
are not additive.

The earlier campaign's 90–97% estimates used a different, undefined denominator.
They are not a directly comparable baseline. The prior-A2 column below is comparable.
A component can meet a narrow parser/register/digital-gain gate while the wider
subsystem still needs transport fixes or device/hardware verification. No physical
fidelity percentage is invented when evidence is insufficient.

## Current completion matrix

| Subsystem | Prior A2 | Unified | Confidence | Implementation / verification boundary |
| --- | ---: | ---: | --- | --- |
| [SCC68070 CPU and internal peripherals](cdi_unified_verified_status_20260907.md#scc) | 55% | **75%** | Medium | Executed MMU recovery and live CDIC/DVC DMA pass; live timer/UART event sequences remain open. |
| [SCC68070 MMU](cdi_unified_verified_status_20260907.md#mmu) | 65% | **75%** | Medium | Executed read/write/fetch/boundary fault/retry, format-F read frame and active-MMU save/load pass; full SSW and internal-cycle semantics remain open. |
| [CDIC](cdi_unified_verified_status_20260907.md#cdic) | 55% | **70%** | Medium | DMA SRAM safety and synthetic twelve-track Q transport pass; TOC, stored Q and physical error/status remain open. |
| [MCD212 display](cdi_unified_verified_status_20260907.md#mcd) | 65% | **65%** | Medium | Mode/control/QHY helpers pass; independent full frames and combined overlay remain unverified. |
| [DVC overall](cdi_unified_verified_status_20260907.md#dvc) | 70% | **70%** | Medium | Live ingress/handshake and control-state tests pass; full decoded movie/physical board fidelity remains open. |
| [MPEG video decode and presentation](cdi_unified_verified_status_20260907.md#mpeg_video) | 55% | **55%** | Low | Packet/event/conversion helpers pass; no retained independent full I/P/B picture corpus or combined displayed-frame oracle. |
| [DVC audio](cdi_unified_verified_status_20260907.md#dvc_audio) | 80% | **80%** | Medium | Broad helper/reference tests and real DMA ingress pass; reference PCM tolerance and physical DSP/DAC edges remain. |
| [XA routing and ADPCM](cdi_unified_verified_status_20260907.md#xa) | 75% | **75%** | Medium | Exhaustive helpers and retained exact 4-bit stereo reference exist; other independent modes, silicon and retail evidence remain incomplete. |
| [CD-DA playback and transport](cdi_unified_verified_status_20260907.md#cdda) | 50% | **65%** | Low | Synthetic sequential audio/data Q transport and repositioning pass; audible output and broader transport semantics remain unverified. |
| [CD-DA Q and other subcode](cdi_unified_verified_status_20260907.md#q) | 40% | **60%** | Low | Live Q track/index-0/1/time/control/CRC pass on a synthetic twelve-track disc; stored Q, TOC and wider subcode remain open. |
| [DMA integration](cdi_unified_verified_status_20260907.md#dma) | 60% | **70%** | Medium | Live DVC transfers and both CDIC SRAM boundary/error directions pass; advanced modes and physical arbitration remain open. |
| [Interrupts](cdi_unified_verified_status_20260907.md#irq) | 65% | **70%** | Medium | Live DVC events, executed MMU fault/recovery and CDIC error status pass; expanded peripheral IRQ sequences remain open. |
| [Device timing](cdi_unified_verified_status_20260907.md#timing) | 60% | **60%** | Medium | Arithmetic and DMA cadence tests pass; cycle-exact CPU/bus and physical cross-device calibration remain unverified. |
| [A/V synchronization](cdi_unified_verified_status_20260907.md#av) | 45% | **45%** | Medium | Long-run arithmetic passes; it is not a 30-minute decoded/presented movie or host-output drift measurement. |
| [Save states](cdi_unified_verified_status_20260907.md#save) | 60% | **60%** | Medium | Live audio/control and MMU query snapshots pass; decoded pictures and active peripheral continuation lack complete fixtures. |
| [SLAVE HLE](cdi_unified_verified_status_20260907.md#slave) | 55% | **55%** | Medium | Command/pointer/readiness helpers pass; several protocols remain stubs and physical mailbox timing is modeled. |
| [Input and peripherals](cdi_unified_verified_status_20260907.md#input) | 45% | **45%** | Medium | Pointer helpers pass; keyboard event delivery, controller breadth and serial waveforms are incomplete. |
| [SERVO and MCU integration](cdi_unified_verified_status_20260907.md#servo) | 15% | **15%** | Low | Structural evidence only for much of the scope; live protocol, feedback and complete firmware runtime remain absent. |
| [Disc handling](cdi_unified_verified_status_20260907.md#disc) | 50% | **55%** | Low | Synthetic mixed-mode Q transport passes; TOC, separate-file gaps, multisession and seek/error fidelity remain open. |
| [Mono-I/II board glue](cdi_unified_verified_status_20260907.md#glue) | 50% | **50%** | Low | Presence/IRQ helpers and live optional-DVC fixture pass; disabled DSP and unmapped MCU interfaces still block Mono-II. |
| [Mono-II functional system](cdi_unified_verified_status_20260907.md#mono2) | 20% | **20%** | Low | Structural tests pass; host DTACK, SPI, enabled DSP and matching-ROM runtime remain absent. |
| [Cross-system audio](cdi_unified_verified_status_20260907.md#all_audio) | 70% | **70%** | Medium | Strong component tests coexist with CD-DA transport defects and missing cross-stream output continuity evidence. |
| [Cross-system video](cdi_unified_verified_status_20260907.md#all_video) | 55% | **55%** | Low | Component helpers pass; independent decoded/composed frames and hardware/title captures remain incomplete. |
| [DSP56000/56001 standalone core](cdi_unified_verified_status_20260907.md#dsp) | New row | **40%** | Low | Three helper test files cover host words, bootstrap relocation, masks, loops and wrapping. No emulator-linked complete firmware, interrupt, ALU or cycle-accuracy campaign. |
| [Compatibility](cdi_audio_compatibility_matrix_20260906.md) | Not estimated | **Not estimated** | Low | Retained retail-runtime certification is missing for required XA/DVC/CD-DA categories; this does not mean no games work. |

## Historical consolidation progress (superseded for changed rows above)

- **SCC68070 55% → 65%:** Timer 1/2, UART and DMA controller packages now receive
  credit for their merged implementation and helper/live-boundary evidence.
- **DMA remains 60% after rounding (raw 58.75 → 61.25):** expanded termination/error APIs add partial advanced-mode
  credit. Existing channel-2 closure is strengthened by explicit START, held-DREQ
  re-arm, immediate abort and the complete 65536-word live transfer.
- **Standalone DSP core 40%, newly scored:** host/bootstrap and partial execution
  are real progress. Mono-II remains 20% because its DSP is disabled, host mapping
  absent and DTACK/SPI/full firmware integration unfinished.
- Audio/video work and narrowly closed MPEG audio parser, recovered FMA Q22
  digital gain and AUDCTL model gates are retained. Full audio/video estimates
  do not automatically rise when historical branches or comparison-only files
  are merged. Interrupt, timing and save-state grades remain limited by their
  existing runtime/physical verification obligations.

## Known gaps and next substantive tasks

1. **CD-DA/Q:** track/index-0/1/relative time now pass live synthetic-disc tests.
   Continue with stored Q and metadata-derived TOC lead packets.

2. **Decoded A/V and save continuity:** retain decoded pictures and PCM across
   sustained presentation, interactive branches and save/load. Timestamp arithmetic
   and a video sequence header do not close this gate.
3. **Newly merged peripheral verification:** live timer match/capture/count IRQs,
   UART mode/break/overrun, active peripheral saves and DMA error injection.
4. **Mono-II/DSP:** complete standalone architectural/firmware execution and
   address-space integration, then required board interfaces; do not enable an
   incomplete device to manufacture system completion.

Work on `cdi-unified` in small code/test/documentation commits. Do not redo closed
de-emphasis or recovered-Q22 work absent a real defect. Keep physical calibration
and retail compatibility claims separate from passing software regression tests.

## Documentation and history

- [Current unified evidence and worksheets](cdi_unified_verified_status_20260907.md).
- [Detailed audio campaign](cdi_audio_fidelity_campaign.md).
- [Historical pre-merge audit and branch baselines](cdi_verified_status_20260906.md).
- [Historical branch inventory and conflict resolutions](cdi_branch_consolidation_20260906.md).
- [Upstream merge and passing build evidence](cdi_upstream_sync_20260906.md).

The specifically named FMV clock/underflow experiment was unavailable in remote
refs and accessible worktrees. No claim is made about its unpushed contents or
the user's Windows checkout. Default-branch setup is complete; old branches remain
recovery references. Historical zero-count/abort and absent-interpreter statements
are superseded by the current unified worksheets, not silently applied to this code.
