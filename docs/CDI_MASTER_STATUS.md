# Philips CD-i / DVC master status

**Active project branch: `cdi-unified`.** All 32 available project branches are
consolidated; see the [merge inventory, resolutions and verification](cdi_branch_consolidation_20260906.md).
The table below records the last pre-merge estimates. SCC68070, DMA, interrupts,
timing, save states and board/DSP integration require scoped percentage reassessment following combined
validation; do not report their old numbers as newly certified unified results.

**Fresh pre-merge verified-status recalculation: 2026-09-06.** This is the canonical project
index. Routine status reads the matrix, verification boundary and next actions here;
do not repeat the audit. [AGENTS.md](../AGENTS.md) defines request modes and worktree
discipline. The detailed audio campaign controls its task list, subject to the
explicit verification corrections in this audit.

## Upstream synchronization

The 2026-09-06 upstream merge imports all 60 missing commits through
`1fb001f9bfab5cf0148fdfe8755659c4a869831b`. See the
[merge scope and validation record](cdi_upstream_sync_20260906.md).
The CD-i implementation and tests are unchanged by upstream; shared runtime and
file-I/O changes passed the new integration build. Prior percentages remain scoped
as below, without automatic increases. GitHub default-branch administration remains
blocked in this workspace; `cdi-unified` is the active development branch regardless.

## Current unified verification

- Branch: `cdi-unified`; verified code: `ade0f78abf4367f07c9d2fd3d383f448110c4cb2` (2026-09-06).
- Upstream through `1fb001f9bfab5cf0148fdfe8755659c4a869831b` is merged.
  CI tested PR merge `3f3acf4d9c42aa9d73db863106b733e9ff433a71`; its tree exactly
  matches the published code merge. See the [synchronization record](cdi_upstream_sync_20260906.md).
