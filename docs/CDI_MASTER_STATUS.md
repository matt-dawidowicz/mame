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

## Assessment and verification

- Source HEAD reviewed: `ad950937b72f258afe087bc2aa65cdb298e4e46b`.
- Last verified production commit: `ade0f78abf4367f07c9d2fd3d383f448110c4cb2`; source/evidence review: 2026-09-07.
- [CI run 34066705811](https://github.com/matt-dawidowicz/mame/actions/runs/34066705811)
  Executed 2026-09-06; **PASS:** 17,393,781 assertions / 219 helper cases; production build and
  emulator-linked integration 21 assertions / 6 cases; DMA liveness GREEN.
- CI's PR merge tree matches the published code merge. Current source/test code
  is unchanged since that gate. This assessment freshly reviews merge deltas and
  evidence scope; it does not claim a new emulator run, hardware measurement or
  retail-title playthrough. Documentation-only changes need no repeat build.
- All matrix rows inherit these exact commits/date. Each linked worksheet records
  implementation, verification, inaccuracies, remaining work, tests, documentation
  and next action. [Machine-readable weights and grades](cdi_unified_verified_status_20260907.json) reproduce
  every estimate.

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
| [SCC68070 CPU and internal peripherals](cdi_unified_verified_status_20260907.md#scc) | 55% | **65%** | Medium | Helper tests and live DVC DMA pass; timer/UART production event sequences and executed MMU faults are not covered. |
| [SCC68070 MMU](cdi_unified_verified_status_20260907.md#mmu) | 65% | **65%** | Medium | Helpers and CPU-suspended translation/save queries pass; exception stack, RTE and actual protected instructions remain unverified. |
| [CDIC](cdi_unified_verified_status_20260907.md#cdic) | 55% | **55%** | Medium | Routing/control helpers and retained buffer evidence exist; live SRAM-boundary and multi-track transport fixtures are missing. |
| [MCD212 display](cdi_unified_verified_status_20260907.md#mcd) | 65% | **65%** | Medium | Mode/control/QHY helpers pass; independent full frames and combined overlay remain unverified. |
| [DVC overall](cdi_unified_verified_status_20260907.md#dvc) | 70% | **70%** | Medium | Live ingress/handshake and control-state tests pass; full decoded movie/physical board fidelity remains open. |
| [MPEG video decode and presentation](cdi_unified_verified_status_20260907.md#mpeg_video) | 55% | **55%** | Low | Packet/event/conversion helpers pass; no retained independent full I/P/B picture corpus or combined displayed-frame oracle. |
| [DVC audio](cdi_unified_verified_status_20260907.md#dvc_audio) | 80% | **80%** | Medium | Broad helper/reference tests and real DMA ingress pass; reference PCM tolerance and physical DSP/DAC edges remain. |
| [XA routing and ADPCM](cdi_unified_verified_status_20260907.md#xa) | 75% | **75%** | Medium | Exhaustive helpers and retained exact 4-bit stereo reference exist; other independent modes, silicon and retail evidence remain incomplete. |
| [CD-DA playback and transport](cdi_unified_verified_status_20260907.md#cdda) | 50% | **50%** | Low | Helper/reference response tests pass; track/index/relative position defects and mixed-mode transport validation remain. |
| [CD-DA Q and other subcode](cdi_unified_verified_status_20260907.md#q) | 40% | **40%** | Low | Control/placement helpers pass; fixed track/index and incorrect relative/lead-out construction remain. |
| [DMA integration](cdi_unified_verified_status_20260907.md#dma) | 60% | **60%** | Medium | Real DVC boundary passes; CDIC SRAM safety, actual error injection and bus arbitration remain open. |
| [Interrupts](cdi_unified_verified_status_20260907.md#irq) | 65% | **65%** | Medium | Helpers and live DVC abort/completion pass; expanded UART/timer/error-source sequences and physical IACK remain unverified. |
| [Device timing](cdi_unified_verified_status_20260907.md#timing) | 60% | **60%** | Medium | Arithmetic and DMA cadence tests pass; cycle-exact CPU/bus and physical cross-device calibration remain unverified. |
| [A/V synchronization](cdi_unified_verified_status_20260907.md#av) | 45% | **45%** | Medium | Long-run arithmetic passes; it is not a 30-minute decoded/presented movie or host-output drift measurement. |
| [Save states](cdi_unified_verified_status_20260907.md#save) | 60% | **60%** | Medium | Live audio/control and MMU query snapshots pass; decoded pictures and active peripheral continuation lack complete fixtures. |
| [SLAVE HLE](cdi_unified_verified_status_20260907.md#slave) | 55% | **55%** | Medium | Command/pointer/readiness helpers pass; several protocols remain stubs and physical mailbox timing is modeled. |
| [Input and peripherals](cdi_unified_verified_status_20260907.md#input) | 45% | **45%** | Medium | Pointer helpers pass; keyboard event delivery, controller breadth and serial waveforms are incomplete. |
| [SERVO and MCU integration](cdi_unified_verified_status_20260907.md#servo) | 15% | **15%** | Low | Structural evidence only for much of the scope; live protocol, feedback and complete firmware runtime remain absent. |
| [Disc handling](cdi_unified_verified_status_20260907.md#disc) | 50% | **50%** | Low | Routing helpers pass; Q/TOC errors, seek/error behavior and mixed-mode/multisession fixtures remain open. |
| [Mono-I/II board glue](cdi_unified_verified_status_20260907.md#glue) | 50% | **50%** | Low | Presence/IRQ helpers and live optional-DVC fixture pass; disabled DSP and unmapped MCU interfaces still block Mono-II. |
| [Mono-II functional system](cdi_unified_verified_status_20260907.md#mono2) | 20% | **20%** | Low | Structural tests pass; host DTACK, SPI, enabled DSP and matching-ROM runtime remain absent. |
| [Cross-system audio](cdi_unified_verified_status_20260907.md#all_audio) | 70% | **70%** | Medium | Strong component tests coexist with CD-DA transport defects and missing cross-stream output continuity evidence. |
| [Cross-system video](cdi_unified_verified_status_20260907.md#all_video) | 55% | **55%** | Low | Component helpers pass; independent decoded/composed frames and hardware/title captures remain incomplete. |
| [DSP56000/56001 standalone core](cdi_unified_verified_status_20260907.md#dsp) | New row | **40%** | Low | Three helper test files cover host words, bootstrap relocation, masks, loops and wrapping. No emulator-linked complete firmware, interrupt, ALU or cycle-accuracy campaign. |
| [Compatibility](cdi_audio_compatibility_matrix_20260906.md) | Not estimated | **Not estimated** | Low | Retained retail-runtime certification is missing for required XA/DVC/CD-DA categories; this does not mean no games work. |

## Progress since the pre-merge assessment

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

1. **CDIC SRAM DMA safety:** bound accesses beyond the last SRAM word; reproduce
   and test both directions and oversized transfers through the real device.
   Distinguish the chosen safe policy from unmeasured hardware wrap/error behavior.
2. **MMU exception delivery:** execute protected fetch/read/write and boundary
   accesses; fix reset/enable/postload ownership of the restartable fault path;
   verify exception frames and RTE. Current tests suspend the CPU for queries.
3. **CD-DA/Q:** synthetic multi-track/mixed-mode fixtures; correct fixed track/index,
   relative-time and incomplete lead-out construction; verify CRC and transport.
4. **Decoded A/V and save continuity:** retain decoded pictures and PCM across
   sustained presentation, interactive branches and save/load. Timestamp arithmetic
   and a video sequence header do not close this gate.
5. **Newly merged peripheral verification:** live timer match/capture/count IRQs,
   UART mode/break/overrun, active peripheral saves and DMA error injection.
6. **Mono-II/DSP:** complete standalone architectural/firmware execution and
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
