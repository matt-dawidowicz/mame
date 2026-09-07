# CD-i verified status audit — 2026-09-06

> Historical pre-merge assessment. Current unified scores, corrected SCC/DSP descriptions and closed CI filter issue are in [the unified audit](cdi_unified_verified_status_20260907.md) and [master status](CDI_MASTER_STATUS.md).

This is a fresh, chunked **source/test/evidence audit** and recalculation, replacing
the initialization's carried percentages. It is not a claim of fresh hardware
measurements, a complete instruction-level proof or a retail-title playthrough.
The source review is focused on implemented paths, integration boundaries and
representative tests, rather than every line of MAME or every legal hardware state.

## Baselines and verification

- Canonical source HEAD: `da9a81c5bdf7c6d24a67b1b608575075ae469c33`, code baseline
  `f0d78dfbda5c7fbbdb11c3fc1ba25a58e731146f`.
- Audio source HEAD: `21ef9f8fb08a640ed177cf9830f344a947da7344`, code baseline
  `442e5050846e680ed653fc1297b66df588b314e2`.
- Remote refs were fetched at audit start; both worktrees were clean. The only
  intervening commits from these code baselines were documentation.
- Fresh local audio helper build/run: **PASS — 17,061,392 assertions / 191 cases**,
  GCC 13.3.0, C++17, `-O1`, Linux. This builds the 25 existing test translation units
  registered in the `cdihelpertests` section of `scripts/src/tests.lua`; it is a
  standalone helper build, not the generated emulator-linked target.
