# Unified CD-i verified status — 2026-09-07

This report updates the consolidation assessment with executed MMU/CDIC and
synthetic Q/TOC evidence. The current source certification is below. Historical figures remain in
[the earlier audit](cdi_verified_status_20260906.md).

## Current certification — 2026-09-07

Verified code: `fe5aabb5089aedb6cbb3a9dc8eac587886cf33e2`. The generic CUE follow-up reproduces
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
Exact-source CI [34077601590](https://github.com/matt-dawidowicz/mame/actions/runs/34077601590) also passes: 219 helper cases / 17,393,781 assertions,
17 integration cases / 11,718 assertions, DMA liveness and generated-source freshness. No sanitizer, all-system build,
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

## Historical consolidation method and evidence boundary

The 23 existing subsystem rows retain their exact A2 obligation weights, grading
scale and five-point rounding. Each worksheet shows old and current grades so
changes are comparable. The new standalone DSP row has its own explicit scope.
Scores estimate engineering completion including implementation and verification;
they are not a percentage of instructions implemented, games working, elapsed
effort, measured hardware fidelity or test coverage. Do not average overlapping
rows into an overall percentage. The old campaign's 90–97% figures use a different,
undefined denominator and cannot be converted into measured losses or gains.

The review compared A2 code `442e5050846e680ed653fc1297b66df588b314e2` with the
current tree, inspected substantive SCC/DMA/DSP changes, read current production
and fixture boundaries for retained defects, and checked the audio/compatibility
ledgers. Unchanged scopes were reconciled by source/test diffs and existing evidence,
not independently re-audited line by line or against every hardware manual.

Verified production baseline: `ade0f78abf4367f07c9d2fd3d383f448110c4cb2`. At the consolidation assessment, HEAD differed from that commit
only in documentation; the current certification above supersedes this baseline. [CI run 34066705811](https://github.com/matt-dawidowicz/mame/actions/runs/34066705811)
was rechecked as successful: **17,393,781 assertions / 219 helper cases** and
**21 assertions / 6 emulator-linked cases**, plus the DVC DMA liveness check.
CI tested `3f3acf4d9c42aa9d73db863106b733e9ff433a71`; its tree exactly matches
the published upstream merge (`d0392992e6081c11d6f9a2227334e4a1faed4f40`).
No redundant build was run for this documentation audit. No new physical captures,
retail playthroughs or complete firmware execution were performed. Existing
hardware-derived evidence remains credited only within its documented scope.

Medium confidence means roughly ±5 points of judgment uncertainty; Low may be
±10 or more. Neither is a statistical interval. Exact weights and rationale are
in the [JSON worksheet](cdi_unified_verified_status_20260907.json).

Most source branches overlap or represent earlier/re-written stages. Their audio
and native-video improvements were already present in A2; merging branch names
does not create additive subsystem progress. The changed raw DMA score remains
within the same five-point reporting band.

## Historical consolidation changes

- **SCC68070: 55% → 65%.** Timer 1/2 scheduling/capture/count, UART frame/mode/
  break/receive handling and controller semantics are substantive implementation
  gains. Their three packages rise from grade 2 to 3. Timer/UART helper tests do
  not establish live event/IRQ or serial waveform accuracy.
- **DMA: 60% → 60% after rounding (58.75 → 61.25 raw).** START/CA ownership, held DREQ re-arm, immediate abort and
  all 65536 operands pass the real SCC/DVC fixture. The existing channel-2 package
  was already grade 4, so those stronger tests do not add duplicate score credit.
  New termination/error APIs raise the advanced-mode package from 1 to 2; actual
  error injection, bus wiring and CDIC bounds remain open.
- **Standalone DSP core: 40%, newly separated.** Host/bootstrap transport and
  partial instruction execution are retained and tested. ALU/AGU, full registers,
  device address spaces, interrupts and timing are incomplete. This is the core
  intended for Mono-II DRVDSP; it does not close VMPEG audio-silicon questions.
- **Mono-II stays 20%; board glue stays 50%.** `DSP56001(...).set_disable()` and
  unmapped host addresses remain in the driver. A partial standalone interpreter
  does not establish operational board integration. The old claim that the core
  itself has no interpreter is obsolete and corrected here.
- **Interrupts 65%, timing 60%, save states 60% remain at their current grades.**
  Additional sources/events/state registration improve implementation breadth,
  but executed fault delivery, bus calibration and active peripheral snapshots
  still limit those existing obligations. Their new implementations are recorded
  without inventing additional test coverage.
- Audio/display source differences are mainly attribution, comments, opt-in
  logging and DMA interface reconciliation. Existing parser, de-emphasis, gain,
  XA and display work is preserved. The alternative `cdicdic_audio.h` and unused
  `cdi_video_timing.h` do not add active production fidelity or test certification.

## Open findings and corrected stale claims

1. **CDIC SRAM bounds: closed software safety defect.** Both live DMA directions
   and boundary/error behavior pass; physical clipping/wrap policy is unmeasured.
2. **MMU executed fault delivery: closed tested software gap.** Regenerated format-F
   RTE, guest repair and instruction retry pass. Full silicon semantics remain open.
3. **CD-DA/Q:** track/index-0/1/relative time now pass live synthetic-disc tests.
   Stored raw Q, complete TOC, CUE higher indexes and four file/pregap layouts
   now pass. Continue with seek-only completion and data-track PCM evidence.

4. **A/V and saves:** 30-minute arithmetic is not a decoded/presented movie.
   The live video save case uses a sequence header, not decoded picture history.
   Output drift, full-frame reference and active peripheral continuation remain.
5. **Historical zero-count/abort description:** the earlier DMA checkpoint says
   zero count stays idle and abort waits for a service tick. Current START makes
   MTC=0 mean 65536 and the reconfiguration callback stops service immediately.
   That checkpoint is now prominently labeled historical/superseded.
6. **CI filter issue F7 is closed:** current filters include machine SCC, DSP,
   shared emulator/utilities/frontend and build-script changes. This is completed
   infrastructure, not an automatic fidelity-percentage increase.

## Retained narrow completion milestones

The audio campaign still records scoped closure for MPEG-1 Layer II parser
classification/access, recovered VMPEG FMA digital Q22 gain/quantization, and the
AUDCTL register model. These are narrow software/model gates, not 100% claims for
all DVC audio, CDIC audio, analogue transitions or silicon behavior. See
[the campaign](cdi_audio_fidelity_campaign.md), sections 1, 10 and 11. No completed
de-emphasis or recovered-Q22 work is reopened by this reassessment.

## Current matrix

| Subsystem | Previous A2 | Unified | Confidence |
| --- | ---: | ---: | --- |
| [SCC68070 CPU and internal peripherals](#scc) | 55% | 75% | Medium |
| [SCC68070 MMU](#mmu) | 65% | 75% | Medium |
| [CDIC](#cdic) | 55% | 70% | Medium |
| [MCD212 display](#mcd) | 65% | 65% | Medium |
| [DVC overall](#dvc) | 70% | 70% | Medium |
| [MPEG video decode and presentation](#mpeg_video) | 55% | 55% | Low |
| [DVC audio](#dvc_audio) | 80% | 80% | Medium |
| [XA routing and ADPCM](#xa) | 75% | 75% | Medium |
| [CD-DA playback and transport](#cdda) | 50% | 65% | Low |
| [CD-DA Q and other subcode](#q) | 40% | 70% | Low |
| [DMA integration](#dma) | 60% | 70% | Medium |
| [Interrupts](#irq) | 65% | 70% | Medium |
| [Device timing](#timing) | 60% | 60% | Medium |
| [A/V synchronization](#av) | 45% | 45% | Medium |
| [Save states](#save) | 60% | 60% | Medium |
| [SLAVE HLE](#slave) | 55% | 55% | Medium |
| [Input and peripherals](#input) | 45% | 45% | Medium |
| [SERVO and MCU integration](#servo) | 15% | 15% | Low |
| [Disc handling](#disc) | 50% | 60% | Low |
| [Mono-I/II board glue](#glue) | 50% | 50% | Low |
| [Mono-II functional system](#mono2) | 20% | 20% | Low |
| [Cross-system audio](#all_audio) | 70% | 70% | Medium |
| [Cross-system video](#all_video) | 55% | 55% | Low |
| [DSP56000/56001 standalone core](#dsp) | Not separately scored | 40% | Low |
| Compatibility | Not estimated | Not estimated | Low |

## Subsystem worksheets

Changed SCC/MMU/CDIC/DMA/interrupt/CD-DA/Q/disc rows use the current certification above;
other rows retain their historical evidence dates as recorded in JSON.
Linked implementation, tests and documentation are repository paths at that HEAD.
Each below-grade-4 rationale is also an explicit remaining obligation/evidence gap.

<a id="scc"></a>

### SCC68070 CPU and internal peripherals

**75% — Medium confidence; raw weighted score 72.5.**

Implementation: CPU/MMU access integration, Timer 0/1/2, expanded UART and DMA are implemented.

Verification: Executed MMU recovery and live CDIC/DVC DMA pass; live timer/UART event sequences remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| CPU access integration | 15 | 2 | 3 | Executed protected read/write/fetch and segment-boundary fault/retry pass; the complete SCC instruction corpus remains open. |
| Timers | 10 | 2 | 3 | Timer 1/2 match timers and capture/count input callbacks are implemented. Exhaustive mode, edge, overflow and status helpers pass; live timer callback/IRQ timing remains untested. |
| DMA controller | 15 | 2 | 3 | Helpers and live SCC/DVC and CDIC boundary/error transfers pass; advanced request modes and arbitration remain open. |
| Interrupt controller | 10 | 3 | 3 | Priority helpers and acknowledgements; physical IACK incomplete. |
| UART | 10 | 2 | 3 | Mode-dependent frames, character completion, receive overrun, break and echo/loopback behavior are implemented. Frame/mask/status helpers pass; live serial modes, overrun and break timing need fixtures. |
| I2C | 10 | 2 | 2 | Master state machine exists; slave and multi-master incomplete. |
| MMU integration | 20 | 2 | 3 | Restart armed across reset/postload; live format-F/RTE whole-instruction recovery passes. Internal-cycle restart and complete SSW fidelity remain open. |
| Reset and persistent state | 10 | 3 | 3 | Timer inputs/outputs, UART break state and DMA state are registered/reset. Active peripheral save/load fixtures remain incomplete. |

Implementation: [src/devices/cpu/m68000/m68kcpu.cpp](../src/devices/cpu/m68000/m68kcpu.cpp), [src/devices/cpu/m68000/scc68070.cpp](../src/devices/cpu/m68000/scc68070.cpp), [src/devices/machine/scc68070.cpp](../src/devices/machine/scc68070.cpp), [src/devices/machine/scc68070.h](../src/devices/machine/scc68070.h), [src/devices/machine/scc68070_helpers.h](../src/devices/machine/scc68070_helpers.h), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdi_dvc_dma_service.h](../src/mame/philips/cdi_dvc_dma_service.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/machine/scc68070.cpp](../tests/emu/machine/scc68070.cpp), [tests/emu/machine/scc68070_peripherals.cpp](../tests/emu/machine/scc68070_peripherals.cpp), [tests/emu/philips/cdi_dvc_audio_dma_integration.cpp](../tests/emu/philips/cdi_dvc_audio_dma_integration.cpp), [tests/emu/philips/cdi_dvc_dma_integration.cpp](../tests/emu/philips/cdi_dvc_dma_integration.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](../tests/emu/philips/cdi_mmu_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](../tests/emu/philips/cdislavehle_response_ready.cpp).

Documentation: [docs/cdi_branch_consolidation_20260906.md](../docs/cdi_branch_consolidation_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md), [docs/cdi_scc68070_mmu_checkpoint_20260906.md](../docs/cdi_scc68070_mmu_checkpoint_20260906.md).

Next action: Add live Timer 1/2 and UART mode/break/overrun fixtures, then I2C gaps.

<a id="mmu"></a>

### SCC68070 MMU

**75% — Medium confidence; raw weighted score 75.**

Implementation: Descriptor translation, permissions and access preflight exist.

Verification: Executed read/write/fetch/boundary fault/retry, format-F read frame and active-MMU save/load pass; full SSW and internal-cycle semantics remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Register interface | 15 | 4 | 4 | Masked descriptor/control state has helpers and live register round trip. |
| Translation and CAM | 25 | 3 | 3 | Live executed translation and descriptor repair pass; duplicate CAM silicon behavior remains unproven. |
| Permissions and operand boundaries | 20 | 3 | 3 | Executed read/write/fetch protection and segment-limit crossing/retry pass; exhaustive transfer forms and lanes remain open. |
| Exception and restart | 25 | 1 | 3 | Live 17-word format-F read frame, fault address/SSW/MSR, guest repair and RTE whole-instruction recovery pass; RR=1, full SSW and internal-cycle restart remain open. |
| Save/load | 10 | 3 | 3 | Active-MMU register/query ram_state round trip followed by executed fault recovery passes; snapshots inside a partial fault cycle remain untested. |
| Silicon timing and ambiguous CAM edges | 5 | 0 | 0 | No distinguishing physical oracle. |

Implementation: [src/devices/cpu/m68000/m68kcpu.cpp](../src/devices/cpu/m68000/m68kcpu.cpp), [src/devices/cpu/m68000/scc68070.cpp](../src/devices/cpu/m68000/scc68070.cpp), [src/devices/machine/scc68070.h](../src/devices/machine/scc68070.h), [src/devices/machine/scc68070_helpers.h](../src/devices/machine/scc68070_helpers.h).

Tests: [tests/emu/machine/scc68070.cpp](../tests/emu/machine/scc68070.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](../tests/emu/philips/cdi_mmu_integration.cpp).

Documentation: [docs/cdi_scc68070_mmu_checkpoint_20260906.md](../docs/cdi_scc68070_mmu_checkpoint_20260906.md).

Next action: Extend transfer-form/SSW coverage, RR=1 and internal-cycle restart only with authoritative evidence.

<a id="cdic"></a>

### CDIC

**70% — Medium confidence; raw weighted score 71.25.**

Implementation: Commands, sector routing, double buffers, XA and AUDCTL are implemented.

Verification: Live Q/TOC and four shared/separate CUE layouts with stored/virtual pregaps pass; CUE higher indexes reach SRAM in BCD. Physical status and timing remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Commands and transport | 15 | 2 | 2 | Read/stop state exists; seek-only completion and errors remain modeled. |
| Mode-2 routing and coding | 15 | 4 | 4 | Audio exhausts standards routing, coding and selected parameter policy. |
| Buffers and delivery | 15 | 3 | 3 | Measured double-buffer behavior; no full disc-device campaign. |
| Audio control and handoff | 15 | 3 | 3 | Audio fixes AUDCTL/playback/sector ownership; physical edges remain. |
| DMA SRAM boundaries | 15 | 1 | 3 | Both live DMA directions stop safely at 16 KiB and report SCC device bus error; clipping/error policy is emulator safety, not measured silicon behavior. |
| TOC and subcode | 15 | 1 | 3 | Live Q/TOC and four shared/separate CUE layouts with stored/virtual pregaps pass; CUE higher indexes reach SRAM in BCD. Physical status and timing remain open. |
| Error/status fidelity | 5 | 1 | 1 | Read failure is bounded; hardware error response incomplete. |
| Active save/load | 5 | 2 | 2 | Fields are registered; CDIC live transport snapshot fixture missing. |

Implementation: [src/devices/machine/scc68070.cpp](../src/devices/machine/scc68070.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdi_dvc_dma_service.h](../src/mame/philips/cdi_dvc_dma_service.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_memory.h](../src/mame/philips/cdicdic_memory.h), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h).

Tests: [tests/emu/machine/scc68070_peripherals.cpp](../tests/emu/machine/scc68070_peripherals.cpp), [tests/emu/philips/cdi_dvc_audio_dma_integration.cpp](../tests/emu/philips/cdi_dvc_audio_dma_integration.cpp), [tests/emu/philips/cdi_dvc_dma_integration.cpp](../tests/emu/philips/cdi_dvc_dma_integration.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdicdic_memory.cpp](../tests/emu/philips/cdicdic_memory.cpp).

Documentation: [docs/cdi_audio_compatibility_matrix_20260906.md](../docs/cdi_audio_compatibility_matrix_20260906.md), [docs/cdi_audio_fidelity.md](../docs/cdi_audio_fidelity.md), [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

Next action: Validate seek-only completion and mixed-mode data-track PCM gating; extend image-layout coverage.

Current live disc evidence: [Q checkpoint](cdi_q_checkpoint_20260907.md).

<a id="mcd"></a>

### MCD212 display

**65% — Medium confidence; raw weighted score 65.**

Implementation: CLUT/RLE/DYUV/RGB/QHY, control lists and pixel effects exist.

Verification: Mode/control/QHY helpers pass; independent full frames and combined overlay remain unverified.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Display modes | 20 | 3 | 3 | CLUT/RLE/DYUV/RGB/QHY implementation; complete pixel-reference corpus absent. |
| ICA/DCA and memory bounds | 20 | 3 | 3 | Audio adds wrapped control fetches; whole live command engine not exhaustive. |
| Field timing | 15 | 3 | 3 | Audio increases control-space testing; physical field edges remain. |
| QHY reconstruction | 15 | 3 | 3 | Token/FIR/quantizer vectors; cold field and odd-sum silicon remain. |
| Matte/cursor/mosaic/weight | 15 | 2 | 2 | Production pixel paths exist; limited independent image oracles. |
| External overlay | 5 | 2 | 2 | Eligibility helper tested; combined-device frame not tested. |
| Save state | 5 | 2 | 2 | Fields saved; active frame reconstruction corpus missing. |
| Physical output certification | 5 | 0 | 0 | No current full-frame hardware match. |

Implementation: [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/mcd212.cpp](../src/mame/philips/mcd212.cpp), [src/mame/philips/mcd212_control_stream.h](../src/mame/philips/mcd212_control_stream.h), [src/mame/philips/mcd212_video.h](../src/mame/philips/mcd212_video.h).

Tests: [tests/emu/philips/mcd212_control_stream.cpp](../tests/emu/philips/mcd212_control_stream.cpp), [tests/emu/philips/mcd212_video.cpp](../tests/emu/philips/mcd212_video.cpp).

Documentation: [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Add complete register-to-pixel and live overlay fixtures, then hardware captures.

<a id="dvc"></a>

### DVC overall

**70% — Medium confidence; raw weighted score 70.**

Implementation: Ingress, registers, audio/video backend, scheduling and DMA operate as a model.

Verification: Live ingress/handshake and control-state tests pass; full decoded movie/physical board fidelity remains open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Ingress and parser | 15 | 4 | 4 | Audio header/access/PES paths have broad deterministic coverage. |
| Registers/status/commands | 15 | 3 | 3 | Stream/CSU and optional presence tested; private event edges unresolved. |
| Audio decode/output | 20 | 3 | 3 | Reference-tested audio; physical arithmetic/output limits remain. |
| Video decode/output | 20 | 2 | 2 | Backend exists; independent I/P/B decode corpus absent. |
| Presentation scheduling | 10 | 2 | 2 | 26-frame temporary decode-ahead and first-PTS anchoring are models. |
| DVC DMA boundary | 10 | 4 | 4 | Live SCC/DVC fixtures now cover explicit START, held-request re-arm, immediate abort, full 65536-word transfer and legal Layer II ingress. Grade 4 remains limited to this tested software handshake scope. |
| Save reconstruction | 5 | 3 | 3 | Live state/queued audio/video header tested, no real decoded A/V continuation. |
| Physical board fidelity | 5 | 0 | 0 | No full VMPEG timing/analogue certification. |

Implementation: [3rdparty/pl_mpeg/pl_mpeg.h](../3rdparty/pl_mpeg/pl_mpeg.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/cdidvc_mpeg_format.h](../src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_save_state.h](../src/mame/philips/cdidvc_save_state.h), [src/mame/philips/cdidvc_utils.h](../src/mame/philips/cdidvc_utils.h), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](../tests/emu/philips/cdi_mmu_integration.cpp), [tests/emu/philips/cdidvc.cpp](../tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](../tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_audio_replay.cpp](../tests/emu/philips/cdidvc_audio_replay.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_state_transitions.cpp](../tests/emu/philips/cdidvc_state_transitions.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](../tests/emu/philips/cdidvc_video_conversion.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

Next action: Prioritize actual branching MPEG presentation and live output evidence.

<a id="mpeg_video"></a>

### MPEG video decode and presentation

**55% — Low confidence; raw weighted score 56.25.**

Implementation: PL_MPEG decode, picture queues and presentation handoff exist.

Verification: Packet/event/conversion helpers pass; no retained independent full I/P/B picture corpus or combined displayed-frame oracle.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Backend decode | 20 | 2 | 2 | PL_MPEG runs; test tree does not retain an independent full video decode oracle. |
| Packet/header handling | 20 | 3 | 3 | PES and sequence/header helpers covered. |
| Picture event/reordering | 15 | 3 | 3 | All picture-type/marker helper combinations, not complete picture decode. |
| Color conversion | 10 | 3 | 3 | Audio compares PL_MPEG RGB/BGRA/ARGB paths; internal consistency only. |
| Presentation/queue timing | 15 | 2 | 2 | Approximate backend queue and timestamp policy remain. |
| MCD212 composition | 10 | 2 | 2 | Production handoff exists; no combined-device pixel fixture. |
| Independent frame/title certification | 10 | 0 | 0 | No retained present-frame or retail-runtime corpus in reviewed scope. |

Implementation: [3rdparty/pl_mpeg/pl_mpeg.h](../3rdparty/pl_mpeg/pl_mpeg.h), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/cdidvc_mpeg_format.h](../src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_utils.h](../src/mame/philips/cdidvc_utils.h), [src/mame/philips/mcd212.cpp](../src/mame/philips/mcd212.cpp), [src/mame/philips/mcd212_control_stream.h](../src/mame/philips/mcd212_control_stream.h), [src/mame/philips/mcd212_video.h](../src/mame/philips/mcd212_video.h).

Tests: [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdidvc.cpp](../tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](../tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](../tests/emu/philips/cdidvc_video_conversion.cpp), [tests/emu/philips/mcd212_control_stream.cpp](../tests/emu/philips/mcd212_control_stream.cpp), [tests/emu/philips/mcd212_video.cpp](../tests/emu/philips/mcd212_video.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Retain an independent decoded I/P/B reference stream and presentation hashes.

<a id="dvc_audio"></a>

### DVC audio

**80% — Medium confidence; raw weighted score 80.**

Implementation: Parser, Layer II decode, queues, recovered digital gain, emphasis and replay are implemented.

Verification: Broad helper/reference tests and real DMA ingress pass; reference PCM tolerance and physical DSP/DAC edges remain.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Parser/access/selection | 15 | 4 | 4 | Exhaustive legal syntax/profile, stream IDs and access forms. |
| Layer II PCM decode | 20 | 3 | 3 | Three repeated frames/reference sampling; tolerances up to 1500 PCM counts. |
| PCM queue | 15 | 3 | 3 | Deterministic pair/zero/refill; physical DAC rule missing. |
| Live DMA ingress | 10 | 4 | 4 | Legal frame traverses actual SCC/DVC device boundary in existing CI. |
| FMA digital gain | 10 | 4 | 4 | Literal recovered Q22 curve, routes and mute tested. |
| De-emphasis response | 10 | 3 | 3 | Standards shelf response tested; physical transition unknown. |
| Termination and switching | 10 | 3 | 3 | Program end/requested-current state tested; interactive PTS branches absent. |
| Audio save/load | 5 | 3 | 3 | Replay and device state tested; real full A/V continuation absent. |
| Physical DSP/DAC attribution | 5 | 0 | 0 | Exact instruction/limiter and waveform remain unavailable. |

Implementation: [3rdparty/pl_mpeg/pl_mpeg.h](../3rdparty/pl_mpeg/pl_mpeg.h), [src/mame/philips/cdiaudio.h](../src/mame/philips/cdiaudio.h), [src/mame/philips/cdiaudio_dsp56001.h](../src/mame/philips/cdiaudio_dsp56001.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/cdidvc_mpeg_format.h](../src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_save_state.h](../src/mame/philips/cdidvc_save_state.h), [src/mame/philips/cdidvc_utils.h](../src/mame/philips/cdidvc_utils.h), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/philips/cdi_audio_arithmetic.cpp](../tests/emu/philips/cdi_audio_arithmetic.cpp), [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdi_fma_attenuation.cpp](../tests/emu/philips/cdi_fma_attenuation.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](../tests/emu/philips/cdi_mmu_integration.cpp), [tests/emu/philips/cdidvc.cpp](../tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](../tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_audio_replay.cpp](../tests/emu/philips/cdidvc_audio_replay.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_state_transitions.cpp](../tests/emu/philips/cdidvc_state_transitions.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](../tests/emu/philips/cdidvc_video_conversion.cpp).

Documentation: [docs/cdi_audio_arithmetic_checkpoint.md](../docs/cdi_audio_arithmetic_checkpoint.md), [docs/cdi_audio_fidelity.md](../docs/cdi_audio_fidelity.md), [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

Next action: Add meaningful branching/output fixtures; do not relabel bounded PCM error as bit-exact.

<a id="xa"></a>

### XA routing and ADPCM

**75% — Medium confidence; raw weighted score 73.75.**

Implementation: Routing, 4/8-bit mono/stereo ADPCM, histories and emphasis are implemented.

Verification: Exhaustive helpers and retained exact 4-bit stereo reference exist; other independent modes, silicon and retail evidence remain incomplete.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Sector routing | 15 | 4 | 4 | Exhaustive routing decisions use a defined media scope. |
| Coding classification | 10 | 4 | 4 | All coding values and reserved classifications tested. |
| Predictor/residual math | 25 | 3 | 3 | Broad helper oracle; retained exact FFmpeg fixture covers 4-bit stereo only. |
| Group/layout/parameters | 20 | 3 | 3 | Both widths/channels and measured copy selection; selected-invalid signaling unknown. |
| History/cadence | 15 | 3 | 3 | Groups/refill/30-minute arithmetic; live DAC/predictor reset not established. |
| Emphasis | 5 | 3 | 3 | Audio has response tests; switch waveform unknown. |
| Silicon and retail path | 10 | 0 | 0 | No one-LSB silicon or media-audited title pass. |

Implementation: [src/mame/philips/cdiaudio.h](../src/mame/philips/cdiaudio.h), [src/mame/philips/cdiaudio_dsp56001.h](../src/mame/philips/cdiaudio_dsp56001.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp).

Tests: [tests/emu/philips/cdi_audio_arithmetic.cpp](../tests/emu/philips/cdi_audio_arithmetic.cpp), [tests/emu/philips/cdi_fma_attenuation.cpp](../tests/emu/philips/cdi_fma_attenuation.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp).

Documentation: [docs/cdi_audio_arithmetic_checkpoint.md](../docs/cdi_audio_arithmetic_checkpoint.md), [docs/cdi_audio_fidelity.md](../docs/cdi_audio_fidelity.md).

Next action: Keep core reference gates; acquire silicon arithmetic and retail-scene evidence.

<a id="cdda"></a>

### CD-DA playback and transport

**65% — Low confidence; raw weighted score 65.**

Implementation: PCM sector handoff, gating, cadence and de-emphasis exist.

Verification: Synthetic audio/data Q, raw-subcode fallback, complete TOC and repositioning pass; audible output and broader transport semantics remain unverified.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Transport command state | 15 | 2 | 2 | Start/stop/read model; no complete pause/seek reference campaign. |
| PCM sector handoff | 20 | 3 | 3 | Audio 588-frame sector model and pre-start buffer; no live output sample capture. |
| Playback gating/cadence | 15 | 3 | 3 | Audio fixes AUDCTL and 75 Hz events. |
| Track/index transitions | 15 | 1 | 3 | Live shared/separate-file stored/virtual pregaps and CUE indexes through 12 pass; wider formats and physical transition timing remain open. |
| Position/subcode | 15 | 1 | 3 | Track-relative and absolute Q, backward command repositioning and lead-out no-fabrication checks pass; seek-only completion and hardware alignment remain open. |
| Pre-emphasis | 10 | 3 | 3 | Image flags and response tested. |
| Mixed-mode runtime | 5 | 0 | 1 | Synthetic audio/data Q delivery passes; audible output, mixed-mode DAC gating and retail compatibility remain unverified. |
| Physical alignment/servo | 5 | 0 | 0 | Unmeasured. |

Implementation: [src/mame/philips/cdiaudio.h](../src/mame/philips/cdiaudio.h), [src/mame/philips/cdiaudio_dsp56001.h](../src/mame/philips/cdiaudio_dsp56001.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp).

Tests: [tests/emu/philips/cdi_audio_arithmetic.cpp](../tests/emu/philips/cdi_audio_arithmetic.cpp), [tests/emu/philips/cdi_fma_attenuation.cpp](../tests/emu/philips/cdi_fma_attenuation.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp).

Documentation: [docs/cdi_audio_arithmetic_checkpoint.md](../docs/cdi_audio_arithmetic_checkpoint.md), [docs/cdi_audio_compatibility_matrix_20260906.md](../docs/cdi_audio_compatibility_matrix_20260906.md), [docs/cdi_audio_fidelity.md](../docs/cdi_audio_fidelity.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md).

Next action: Validate data-track DAC gating and seek-only completion; extend image layouts.

Current live disc evidence: [Q checkpoint](cdi_q_checkpoint_20260907.md).

<a id="q"></a>

### CD-DA Q and other subcode

**70% — Low confidence; raw weighted score 71.25.**

Implementation: ADR/control, buffer placement, cadence and partial Q/TOC synthesis exist.

Verification: Live 45-packet TOC verifies all twelve tracks, triplicate points, absolute starts, first/last track and complete A2 lead-out. Physical lead-in and multisession remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Control/ADR | 10 | 4 | 4 | Audio preserves all encoded control/ADR combinations. |
| Location/cadence | 15 | 4 | 4 | Measured trailer offset and 75 Hz model reflected in production. |
| Track/index/relative position | 25 | 1 | 3 | Four CUE layouts verify INDEX 00/01 and higher indexes through 12, countdown, relative reset and absolute MSF; valid raw Q overrides metadata. Wider formats and hardware remain open. |
| TOC/lead packets | 20 | 1 | 3 | Live 45-packet TOC verifies all twelve tracks, triplicate points, absolute starts, first/last track and complete A2 lead-out. Physical lead-in and multisession remain open. |
| CRC oracle | 10 | 1 | 3 | Independent bitwise ten-byte/inverted CRC oracle passes all 33 delivered packets; existing production CRC needs no change. |
| P/R-W/multisession | 10 | 0 | 0 | Not implemented as faithful delivery. |
| Disc reference campaign | 10 | 0 | 2 | Four synthetic mixed-mode CUE layouts run through real CDIC commands, timer and SRAM. Generic CHD stored/virtual gaps and padding pass; hardware, multisession and retail remain open. |

Implementation: [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h).

Tests: [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp).

Documentation: [docs/cdi_audio_compatibility_matrix_20260906.md](../docs/cdi_audio_compatibility_matrix_20260906.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md).

Next action: Validate stored Q modes 2/3 and multisession inputs; retain physical lead-in/status uncertainty.

Current live disc evidence: [Q checkpoint](cdi_q_checkpoint_20260907.md).

<a id="dma"></a>

### DMA integration

**70% — Medium confidence; raw weighted score 71.25.**

Implementation: SCC-owned transfers, START, full counts, held-request re-arm and error APIs exist.

Verification: Live DVC transfers and both CDIC SRAM boundary/error directions pass; advanced modes and physical arbitration remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Controller registers | 20 | 3 | 3 | Masks/start/abort/COC implemented; remaining register ambiguity. |
| Address/count sequencing | 15 | 3 | 3 | Pure modes/wrap tested; device-specific coverage incomplete. |
| DVC channel 2 | 25 | 4 | 4 | Existing normal/audio ingress fixtures plus merged explicit START, held-request re-arm, immediate abort and 65536-word live transfer pass. |
| CDIC channel 1 | 20 | 1 | 3 | Real DMACTL transfers exercise both directions, final legal word, oversized count, preserved remaining count and device bus error. Physical wrap/arbitration remain open. |
| Advanced request/error modes | 10 | 1 | 2 | Device-termination and memory/device-error APIs now set status and update IPL. Their helper status rules pass; actual bus-error wiring, chaining/burst and error-injection fixtures remain open. |
| Bus timing | 10 | 0 | 0 | No cycle-exact arbitration/IACK/DREQ proof. |

Implementation: [src/devices/machine/scc68070.cpp](../src/devices/machine/scc68070.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdi_dvc_dma_service.h](../src/mame/philips/cdi_dvc_dma_service.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_memory.h](../src/mame/philips/cdicdic_memory.h), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h).

Tests: [tests/emu/machine/scc68070_peripherals.cpp](../tests/emu/machine/scc68070_peripherals.cpp), [tests/emu/philips/cdi_dvc_audio_dma_integration.cpp](../tests/emu/philips/cdi_dvc_audio_dma_integration.cpp), [tests/emu/philips/cdi_dvc_dma_integration.cpp](../tests/emu/philips/cdi_dvc_dma_integration.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdicdic_memory.cpp](../tests/emu/philips/cdicdic_memory.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

Next action: Extend controller error/request-mode and bus-arbitration coverage.

<a id="irq"></a>

### Interrupts

**70% — Medium confidence; raw weighted score 68.75.**

Implementation: Priority arbitration, acknowledgements and device IRQ sources exist.

Verification: Live DVC events, executed MMU fault/recovery and CDIC error status pass; expanded peripheral IRQ sequences remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| SCC arbitration/ack | 25 | 3 | 3 | Priority/source helpers and register implementation. |
| CDIC source control | 20 | 3 | 3 | Audio measured AUDCTL/XBUF/ABUF gating. |
| SLAVE response IRQ | 20 | 3 | 3 | Readiness/replacement helpers; mailbox timing still a model. |
| DVC status/IRQ | 20 | 3 | 3 | Audio integration sees synchronized completion/CSU. |
| All fault/error sources | 10 | 1 | 2 | Executed MMU exceptions and CDIC device-error status pass; expanded UART/timer/error-source and physical IACK sequences remain open. |
| Physical IACK edge timing | 5 | 0 | 0 | No trace-derived closure. |

Implementation: [src/devices/machine/scc68070.cpp](../src/devices/machine/scc68070.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdi_dvc_dma_service.h](../src/mame/philips/cdi_dvc_dma_service.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/machine/scc68070.cpp](../tests/emu/machine/scc68070.cpp), [tests/emu/machine/scc68070_peripherals.cpp](../tests/emu/machine/scc68070_peripherals.cpp), [tests/emu/philips/cdi_dvc_audio_dma_integration.cpp](../tests/emu/philips/cdi_dvc_audio_dma_integration.cpp), [tests/emu/philips/cdi_dvc_dma_integration.cpp](../tests/emu/philips/cdi_dvc_dma_integration.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](../tests/emu/philips/cdislavehle_response_ready.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Add live timer/UART/error-source and acknowledgement sequences.

<a id="timing"></a>

### Device timing

**60% — Medium confidence; raw weighted score 60.**

Implementation: Raster, sector/sample, DCLK, SCC timer and DMA service models exist.

Verification: Arithmetic and DMA cadence tests pass; cycle-exact CPU/bus and physical cross-device calibration remain unverified.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Video raster model | 25 | 3 | 3 | Table-derived profiles; pin waveforms unavailable. |
| CDIC sector/sample cadence | 25 | 3 | 3 | Audio long-run helper identity and captured buffer cadence. |
| DVC clocks/scheduling | 25 | 3 | 3 | Advanced DCLK and rational helper tests; actual sequence output absent. |
| CPU/bus service timing | 15 | 1 | 1 | Timer 1/2 now have event scheduling, and DVC service/re-arm passes. This package grades CPU/bus cycle accuracy, arbitration and DTACK; those remain incomplete, so its grade does not increase. |
| Physical cross-device calibration | 10 | 0 | 0 | No full calibration corpus. |

Implementation: [src/devices/cpu/m68000/scc68070.cpp](../src/devices/cpu/m68000/scc68070.cpp), [src/devices/machine/scc68070.cpp](../src/devices/machine/scc68070.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_memory.h](../src/mame/philips/cdicdic_memory.h), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/mcd212.cpp](../src/mame/philips/mcd212.cpp), [src/mame/philips/mcd212_control_stream.h](../src/mame/philips/mcd212_control_stream.h), [src/mame/philips/mcd212_video.h](../src/mame/philips/mcd212_video.h).

Tests: [tests/emu/machine/scc68070.cpp](../tests/emu/machine/scc68070.cpp), [tests/emu/machine/scc68070_peripherals.cpp](../tests/emu/machine/scc68070_peripherals.cpp), [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdicdic_memory.cpp](../tests/emu/philips/cdicdic_memory.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/mcd212_control_stream.cpp](../tests/emu/philips/mcd212_control_stream.cpp), [tests/emu/philips/mcd212_video.cpp](../tests/emu/philips/mcd212_video.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_branch_consolidation_20260906.md](../docs/cdi_branch_consolidation_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Exercise Timer 1/2 and UART event timing in live fixtures; measure CPU/bus and cross-device timing separately.

<a id="av"></a>

### A/V synchronization

**45% — Medium confidence; raw weighted score 46.25.**

Implementation: Clock arithmetic, packet scheduling and discontinuity controls exist.

Verification: Long-run arithmetic passes; it is not a 30-minute decoded/presented movie or host-output drift measurement.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Clock-domain arithmetic | 20 | 4 | 4 | Audio long-run rational helper tests close arithmetic accumulation scope. |
| Packet scheduling | 20 | 3 | 3 | DCLK-advanced scheduling exists; device presentation not fully validated. |
| Discontinuities | 15 | 3 | 3 | Re-anchoring and command cycles covered; branching MPEG absent. |
| Decoded/presented continuous A/V | 20 | 0 | 0 | 30-minute tests calculate timestamps; they do not decode/render a movie. |
| Host output drift | 15 | 0 | 0 | Not measured. |
| Physical clock/branch latency | 10 | 0 | 0 | Not measured. |

Implementation: [3rdparty/pl_mpeg/pl_mpeg.h](../3rdparty/pl_mpeg/pl_mpeg.h), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/cdidvc_mpeg_format.h](../src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_utils.h](../src/mame/philips/cdidvc_utils.h).

Tests: [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdidvc.cpp](../tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](../tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](../tests/emu/philips/cdidvc_video_conversion.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

Next action: Run a meaningful decoded/presented 30-minute A/V fixture and repeated branches.

<a id="save"></a>

### Save states

**60% — Medium confidence; raw weighted score 57.5.**

Implementation: State registration and decoder replay cover multiple devices.

Verification: Live audio/control and MMU query snapshots pass; decoded pictures and active peripheral continuation lack complete fixtures.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| DVC audio replay/device state | 25 | 3 | 3 | Audio adds end/stream/live ram_state paths. |
| Decoded video continuation | 20 | 2 | 2 | Queues mirrored; live fixture holds sequence header without decoded pictures. |
| MMU state | 15 | 3 | 3 | Audio query/descriptor ram_state round trip; no faulted CPU continuation. |
| CDIC active transport | 15 | 2 | 2 | Registered fields but no full device transport snapshot fixture. |
| SLAVE partial commands | 10 | 2 | 2 | Registered parser/response state; partial-command round trip missing. |
| Active UART/I2C/DMA | 10 | 1 | 1 | More UART/timer fields and held DREQ are saved, but no new active UART/I2C or held-request DMA round-trip fixture was added. Registration alone does not close continuation behavior. |
| Capacity/error policy | 5 | 2 | 2 | 8 MiB/32 MiB replay caps can invalidate snapshots; recovery modeled. |

Implementation: [src/devices/cpu/m68000/m68kcpu.cpp](../src/devices/cpu/m68000/m68kcpu.cpp), [src/devices/cpu/m68000/scc68070.cpp](../src/devices/cpu/m68000/scc68070.cpp), [src/devices/machine/scc68070.h](../src/devices/machine/scc68070.h), [src/devices/machine/scc68070_helpers.h](../src/devices/machine/scc68070_helpers.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_memory.h](../src/mame/philips/cdicdic_memory.h), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_save_state.h](../src/mame/philips/cdidvc_save_state.h), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/machine/scc68070.cpp](../tests/emu/machine/scc68070.cpp), [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](../tests/emu/philips/cdi_mmu_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdicdic_memory.cpp](../tests/emu/philips/cdicdic_memory.cpp), [tests/emu/philips/cdidvc_audio_replay.cpp](../tests/emu/philips/cdidvc_audio_replay.cpp), [tests/emu/philips/cdidvc_state_transitions.cpp](../tests/emu/philips/cdidvc_state_transitions.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_scc68070_mmu_checkpoint_20260906.md](../docs/cdi_scc68070_mmu_checkpoint_20260906.md).

Next action: Test real decoded A/V plus active CDIC/UART/I2C/SLAVE continuations.

<a id="slave"></a>

### SLAVE HLE

**55% — Medium confidence; raw weighted score 56.25.**

Implementation: Bounded HLE parser, responses, pointer and control paths exist.

Verification: Command/pointer/readiness helpers pass; several protocols remain stubs and physical mailbox timing is modeled.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Parser/command assembly | 20 | 3 | 3 | Bounded exhaustive command descriptors; actual mailbox wiring uncertain. |
| Response/readiness | 20 | 3 | 3 | Helper timing/replacement tests; fixed HLE delays remain. |
| Pointer path | 15 | 3 | 3 | Wrapping/clamping and packets covered. |
| Audio/reset/LCD control | 10 | 2 | 2 | Production side effects; physical transition/reset behavior not verified. |
| Remaining protocols | 20 | 1 | 1 | Keyboard flag, memory set/clear, disc base, developer/X-bus are stubs. |
| Save/reset | 10 | 2 | 2 | Registered state without active partial-command proof. |
| Physical mailbox | 5 | 0 | 0 | Async DTACK/depth/timing not verified. |

Implementation: [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp), [src/mame/philips/cdislavehle_state.h](../src/mame/philips/cdislavehle_state.h).

Tests: [tests/emu/philips/cdislavehle_commands.cpp](../tests/emu/philips/cdislavehle_commands.cpp), [tests/emu/philips/cdislavehle_pointer.cpp](../tests/emu/philips/cdislavehle_pointer.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](../tests/emu/philips/cdislavehle_response_ready.cpp), [tests/emu/philips/cdislavehle_transport.cpp](../tests/emu/philips/cdislavehle_transport.cpp).

Documentation: [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Add partial-command snapshots and evidence-backed missing commands.

<a id="input"></a>

### Input and peripherals

**45% — Medium confidence; raw weighted score 43.75.**

Implementation: Pointer coordinates, buttons, packets and host input mapping exist.

Verification: Pointer helpers pass; keyboard event delivery, controller breadth and serial waveforms are incomplete.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Pointer coordinates/packets | 35 | 3 | 3 | Production callbacks and helper vectors. |
| Buttons/host input | 20 | 2 | 2 | Machine ports exist; controller hardware/runtime breadth limited. |
| Keyboard events | 20 | 0 | 0 | Enable command stores flag; no event source. |
| Discovery/type | 10 | 1 | 1 | Fixed pointer type reply. |
| LCD/test plug | 10 | 2 | 2 | Mono-I paths exist; MCU-based panels blank. |
| Serial timing | 5 | 0 | 0 | Host polling is not serial waveform emulation. |

Implementation: [src/devices/cpu/dsp56000/dsp56000.cpp](../src/devices/cpu/dsp56000/dsp56000.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdimono2.h](../src/mame/philips/cdimono2.h), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp), [src/mame/philips/cdislavehle_state.h](../src/mame/philips/cdislavehle_state.h).

Tests: [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdimono2.cpp](../tests/emu/philips/cdimono2.cpp), [tests/emu/philips/cdislavehle_commands.cpp](../tests/emu/philips/cdislavehle_commands.cpp), [tests/emu/philips/cdislavehle_pointer.cpp](../tests/emu/philips/cdislavehle_pointer.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](../tests/emu/philips/cdislavehle_response_ready.cpp), [tests/emu/philips/cdislavehle_transport.cpp](../tests/emu/philips/cdislavehle_transport.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Validate controller variants; implement keyboard only from protocol evidence.

<a id="servo"></a>

### SERVO and MCU integration

**15% — Low confidence; raw weighted score 12.5.**

Implementation: MCU devices and board topology exist.

Verification: Structural evidence only for much of the scope; live protocol, feedback and complete firmware runtime remain absent.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Board topology | 15 | 2 | 2 | Documented MCU devices/maps. |
| MCU integration | 20 | 1 | 1 | CPU devices exist; system links incomplete. |
| Command protocol | 25 | 0 | 0 | No complete live SERVO/SLAVE command exchange. |
| Transport feedback | 20 | 0 | 0 | HLE fixed seek has no physical feedback model. |
| Timing | 10 | 0 | 0 | No calibrated MCU/drive communication. |
| Runtime proof | 10 | 0 | 0 | No retained integrated servo firmware pass. |

Implementation: [src/devices/cpu/dsp56000/dsp56000.cpp](../src/devices/cpu/dsp56000/dsp56000.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdimono2.h](../src/mame/philips/cdimono2.h), [src/mame/philips/cdislavehle.cpp](../src/mame/philips/cdislavehle.cpp), [src/mame/philips/cdislavehle_state.h](../src/mame/philips/cdislavehle_state.h).

Tests: [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdimono2.cpp](../tests/emu/philips/cdimono2.cpp), [tests/emu/philips/cdislavehle_commands.cpp](../tests/emu/philips/cdislavehle_commands.cpp), [tests/emu/philips/cdislavehle_pointer.cpp](../tests/emu/philips/cdislavehle_pointer.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](../tests/emu/philips/cdislavehle_response_ready.cpp), [tests/emu/philips/cdislavehle_transport.cpp](../tests/emu/philips/cdislavehle_transport.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Establish SPI/DTACK/host interfaces before claiming servo runtime.

<a id="disc"></a>

### Disc handling

**60% — Low confidence; raw weighted score 60.**

Implementation: Sector ingress, data/audio filters and command transport model exist.

Verification: Synthetic Q/TOC, four CUE layouts, normalized higher indexes, payload/subcode offsets and truncated-read errors pass. Generic CHD gaps/padding pass; multisession and physical status remain open.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Sector ingress | 25 | 3 | 3 | Audio distinguishes CD-DA/header validation and endian word paths. |
| Data/audio filtering | 20 | 3 | 3 | Audio has exhaustive routing helper; complete device/media proof pending. |
| Command transport | 20 | 2 | 2 | Read/stop model; seek-only behavior unresolved. |
| TOC/subcode | 15 | 1 | 3 | Synthetic Q/TOC, four CUE layouts, normalized higher indexes, payload/subcode offsets and truncated-read errors pass. Generic CHD gaps/padding pass; multisession and physical status remain open. |
| Error recovery | 10 | 1 | 1 | Truncated generic payload/subcode reads fail explicitly; physical CDIC error/status recovery remains unmodeled. |
| Mixed-mode/multisession | 10 | 0 | 1 | Four shared/separate stored/virtual-gap CUE layouts pass Q delivery; multisession, other containers and mixed-mode PCM output remain open. |

Implementation: [src/devices/cpu/dsp56000/dsp56000.cpp](../src/devices/cpu/dsp56000/dsp56000.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_memory.h](../src/mame/philips/cdicdic_memory.h), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h), [src/mame/philips/cdimono2.h](../src/mame/philips/cdimono2.h).

Tests: [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdicdic_memory.cpp](../tests/emu/philips/cdicdic_memory.cpp), [tests/emu/philips/cdimono2.cpp](../tests/emu/philips/cdimono2.cpp).

Documentation: [docs/cdi_audio_compatibility_matrix_20260906.md](../docs/cdi_audio_compatibility_matrix_20260906.md), [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Establish seek-only completion and data-track PCM handoff with controller/output evidence; expand multisession and malformed-image fixtures.

Current live disc evidence: [Q checkpoint](cdi_q_checkpoint_20260907.md).

<a id="glue"></a>

### Mono-I/II board glue

**50% — Low confidence; raw weighted score 50.**

Implementation: Mono-I maps, optional DVC and partial Mono-II wiring exist.

Verification: Presence/IRQ helpers and live optional-DVC fixture pass; disabled DSP and unmapped MCU interfaces still block Mono-II.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Mono-I maps/callbacks | 25 | 3 | 3 | Configured CPU/display/CDIC/SLAVE with limited runtime evidence. |
| Optional DVC | 15 | 4 | 4 | Audio live fixture verifies actual presence/absence. |
| Mono-II map | 20 | 2 | 2 | Structural map has deliberately unpopulated host ranges. |
| MCU wiring | 15 | 1 | 1 | IRQ2 connected; SPI/DTACK absent. |
| DSP/LEMM | 15 | 0 | 0 | Standalone DSP host/bootstrap and partial execution now exist, but the Mono-II DSP remains disabled and its host range unmapped. No credit for operational board integration; see the separate DSP-core row. |
| Firmware/runtime breadth | 10 | 1 | 1 | Historical Mono-I smoke only; no Mono-II/CD playback pass. |

Implementation: [src/devices/cpu/dsp56000/dsp56000.cpp](../src/devices/cpu/dsp56000/dsp56000.cpp), [src/devices/machine/scc68070.cpp](../src/devices/machine/scc68070.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdi_dvc_dma_service.h](../src/mame/philips/cdi_dvc_dma_service.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdimono2.h](../src/mame/philips/cdimono2.h).

Tests: [tests/emu/machine/scc68070_peripherals.cpp](../tests/emu/machine/scc68070_peripherals.cpp), [tests/emu/philips/cdi_dvc_audio_dma_integration.cpp](../tests/emu/philips/cdi_dvc_audio_dma_integration.cpp), [tests/emu/philips/cdi_dvc_dma_integration.cpp](../tests/emu/philips/cdi_dvc_dma_integration.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdimono2.cpp](../tests/emu/philips/cdimono2.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Validate complete standalone DRVDSP firmware before mapping/enabling it; implement required DTACK/SPI interfaces.

<a id="mono2"></a>

### Mono-II functional system

**20% — Low confidence; raw weighted score 20.**

Implementation: Board map, reset and IRQ structure exist.

Verification: Structural tests pass; host DTACK, SPI, enabled DSP and matching-ROM runtime remain absent.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Board map/configuration | 25 | 2 | 2 | Clocks/RAM/ROM/NVRAM/display structure present. |
| IRQ/reset | 10 | 3 | 3 | Port-B IRQ2 translation tested. |
| Host mailbox/DTACK | 20 | 0 | 0 | Blocked. |
| SERVO-SLAVE SPI | 15 | 0 | 0 | Blocked. |
| DSP/LEMM/audio | 20 | 0 | 0 | The partial standalone core is retained, but DRVDSP is disabled and board host mapping is absent. This functional-system obligation remains unimplemented. |
| Firmware/CD runtime | 10 | 0 | 0 | No retained matching-ROM boot. |

Implementation: [src/devices/cpu/dsp56000/dsp56000.cpp](../src/devices/cpu/dsp56000/dsp56000.cpp), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdimono2.h](../src/mame/philips/cdimono2.h).

Tests: [tests/emu/philips/cdi_dvc_edge_integration.cpp](../tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdimono2.cpp](../tests/emu/philips/cdimono2.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Complete and validate the DSP firmware path plus host DTACK and MCU SPI before claiming Mono-II operation.

<a id="all_audio"></a>

### Cross-system audio

**70% — Medium confidence; raw weighted score 68.75.**

Implementation: XA, MPEG audio, queues, control, gain and emphasis are implemented.

Verification: Strong component tests and synthetic CD-DA Q/TOC coexist with missing audible transport and cross-stream continuity evidence.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| XA compressed decode | 20 | 3 | 3 | Native decode broad; silicon arithmetic unproven. |
| Layer II compressed decode | 20 | 3 | 3 | Software reference is tolerant, not bit-exact. |
| Buffer/output delivery | 20 | 3 | 3 | Software queues/cadence tested; physical edges missing. |
| CD-DA transport | 15 | 2 | 2 | Synthetic Q/TOC delivery now passes; audible mixed-mode transport and complete seek/pause behavior remain open. |
| Gain/emphasis/control | 15 | 3 | 3 | Q22 and filter tests; CDIC quantizer and switch unknown. |
| Cross-stream/output continuity | 10 | 2 | 2 | Helpers/live commands; no true branching/movie/output capture. |

Implementation: [3rdparty/pl_mpeg/pl_mpeg.h](../3rdparty/pl_mpeg/pl_mpeg.h), [src/mame/philips/cdiaudio.h](../src/mame/philips/cdiaudio.h), [src/mame/philips/cdiaudio_dsp56001.h](../src/mame/philips/cdiaudio_dsp56001.h), [src/mame/philips/cdicdic.cpp](../src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_state.h](../src/mame/philips/cdicdic_state.h), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/cdidvc_mpeg_format.h](../src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_utils.h](../src/mame/philips/cdidvc_utils.h).

Tests: [tests/emu/philips/cdi_audio_arithmetic.cpp](../tests/emu/philips/cdi_audio_arithmetic.cpp), [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdi_fma_attenuation.cpp](../tests/emu/philips/cdi_fma_attenuation.cpp), [tests/emu/philips/cdicdic.cpp](../tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdidvc.cpp](../tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](../tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](../tests/emu/philips/cdidvc_video_conversion.cpp).

Documentation: [docs/cdi_audio_arithmetic_checkpoint.md](../docs/cdi_audio_arithmetic_checkpoint.md), [docs/cdi_audio_compatibility_matrix_20260906.md](../docs/cdi_audio_compatibility_matrix_20260906.md), [docs/cdi_audio_fidelity.md](../docs/cdi_audio_fidelity.md), [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

Next action: Fix source defects first, then reference-disc and real A/V output gaps.

<a id="all_video"></a>

### Cross-system video

**55% — Low confidence; raw weighted score 53.75.**

Implementation: Native display, MPEG backend, composition and presentation model exist.

Verification: Component helpers pass; independent decoded/composed frames and hardware/title captures remain incomplete.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Native display | 35 | 3 | 3 | Documented MCD212 model; image corpus incomplete. |
| MPEG decoded pictures | 25 | 2 | 2 | Backend implementation, no independent full decode corpus. |
| Composition | 20 | 2 | 2 | Eligibility tests do not prove combined rendered pixels. |
| Presentation | 10 | 2 | 2 | Scheduling model still requires runtime validation. |
| Hardware/title captures | 10 | 0 | 0 | No retained current complete-frame certification. |

Implementation: [3rdparty/pl_mpeg/pl_mpeg.h](../3rdparty/pl_mpeg/pl_mpeg.h), [src/mame/philips/cdi.cpp](../src/mame/philips/cdi.cpp), [src/mame/philips/cdidvc.cpp](../src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](../src/mame/philips/cdidvc_fidelity.h), [src/mame/philips/cdidvc_mpeg_format.h](../src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_utils.h](../src/mame/philips/cdidvc_utils.h), [src/mame/philips/mcd212.cpp](../src/mame/philips/mcd212.cpp), [src/mame/philips/mcd212_control_stream.h](../src/mame/philips/mcd212_control_stream.h), [src/mame/philips/mcd212_video.h](../src/mame/philips/mcd212_video.h).

Tests: [tests/emu/philips/cdi_dvc_state_integration.cpp](../tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdidvc.cpp](../tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](../tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](../tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](../tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdidvc_timing.cpp](../tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](../tests/emu/philips/cdidvc_video_conversion.cpp), [tests/emu/philips/mcd212_control_stream.cpp](../tests/emu/philips/mcd212_control_stream.cpp), [tests/emu/philips/mcd212_video.cpp](../tests/emu/philips/mcd212_video.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](../docs/cdi_audio_fidelity_campaign.md), [docs/cdi_audio_final_certification_20260906.md](../docs/cdi_audio_final_certification_20260906.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](../docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_modernization_status.md](../docs/cdi_modernization_status.md).

Next action: Add independent full-frame decode/composition/presentation evidence.

<a id="dsp"></a>

### DSP56000/56001 standalone core

**40% — Low confidence; raw weighted score 40.**

Implementation: Standard host/bootstrap transport and a partial JMP/MOVE/MOVEP/MOVEM/DO/JCLR interpreter exist; this is not a complete DSP ISA or enabled Mono-II device.

Verification: Three helper test files cover host words, bootstrap relocation, masks, loops and wrapping. No emulator-linked complete firmware, interrupt, ALU or cycle-accuracy campaign.

| Obligation | Weight | Previous grade | Current grade | Rationale and remaining gaps |
| --- | ---: | ---: | ---: | --- |
| Host interface | 20 | — | 3 | Host byte/24-bit transport and status helpers are tested; complete interrupt/peripheral integration is missing. |
| Bootstrap transport and relocation | 15 | — | 3 | Bootstrap loading, register side effects and relocation subset have deterministic vectors; no full live firmware gate. |
| Instruction set, register file, ALU and AGU | 30 | — | 1 | Only a small relocation subset and partial registers are implemented; unsupported instructions stop execution. |
| P/X/Y memory integration | 15 | — | 1 | Temporary arrays back execution rather than the declared device address spaces. |
| Interrupts and DSP peripherals | 10 | — | 0 | execute_set_input is empty; complete interrupt/peripheral behavior is absent. |
| Instruction cycle accuracy | 5 | — | 0 | One provisional count per instruction is not an instruction-accurate timing implementation. |
| Save/reset continuation | 5 | — | 2 | State is registered and host state is tested; full active CPU/firmware round-trip coverage is absent. |

Implementation: [src/devices/cpu/dsp56000/dsp56000.cpp](../src/devices/cpu/dsp56000/dsp56000.cpp), [src/devices/cpu/dsp56000/dsp56000.h](../src/devices/cpu/dsp56000/dsp56000.h), [src/devices/cpu/dsp56000/dsp56000execute.h](../src/devices/cpu/dsp56000/dsp56000execute.h), [src/devices/cpu/dsp56000/dsp56000host.h](../src/devices/cpu/dsp56000/dsp56000host.h).

Tests: [tests/emu/cpu/dsp56000.cpp](../tests/emu/cpu/dsp56000.cpp), [tests/emu/cpu/dsp56000_decode.cpp](../tests/emu/cpu/dsp56000_decode.cpp), [tests/emu/cpu/dsp56000_wrap.cpp](../tests/emu/cpu/dsp56000_wrap.cpp).

Documentation: [docs/cdi_branch_consolidation_20260906.md](../docs/cdi_branch_consolidation_20260906.md).

Next action: Complete architecture/address-space integration and execute the full DRVDSP firmware path before enabling the board device.

## Recalculate and validate

```python
import json
from pathlib import Path
report = json.loads(Path('docs/cdi_unified_verified_status_20260907.json').read_text())
for row in report['subsystems']:
    assert sum(p['weight'] for p in row['packages']) == 100
    assert all(p['grade'] in range(5) for p in row['packages'])
    raw = sum(p['weight'] * p['grade'] / 4 for p in row['packages'])
    assert raw == row['raw_percent']
    assert int((raw + 2.5) // 5) * 5 == row['percent']
    print(row['subsystem'], row['percent'], row['confidence'])
```