- [CI run 34066705811](https://github.com/matt-dawidowicz/mame/actions/runs/34066705811): **PASS** — helper target
  **17,393,781 assertions / 219 cases**; emulator-linked production build and
  integration target **21 assertions / 6 cases**. Local helpers and DMA liveness
  audit also pass.
- Integration now covers explicit SCC START, held-request re-arm, immediate abort
  and an entire 65536-word transfer. Prior MMU and A/V test-scope caveats still apply.
- The old completion matrix is retained as historical evidence. Merging and passing
  regressions do not automatically increase fidelity percentages. Review the
  [changed implementation scopes](cdi_branch_consolidation_20260906.md#integration-changes-required-by-the-merge)
  for SCC/DMA/DSP before assigning replacement values.
- Next development task: CDIC SRAM bounds; then executed MMU exception delivery.

## Historical assessed branches and last verification

| ID | Branch | Source HEAD assessed | Code baseline | Last verification |
| --- | --- | --- | --- | --- |
| C2 | `cdi-dvc-modernization` | `da9a81c5bdf7c6d24a67b1b608575075ae469c33` | `f0d78dfbda5c7fbbdb11c3fc1ba25a58e731146f` | 2026-09-06: fresh source/diff/test review; no fresh canonical runtime execution. |
| A2 | `audio/cdi-fidelity-100-campaign-20260905` | `21ef9f8fb08a640ed177cf9830f344a947da7344` | `442e5050846e680ed653fc1297b66df588b314e2` | 2026-09-06: fresh source/test review and local helper run, 17,061,392 assertions / 191 cases PASS. |

All matrix rows inherit these exact branch-specific last-reviewed commits/dates.
Audio's last emulator-linked integration run remains existing CI at `442e505`:
[34057720217](https://github.com/matt-dawidowicz/mame/actions/runs/34057720217),
**21 assertions / six cases PASS**. It was not rerun locally. Source HEADs above
include the prior documentation commit; the code baselines do not. Documentation
commits publishing this audit are newer HEADs, not new emulator test milestones.

The unified branch includes the audio/MMU work, DSP bootstrap/interpreter audit,
held-request DMA re-arm policy and the SCC peripheral completion implementation.
Timer 1/2 and UART behavior now extend beyond the A2 snapshot. Its percentages
remain scoped historical estimates until reviewed on the combined source.
The unpushed Windows experiment was unavailable; intentional local Windows files
were not accessed or modified.

## Meaning of the new percentages

These are **fresh scoped engineering-completion estimates**, incorporating
implementation and validation obligations. They are not code coverage, measured
hardware-fidelity percentages, game compatibility rates or effort remaining.
They are recalculated from [weighted source/test worksheets](cdi_verified_status_20260906.md)
and [machine-readable weights/grades](cdi_verified_status_20260906.json).

Each row has obligations totaling 100 weight points, graded 0 (absent), 1
(placeholder or blocking defect), 2 (partial/model), 3 (meaningfully tested with
remaining gaps), or 4 (closed within the named scope). The weighted result is
rounded to five points. Weights/grades are explicit engineering judgments.
Medium confidence means approximately ±5 points of judgment uncertainty; Low may
be ±10 or more. These are not statistical intervals. Old carried percentages had
no equivalent denominator; differences from them are not measured regressions.
No overall project percentage is calculated from overlapping subsystem rows.

A 100% statement applies only to a scope with no known technically defensible
remaining work. Physical unknowns remain open wherever physical fidelity is in scope.
The parser classification and AUDCTL register-model gates retain their narrowly
scoped 100% claims; whole audio, CDIC, MMU and DVC do not.

## Last assessed completion matrix (pre-merge)

Each linked subsystem opens its implementation/evidence worksheet, known
inaccuracies, remaining obligations, relevant source/tests/docs and next action.
The current table supersedes the initialization's stale/unestimated engineering
rows. Compatibility remains unquantifiable as a games-working percentage.

| Subsystem / detailed evidence | Canonical C2 | Audio A2 | Confidence | Implementation and verification snapshot |
| --- | ---: | ---: | --- | --- |
| [SCC68070 CPU and internal peripherals](cdi_verified_status_20260906.md#scc--scc68070-cpu-and-internal-peripherals) | 50% | 55% | Medium | CPU/peripheral paths exist; timers/UART/I2C incomplete; MMU fault defect. |
| [SCC68070 MMU](cdi_verified_status_20260906.md#mmu--scc68070-mmu) | 15% | 65% | Medium | Translation helpers/live query-save pass; actual exception path incomplete. |
| [CDIC](cdi_verified_status_20260906.md#cdic--cdic) | 40% | 55% | Medium | Routing/AUDCTL/buffers improved; DMA bounds and Q fields defective. |
| [MCD212 display](cdi_verified_status_20260906.md#mcd--mcd212-display) | 60% | 65% | Medium | Modes/QHY/control/timing tested; no full-frame or combined-overlay oracle. |
| [DVC overall](cdi_verified_status_20260906.md#dvc--dvc-overall) | 50% | 70% | Medium | Audio/ingress/DMA stronger; video presentation remains a model. |
| [MPEG video decode and presentation](cdi_verified_status_20260906.md#mpeg_video--mpeg-video-decode-and-presentation) | 55% | 55% | Low | Backend and event helpers exist; no independent full I/P/B frame corpus. |
| [DVC audio](cdi_verified_status_20260906.md#dvc_audio--dvc-audio) | 40% | 80% | Medium | Parser/PCM/DMA/gain/response tested; tolerant decoder reference, physical edges open. |
| [XA routing and ADPCM](cdi_verified_status_20260906.md#xa--xa-routing-and-adpcm) | 50% | 75% | Medium | Exhaustive helpers and one exact 4-bit stereo reference; silicon/mode breadth open. |
| [CD-DA playback and transport](cdi_verified_status_20260906.md#cdda--cd-da-playback-and-transport) | 30% | 50% | Low | 75 Hz/588-frame/gating model; track, position and transport fixtures incomplete. |
| [CD-DA Q and other subcode](cdi_verified_status_20260906.md#q--cd-da-q-and-other-subcode) | 20% | 40% | Low | ADR/control/location covered; track/index/time hard-coded and TOC incomplete. |
| [DMA integration](cdi_verified_status_20260906.md#dma--dma-integration) | 45% | 60% | Medium | DVC live gate passes; CDIC SRAM boundary unsafe; advanced modes incomplete. |
| [Interrupts](cdi_verified_status_20260906.md#irq--interrupts) | 55% | 65% | Medium | Priority/ack/readiness tested; error-source breadth and pin timing incomplete. |
| [Device timing](cdi_verified_status_20260906.md#timing--device-timing) | 50% | 60% | Medium | Raster/sector/DCLK models tested; CPU-bus and physical calibration incomplete. |
| [A/V synchronization](cdi_verified_status_20260906.md#av--av-synchronization) | 30% | 45% | Medium | Arithmetic tests pass; no actual 30-minute decoded/presented A/V campaign. |
| [Save states](cdi_verified_status_20260906.md#save--save-states) | 45% | 60% | Medium | Audio and MMU query snapshots pass; decoded pictures/active peripherals incomplete. |
| [SLAVE HLE](cdi_verified_status_20260906.md#slave--slave-hle) | 55% | 55% | Medium | Bounded parser/pointer/response model; several commands remain stubs. |
| [Input and peripherals](cdi_verified_status_20260906.md#input--input-and-peripherals) | 45% | 45% | Medium | Pointer/button path exists; keyboard delivery and controller breadth incomplete. |
| [SERVO and MCU integration](cdi_verified_status_20260906.md#servo--servo-and-mcu-integration) | 15% | 15% | Low | MCU topology exists; full protocol, feedback and runtime integration absent. |
| [Disc handling](cdi_verified_status_20260906.md#disc--disc-handling) | 40% | 50% | Low | Sector/filter model works; Q/TOC/errors/mixed-mode behavior incomplete. |
| [Mono-I/II board glue](cdi_verified_status_20260906.md#glue--mono-iii-board-glue) | 45% | 50% | Low | Mono-I and optional DVC configured; Mono-II interfaces incomplete. |
| [Mono-II functional system](cdi_verified_status_20260906.md#mono2--mono-ii-functional-system) | 20% | 20% | Low | Structural map/IRQ present; DTACK/SPI/DSP/runtime blocked. |
| [Cross-system audio](cdi_verified_status_20260906.md#all_audio--cross-system-audio) | 40% | 70% | Medium | XA/Layer II/output/control improved; CD-DA and real continuity remain. |
| [Cross-system video](cdi_verified_status_20260906.md#all_video--cross-system-video) | 55% | 55% | Low | Native display plus MPEG backend; frame/composition/runtime evidence limited. |
| Compatibility | Not estimated | Not estimated | Low | No retained retail-runtime certification in the required XA/DVC/CD-DA categories: 0/3 certified, not 0% playable. |

## Verification boundary and new findings

- **New passing execution:** the existing 25 helper translation units were built
  using GCC 13.3.0, C++17 and `-O1`, and all 191 cases passed. The audit contains
  the reproducible command. This is not a full emulator build or new hardware test.
- **CDIC DMA bounds defect (both branches):** DMACTL 0x3ffe plus a two-word transfer
  increments beyond the 0x4000-byte SRAM allocation. Source-confirmed; full-device
  runtime reproduction remains to be added. This prevents high CDIC/DMA completion.
- **MMU fault-delivery defect (audio):** the fault path calls Musashi's restartable
  bus-error API without enabling the restart mode it requires. Source-traced;
  executed-CPU reproduction remains necessary. The passing fixture suspends the
  CPU and tests translation queries/save state, so “only silicon questions remain”
  was incorrect. Broad architectural certification is reopened.
- **Q-subcode defects (both):** normal track/index fields are fixed to 01 and
  relative time is populated from absolute MSF. TOC lead-out construction also lacks
  audio track lengths in its accumulator. These are software work, not only unknown
  physical transport timing.
- **A/V coverage limit:** thirty-minute helpers calculate clock positions; they do
  not decode/present a movie. The simultaneous save fixture has queued audio plus a
  video sequence header, not decoded video pictures. Meaningful movie/branch/output
  and decoded-picture continuation gates remain open.
- **PCM/video evidence limit:** Layer II tests use sampled values and tolerances
  up to 1500 PCM counts; they do not prove bit-exact silicon. Video conversion tests
  compare the same backend's output formats. The 26-picture temporary decode-ahead
  model and finite save-replay capacities also remain material boundaries.
- **CI trigger gap F7 resolved during consolidation:** the fast workflow now
  includes `src/devices/machine/scc68070*` and the DSP source/test paths.

See [audit findings F1-F7](cdi_verified_status_20260906.md#new-findings-and-corrected-verification-claims)
for exact production functions, fixture limitations and evidence classifications.
No production fixes or new retail/hardware measurements were performed in this audit.

## Next substantive engineering tasks

1. **CDIC channel-1 SRAM bounds:** reproduce the end-of-SRAM and oversized-transfer
   cases, make production accesses safe, test both directions and add a live fixture.
   Distinguish safe implementation policy from unmeasured physical wrap/error behavior.
2. **MMU exception delivery:** execute protected fetch/read/write and cross-boundary
   operands; repair reset/enable/save-load ownership of the fault mechanism; verify
   vector, state and restart behavior. Update the SCC CI path filters.
3. **CD-DA/Q:** use a synthetic multi-track/mixed-mode image to establish independent
   track/index/relative/absolute time, lead-out and CRC expectations; fix discrepancies
   and exercise transport/save-load. Keep physical seek latency separately labeled.
4. **Decoded A/V:** retain meaningful SCR/PTS/DCLK, decoded pictures and PCM across
   sustained playback, repeated branches and save/load. Existing command cycling is
   useful but does not close that gate.
5. Continue firmware/capture acquisition for exact VMPEG/CDIC arithmetic and DAC
   transitions when evidence exists; do not redo recovered Q22 or de-emphasis.

Perform these as small code/test/docs commits on `cdi-unified`. Historical branches
remain references. The first task stays CDIC SRAM safety after merge validation.

## Subsystem-specific next-action index

| Subsystem | Known remaining work / next action |
| --- | --- |
| SCC68070 CPU and internal peripherals | Fix and execute-test MMU fault delivery; then timer/UART/I2C gaps. |
| SCC68070 MMU | Enable the correct fault path and test executed fetch/read/write faults and RTE. |
| CDIC | Repair the channel-1 SRAM boundary before more audio-fidelity work. |
| MCD212 display | Add complete register-to-pixel and live overlay fixtures, then hardware captures. |
| DVC overall | Prioritize actual branching MPEG presentation and live output evidence. |
| MPEG video decode and presentation | Retain an independent decoded I/P/B reference stream and presentation hashes. |
| DVC audio | Add meaningful branching/output fixtures; do not relabel bounded PCM error as bit-exact. |
| XA routing and ADPCM | Keep core reference gates; acquire silicon arithmetic and retail-scene evidence. |
| CD-DA playback and transport | Fix position metadata with a synthetic mixed-mode reference disc. |
| CD-DA Q and other subcode | Implement correct track/index/relative time and independently check CRC/lead packets. |
| DMA integration | Add CDIC end-of-SRAM transfer regression and resolve bounded behavior. |
| Interrupts | Add live error-source assertions alongside each new transfer/fault fixture. |
| Device timing | Separate timer/sector arithmetic from actual bus and presentation timing. |
| A/V synchronization | Run a meaningful decoded/presented 30-minute A/V fixture and repeated branches. |
| Save states | Test real decoded A/V plus active CDIC/UART/I2C/SLAVE continuations. |
| SLAVE HLE | Add partial-command snapshots and evidence-backed missing commands. |
| Input and peripherals | Validate controller variants; implement keyboard only from protocol evidence. |
| SERVO and MCU integration | Establish SPI/DTACK/host interfaces before claiming servo runtime. |
| Disc handling | Repair CDIC boundaries and build reference-disc transport tests. |
| Mono-I/II board glue | Preserve Mono-I gates; scope asynchronous mailbox and SPI separately. |
| Mono-II functional system | Implement necessary CPU-bus, MCU SPI and DSP foundations; runtime remains unproven. |
| Cross-system audio | Fix source defects first, then reference-disc and real A/V output gaps. |
| Cross-system video | Add independent full-frame decode/composition/presentation evidence. |
| Compatibility | Retain a disc-identified scene for each XA/DVC/CD-DA path, with exact commit, machine/BIOS, inputs and output evidence. |

## Relevant tests and documentation

The [audit evidence registry](cdi_verified_status_20260906.md#evidence-registry)
links every subsystem's relevant source files, tests and documents at the assessed
code commit. The [weighted worksheets](cdi_verified_status_20260906.md#weighted-subsystem-worksheets)
record implementation status, verified scope and known gaps for each obligation.

| Area | Regression entry points | Documentation |
| --- | --- | --- |
| CPU/MMU/DMA/IRQ | `tests/emu/machine/scc68070.cpp`; `cdi_mmu_integration.cpp`; DVC DMA/edge/audio integration fixtures | MMU checkpoint, modernization register audit, audit F1/F2/F7. |
| CDIC/XA/CD-DA | `tests/emu/philips/cdicdic.cpp`, `cdicdic_memory.cpp`, `cdi_audio_arithmetic.cpp` | Audio campaign sections 6-8 and 11-16; audit F1/F3. |
| DVC audio | `cdidvc_audio_format.cpp`, `cdidvc_audio_reference.cpp`, `cdidvc_audio_replay.cpp`, `cdi_fma_attenuation.cpp` | Audio campaign/evidence/arithmetic ledgers; audit F5. |
| MCD212/MPEG video | `mcd212_video.cpp`, `mcd212_control_stream.cpp`, `cdidvc_video_conversion.cpp`, DVC event tests | Modernization display audit; audit F4/F6. |
| Timing/A-V/save | `cdidvc_timing.cpp`, `cdidvc_avsync_threshold.cpp`, `cdi_dvc_state_integration.cpp`, replay/state tests | Audio campaign sections 4/9/17 and DMA/A-V checkpoint; audit F4/F6. |
| SLAVE/input/boards | `cdislavehle_*` tests, `cdimono2.cpp`, optional-DVC edge fixture | Modernization Phases D/E and board audit. |
| Compatibility | Model tests are not retail tests | Audio compatibility matrix and audit compatibility boundary. |

Unqualified test filenames in this table are under `tests/emu/philips/`. Audio-only
files are absent from canonical. The source-pinned audit registry works on both
branches even where a local campaign document is absent.

## Historical evidence and maintenance

The old [modernization document](cdi_modernization_status.md) retains historical
Phase A-E dates, percentages and pre-rewrite hashes; do not report them as the latest
state. Its absent-DMA claim was already qualified during initialization. The new
audit additionally supersedes broad MMU closure, overstated movie/save tests and
“only hardware blockers” language. Audio campaign, MMU checkpoint and final-report
notices direct readers to these corrections.

For future routine **status**, read the unified merge verification first, then
report unaffected inherited estimates and clearly label changed rows pending
reassessment. Do not repeat the whole audit. For
**verified status**, review only the requested or demonstrably changed scopes and
update their worksheets/weights with reasons. For **development**, inspect current
branch/HEAD/worktree, affected docs and commits, implement the next real task, run
appropriate gates and publish coherent commits when authorized. Every substantive
change updates affected master rows, exact verification commit/date, tests and next
steps. Preserve unrelated work and keep implementation branches isolated.