- Existing full-machine CI remains **21 assertions / 6 cases PASS** at audio code
  baseline, run [34057720217](https://github.com/matt-dawidowicz/mame/actions/runs/34057720217).
  It was not rebuilt or rerun locally for this audit. Its exact fixture scope was
  inspected, which exposed the limits below. The apparent small assertion count
  wraps many fixture-internal `expect` conditions; it is not a count of all checks.
- Canonical was assessed through current source/diffs and its existing test tree;
  its historical test results were not relabeled as a fresh canonical execution.
- No new firmware, commercial media, analogue captures or physical timing data
  were acquired. Existing hardware/specification claims were traced to repository
  evidence ledgers, not independently remeasured or fully rederived from manuals.

Reproduce the fresh helper build from the audio baseline:

```python
import re, subprocess
from pathlib import Path
block = Path('scripts/src/tests.lua').read_text().split('project("cdihelpertests")')[1].split('-- Full-machine')[0]
files = [p for p in re.findall(r'MAME_DIR \.\. "([^"]+\.cpp)"', block) if p.startswith('tests/')]
subprocess.run(['g++', '-std=c++17', '-O1', '-pthread',
    '-I3rdparty/catch/single_include', '-Isrc/mame/philips',
    '-Isrc/devices/machine', '-Isrc/lib/util', *files,
    '-o', '/tmp/cdi-audit-helpers'], check=True)
subprocess.run(['/tmp/cdi-audit-helpers'], check=True)
```

## Recalculation method

A percentage here means **estimated engineering completion of the stated scope**,
including known implementation and validation obligations. It is not percentage
of games working, coverage, measured silicon fidelity, or engineering hours spent.
Each row has named work packages totaling 100 weight points. Importance weights
and grades are engineering judgments made explicit so another developer can
challenge them; the resulting arithmetic is reproducible, not an objective physical
measurement. A hardware/runtime obligation with no evidence receives no credit.

| Grade | Credit | Meaning |
| --- | --- | --- |
| 0 | 0% | Absent, or no qualifying evidence for this obligation. |
| 1 | 25% | Placeholder/limited implementation, or a blocking functional defect. |
| 2 | 50% | Working model with substantial gaps or thin verification. |
| 3 | 75% | Implemented and meaningfully tested; important scope/evidence gaps remain. |
| 4 | 100% | Closed **only for the named bounded obligation**, with no known remaining work. |

`raw = sum(weight * grade / 4)`; report to the nearest **five percentage points**.
Medium-confidence estimates have roughly ±5-point judgment uncertainty; Low can
be ±10 or more. These are not statistical confidence intervals. A known open item
prevents grade 4 for its encompassing package. Parser classification may be closed
while physical malformed-input response, in a different package, remains open.

The JSON companion records each weight, grade, rationale, raw sum and rounded
result. The same denominator is used on both branches. Old percentages did not
have this denominator: a lower new estimate is not proof of a regression. Parent
rows are independently scoped summaries, not averages of overlapping child rows;
do not average them into an overall project percentage.

## New findings and corrected verification claims

### F1 — CDIC DMA can leave its SRAM allocation (both branches)

**Source-confirmed memory-safety defect; no full-device runtime reproduction in this audit.**
`cdicdic_device::device_start` allocates 0x4000 bytes. In `regs_w` DMACTL handling,
`device_index = (m_dma_control & 0x3fff) >> 1`, and each transfer increments it
without masking or checking the next SRAM access. `dma_channel_transfer` decrements
only the SCC count; it does not bound the CDIC device index.

A two-word transfer beginning at DMACTL 0x3ffe accesses byte offsets 0x3ffe/0x3fff,
then 0x4000/0x4001. The second operand is outside the allocation. Canonical uses a
native-word pointer; audio uses endian-explicit helpers, but both retain this bug.
This is a software defect independent of whether real hardware wraps or errors.
The fix must bound memory and test the chosen policy without inventing silicon.

**Next:** regression at the last SRAM word and oversize transfer, in both directions,
then a production fix and live CDIC channel-1 fixture. Highest-priority development
item because the known code path can access host memory outside the allocation.

### F2 — SCC MMU restartable fault path is not enabled (audio branch)

**Source-traced functional defect; executed-CPU reproduction still required.**
`scc68070_device::translate_address` calls `set_buserror_details(..., true)` for a
violation. In Musashi that function raises `m_mmu_tmp_buserror_occurred` only if
`m_can_instruction_restart` is true. Common initialization/reset leave it false;
`set_emmu_enable` enables it, but no call exists in the reviewed SCC68070/CD-i path.
Writing MCR.EN updates the SCC register without enabling that Musashi mechanism.
The call therefore records fault metadata without activating the advertised
restartable exception route. Rejected read callbacks return sentinel values.

The current MMU integration fixture suspends the CPU, calls
`device_memory_interface::translate` (side effects disabled), and round-trips a
`ram_state`. It does not execute protected memory operations, observe the bus-error
vector, validate exception stack state, or execute RTE. Green helpers and that
fixture cannot establish this behavior. The earlier “architectural gates closed /
only silicon questions remain” claim is withdrawn for the broad MMU scope.

**Next:** executed fetch/read/write/protected-cross-boundary tests, correct reset/
enable/postload fault-path ownership and exception/restart semantics. Merely adding
one flag write is not enough to certify SCC-specific stack/retry behavior.

### F3 — Q position and TOC remain functionally incomplete (both branches)

**Source-confirmed field-generation defects/limitations.** In non-TOC
`process_disc_sector`, track and index are fixed to 0x01, and relative MSF fields
reuse the absolute `m_curr_lba + 150` values. Later tracks and pregaps cannot be
reported correctly. Actual track lookup is used for CD-DA ADR/control on audio,
but does not reach the track/index fields. The TOC `frames` accumulator adds data
track lengths while omitting audio track lengths before creating its A2 packet;
this is not a general disc lead-out oracle. CRC generation also lacks an
independent reference test; no specific CRC replacement is asserted here.

**Next:** a synthetic multi-track/mixed-mode disc and independent Q/TOC fixtures;
fix track/index/relative-time/lead-out discrepancies before claiming transport fidelity.

### F4 — A/V tests do not establish decoded movie or physical output continuity

The 30-minute tests in `cdidvc_avsync_threshold.cpp` loop over calculated frame
numbers and call clock helpers. No MPEG video packets are decoded, no frames are
presented and no sound device is recorded by those tests. The simultaneous device
save fixture feeds a decoded audio frame and a valid video **sequence header**;
it does not retain decoded video pictures. Its continuation hash tracks guest-visible
state while cycling commands, not full PCM plus presented-picture equality.

Those are useful passing tests. They do not close continuous decoded/presented A/V,
interactive branching or a decoded-video save/load gate. The updated campaign
checkbox wording preserves the actual passed scopes and leaves those larger ones open.

### F5 — Layer II reference agreement is tolerant, not bit-exact

`require_reference_match` inspects 64 selected stereo sample positions plus aggregate
metrics from three repeated frames. Limits are 600/1200/1500 PCM counts depending
on mode, 6.25% absolute-sum difference and 12.5% energy difference. The dual-channel
case changes the stereo header and reuses its reference. These are useful regression
bounds, not exhaustive decoder conformance or bit-exact VMPEG PCM. XA's retained
4-bit stereo fixture has a stronger exact software-reference result, but is not
an oracle for every XA mode or CDIC silicon arithmetic.

### F6 — Broad video and save-state boundaries remain open

`video_decoder_pump` retains an explicitly temporary 26-picture host decode-ahead
probe and first-PTS-relative presentation model. The RGB conversion regression
compares PL_MPEG output formats with each other; it is not an independent frame
oracle. MCD212's extensive control/token tests do not exercise the complete live
render pipeline. Save journals have finite 8 MiB audio / 32 MiB video bounds;
invalid/overflow layouts follow failure/reset policy, not general continuation.
DVC configurations still carry `MACHINE_NOT_WORKING` and do not declare
`MACHINE_SUPPORTS_SAVE` in the driver registration, unlike base Mono-I. Flags are
not a measured completion score, but cannot be replaced with assumed title support.

### F7 — CI change detection has an SCC coverage hole

The audio fast workflow's push/pull-request path filter includes the SCC CPU wrapper
and `tests/emu/machine/scc68070.cpp`, but omits `src/devices/machine/scc68070*`, where
the device MMU policy and helpers live. A device-only change can therefore skip
that gate. This did not invalidate the inspected green run, which was triggered
by test changes, but the next SCC fix should update the filter or explicitly run
its gate. This infrastructure observation is not an emulator-fidelity percentage.

## Fresh master matrix

| Subsystem | Canonical | Audio branch | Confidence |
| --- | ---: | ---: | --- |
| SCC68070 CPU and internal peripherals | 50% | 55% | Medium |
| SCC68070 MMU | 15% | 65% | Medium |
| CDIC | 40% | 55% | Medium |
| MCD212 display | 60% | 65% | Medium |
| DVC overall | 50% | 70% | Medium |
| MPEG video decode and presentation | 55% | 55% | Low |
| DVC audio | 40% | 80% | Medium |
| XA routing and ADPCM | 50% | 75% | Medium |
| CD-DA playback and transport | 30% | 50% | Low |
| CD-DA Q and other subcode | 20% | 40% | Low |
| DMA integration | 45% | 60% | Medium |
| Interrupts | 55% | 65% | Medium |
| Device timing | 50% | 60% | Medium |
| A/V synchronization | 30% | 45% | Medium |
| Save states | 45% | 60% | Medium |
| SLAVE HLE | 55% | 55% | Medium |
| Input and peripherals | 45% | 45% | Medium |
| SERVO and MCU integration | 15% | 15% | Low |
| Disc handling | 40% | 50% | Low |
| Mono-I/II board glue | 45% | 50% | Low |
| Mono-II functional system | 20% | 20% | Low |
| Cross-system audio | 40% | 70% | Medium |
| Cross-system video | 55% | 55% | Low |

**Compatibility remains unmeasured.** The campaign ledger has no retained retail
runtime certification in its three required path categories (XA, DVC and CD-DA):
**0 of 3 categories certified**, not 0% games playable. The potential title universe
and success criterion are not defined well enough to estimate actual compatibility.

Scoped Layer II parser classification and AUDCTL register-model gates retain their
previous 100% statements; neither is extended to decoder error response, internal
silicon arithmetic, physical DAC behavior or complete playback.

## Weighted subsystem worksheets

All worksheets were reviewed on 2026-09-06 at the exact source baselines above.
For the audio branch the last fresh executed test baseline is `21ef9f8` (code
identical to `442e505`); for canonical it is **not a fresh runtime verification**.
The full paths in the evidence registry identify implementation, relevant tests and
subsystem documentation. Each rationale distinguishes implemented/model behavior
from what is verified and names the unclosed work. Scores do not imply every cited
source file or every supported state was exercised.

### scc — SCC68070 CPU and internal peripherals

Estimate: **50% canonical / 55% audio**; confidence **Medium**. Raw worksheet totals: 50 / 55.

Evidence groups: [CPU], [MMU], [DMA], [IRQ].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| CPU access integration | 15 | 2 | 2 | Musashi access callbacks exist; SCC-specific execution/fault corpus is missing. |
| Timers | 10 | 2 | 2 | Timer 0 modeled; Timer 1/2 counters lack functional scheduling. |
| DMA controller | 15 | 2 | 2 | Basic transfer engine; CDIC boundary defect and advanced modes remain. |
| Interrupt controller | 10 | 3 | 3 | Priority helpers and acknowledgements; physical IACK incomplete. |
| UART | 10 | 2 | 2 | Data/clock/side effects exist; forced TXEMT, fixed ten-bit timing and break stubs. |
| I2C | 10 | 2 | 2 | Master state machine exists; slave and multi-master incomplete. |
| MMU integration | 20 | 1 | 2 | Canonical stores registers; audio translates but restart enable is missing. |
| Reset and persistent state | 10 | 3 | 3 | Registered state and deterministic reset; active peripheral round trips incomplete. |

**Next action:** Fix and execute-test MMU fault delivery; then timer/UART/I2C gaps.

### mmu — SCC68070 MMU

Estimate: **15% canonical / 65% audio**; confidence **Medium**. Raw worksheet totals: 16.25 / 62.5.

Evidence groups: [MMU].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Register interface | 15 | 3 | 4 | Masked descriptor/control state has helpers and live register round trip. |
| Translation and CAM | 25 | 0 | 3 | Audio has descriptor translation; instruction execution is not integration-tested. |
| Permissions and operand boundaries | 20 | 0 | 3 | Helper permissions/preflight exist; live protected writes are not exercised. |
| Exception and restart | 25 | 0 | 1 | Fault metadata call exists but m_can_instruction_restart stays false on this path. |
| Save/load | 10 | 2 | 3 | Audio ram_state round trip tests translation queries with the CPU suspended. |
| Silicon timing and ambiguous CAM edges | 5 | 0 | 0 | No distinguishing physical oracle. |

**Next action:** Enable the correct fault path and test executed fetch/read/write faults and RTE.

### cdic — CDIC

Estimate: **40% canonical / 55% audio**; confidence **Medium**. Raw worksheet totals: 37.5 / 56.25.

Evidence groups: [CDIC], [XA], [CDDA], [DMA].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Commands and transport | 15 | 2 | 2 | Read/stop state exists; seek-only completion and errors remain modeled. |
| Mode-2 routing and coding | 15 | 2 | 4 | Audio exhausts standards routing, coding and selected parameter policy. |
| Buffers and delivery | 15 | 2 | 3 | Measured double-buffer behavior; no full disc-device campaign. |
| Audio control and handoff | 15 | 1 | 3 | Audio fixes AUDCTL/playback/sector ownership; physical edges remain. |
| DMA SRAM boundaries | 15 | 1 | 1 | Unbounded device_index can leave the 16 KiB allocation on both branches. |
| TOC and subcode | 15 | 1 | 1 | Hard-coded position fields and partial TOC synthesis. |
| Error/status fidelity | 5 | 1 | 1 | Read failure is bounded; hardware error response incomplete. |
| Active save/load | 5 | 2 | 2 | Fields are registered; CDIC live transport snapshot fixture missing. |

**Next action:** Repair the channel-1 SRAM boundary before more audio-fidelity work.

### mcd — MCD212 display

Estimate: **60% canonical / 65% audio**; confidence **Medium**. Raw worksheet totals: 60 / 65.

Evidence groups: [MCD].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Display modes | 20 | 3 | 3 | CLUT/RLE/DYUV/RGB/QHY implementation; complete pixel-reference corpus absent. |
| ICA/DCA and memory bounds | 20 | 2 | 3 | Audio adds wrapped control fetches; whole live command engine not exhaustive. |
| Field timing | 15 | 3 | 3 | Audio increases control-space testing; physical field edges remain. |
| QHY reconstruction | 15 | 3 | 3 | Token/FIR/quantizer vectors; cold field and odd-sum silicon remain. |
| Matte/cursor/mosaic/weight | 15 | 2 | 2 | Production pixel paths exist; limited independent image oracles. |
| External overlay | 5 | 2 | 2 | Eligibility helper tested; combined-device frame not tested. |
| Save state | 5 | 2 | 2 | Fields saved; active frame reconstruction corpus missing. |
| Physical output certification | 5 | 0 | 0 | No current full-frame hardware match. |

**Next action:** Add complete register-to-pixel and live overlay fixtures, then hardware captures.

### dvc — DVC overall

Estimate: **50% canonical / 70% audio**; confidence **Medium**. Raw worksheet totals: 47.5 / 70.

Evidence groups: [DVC], [AV], [SAVE].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Ingress and parser | 15 | 2 | 4 | Audio header/access/PES paths have broad deterministic coverage. |
| Registers/status/commands | 15 | 2 | 3 | Stream/CSU and optional presence tested; private event edges unresolved. |
| Audio decode/output | 20 | 2 | 3 | Reference-tested audio; physical arithmetic/output limits remain. |
| Video decode/output | 20 | 2 | 2 | Backend exists; independent I/P/B decode corpus absent. |
| Presentation scheduling | 10 | 2 | 2 | 26-frame temporary decode-ahead and first-PTS anchoring are models. |
| DVC DMA boundary | 10 | 2 | 4 | Audio live normal/edge/complete Layer II ingress fixtures close that software scope. |
| Save reconstruction | 5 | 2 | 3 | Live state/queued audio/video header tested, no real decoded A/V continuation. |
| Physical board fidelity | 5 | 0 | 0 | No full VMPEG timing/analogue certification. |

**Next action:** Prioritize actual branching MPEG presentation and live output evidence.

### mpeg_video — MPEG video decode and presentation

Estimate: **55% canonical / 55% audio**; confidence **Low**. Raw worksheet totals: 53.75 / 56.25.

Evidence groups: [DVC], [MCD], [AV].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Backend decode | 20 | 2 | 2 | PL_MPEG runs; test tree does not retain an independent full video decode oracle. |
| Packet/header handling | 20 | 3 | 3 | PES and sequence/header helpers covered. |
| Picture event/reordering | 15 | 3 | 3 | All picture-type/marker helper combinations, not complete picture decode. |
| Color conversion | 10 | 2 | 3 | Audio compares PL_MPEG RGB/BGRA/ARGB paths; internal consistency only. |
| Presentation/queue timing | 15 | 2 | 2 | Approximate backend queue and timestamp policy remain. |
| MCD212 composition | 10 | 2 | 2 | Production handoff exists; no combined-device pixel fixture. |
| Independent frame/title certification | 10 | 0 | 0 | No retained present-frame or retail-runtime corpus in reviewed scope. |

**Next action:** Retain an independent decoded I/P/B reference stream and presentation hashes.

### dvc_audio — DVC audio

Estimate: **40% canonical / 80% audio**; confidence **Medium**. Raw worksheet totals: 40 / 80.

Evidence groups: [DVC], [ARITH], [AV], [SAVE].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Parser/access/selection | 15 | 2 | 4 | Exhaustive legal syntax/profile, stream IDs and access forms. |
| Layer II PCM decode | 20 | 2 | 3 | Three repeated frames/reference sampling; tolerances up to 1500 PCM counts. |
| PCM queue | 15 | 2 | 3 | Deterministic pair/zero/refill; physical DAC rule missing. |
| Live DMA ingress | 10 | 2 | 4 | Legal frame traverses actual SCC/DVC device boundary in existing CI. |
| FMA digital gain | 10 | 1 | 4 | Literal recovered Q22 curve, routes and mute tested. |
| De-emphasis response | 10 | 0 | 3 | Standards shelf response tested; physical transition unknown. |
| Termination and switching | 10 | 2 | 3 | Program end/requested-current state tested; interactive PTS branches absent. |
| Audio save/load | 5 | 2 | 3 | Replay and device state tested; real full A/V continuation absent. |
| Physical DSP/DAC attribution | 5 | 0 | 0 | Exact instruction/limiter and waveform remain unavailable. |

**Next action:** Add meaningful branching/output fixtures; do not relabel bounded PCM error as bit-exact.

### xa — XA routing and ADPCM

Estimate: **50% canonical / 75% audio**; confidence **Medium**. Raw worksheet totals: 51.25 / 73.75.

Evidence groups: [XA], [ARITH].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Sector routing | 15 | 2 | 4 | Exhaustive routing decisions use a defined media scope. |
| Coding classification | 10 | 3 | 4 | All coding values and reserved classifications tested. |
| Predictor/residual math | 25 | 3 | 3 | Broad helper oracle; retained exact FFmpeg fixture covers 4-bit stereo only. |
| Group/layout/parameters | 20 | 2 | 3 | Both widths/channels and measured copy selection; selected-invalid signaling unknown. |
| History/cadence | 15 | 2 | 3 | Groups/refill/30-minute arithmetic; live DAC/predictor reset not established. |
| Emphasis | 5 | 0 | 3 | Audio has response tests; switch waveform unknown. |
| Silicon and retail path | 10 | 0 | 0 | No one-LSB silicon or media-audited title pass. |

**Next action:** Keep core reference gates; acquire silicon arithmetic and retail-scene evidence.

### cdda — CD-DA playback and transport

Estimate: **30% canonical / 50% audio**; confidence **Low**. Raw worksheet totals: 28.75 / 48.75.

Evidence groups: [CDDA], [ARITH].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Transport command state | 15 | 2 | 2 | Start/stop/read model; no complete pause/seek reference campaign. |
| PCM sector handoff | 20 | 2 | 3 | Audio 588-frame sector model and pre-start buffer; no live output sample capture. |
| Playback gating/cadence | 15 | 1 | 3 | Audio fixes AUDCTL and 75 Hz events. |
| Track/index transitions | 15 | 1 | 1 | Current Q track/index stay 01 regardless of later track. |
| Position/subcode | 15 | 1 | 1 | Relative fields reuse absolute MSF; no position oracle. |
| Pre-emphasis | 10 | 0 | 3 | Image flags and response tested. |
| Mixed-mode runtime | 5 | 0 | 0 | No retained fixture. |
| Physical alignment/servo | 5 | 0 | 0 | Unmeasured. |

**Next action:** Fix position metadata with a synthetic mixed-mode reference disc.

### q — CD-DA Q and other subcode

Estimate: **20% canonical / 40% audio**; confidence **Low**. Raw worksheet totals: 20 / 38.75.

Evidence groups: [CDDA].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Control/ADR | 10 | 1 | 4 | Audio preserves all encoded control/ADR combinations. |
| Location/cadence | 15 | 1 | 4 | Measured trailer offset and 75 Hz model reflected in production. |
| Track/index/relative position | 25 | 1 | 1 | Hard-coded 01 fields and absolute MSF in relative slots. |
| TOC/lead packets | 20 | 1 | 1 | Synthesis remains partial; audio frames are omitted from frames accumulator. |
| CRC oracle | 10 | 1 | 1 | Existing CRC loop includes twelve bytes; independent packet oracle absent. |
| P/R-W/multisession | 10 | 0 | 0 | Not implemented as faithful delivery. |
| Disc reference campaign | 10 | 0 | 0 | Missing. |

**Next action:** Implement correct track/index/relative time and independently check CRC/lead packets.

### dma — DMA integration

Estimate: **45% canonical / 60% audio**; confidence **Medium**. Raw worksheet totals: 46.25 / 58.75.

Evidence groups: [DMA], [CDIC].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Controller registers | 20 | 3 | 3 | Masks/start/abort/COC implemented; remaining register ambiguity. |
| Address/count sequencing | 15 | 3 | 3 | Pure modes/wrap tested; device-specific coverage incomplete. |
| DVC channel 2 | 25 | 2 | 4 | Audio has normal/abort/restart/Layer II live gate. |
| CDIC channel 1 | 20 | 1 | 1 | Synchronous unbounded SRAM loop; guest can cross allocation. |
| Advanced request/error modes | 10 | 1 | 1 | Burst/chain/NDT/full bus errors incomplete. |
| Bus timing | 10 | 0 | 0 | No cycle-exact arbitration/IACK/DREQ proof. |

**Next action:** Add CDIC end-of-SRAM transfer regression and resolve bounded behavior.

### irq — Interrupts

Estimate: **55% canonical / 65% audio**; confidence **Medium**. Raw worksheet totals: 56.25 / 66.25.

Evidence groups: [IRQ], [DMA].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| SCC arbitration/ack | 25 | 3 | 3 | Priority/source helpers and register implementation. |
| CDIC source control | 20 | 2 | 3 | Audio measured AUDCTL/XBUF/ABUF gating. |
| SLAVE response IRQ | 20 | 3 | 3 | Readiness/replacement helpers; mailbox timing still a model. |
| DVC status/IRQ | 20 | 2 | 3 | Audio integration sees synchronized completion/CSU. |
| All fault/error sources | 10 | 1 | 1 | MMU exception delivery and broader error sources incomplete. |
| Physical IACK edge timing | 5 | 0 | 0 | No trace-derived closure. |

**Next action:** Add live error-source assertions alongside each new transfer/fault fixture.

### timing — Device timing

Estimate: **50% canonical / 60% audio**; confidence **Medium**. Raw worksheet totals: 47.5 / 60.

Evidence groups: [CPU], [CDIC], [MCD], [AV].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Video raster model | 25 | 3 | 3 | Table-derived profiles; pin waveforms unavailable. |
| CDIC sector/sample cadence | 25 | 2 | 3 | Audio long-run helper identity and captured buffer cadence. |
| DVC clocks/scheduling | 25 | 2 | 3 | Advanced DCLK and rational helper tests; actual sequence output absent. |
| CPU/bus service timing | 15 | 1 | 1 | SCC/DMA/DTACK timing incomplete. |
| Physical cross-device calibration | 10 | 0 | 0 | No full calibration corpus. |

**Next action:** Separate timer/sector arithmetic from actual bus and presentation timing.

### av — A/V synchronization

Estimate: **30% canonical / 45% audio**; confidence **Medium**. Raw worksheet totals: 27.5 / 46.25.

Evidence groups: [AV], [DVC].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Clock-domain arithmetic | 20 | 2 | 4 | Audio long-run rational helper tests close arithmetic accumulation scope. |
| Packet scheduling | 20 | 2 | 3 | DCLK-advanced scheduling exists; device presentation not fully validated. |
| Discontinuities | 15 | 2 | 3 | Re-anchoring and command cycles covered; branching MPEG absent. |
| Decoded/presented continuous A/V | 20 | 0 | 0 | 30-minute tests calculate timestamps; they do not decode/render a movie. |
| Host output drift | 15 | 0 | 0 | Not measured. |
| Physical clock/branch latency | 10 | 0 | 0 | Not measured. |

**Next action:** Run a meaningful decoded/presented 30-minute A/V fixture and repeated branches.

### save — Save states

Estimate: **45% canonical / 60% audio**; confidence **Medium**. Raw worksheet totals: 43.75 / 57.5.

Evidence groups: [SAVE], [MMU], [CDIC].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| DVC audio replay/device state | 25 | 2 | 3 | Audio adds end/stream/live ram_state paths. |
| Decoded video continuation | 20 | 2 | 2 | Queues mirrored; live fixture holds sequence header without decoded pictures. |
| MMU state | 15 | 1 | 3 | Audio query/descriptor ram_state round trip; no faulted CPU continuation. |
| CDIC active transport | 15 | 2 | 2 | Registered fields but no full device transport snapshot fixture. |
| SLAVE partial commands | 10 | 2 | 2 | Registered parser/response state; partial-command round trip missing. |
| Active UART/I2C/DMA | 10 | 1 | 1 | Fields exist; end-to-end snapshots missing. |
| Capacity/error policy | 5 | 2 | 2 | 8 MiB/32 MiB replay caps can invalidate snapshots; recovery modeled. |

**Next action:** Test real decoded A/V plus active CDIC/UART/I2C/SLAVE continuations.

### slave — SLAVE HLE

Estimate: **55% canonical / 55% audio**; confidence **Medium**. Raw worksheet totals: 56.25 / 56.25.

Evidence groups: [SLAVE].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Parser/command assembly | 20 | 3 | 3 | Bounded exhaustive command descriptors; actual mailbox wiring uncertain. |
| Response/readiness | 20 | 3 | 3 | Helper timing/replacement tests; fixed HLE delays remain. |
| Pointer path | 15 | 3 | 3 | Wrapping/clamping and packets covered. |
| Audio/reset/LCD control | 10 | 2 | 2 | Production side effects; physical transition/reset behavior not verified. |
| Remaining protocols | 20 | 1 | 1 | Keyboard flag, memory set/clear, disc base, developer/X-bus are stubs. |
| Save/reset | 10 | 2 | 2 | Registered state without active partial-command proof. |
| Physical mailbox | 5 | 0 | 0 | Async DTACK/depth/timing not verified. |

**Next action:** Add partial-command snapshots and evidence-backed missing commands.

### input — Input and peripherals

Estimate: **45% canonical / 45% audio**; confidence **Medium**. Raw worksheet totals: 43.75 / 43.75.

Evidence groups: [SLAVE], [BOARD].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Pointer coordinates/packets | 35 | 3 | 3 | Production callbacks and helper vectors. |
| Buttons/host input | 20 | 2 | 2 | Machine ports exist; controller hardware/runtime breadth limited. |
| Keyboard events | 20 | 0 | 0 | Enable command stores flag; no event source. |
| Discovery/type | 10 | 1 | 1 | Fixed pointer type reply. |
| LCD/test plug | 10 | 2 | 2 | Mono-I paths exist; MCU-based panels blank. |
| Serial timing | 5 | 0 | 0 | Host polling is not serial waveform emulation. |

**Next action:** Validate controller variants; implement keyboard only from protocol evidence.

### servo — SERVO and MCU integration

Estimate: **15% canonical / 15% audio**; confidence **Low**. Raw worksheet totals: 12.5 / 12.5.

Evidence groups: [BOARD], [SLAVE].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Board topology | 15 | 2 | 2 | Documented MCU devices/maps. |
| MCU integration | 20 | 1 | 1 | CPU devices exist; system links incomplete. |
| Command protocol | 25 | 0 | 0 | No complete live SERVO/SLAVE command exchange. |
| Transport feedback | 20 | 0 | 0 | HLE fixed seek has no physical feedback model. |
| Timing | 10 | 0 | 0 | No calibrated MCU/drive communication. |
| Runtime proof | 10 | 0 | 0 | No retained integrated servo firmware pass. |

**Next action:** Establish SPI/DTACK/host interfaces before claiming servo runtime.

### disc — Disc handling

Estimate: **40% canonical / 50% audio**; confidence **Low**. Raw worksheet totals: 38.75 / 50.

Evidence groups: [CDIC], [CDDA], [BOARD].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Sector ingress | 25 | 2 | 3 | Audio distinguishes CD-DA/header validation and endian word paths. |
| Data/audio filtering | 20 | 2 | 3 | Audio has exhaustive routing helper; complete device/media proof pending. |
| Command transport | 20 | 2 | 2 | Read/stop model; seek-only behavior unresolved. |
| TOC/subcode | 15 | 1 | 1 | Partial synthesis and concrete position defects. |
| Error recovery | 10 | 1 | 1 | Bounded failed reads without accurate error/status machinery. |
| Mixed-mode/multisession | 10 | 0 | 0 | No retained reference campaign. |

**Next action:** Repair CDIC boundaries and build reference-disc transport tests.

### glue — Mono-I/II board glue

Estimate: **45% canonical / 50% audio**; confidence **Low**. Raw worksheet totals: 42.5 / 50.

Evidence groups: [BOARD], [DMA].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Mono-I maps/callbacks | 25 | 3 | 3 | Configured CPU/display/CDIC/SLAVE with limited runtime evidence. |
| Optional DVC | 15 | 2 | 4 | Audio live fixture verifies actual presence/absence. |
| Mono-II map | 20 | 2 | 2 | Structural map has deliberately unpopulated host ranges. |
| MCU wiring | 15 | 1 | 1 | IRQ2 connected; SPI/DTACK absent. |
| DSP/LEMM | 15 | 0 | 0 | Disabled DSP56001 execute stub; host/execution absent. |
| Firmware/runtime breadth | 10 | 1 | 1 | Historical Mono-I smoke only; no Mono-II/CD playback pass. |

**Next action:** Preserve Mono-I gates; scope asynchronous mailbox and SPI separately.

### mono2 — Mono-II functional system

Estimate: **20% canonical / 20% audio**; confidence **Low**. Raw worksheet totals: 20 / 20.

Evidence groups: [BOARD].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Board map/configuration | 25 | 2 | 2 | Clocks/RAM/ROM/NVRAM/display structure present. |
| IRQ/reset | 10 | 3 | 3 | Port-B IRQ2 translation tested. |
| Host mailbox/DTACK | 20 | 0 | 0 | Blocked. |
| SERVO-SLAVE SPI | 15 | 0 | 0 | Blocked. |
| DSP/LEMM/audio | 20 | 0 | 0 | Disabled core without execution/host interface. |
| Firmware/CD runtime | 10 | 0 | 0 | No retained matching-ROM boot. |

**Next action:** Implement necessary CPU-bus, MCU SPI and DSP foundations; runtime remains unproven.

### all_audio — Cross-system audio

Estimate: **40% canonical / 70% audio**; confidence **Medium**. Raw worksheet totals: 40 / 68.75.

Evidence groups: [XA], [DVC], [CDDA], [ARITH], [AV].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| XA compressed decode | 20 | 2 | 3 | Native decode broad; silicon arithmetic unproven. |
| Layer II compressed decode | 20 | 2 | 3 | Software reference is tolerant, not bit-exact. |
| Buffer/output delivery | 20 | 2 | 3 | Software queues/cadence tested; physical edges missing. |
| CD-DA transport | 15 | 1 | 2 | Audio improves gating; track/subcode defects persist. |
| Gain/emphasis/control | 15 | 1 | 3 | Q22 and filter tests; CDIC quantizer and switch unknown. |
| Cross-stream/output continuity | 10 | 1 | 2 | Helpers/live commands; no true branching/movie/output capture. |

**Next action:** Fix source defects first, then reference-disc and real A/V output gaps.

### all_video — Cross-system video

Estimate: **55% canonical / 55% audio**; confidence **Low**. Raw worksheet totals: 53.75 / 53.75.

Evidence groups: [MCD], [DVC], [AV].

| Obligation | Weight | Canonical grade | Audio grade | Implementation, evidence and remaining boundary |
| --- | ---: | ---: | ---: | --- |
| Native display | 35 | 3 | 3 | Documented MCD212 model; image corpus incomplete. |
| MPEG decoded pictures | 25 | 2 | 2 | Backend implementation, no independent full decode corpus. |
| Composition | 20 | 2 | 2 | Eligibility tests do not prove combined rendered pixels. |
| Presentation | 10 | 2 | 2 | Scheduling model still requires runtime validation. |
| Hardware/title captures | 10 | 0 | 0 | No retained current complete-frame certification. |

**Next action:** Add independent full-frame decode/composition/presentation evidence.

## Evidence registry

Source links below are pinned to audio code baseline `442e505`; canonical scores
were checked against the same paths at `f0d78df` and their branch diff. Audio-only
files do not exist on canonical and receive no canonical implementation credit.
Links to this audit's repository documents should be read at the current branch.

### CPU

Source: [src/devices/machine/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/machine/scc68070.cpp), [src/devices/cpu/m68000/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/cpu/m68000/scc68070.cpp).

Tests: [tests/emu/machine/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/machine/scc68070.cpp).

Documentation: [docs/cdi_modernization_status.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_modernization_status.md).

[CPU]: #cpu

### MMU

Source: [src/devices/machine/scc68070.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/machine/scc68070.h), [src/devices/machine/scc68070_helpers.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/machine/scc68070_helpers.h), [src/devices/cpu/m68000/m68kcpu.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/cpu/m68000/m68kcpu.cpp), [src/devices/cpu/m68000/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/cpu/m68000/scc68070.cpp).

Tests: [tests/emu/machine/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/machine/scc68070.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_mmu_integration.cpp).

Documentation: [docs/cdi_scc68070_mmu_checkpoint_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_scc68070_mmu_checkpoint_20260906.md).

[MMU]: #mmu

### DMA

Source: [src/devices/machine/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/machine/scc68070.cpp), [src/mame/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic.cpp), [src/mame/philips/cdi.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdi.cpp).

Tests: [tests/emu/philips/cdi_dvc_dma_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_dvc_dma_integration.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_dvc_edge_integration.cpp), [tests/emu/philips/cdi_dvc_audio_dma_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_dvc_audio_dma_integration.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

[DMA]: #dma

### IRQ

Source: [src/devices/machine/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/machine/scc68070.cpp), [src/mame/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic.cpp), [src/mame/philips/cdidvc.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc.cpp), [src/mame/philips/cdislavehle.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/machine/scc68070.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/machine/scc68070.cpp), [tests/emu/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdislavehle_response_ready.cpp).

Documentation: [docs/cdi_modernization_status.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_modernization_status.md).

[IRQ]: #irq

### CDIC

Source: [src/mame/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_state.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic_state.h), [src/mame/philips/cdicdic_memory.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic_memory.h).

Tests: [tests/emu/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdicdic.cpp), [tests/emu/philips/cdicdic_memory.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdicdic_memory.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity_campaign.md).

[CDIC]: #cdic

### XA

Source: [src/mame/philips/cdicdic_state.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic_state.h), [src/mame/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic.cpp).

Tests: [tests/emu/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdicdic.cpp).

Documentation: [docs/cdi_audio_fidelity.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity.md).

[XA]: #xa

### CDDA

Source: [src/mame/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic.cpp), [src/mame/philips/cdicdic_state.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic_state.h).

Tests: [tests/emu/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdicdic.cpp).

Documentation: [docs/cdi_audio_final_certification_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_final_certification_20260906.md), [docs/cdi_audio_compatibility_matrix_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_compatibility_matrix_20260906.md).

[CDDA]: #cdda

### ARITH

Source: [src/mame/philips/cdiaudio.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdiaudio.h), [src/mame/philips/cdiaudio_dsp56001.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdiaudio_dsp56001.h), [src/mame/philips/cdidvc.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc.cpp).

Tests: [tests/emu/philips/cdi_audio_arithmetic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_audio_arithmetic.cpp), [tests/emu/philips/cdi_fma_attenuation.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_fma_attenuation.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_audio_reference.cpp).

Documentation: [docs/cdi_audio_arithmetic_checkpoint.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_arithmetic_checkpoint.md), [docs/cdi_audio_fidelity.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity.md).

[ARITH]: #arith

### MCD

Source: [src/mame/philips/mcd212.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/mcd212.cpp), [src/mame/philips/mcd212_video.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/mcd212_video.h), [src/mame/philips/mcd212_control_stream.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/mcd212_control_stream.h), [src/mame/philips/cdi.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdi.cpp).

Tests: [tests/emu/philips/mcd212_video.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/mcd212_video.cpp), [tests/emu/philips/mcd212_control_stream.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/mcd212_control_stream.cpp).

Documentation: [docs/cdi_modernization_status.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_modernization_status.md).

[MCD]: #mcd

### DVC

Source: [src/mame/philips/cdidvc.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_mpeg_format.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc_mpeg_format.h), [src/mame/philips/cdidvc_utils.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc_utils.h), [3rdparty/pl_mpeg/pl_mpeg.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/3rdparty/pl_mpeg/pl_mpeg.h).

Tests: [tests/emu/philips/cdidvc.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc.cpp), [tests/emu/philips/cdidvc_audio_format.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_audio_format.cpp), [tests/emu/philips/cdidvc_audio_reference.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_audio_reference.cpp), [tests/emu/philips/cdidvc_video_conversion.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_video_conversion.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity_campaign.md).

[DVC]: #dvc

### AV

Source: [src/mame/philips/cdidvc.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_fidelity.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc_fidelity.h).

Tests: [tests/emu/philips/cdidvc_timing.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_timing.cpp), [tests/emu/philips/cdidvc_avsync_threshold.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_avsync_threshold.cpp), [tests/emu/philips/cdi_dvc_state_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_dvc_state_integration.cpp).

Documentation: [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md), [docs/cdi_audio_final_certification_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_final_certification_20260906.md).

[AV]: #av

### SAVE

Source: [src/mame/philips/cdidvc.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc.cpp), [src/mame/philips/cdidvc_save_state.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdidvc_save_state.h), [src/mame/philips/cdicdic.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdicdic.cpp), [src/mame/philips/cdislavehle.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdislavehle.cpp).

Tests: [tests/emu/philips/cdidvc_audio_replay.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_audio_replay.cpp), [tests/emu/philips/cdidvc_state_transitions.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdidvc_state_transitions.cpp), [tests/emu/philips/cdi_dvc_state_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_dvc_state_integration.cpp), [tests/emu/philips/cdi_mmu_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_mmu_integration.cpp).

Documentation: [docs/cdi_audio_fidelity_campaign.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_audio_fidelity_campaign.md).

[SAVE]: #save

### SLAVE

Source: [src/mame/philips/cdislavehle.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdislavehle.cpp), [src/mame/philips/cdislavehle_state.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdislavehle_state.h), [src/mame/philips/cdi.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdi.cpp).

Tests: [tests/emu/philips/cdislavehle_commands.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdislavehle_commands.cpp), [tests/emu/philips/cdislavehle_pointer.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdislavehle_pointer.cpp), [tests/emu/philips/cdislavehle_transport.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdislavehle_transport.cpp), [tests/emu/philips/cdislavehle_response_ready.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdislavehle_response_ready.cpp).

Documentation: [docs/cdi_modernization_status.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_modernization_status.md).

[SLAVE]: #slave

### BOARD

Source: [src/mame/philips/cdi.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdi.cpp), [src/mame/philips/cdimono2.h](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/mame/philips/cdimono2.h), [src/devices/cpu/dsp56000/dsp56000.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/src/devices/cpu/dsp56000/dsp56000.cpp).

Tests: [tests/emu/philips/cdimono2.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdimono2.cpp), [tests/emu/philips/cdi_dvc_edge_integration.cpp](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/tests/emu/philips/cdi_dvc_edge_integration.cpp).

Documentation: [docs/cdi_modernization_status.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_modernization_status.md), [docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md](https://github.com/matt-dawidowicz/mame/blob/442e5050846e680ed653fc1297b66df588b314e2/docs/cdi_dma_av_optional_dvc_checkpoint_20260906.md).

[BOARD]: #board

## Recalculate the worksheets

```python
import json
from pathlib import Path
report = json.loads(Path('docs/cdi_verified_status_20260906.json').read_text())
for row in report['subsystems']:
    assert sum(p['weight'] for p in row['packages']) == 100
    for branch in ('canonical', 'audio'):
        assert all(p[branch + '_grade'] in range(5) for p in row['packages'])
        raw = sum(p['weight'] * p[branch + '_grade'] / 4 for p in row['packages'])
        estimate = int((raw + 2.5) // 5) * 5
        assert raw == row[branch + '_raw_percent']
        assert estimate == row[branch + '_percent']
        print(row['subsystem'], branch, estimate)
```

This audit changes documentation and estimates only. F1-F3 remain open code work;
no fix or runtime reproduction is claimed. Future development should tackle F1,
then F2, then the CD-DA/Q reference batch, with each real fix tested and committed.
