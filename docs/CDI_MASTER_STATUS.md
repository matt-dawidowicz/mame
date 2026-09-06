# Philips CD-i / DVC master status

Status reconciliation date: **2026-09-06**. This is the canonical high-level index
for the modernization project. For routine status, read the branch index, master
matrix and next-batch section; open individual subsystem records only as needed.
The [repository workflow](../AGENTS.md) defines status, verified-status and
development modes. The detailed audio campaign remains authoritative on its branch.

This initialization reconciles existing documentation, branch/commit metadata,
test-file presence and retained CI results. It is **not a fresh whole-emulator
audit**, hardware certification or title playthrough. No emulator code changed.

## Branch and certification index

| ID | Branch / evidence | Exact code or source baseline | Verification date and scope |
| --- | --- | --- | --- |
| C0 | `cdi-dvc-modernization` | `f0d78dfbda5c7fbbdb11c3fc1ba25a58e731146f` | 2026-09-06: branch, documentation, history and live DMA fixture presence checked. No fresh runtime tests. Historical Phase A-E results remain dated 2026-08-23/24. |
| A0 | `audio/cdi-fidelity-100-campaign-20260905` | `442e5050846e680ed653fc1297b66df588b314e2` | 2026-09-06: latest branch code baseline and successful fast-CI job/log verified; 17,061,392 assertions / 191 helper cases and 21 assertions / 6 integration cases. |
| A1 | Audio final-report certification | `0416e3067975236d3ca4a90c0cb461d3a153398d` | 2026-09-06: successful CI metadata independently checked; report records 16,862,832 assertions / 187 helper cases and 21 / 6 integration cases. |
| M1 | MMU architectural milestone, audio branch | `0c433789c840c116aa4ad31ccbefa9daa1f8fd2d` | 2026-09-06: recorded certification in the MMU checkpoint, run 34051815017; MMU integration remains in the successful A0 gate. |

Canonical and audio baselines above precede the status-infrastructure documentation
commits. Obtain the current branch and HEAD with `git branch --show-current` and
`git rev-parse HEAD`; do not confuse a documentation commit with a new code milestone.
The audio branch also contains SCC68070 MMU and MCD212 work. Nothing in this index
merges that implementation into canonical.

