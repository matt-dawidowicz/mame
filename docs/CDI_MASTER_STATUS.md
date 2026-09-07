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

## Current certification — 2026-09-07

Candidate code is based on `39885b7d304ccd7947be45e643a37d7294b84062`; its exact committed source is certified
in the subsequent documentation-only update. The generic CUE follow-up reproduces
210 failed assertions before the fix (3 cases / 3,001 assertions), then passes
3,436 assertions in those cases. Full expanded local gates pass:

- Production CD-i emulator build and `./mame -validate` (exit 0).
- 17,393,781 assertions / 219 helper cases.
- 11,718 assertions / 17 emulator-linked cases, including generic CD-ROM tests.
- DVC DMA liveness GREEN; Musashi regeneration produces no tracked-source diff.

The reader now normalizes CUE indexes, owns pregaps in the upcoming track,
distinguishes logical/physical storage offsets, resets offsets across FILE changes,
and rejects short payload/subcode reads. CDIC uses these indexes for fallback Q.
Four generated CUE layouts and padded CHDs exercise this without retail assets.
The prior Q/TOC CI run [34075842095](https://github.com/matt-dawidowicz/mame/actions/runs/34075842095)
is historical; new-source CI is pending publication. No sanitizer, all-system build,
physical hardware or retail playthrough is claimed.

Same weighted worksheet: SCC **75%**, MMU **75%**, CDIC **70%**, DMA **70%**,
interrupts **70%**, CD-DA **65%**, Q/subcode **70%**, disc handling **60%**.
This strengthens existing obligations without closing their wider evidence gaps;
physical fidelity and compatibility remain unestimated. See the
[Q checkpoint](cdi_q_checkpoint_20260907.md#generic-cue-and-chd-follow-up--2026-09-07)
for implementation, reproduction and uncertainty.

Generic CUE higher indexes and shared/separate stored/virtual gaps now pass,
including live CDIC SRAM, CHD padding and explicit truncated-read failure.
Next: seek-only completion and data-track PCM handoff once controller/output
evidence establishes the expected behavior; multisession and other formats remain open.

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
| [CDIC](cdi_unified_verified_status_20260907.md#cdic) | 55% | **70%** | Medium | Live Q/TOC and four shared/separate CUE layouts with stored/virtual pregaps pass; CUE higher indexes reach SRAM in BCD. Physical status and timing remain open. |
| [MCD212 display](cdi_unified_verified_status_20260907.md#mcd) | 65% | **65%** | Medium | Mode/control/QHY helpers pass; independent full frames and combined overlay remain unverified. |
| [DVC overall](cdi_unified_verified_status_20260907.md#dvc) | 70% | **70%** | Medium | Live ingress/handshake and control-state tests pass; full decoded movie/physical board fidelity remains open. |
| [MPEG video decode and presentation](cdi_unified_verified_status_20260907.md#mpeg_video) | 55% | **55%** | Low | Packet/event/conversion helpers pass; no retained independent full I/P/B picture corpus or combined displayed-frame oracle. |
| [DVC audio](cdi_unified_verified_status_20260907.md#dvc_audio) | 80% | **80%** | Medium | Broad helper/reference tests and real DMA ingress pass; reference PCM tolerance and physical DSP/DAC edges remain. |
| [XA routing and ADPCM](cdi_unified_verified_status_20260907.md#xa) | 75% | **75%** | Medium | Exhaustive helpers and retained exact 4-bit stereo reference exist; other independent modes, silicon and retail evidence remain incomplete. |
| [CD-DA playback and transport](cdi_unified_verified_status_20260907.md#cdda) | 50% | **65%** | Low | Synthetic audio/data Q, raw-subcode fallback, complete TOC and repositioning pass; audible output and broader transport semantics remain unverified. |
| [CD-DA Q and other subcode](cdi_unified_verified_status_20260907.md#q) | 40% | **70%** | Low | Live 45-packet TOC verifies all twelve tracks, triplicate points, absolute starts, first/last track and complete A2 lead-out. Physical lead-in and multisession remain open. |
| [DMA integration](cdi_unified_verified_status_20260907.md#dma) | 60% | **70%** | Medium | Live DVC transfers and both CDIC SRAM boundary/error directions pass; advanced modes and physical arbitration remain open. |
| [Interrupts](cdi_unified_verified_status_20260907.md#irq) | 65% | **70%** | Medium | Live DVC events, executed MMU fault/recovery and CDIC error status pass; expanded peripheral IRQ sequences remain open. |
| [Device timing](cdi_unified_verified_status_20260907.md#timing) | 60% | **60%** | Medium | Arithmetic and DMA cadence tests pass; cycle-exact CPU/bus and physical cross-device calibration remain unverified. |
| [A/V synchronization](cdi_unified_verified_status_20260907.md#av) | 45% | **45%** | Medium | Long-run arithmetic passes; it is not a 30-minute decoded/presented movie or host-output drift measurement. |
| [Save states](cdi_unified_verified_status_20260907.md#save) | 60% | **60%** | Medium | Live audio/control and MMU query snapshots pass; decoded pictures and active peripheral continuation lack complete fixtures. |
| [SLAVE HLE](cdi_unified_verified_status_20260907.md#slave) | 55% | **55%** | Medium | Command/pointer/readiness helpers pass; several protocols remain stubs and physical mailbox timing is modeled. |
| [Input and peripherals](cdi_unified_verified_status_20260907.md#input) | 45% | **45%** | Medium | Pointer helpers pass; keyboard event delivery, controller breadth and serial waveforms are incomplete. |
| [SERVO and MCU integration](cdi_unified_verified_status_20260907.md#servo) | 15% | **15%** | Low | Structural evidence only for much of the scope; live protocol, feedback and complete firmware runtime remain absent. |
| [Disc handling](cdi_unified_verified_status_20260907.md#disc) | 50% | **60%** | Low | Synthetic Q/TOC, four CUE layouts, normalized higher indexes, payload/subcode offsets and truncated-read errors pass. Generic CHD gaps/padding pass; multisession and physical status remain open. |
| [Mono-I/II board glue](cdi_unified_verified_status_20260907.md#glue) | 50% | **50%** | Low | Presence/IRQ helpers and live optional-DVC fixture pass; disabled DSP and unmapped MCU interfaces still block Mono-II. |
| [Mono-II functional system](cdi_unified_verified_status_20260907.md#mono2) | 20% | **20%** | Low | Structural tests pass; host DTACK, SPI, enabled DSP and matching-ROM runtime remain absent. |
| [Cross-system audio](cdi_unified_verified_status_20260907.md#all_audio) | 70% | **70%** | Medium | Strong component tests and synthetic CD-DA Q/TOC coexist with missing audible transport and cross-stream continuity evidence. |
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
   Stored raw Q, complete TOC, CUE higher indexes and four file/pregap layouts
   now pass. Continue with seek-only completion and data-track PCM evidence.

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
