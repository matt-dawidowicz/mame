# Philips CD-i / DVC master status

**Active and default branch: `cdi-unified`. Current unified assessment: 2026-09-07.**
All 32 available project branches are consolidated and the 60 missing upstream
commits through `1fb001f9bfab5cf0148fdfe8755659c4a869831b` are merged. Original
branches and fork `master` were subsequently deleted at the user's request; only
`cdi-unified` remains in the fork and active WSL clone. See the
[branch cleanup record](cdi_branch_cleanup_20260907.md),
[consolidation record](cdi_branch_consolidation_20260906.md) and
[upstream synchronization](cdi_upstream_sync_20260906.md).

This is the canonical high-level index. Routine status reads this document;
verified status updates affected worksheets; development resumes the next action.
[AGENTS.md](../AGENTS.md) defines those modes. The audio campaign remains the
detailed audio ledger on this unified branch.

## Active decoded A/V checkpoint — 2026-09-07

Original changing stereo MPEG audio and two original I/P/B video scenes now run
through the live DVC for 31 seconds across 12 scheduled scene resets. All 768
baseline pictures match independent FFmpeg pixels within 3 RGB levels. A normal
scheduled save/load repeats 747 pictures, 2,644,236 channel samples, 1,499 sound
callback times/counts and timed IRQ/status observations exactly.

The regression reproduced and fixes ring-buffer EOF compaction, delayed reference
output after a trailing B picture, and periodic IRQ phase shifting after load.
See the [decoded A/V checkpoint](cdi_decoded_av_checkpoint_20260907.md) for reference
provenance, decoder tolerances, commands and remaining scope.