- [A0 CI run](https://github.com/matt-dawidowicz/mame/actions/runs/34057720217)
  and [job](https://github.com/matt-dawidowicz/mame/actions/runs/34057720217/job/101552555286).
- [A1 CI run](https://github.com/matt-dawidowicz/mame/actions/runs/34055417260).
- The named `dvc/fmv-clock-underflow-trace-20260905` branch was not returned by
  the remote branch enumeration on 2026-09-06. It may be local or renamed; do not
  recreate, merge or substitute another experimental branch by assumption.
- This work used a fresh documentation-focused checkout. It cannot inspect or
  certify uncommitted files in `C:\Users\thema\mame-cdi-modernization`.

## How to interpret completion

**Completion** is a scoped engineering estimate. **Confidence** rates the strength
of evidence behind the recorded assessment, not the fraction of working games.
High means a narrow, reproducible standard/reference or measured gate; Medium means
substantial tests/docs with unresolved integration/hardware breadth; Low means sparse
runtime evidence, historical provenance or insufficient scope for a current estimate.

**Recorded** percentages below are preserved from the existing subsystem table;
they were not recalculated from code in this documentation task. **Stale** means
later substantive work invalidates using that number as a current estimate. A dash
means **Not estimated**, not zero. No overall project percentage is defensible from
these overlapping rows. Historical functional and fidelity percentages are kept
separate because they measure different scopes.

100% applies only to its explicitly named narrow gate. A tested implementation,
specification-derived behavior, real-hardware verification and demonstrated retail
compatibility are different evidence levels. Unmeasured silicon behavior is still
remaining work in any scope that includes it.

## Master completion matrix

Each subsystem's implementation, verification, inaccuracies, remaining work, tests,
documents and next action are expanded in the numbered records below. The evidence
ID resolves the exact last checked commit/date above; it does not imply that every
behavior in that row was tested by that gate.

| Record / subsystem | Canonical C0 recorded completion | Latest audio-branch record / current assessment | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| 1. SCC68070 internal peripherals | 60% | 60% historical; stale after MMU work, current total not estimated | Low | C0, A0, M1 |
| 2. SCC68070 MMU | Not estimated; storage only | Architectural gates closed; physical fidelity not estimated | Medium | M1, A0 |
| 3. CDIC | 70% functional / 50% fidelity | 70% functional / 60% fidelity recorded | Medium | C0, A0, A1 |
| 4. MCD212 display | 70% | 70% recorded; A0 adds exhaustive timing/QHY control coverage | Medium | C0, A0 |
| 5. DVC overall | 50% | 58% historical; stale after DMA/state/clock work, current total not estimated | Low | C0, A0, A1 |
| 6. MPEG video decode / presentation | Not estimated | Not estimated; no independent completion denominator | Low | C0, A0 |
| 7. DVC audio overall | Not estimated separately | Software/reference milestone passed; strict overall 100% withheld; current percentage not estimated | Medium | A1, A0 |
| 8. XA routing / ADPCM | Not estimated separately | Software/reference gates covered; silicon rounding/error response open; not estimated | Medium | A1, A0 |
| 9. CD-DA / Q subcode | Not estimated separately | Sector/output model covered; transport/subcode fixtures incomplete; not estimated | Low | A1, A0 |
| 10. DMA | 60% combined IRQ/DMA estimate only | Live DVC normal/edge/audio ingress gates covered; separate total not estimated | Medium | C0, A0 |
| 11. Interrupts | 60% combined IRQ/DMA estimate only | Arbitration/acknowledgement tested; separate total not estimated | Medium | C0, A0 |
| 12. Timing / A/V synchronization | Not estimated separately | Software clock arithmetic bounded; runtime/hardware timing incomplete; not estimated | Medium | A1, A0 |
| 13. Save states | 60% | 65% historical; stale after live MMU/A-V work, current total not estimated | Medium | C0, M1, A0 |
| 14. SLAVE HLE / input / peripherals | SLAVE 65% functional / 45% fidelity; input 50% | Same recorded estimates | Medium | C0, A0 |
| 15. SERVO / disc handling | SERVO 30%; disc handling not separately estimated | Same recorded SERVO estimate; disc handling not estimated | Low | C0, A1 |
| 16. Board glue / Mono-II | Glue 60%; Mono-II 60% structural, 20% runtime, 30% fidelity | Same recorded estimates; no Mono-II boot proof | Low | C0, A0 |
| 17. Compatibility | Not estimated | No retained retail-title pass in the reviewed campaign matrix | Low | A1 |
| 18. Cross-system audio / video | Audio 50%; video 60% | Audio 60% historical and stale; video 60% recorded | Low | C0, A1, A0 |

The source of the C0 values is [the historical subsystem table](cdi_modernization_status.md#subsystem-confidence).
The audio-side recorded values come from the same file at A0, linked in the
document registry below. They are coarse implementation-confidence estimates, not
newly certified completion percentages. Narrow audio results are more informative:

| Narrow audio scope at A0 | Recorded completion / disposition | Confidence | Limit |
| --- | --- | --- | --- |
| MPEG-1 Layer II parser, campaign section 1 | 100%, explicitly recorded | High | Syntax/profile classification, 32 stream IDs and both access forms; excludes decoder CRC/error response and physical event timing. |
| AUDCTL register behavior, section 11 | 100%, explicitly recorded | High | Measured/readback software interface; excludes DAC queue/flush edges and an invented meaning for bit 12. |
| VMPEG FMA digital attenuation table, section 10 | Digital quantization gate closed; no new percentage assigned | High | Recovered 128-entry Q22 table; excludes Mono-I CDIC quantization and analogue transitions/floor. |
| Layer II decode, PCM/save/DMA, XA, clocks | Software/reference gates passed within each documented scope | Medium | Does not certify every physical edge, stream/title or silicon implementation. |
| DSP rounding/saturation and de-emphasis | Constrained arithmetic and standards response tested | Medium | Exact Philips instruction path, CDIC arithmetic and physical switching remain open. |

The campaign's “Current baseline” list is explicitly **before the campaign**.
Its 97/92/62/52/50/etc. values must not be reported as current estimates. Likewise,
checked test-deliverable boxes are not a percentage of total hardware fidelity.

## Subsystem records

### 1. SCC68070 internal peripherals

- **Implementation:** C0 has deterministic reset, DMA register sequencing, IRQ
  arbitration, I2C/UART side-effect helpers and Timer 0 reload behavior. A0 adds the
  MMU implementation in record 2. Timer 1/2, UART break/framing/queue realism and
  I2C slave/multi-master behavior remain incomplete.
- **Verification:** historical Phase A reports 796 assertions / 6 SCC cases and
  native/linked validation; these are recorded 2026-08-23 results, not fresh C0
  executions. Last current gate: A0 (2026-09-06), maintained SCC helper/integration
  selection. Exact peripheral pin/bus behavior is not verified.
- **Known inaccuracies / remaining:** forced UART status bits and oversized queues,
  timer input ownership, arbitration/slave I2C, asynchronous `/DTACK`, and bus timing.
- **Tests/docs:** `tests/emu/machine/scc68070.cpp`; historical Phase A and register
  audit [D0]; MMU checkpoint [D6].
- **Next:** a bounded timer/input-routing or asynchronous-bus task with evidence;
  do not reimplement the audio-branch MMU. Re-estimate the overall row only during
  a requested SCC-focused verified-status pass.

### 2. SCC68070 MMU

- **Implementation:** C0 stores masked registers only. A0 implements CAM selection,
  translation, access permissions, stack direction, full-operand preflight, MSR
  faults and restartable CPU bus errors; internal accesses and DMA bypass CPU MMU.
- **Verification:** M1 certification dated 2026-09-06; A0 retains green helper and
  live register/translation/save-load integration. This proves the documented
  architectural model, not exact physical exception timing.
- **Known inaccuracies / remaining:** duplicate-CAM behavior is a conservative
  model; silicon retry, boundary/wrap, contention and bus-error timing need evidence.
- **Tests/docs:** `tests/emu/machine/scc68070.cpp`,
  `tests/emu/philips/cdi_mmu_integration.cpp`; [D6].
- **Next:** retain the closed architectural milestone; change only for a concrete
  regression or new evidence. Integration into canonical is a separate future batch.

### 3. CDIC

- **Implementation:** explicit command/transport/filter/buffer/IRQ HLE. A0 adds
  measured AUDCTL 13/11/0 behavior, filter latching at read/`$2e`, XA ingress restart,
  double-buffer transport, CD-DA per-sector delivery and emphasis routing.
- **Verification:** C0 historical Phase C: 69 assertions / 8 focused cases, recorded
  2026-08-24. A0/A1 software/reference and selected retained 210/05 captures support
  the newer model. Last checked maintained gate: A0, 2026-09-06.
- **Known inaccuracies / remaining:** six-sector seek model, incomplete seek-only
  completion, DSEL/unknown register bits, error/EDC/ECC status, subcode and DAC edges.
- **Tests/docs:** `tests/emu/philips/cdicdic.cpp`, `cdicdic_memory.cpp` in the same
  test directory; [D0], [D1] sections 6-8, 11-16, [D2], [D5].
- **Next:** implement the synthetic CD-DA/Q fixture batch described below; retain
  hardware-specific command/servo timing as a separate evidence question.

### 4. MCD212 display

- **Implementation:** display modes including Extended Case QHY, interpolation,
  masks, ICA/DCA control, PAL/NTSC/interlace timing and external-video eligibility.
  Audio branch also has bounded control-stream RAM reads.
- **Verification:** documented primary-source audit and historical Phase B tests;
  A0 adds four exhaustive timing/QHY control cases to the green fast gate on
  2026-09-06. No new commercial-title or full-frame hardware capture is implied.
- **Known inaccuracies / remaining:** odd-sum/cold-field QHY behavior, chip revision
  attribution, region/cursor/mosaic pixel captures, memory contention and pin edges.
- **Tests/docs:** `tests/emu/philips/mcd212_video.cpp`, `mcd212_control_stream.cpp`
  on audio branch; [D0] display audit, [D7].
- **Next:** register-correlated output/frame evidence or a live combined overlay
  fixture; avoid repeating A0's exhaustive helper coverage.

### 5. DVC overall

- **Implementation:** functional VMPEG model with MPEG backend, stream/status
  handling, SCC-owned DMA, PCM/video queues, optional device presence and save replay.
- **Verification:** C0 contains the real cross-device DMA fixture; A0/A1 additionally
  cover DMA edges, Layer II feed, optional presence and simultaneous A/V save/load.
  Last checked gate: A0, 2026-09-06. Earlier scene success is not general compatibility.
- **Known inaccuracies / remaining:** cycle-level VMPEG behavior, malformed/CRC
  signaling, physical DAC/CSU/FIFO edges, presentation and title branch latency.
- **Tests/docs:** `tests/emu/philips/cdidvc.cpp`, `cdi_dvc_dma_integration.cpp`,
  `cdi_dvc_edge_integration.cpp`, `cdi_dvc_audio_dma_integration.cpp`,
  `cdi_dvc_state_integration.cpp`; [D1], [D3], [D7].
- **Next:** the controlled branching-FMV fixture after CD-DA/Q; inspect the first
  reproducible failing boundary instead of adding a title-specific bypass.

### 6. MPEG video decode / presentation

- **Implementation:** existing MPEG decode/presentation and MCD212 external-video
  handoff, plus audio-branch conversion/control-boundary regressions.
- **Verification:** A0 includes existing DVC helper tests and a valid 25 Hz video
  sequence header in the simultaneous-state integration. This does not prove a
  complete decoded/presented branching MPEG movie or every video profile.
- **Known inaccuracies / remaining:** no independently defined completion scope;
  frame/pixel reference corpus, presentation/overlay timing and real branch evidence
  remain incomplete. Last checked gate A0, 2026-09-06, limited to those fixtures.
- **Tests/docs:** `tests/emu/philips/cdidvc_video_conversion.cpp`,
  `cdidvc_timestamp_format.cpp`, `cdidvc_timing.cpp`, `cdi_dvc_state_integration.cpp`;
  [D0], [D3], [D7].
- **Next:** meaningful A/V packets, decode dependencies and presentation timestamps
  in a branching fixture; define a video-specific evidence ledger during that task.

### 7. DVC audio

- **Implementation:** legal Layer II parser/profile, all 32 FMA IDs, direct-frame
  access, independent decoder references, deterministic PCM/termination/switch/save
  state, live DMA ingress, Q22 attenuation and standards de-emphasis on A0.
- **Verification:** A1 software/reference milestone and A0 green gate, 2026-09-06.
  Parser 100% is narrow; no aggregate DVC-audio percentage was certified.
- **Known inaccuracies / remaining:** VMPEG CRC/profile-error and prohibited-rate
  response, exact DSP instruction/limiter selection, DAC/CSU transitions and
  meaningful timestamped interactive branches. Q22 nearest-even is architecture-
  constrained, not proof of Philips firmware's instruction sequence.
- **Tests/docs:** `tests/emu/philips/cdidvc_audio_format.cpp`,
  `cdidvc_audio_reference.cpp`, `cdidvc_audio_replay.cpp`, `cdi_fma_attenuation.cpp`,
  `cdi_audio_arithmetic.cpp`, plus record 5 integration; [D1]-[D3], [D5], [D8].
- **Next:** close software fixture gaps; acquire distinguishing firmware/capture
  evidence before changing already bounded saturation/rounding models.

### 8. XA routing / ADPCM

- **Implementation:** Mode-2 routing/coding, 4/8-bit mono/stereo prediction,
  hardware-selected redundant parameters, deterministic malformed-group concealment,
  both XA rates, double-buffer transport and de-emphasis on A0.
- **Verification:** A0/A1, 2026-09-06: exhaustive legal states and a two-sector
  4-bit stereo output matching FFmpeg; selected-copy/buffer captures are retained
  upstream references. This is not exhaustive bit-exact silicon proof.
- **Known inaccuracies / remaining:** CDIC internal rounding/width, selected-invalid
  error/status/concealment, player-specific coding anomalies, reset/DAC boundaries.
- **Tests/docs:** `tests/emu/philips/cdicdic.cpp`, `cdi_audio_arithmetic.cpp`;
  [D1] sections 6-8, 13-14, [D2], [D5].
- **Next:** retain a media-audited retail XA scene; pursue discriminating one-LSB
  vectors only with a suitable hardware/reference output oracle.

### 9. CD-DA and Q subcode

- **Implementation:** A0 delivers 588 stereo frames per 75 Hz sector, bypasses CDIC
  RAM for PCM, retains a pre-start sector, gates playback with AUDCTL and preserves
  Q control/ADR and emphasis. Position/index/lead Q packets remain synthesized.
- **Verification:** sector/output model covered by A0/A1, 2026-09-06; full transport,
  mixed-mode, pregap/index and Q-position reference fixtures are explicitly missing.
- **Known inaccuracies / remaining:** play/pause/resume/stop/seek model validation,
  track/lead/multi-session/R-W behavior, physical servo latency and sample alignment.
- **Tests/docs:** `tests/emu/philips/cdicdic.cpp`, `cdi_audio_arithmetic.cpp`;
  [D1] sections 15-16, [D3]-[D5].
- **Next:** highest-priority software batch: synthetic mixed-mode/Q reference
  fixtures, with production fixes only where the oracle demonstrates a defect.

### 10. DMA

- **Implementation:** SCC owns transfer counters/addresses/completion; CDIC channel
  1 and DVC channel 2 are clients. C0 adds live DVC transfer; A0 adds zero-count
  policy, partial transfer, abort/restart and a complete legal Layer II frame feed.
- **Verification:** fixture presence checked at C0; A0 live gate is green on
  2026-09-06. No new physical DREQ/bus capture or full CDIC live DMA claim.
- **Known inaccuracies / remaining:** burst/cycle-steal/chaining, NDT/bus errors,
  arbitration and exact request timing. Zero-count behavior remains labeled policy.
- **Tests/docs:** `tests/emu/machine/scc68070.cpp` and record 5 DMA fixtures;
  [D0], [D1] section 5, [D7].
- **Next:** a bounded CDIC channel-1 live fixture or evidenced bus-error/request-mode
  batch; preserve the green DVC audio boundary.

### 11. Interrupts

- **Implementation:** SCC priority arbitration, CDIC source gating/read-to-clear,
  SLAVE readiness/IRQ2 and DVC completion/status acknowledgements are modeled.
- **Verification:** historical register/helper gates plus A0 synchronized SCC/DVC
  IRQ observation, 2026-09-06. A same-callback fixture assumption was corrected to
  observe the next scheduler turn; this is not measured physical propagation delay.
- **Known inaccuracies / remaining:** pin-level IACK/DREQ timing, full error-source
  coverage, unknown status bits and physical DAC-related interrupts.
- **Tests/docs:** `tests/emu/machine/scc68070.cpp`, `tests/emu/philips/cdicdic.cpp`,
  `cdislavehle_response_ready.cpp`, `cdi_dvc_edge_integration.cpp`; [D0], [D6], [D7].
- **Next:** add the relevant IRQ expectation to each substantive transfer/transport
  fixture; do not infer hardware timing from scheduler mechanics.

### 12. Timing and A/V synchronization

- **Implementation:** MCD212 field model, 75 Hz CDIC transport and rational
  sample-to-90 kHz / DCLK conversion. Audio branch schedules against advanced DCLK.
- **Verification:** A0/A1, 2026-09-06: 30-minute XA and MPEG software-clock fixtures;
  25 Hz boundaries exact, 30000/1001 bounded to one 90 kHz / one 45 kHz tick;
  deterministic discontinuity re-anchoring. These are arithmetic tolerances.
- **Known inaccuracies / remaining:** physical oscillators/servo, host audio/video
  presentation drift, branch latency, seek/reset and DAC sample edges.
- **Tests/docs:** `tests/emu/philips/cdidvc_timing.cpp`, `cdidvc_avsync_threshold.cpp`,
  `cdidvc_dclk_wrap.cpp`, `cdicdic.cpp`, `mcd212_video.cpp`; [D1] sections 8-9,
  [D3], [D5], [D7].
- **Next:** meaningful timestamped branching A/V and retained runtime telemetry;
  never tune a clock to conceal an unidentified queue/presentation problem.

### 13. Save states

- **Implementation:** SCC persistent state, CDIC/SLAVE state and DVC opaque decoder
  replay; A0 includes MMU and simultaneous A/V reconstruction.
- **Verification:** M1/A0/A1, 2026-09-06: real MMU state and DVC requested/current/end
  state, queued audio plus video header, repeated continuation hash. This is not
  whole-machine coverage of every active peripheral operation.
- **Known inaccuracies / remaining:** active I2C/UART/CDIC DMA and partial SLAVE
  command round trips, broad retail A/V continuation, cross-version limitations.
- **Tests/docs:** `tests/emu/philips/cdidvc_audio_replay.cpp`,
  `cdidvc_state_transitions.cpp`, `cdi_dvc_state_integration.cpp`,
  `cdi_mmu_integration.cpp`; [D0], [D1] section 4, [D6], [D7].
- **Next:** save/load during the new CD-DA/Q or branching fixture, then targeted
  active-peripheral snapshots; do not repeat the already green A/V command-cycle test.

### 14. SLAVE HLE, input and peripherals

- **Implementation:** bounded command parser, response readiness/replacement,
  pointer wrap/clamp, reset and service-backed revision response. Keyboard source,
  disc-base, developer/X-bus protocol and real SERVO linkage remain incomplete.
- **Verification:** historical Phase D (2026-08-24) reports 3,262 parser assertions /
  5 cases and firmware-only Mono-I smoke. Current source/test presence C0 and
  maintained regression A0 checked 2026-09-06; no broad controller hardware proof.
- **Known inaccuracies / remaining:** fixed delays/status, mailbox wiring/depth,
  pointer polling/ranges across machines, partial-command save and mute/reset timing.
- **Tests/docs:** `tests/emu/philips/cdislavehle_commands.cpp`,
  `cdislavehle_pointer.cpp`, `cdislavehle_response_ready.cpp`,
  `cdislavehle_transport.cpp`; [D0] Phase D.
- **Next:** a bounded partial-command snapshot or controller trace task; keyboard
  and SERVO protocol require evidence rather than fabricated replies.

### 15. SERVO and disc handling

- **Implementation:** HLE sector reads/filters, fixed seek delay, synthesized TOC
  and bounded failed-read termination; Mono-II SERVO/SLAVE SPI is structurally known
  but lacks the required pin-level integration.
- **Verification:** C0 historical command tests and A0 CDIC tests; reviewed 2026-09-06.
  No independent current disc-handling completion or real servo certification.
- **Known inaccuracies / remaining:** physical seek/track loss, seek-only command,
  TOC ordering, error/EDC/ECC, mixed-mode/multi-session and true SERVO exchanges.
- **Tests/docs:** `tests/emu/philips/cdicdic.cpp`, `cdimono2.cpp`; [D0], [D3]-[D5].
- **Next:** synthetic disc boundary coverage in record 9; keep physical transport
  latency as an explicit model until a trace distinguishes it.

### 16. Board glue and Mono-II

- **Implementation:** Mono-I optional DVC configurations; Mono-II mapped structural
  skeleton, IRQ2 connection and disabled DSP placeholder.
- **Verification:** historical Phase E (2026-08-24) reports 327 structural assertions /
  5 cases, `-validate cdimono2` and device enumeration; A0 covers optional DVC presence.
  Last evidence review 2026-09-06. No Mono-II firmware/title boot is established.
- **Known inaccuracies / remaining:** asynchronous `/DTACK`, host mailbox, SPI pins,
  DSP execution/host/local memory/LEMM, matching firmware runtime evidence.
- **Tests/docs:** `tests/emu/philips/cdimono2.cpp`, `cdi_dvc_edge_integration.cpp`;
  [D0] Phase E, [D7].
- **Next:** a separately scoped bus/mailbox foundation when justified; structural
  validation cannot be reported as CD playback or DSP functionality.

### 17. Compatibility

- **Implementation status:** representative title/media ledger exists; a ledger is
  not a code feature or a successful playthrough.
- **Verification:** [D4] at A1, 2026-09-06, records software-list properties only:
  `7thguest` DVC plus audio disc (still `supported="no"`), `burncycm` multi-track
  CD-Audio, and `hotmariou`/`linkfoeu` non-DVC candidates without proven XA scenes.
- **Known inaccuracies / remaining:** no retained campaign retail-title passes.
  Helper/integration success cannot substitute for title traces or media audits.
- **Tests/docs:** subsystem suites supply model evidence only; [D4] defines the
  future title-runtime procedure. No reviewed dedicated retail-title regression.
- **Next:** record disc SHA-1, machine/BIOS, exact commit, commands/inputs, scene,
  audio path and save/load/transition trace for representative titles.

### 18. Cross-system audio and video

- **Implementation:** aggregation of the CDIC/DVC/MCD212/SLAVE paths above.
- **Verification:** historical audio 50% / video 60% at C0; audio 60% / video 60%
  in A0's older broad table. A1/A0 show newer audio gates without a new aggregate
  estimate. Last review 2026-09-06; no independent aggregate verification commit.
- **Known inaccuracies / remaining:** all applicable record 4-17 limits; these
  overlapping estimates must not be averaged into a project completion percentage.
- **Tests/docs:** use constituent tests, [D0]-[D5].
- **Next:** report constituent scopes and close the finite reference-fixture gaps;
  assign a new aggregate only with an explicit evidence-based denominator.

## Documentation contradictions reconciled here

1. **Canonical historical freshness/provenance:** [D0] says “Last reviewed
   2026-08-24” and cites pre-rewrite Phase A-D hashes, while canonical contains
   later 2026-09-03/05 commits. Historical test dates/hashes have not been retagged
   as fresh certification of rewritten commits. C0 identifies the inspected tree.
2. **Missing DMA fixture claim:** [D0]'s Phase A paragraph says no live DVC-to-SCC
   fixture exists. C0 actually adds `cdi_dvc_dma_integration.cpp`. Its historical
   paragraph is qualified in this infrastructure update. A0 also covers edges and
   complete Layer II ingress.
3. **Audio-branch MMU text:** [D0] at A0 still says “storage only/no translation.”
   [D6], implementation history and A0's integration gate supersede that claim.
   The statement remains true for canonical C0.
4. **Audio next tasks:** the older broad table and [D2] still list attenuation
   coefficients, active A/V saves and clock tests as unfinished. [D1], [D3], [D7]
   and [D8] record completed digital Q22, live state and arithmetic-clock work.
   Physical transitions and exact instruction attribution remain open.
5. **Unchecked deliverables:** [D1] still leaves compatibility-matrix and final-report
   deliverables unchecked, although [D4] and [D3] exist. Their existence is complete;
   retail-title certification and strict campaign-wide 100% are not. Do not mark
   the stricter certification requirement satisfied merely because a report exists.
6. **“Hardware blockers” is too broad:** [D5] provides acquisition procedures, but
   [D3] correctly distinguishes software-testable CD-DA/Q/mixed-mode and branching
   FMV gaps. These are actionable engineering work, not all hardware-only blockers.
7. **Latest CI versus final report:** [D3]'s A1 baseline remains valid historical
   certification. A0 subsequently adds four MCD212 cases and passes with the newer
   counts in the branch index. A0 success is a fast helper/integration result, not
   an unperformed full native build, validator run or retail playthrough.

Older detailed documents remain historical records. This index explicitly supersedes
their listed stale high-level statements and routes each task to the newer scoped
document. Avoid a broad rewrite of historical audits merely to answer “status.”

## Next substantive engineering batch

**Branch:** `audio/cdi-fidelity-100-campaign-20260905`.

1. Read [D1] sections 15-16, [D3]'s remaining software gaps and the CDIC Q/TOC
   production path. Build a deterministic synthetic mixed-mode image/reference
   fixture with multiple audio tracks, INDEX 00/01, a data/audio boundary,
   pre-emphasis and sector-marked PCM.
2. Establish independently expected Q control/ADR, track/index, relative/absolute
   time and sector/sample progression. Exercise model play/pause/resume/stop/seek,
   track and lead boundaries and save/load. Fix production discrepancies demonstrated
   by that oracle. Do not use the implementation itself as its only test oracle.
3. Keep physical servo delay, true DAC edges and unavailable R-W observations
   separate. Update [D1], [D3], [D4] and master record 9 with exact evidence.
4. Run the audio campaign's substantive-change gates: DVC audio, CDIC/XA,
   replay/save, SCC DMA, live DVC integration and all Philips helpers;
   full native/`cdivalidate` gates at milestone folds as specified in [D1].
5. Commit and push a coherent code/test/docs batch. Then implement a controlled
   branching Full Motion fixture with meaningful SCR/PTS/DCLK and both decoded
   audio/video branches. Repeated command-bit cycling alone does not close that gate.

Evidence acquisition for VMPEG P-program instructions, CDIC quantization/rounding
and analogue transitions can proceed when suitable inputs exist. Do not change the
current constrained arithmetic solely because those evidence questions are open.

## Document registry

Canonical [D0] is a relative link. Audio references are pinned to A0 so this index
also works on canonical, where the campaign documents are absent. When developing
on the audio branch, read each **current local path**, and update this index after
new substantive evidence. Pins are evidence baselines, not instructions to ignore
newer committed work.

- **D0**: [Historical modernization detail](cdi_modernization_status.md);
  [audio-branch version](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_modernization_status.md).
- **D1**: [Audio campaign matrix](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity_campaign.md).
- **D2**: [Audio evidence ledger](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity.md).
- **D3**: [Final audio certification](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_final_certification_20260906.md).
- **D4**: [Audio compatibility matrix](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_compatibility_matrix_20260906.md).
- **D5**: [Audio evidence blockers](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_hardware_evidence_blockers_20260906.md).
- **D6**: [SCC68070 MMU checkpoint](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_scc68070_mmu_checkpoint_20260906.md).
- **D7**: [DVC DMA/A-V/optional-presence checkpoint](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).
- **D8**: [Audio arithmetic checkpoint](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_arithmetic_checkpoint.md).

## Maintaining this index

- Routine status reads this index. State the branch scope, carried/stale estimates,
  confidence, known gaps and next task without repeating the original audit.
- Before development or verified status, inspect branch/HEAD/worktree, the affected
  records, current subsystem documentation and recent relevant commits.
- For a changed row, record implementation, verification scope, inaccuracies,
  remaining work, tests/docs, exact verified code commit/date and next action.
  Update source pins and CI references once; keep historical results labeled.
- A current branch advancing beyond this snapshot is a reason to check its relevant
  changes, not a reason to re-audit unrelated emulator subsystems.
- Keep canonical and campaign implementation states separate. This shared
  documentation infrastructure may be copied between them without folding code.

Initialization validation: repository branch/HEAD and file-path checks; source
document reconciliation; GitHub CI metadata/log review; Markdown links/references
and `git diff --check`. No new emulator tests or hardware/title runs were performed.

[D0]: cdi_modernization_status.md
[D1]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity_campaign.md
[D2]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity.md
[D3]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_final_certification_20260906.md
[D4]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_compatibility_matrix_20260906.md
[D5]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_hardware_evidence_blockers_20260906.md
[D6]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_scc68070_mmu_checkpoint_20260906.md
[D7]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md
[D8]: https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_arithmetic_checkpoint.md