Exact-source [CI 34085809119](https://github.com/matt-dawidowicz/mame/actions/runs/34085809119)
passes for `ee2a74166d8ffa54bf7602bbf4cb47889e43eb62`: 21 integration cases / 11,912
assertions and 221 helper cases / 17,394,016 assertions, generated Musashi freshness
and DVC DMA liveness. The local production CD-i build/validity and focused decoder
ASan check also pass. This subsequent documentation-only certification changes no
production or test behavior; full all-system/Windows/macOS and physical/retail
validation are outside this gate.

Only two previously absent evidence obligations gain limited credit: MPEG video
55% to 60% (raw 56.25 to 58.75), and A/V synchronization 45% to 50% (raw 46.25 to
51.25). Other grades are carried unchanged with updated evidence. These remain
scoped engineering judgments, not hardware fidelity or game compatibility.
The 30-minute uninterrupted decoded gate, interactive branching, varied motion,
combined MCD212 output and physical/retail validation remain open.

## Previous CD-DA save/load checkpoint — 2026-09-07

The new live regression saves during active playback, advances, restores and
repeats the continuation. All 93,492 stereo channel samples, sound callback times,
Q words/positions and IRQ/register observations match exactly in four scenarios,
including pending IRQ, an audio-to-data transition and pre-emphasis. Existing
production code passes; no emulator fix was required. Local gates: 96 focused
assertions, 11,892 assertions / 20 integration cases and 17,393,781 assertions /
219 helper cases; validity exit 0 and DMA liveness GREEN. See the
[save/load checkpoint](cdi_cdda_save_checkpoint_20260907.md) for scope and sensitivity evidence.
Exact-source [CI 34082755572](https://github.com/matt-dawidowicz/mame/actions/runs/34082755572) passes for
`65a6a7ac306a613cce4fd4e3c543a474476d437e`: 20 integration cases / 11,892 assertions,
219 helper cases / 17,393,781 assertions, generated-source freshness and DMA
liveness. This subsequent documentation-only certification changes no test or
production behavior.

At this earlier CD-DA checkpoint, grades remained unchanged; decoded A/V evidence is superseded by the active checkpoint above.

## Previous transport certification — 2026-09-07

Verified source: `adf57f583a861e5573e73fa0519e6d5fc76f288a`. Local gates pass:

- Production CD-i emulator build and `./mame -validate` (exit 0).
- 17,393,781 assertions / 219 helper cases.
- 11,796 assertions / 19 emulator-linked cases.
- DVC DMA liveness GREEN; regenerated Musashi output matches committed sources.

The supplied cdapdriv firmware establishes driver-controlled seek completion:
successive Q positions, XBUF IRQ acknowledgement, then software clearing DBUF bit
14. Live rounded/backward seek, abort and read-after-seek fixtures pass without
changing this production behavior. A separate PCM regression reproduces data-track
bytes entering both DACs; the fix gates PCM handoff by image track type while
retaining Q delivery. Exact audio sample counts and data-to-audio resumption pass.
See the [transport checkpoint](cdi_transport_checkpoint_20260907.md).

Exact-source transport CI [34080100953](https://github.com/matt-dawidowicz/mame/actions/runs/34080100953)
also passes: 219 helper cases / 17,393,781 assertions and 19 integration cases /
11,796 assertions, DMA liveness and generated-source freshness. No new physical capture,
executed firmware playthrough, sanitizer or full all-system MAME build is claimed.

Scoped estimates remain SCC **75%**, MMU **75%**, CDIC **70%**, DMA **70%**,
interrupts **70%**, CD-DA **65%**, Q/subcode **70%**, disc handling **60%**.
The new evidence strengthens partial obligations; it does not close full command,
save/restore, reference-media or hardware-fidelity scope.

Driver-controlled seek completion now passes a live Q/IRQ/DBUF fixture.
Data-track bytes are excluded from CD-DA PCM, with exact digital sample counts
verified through audio/data transitions. Active CD-DA save/load now also passes
the CD-DA checkpoint above. Decoded A/V evidence is now recorded in the active
checkpoint; broader transport error and multisession evidence remains open.

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
| [DVC overall](cdi_unified_verified_status_20260907.md#dvc) | 70% | **70%** | Medium | 31-second original decoded A/V, 12 scene resets and active save/load pass; broad movie behavior and physical board fidelity remain open. |
| [MPEG video decode and presentation](cdi_unified_verified_status_20260907.md#mpeg_video) | 55% | **60%** | Low | Full pixels of original I/P/B scenes match FFmpeg and EOF frame loss is fixed; varied motion and combined displayed-frame coverage remain open. |
| [DVC audio](cdi_unified_verified_status_20260907.md#dvc_audio) | 80% | **80%** | Medium | Changing stereo PCM matches FFmpeg within documented tolerance and restores exactly; broader stream controls and physical DSP/DAC edges remain open. |
| [XA routing and ADPCM](cdi_unified_verified_status_20260907.md#xa) | 75% | **75%** | Medium | Exhaustive helpers and retained exact 4-bit stereo reference exist; other independent modes, silicon and retail evidence remain incomplete. |
| [CD-DA playback and transport](cdi_unified_verified_status_20260907.md#cdda) | 50% | **65%** | Low | Live seek, exact mixed-mode PCM counts and four active CD-DA save/load continuations pass; physical output and broader transport/save semantics remain unverified. |
| [CD-DA Q and other subcode](cdi_unified_verified_status_20260907.md#q) | 40% | **70%** | Low | Live 45-packet TOC verifies all twelve tracks, triplicate points, absolute starts, first/last track and complete A2 lead-out. Physical lead-in and multisession remain open. |
| [DMA integration](cdi_unified_verified_status_20260907.md#dma) | 60% | **70%** | Medium | Live DVC transfers and both CDIC SRAM boundary/error directions pass; advanced modes and physical arbitration remain open. |
| [Interrupts](cdi_unified_verified_status_20260907.md#irq) | 65% | **70%** | Medium | Live DVC status/acknowledgement and restored periodic phase pass alongside MMU/CDIC gates; expanded peripheral IRQ sequences remain open. |
| [Device timing](cdi_unified_verified_status_20260907.md#timing) | 60% | **60%** | Medium | Live decoded frame timing and restored DVC timer phase pass; CPU/bus cycle accuracy and physical calibration remain unverified. |
| [A/V synchronization](cdi_unified_verified_status_20260907.md#av) | 45% | **50%** | Medium | 31-second decoded reference playback passes through 12 scene resets and save/load; 30-minute continuous and host-output drift measurements remain open. |
| [Save states](cdi_unified_verified_status_20260907.md#save) | 60% | **60%** | Medium | Active decoded A/V, MMU and CD-DA continuations pass; more snapshot phases and active peripheral modes remain open. |
| [SLAVE HLE](cdi_unified_verified_status_20260907.md#slave) | 55% | **55%** | Medium | Command/pointer/readiness helpers pass; several protocols remain stubs and physical mailbox timing is modeled. |
| [Input and peripherals](cdi_unified_verified_status_20260907.md#input) | 45% | **45%** | Medium | Pointer helpers pass; keyboard event delivery, controller breadth and serial waveforms are incomplete. |
| [SERVO and MCU integration](cdi_unified_verified_status_20260907.md#servo) | 15% | **15%** | Low | Structural evidence only for much of the scope; live protocol, feedback and complete firmware runtime remain absent. |
| [Disc handling](cdi_unified_verified_status_20260907.md#disc) | 50% | **60%** | Low | Synthetic Q/TOC, four CUE layouts, normalized higher indexes, payload/subcode offsets and truncated-read errors pass. Generic CHD gaps/padding pass; multisession and physical status remain open. |
| [Mono-I/II board glue](cdi_unified_verified_status_20260907.md#glue) | 50% | **50%** | Low | Presence/IRQ helpers and live optional-DVC fixture pass; disabled DSP and unmapped MCU interfaces still block Mono-II. |
| [Mono-II functional system](cdi_unified_verified_status_20260907.md#mono2) | 20% | **20%** | Low | Structural tests pass; host DTACK, SPI, enabled DSP and matching-ROM runtime remain absent. |
| [Cross-system audio](cdi_unified_verified_status_20260907.md#all_audio) | 70% | **70%** | Medium | Live changing DVC PCM and mixed-mode CD-DA gates pass; interactive cross-stream, host-output and physical continuity remain open. |
| [Cross-system video](cdi_unified_verified_status_20260907.md#all_video) | 55% | **55%** | Low | Original decoded DVC pixels and short presentation pass; varied video, combined MCD212 frames and hardware/title captures remain open. |
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
   now pass. Driver-controlled seek and data-track PCM exclusion also pass;
   active PCM save/load now passes four live scenarios. Continue with broader
   transport error semantics and other active modes.

2. **Broader decoded A/V:** the 31-second original reference and active save/load
   gate passes. Extend moving/textured I/P/B content, rates/dimensions, pause and
   interactive stream branches, multiple snapshot phases and combined MCD212
   output. Retain a 30-minute uninterrupted decoded run before closing long-run
   continuity; calibrate host and physical timing separately.
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
the user's Windows checkout. Default-branch setup and obsolete-branch deletion are complete;
merged commits remain reachable on unified and staging recovery is documented
in the branch cleanup record. Historical zero-count/abort and absent-interpreter statements
are superseded by the current unified worksheets, not silently applied to this code.
