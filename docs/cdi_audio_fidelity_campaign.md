# CD-i Audio Fidelity Campaign

## Goal

Bring the CD-i audio implementation from high functional completeness to a defensible 100% completion state, where every remaining audio area is either directly validated against authoritative documentation, real hardware, or trusted reference captures, or explicitly documented as an implementation model when hardware attribution cannot yet be proven.

This is a fidelity campaign, not a compatibility-hack campaign. Do not trade correctness for title-specific behavior.

Base branch/commit:

- `cdi-dvc-modernization`
- `f0d78dfbda5c7fbbdb11c3fc1ba25a58e731146f`

Dedicated work branch:

- `audio/cdi-fidelity-100-campaign-20260905`

## Current baseline

Approximate engineering-completion estimates before this campaign:

- DVC MPEG parser: 97%
- DVC MPEG decode: 92%
- DVC PCM output: 90%
- DVC save-state audio: 92%
- DVC DMA audio ingress: 94%
- XA sector routing: 94%
- XA ADPCM decoding: 92%
- XA coding validation: 94%
- XA timing: 85%
- CDIC audio buffering: 95%
- Short-term A/V sync: 82%
- Long-term A/V sync: 63%
- Decoder-clock calibration: 68%
- Attenuation: 80%
- Attenuation quantization: 62%
- AUDCTL fidelity: 60%
- DAC mute/flush edges: 58%
- DSP saturation: 52%
- Silicon rounding: 50%
- De-emphasis: 40%
- CD-DA playback: 85%
- CD-DA exact timing: 70%
- CD-DA subcode fidelity: 60%
- Audio decoder termination: 78%
- Buffer starvation: 72%
- Stream switching: 75%
- Malformed XA group signaling: 60%

These percentages are working estimates of implementation plus evidence completeness, not code-coverage percentages.

## Non-negotiable rules

- Keep DVC/CDIC/XA audio as a permanent regression gate.
- Do not assume host decoder behavior equals VMPEG silicon behavior without evidence.
- Separate confirmed hardware behavior from compatibility/implementation models in comments and docs.
- No title-specific audio hacks unless they reveal and fix a general hardware-model defect.
- Preserve save-state determinism and existing audio replay reconstruction.
- Preserve live DVC-to-SCC DMA semantics and the certified integration fixture.
- Preserve intentional local fast-build/rgbutil/Catch work when operating in the Windows worktree; never reset, stash, or discard those changes automatically.
- Do not interpret Catch 1.7 plus Clang 22 ASan host crashes as production audio defects without independent evidence.

## Completion matrix

### 1. DVC MPEG-1 Layer II parser

- [x] Exhaustively validate legal indexed MPEG-1 Layer II header combinations.
- [x] Validate rejected sync/version/layer/bitrate/sample-rate combinations.
- [x] Add malformed/truncated/resynchronization vectors.
- [x] Verify stream-ID and PES routing interaction for audio packets.
- [x] Accept an audio access point beginning directly at MPEG audio frame sync as well as at a pack start code.
- [x] Document Full Motion profile behavior that differs from conventional MPEG expectations.

The May 1994 Green Book profile is now kept separate from generic MPEG syntax.
An exhaustive profile oracle covers the Layer II mono/stereo bitrate table,
44.1 kHz-only sampling, reserved private bit, and the two allowed emphasis values.
It also records that free format is forbidden and bitrate/sample frequency cannot
change inside one audio sequence.  MAME diagnoses an out-of-profile initial header
but continues to decode because the physical VMPEG error response is not specified;
that compatibility policy is not presented as hardware behavior.

Green Book IX.5.3.1.5 permits all MPEG audio Stream IDs `C0`-`DF`, and
IX.8.2.4 represents the selected stream as 0-31.  The inherited four-bit FMA
selector incorrectly aliased `Cx` and `Dx` streams.  FMA selection and readback
now retain five bits, FMV retains four, and the production start-code classifier
separates pack start, program end, selected PES, and skipped packet paths.  Tests
exhaust every start-code byte for every legal FMA/FMV selector and every first
PES-header byte for all 32 selected audio streams.  Exact desired/current-register
transition and CSU-event timing remain in section 17.

Green Book IX.5.4.3.2 additionally allows the MPEG Audio Pointer to identify the
first byte of a pack start code or an audio-frame synchronization word.  FMA ingress
now has one production-used, save-stateable access router that detects either form.
A direct frame is consumed by its decoded header length before start-code scanning
resumes, so a coincidental `00 00 01` in compressed payload cannot terminate it.
Regressions exhaust both CRC states and all 65,536 remaining header-bit combinations,
decode four directly accessed frames without byte loss, resume program parsing at
the exact following prefix, and compare continuation from mid-header and mid-frame
state snapshots.

This row meets its 100% gate for the scoped MPEG-1 Layer II parser: no known
standards-visible parser defect remains, every accepted/rejected syntax and access
route has deterministic coverage, and unmeasured VMPEG malformed/profile-error
signaling remains explicitly isolated in the decode/error boundary rather than
invented here.

100% gate: deterministic parser behavior over the legal profile space plus adversarial malformed vectors, with no known unisolated VMPEG-specific behavior. **Met.**

### 2. MPEG Layer II decode path

- [x] Compare PL_MPEG output against at least one independent MPEG Layer II decoder for synthetic/legal fixtures.
- [x] Validate mono, dual-channel, stereo, and joint-stereo transitions.
- [x] Validate decoder termination at exact frame boundary, partial frame, and end-of-stream.
- [x] Validate clipping and rounding assumptions and document backend-vs-hardware uncertainty.

Software/reference gates are complete for this row.  A 100% hardware-fidelity
claim remains blocked by the explicitly recorded VMPEG CRC, free-format,
in-stream rate-change, and DSP-rounding evidence limits.

### 3. DVC PCM buffering/output

- [x] Validate queue refill/starvation behavior.
- [x] Validate legal-profile stream restart and partial-frame discard behavior.
- [ ] Validate exact response to prohibited in-stream output-rate changes.
- [ ] Validate mute/unmute, flush, and decoder-reset transitions.
- [x] Add deterministic PCM hashes for representative transition fixtures.

The production output path now uses the same pure stereo-frame transition helper
as the regression.  Timestamp silence has priority, incomplete pairs are retained,
starvation produces deterministic zero, draining compacts exactly once, and refill
resumes at its first sample.  The retained transition fixture hashes to `fdc1c8bc`.
On a requested stream change the compressed decoder is recreated, preventing an
unfinished old-stream frame from consuming bytes from the new stream, while PCM
that was already decoded remains queued because the physical DAC flush edge is
not specified.  Every legal Full Motion stream remains at 44.1 kHz; VMPEG's exact
response to prohibited 48/32 kHz changes is still an evidence gap.
Green Book IX.5.4.3.1 requires muted output while no decoded frames are available;
the exact VMPEG DAC edge and underflow interrupt timing remain open.

### 4. DVC audio save states

- [x] Keep current mid-frame replay equivalence test.
- [x] Add snapshots at frame boundary, partial frame, starvation, and backend flush/end-marker boundaries.
- [x] Preserve pending/current/end stream-control state across a deterministic snapshot.
- [ ] Add full program-sequence-end and stream-switch device snapshots.
- [ ] Add active simultaneous A/V save/load runtime regression.
- [ ] Validate long post-load continuation hashes/timestamps.

The save image now records both PL_MPEG's input-end marker and whether its opaque
buffer has already observed that end.  Postload reapplies the marker after journal
replay and recreates the terminal failed decode only when it occurred live.  Tests
cover a three-byte pre-header, exact frame boundary, partial following frame,
starvation/refill, observed end, unobserved signalled end, and reopening after end.

### 5. DVC DMA audio ingress

- [ ] Preserve certified live SCC68070 channel-2 integration fixture.
- [ ] Add abort/restart/zero-count/partial-transfer cases.
- [ ] Validate audio decoder feed boundary across DMA completion.

### 6. XA sector routing and coding validation

- [x] Exhaustive Mode-2 sector routing matrix for file/channel/submode combinations.
- [x] Exhaustive supported/unsupported XA coding combinations.
- [x] Validate EOF/EOR and audio/data mixed-sector transitions.
- [x] Add malformed/contradictory group signaling cases.

The sector-level portion now exhausts 67,108,864 file/channel/submode/channel-mask
states, all 256 coding bytes, and every unequal pair of duplicated subheader byte
values.  Sound-group validation checks every redundant parameter position and all
256 values in each position.  Direct CD-i 210/05 measurements establish the copies
selected by the CDIC: bytes 12-15 for 8-bit groups and bytes 4-7 plus 12-15 for
4-bit groups.  A disagreement is therefore diagnostic rather than a reason to mute
an otherwise valid selected parameter.  Reserved values in the selected copy retain
their duration as silence without advancing predictor history; that concealment and
any physical error/status signal remain explicit implementation/evidence limitations.

### 7. XA ADPCM decode

- [x] Exhaust predictor/filter/range combinations.
- [x] Compare decoder output against independent XA decoder/reference vectors.
- [x] Validate clipping, predictor-history reset, channel interleave, 4-bit/8-bit modes, mono/stereo, and 18.9/37.8 kHz.
- [ ] Resolve exact rounding/saturation rules where observable.

The software/reference portion covers 813,888 legal predictor input states, all raw
4-bit and 8-bit code expansions, consecutive-group history, both channel layouts,
both sampling-rate codes, and group-buffer boundaries.  A deterministic two-sector
4-bit stereo fixture produces exactly the same 16,128 PCM bytes as FFmpeg's independent
fixed-point `adpcm_xa` decoder.  The Green Book defines the coefficients, ranges,
16-bit output, and final clipping, but not the CDIC's internal accumulator width or
rounding circuit.  The existing `+0.5` fixed-point model is independently corroborated
by FFmpeg; it is not yet promoted to silicon-confirmed behavior.

### 8. XA/CDIC timing and buffering

- [x] Validate double-buffer sequence and IRQ/termination boundaries under sustained audio.
- [x] Validate sector cadence for every legal coding mode.
- [x] Validate Mode-2 filter-update latching and replacement-read ingress restart against hardware captures.
- [x] Prove the HLE sample/sector clock has zero arithmetic drift over at least 30 minutes.
- [ ] Validate stop/reset command timing through the hidden DAC and predictor boundaries.
- [ ] Measure real-media, host-output, and physical-hardware long-run drift against sector timestamps.

Mono-I captures establish first/second CD-fed XA buffers `$2800`/`$3200`, the
per-buffer sound-map ABUF boundary, read-to-clear termination, and interrupt-masked
abort behavior.  The save-stateable transport tests cover initial wait, alternating
refill, starvation, restart, `$ff`, and abort state.  A second retained capture proves
that FILE/CHAN/ACHAN changes become active through command `$2e`.  Because each channel
is observed for an even 20 sectors, that trace cannot distinguish a buffer reset from
continuous alternation and MAME does not invent one.  A replacement Mode-2 read does
prove that its first selected audio sector returns to `$2800`.  MAME now latches
programmed filters at `$2e` and new-read boundaries, while only a new read restarts the
CD-fed ingress halves.  It does not clear AUDCTL playback, queued host PCM, or predictor
history whose physical edge is not established.

For all 16 coding-field-valid bytes, the decoded samples and 37.8/18.9 kHz clock
produce exactly the corresponding 2/4/8/16 sector periods at 75 Hz.  A 135,000-tick
regression (30 minutes) drives the production buffer state in the device's timer order
and proves zero sample-count error and no duplicate/drop at exact refill boundaries.
That closes software arithmetic drift, not physical drive/host-output drift.  Exact
timer phase, seek latency, stop/reset DAC behavior, predictor reset, and real-media
long-run measurements remain open, so this area is not 100%.

### 9. A/V synchronization and decoder clock

- [ ] Instrument audio sample clock against SCR/PTS/DCLK.
- [ ] Run at least a 30-minute continuous MPEG A/V fixture/title with drift telemetry.
- [ ] Run repeated interactive FMV scene transitions.
- [ ] Establish an acceptable drift threshold from the MPEG/CD-i timing model rather than visual judgment.
- [ ] Prove no monotonic drift accumulation across resets, seeks, pause/continue, and stream changes.

### 10. Attenuation and quantization

- [x] Identify authoritative attenuation register semantics.
- [ ] Derive the exact transfer function or lookup table.
- [ ] Verify every register step with hardware measurements or a documented formula.
- [x] Add exhaustive amplitude test vectors.
- [ ] Verify channel independence and transition behavior.

Green Book IV.6.3 fixes the four public paths (LL, LR, RR, RL), bit-7 mute,
and the seven-bit nominal one-decibel setting.  MAME now has a shared nominal
transfer/matrix helper and exhaustive coverage of all 256 byte values plus all
four paths at every non-mute setting.  The FMA DSP indirect transaction is no
longer telemetry-only: a retained `madriv` trace fixes mode `$80`, target `$93`,
and wire order RR, LR, RL, LL.  Its state is saved, partial transfers resume
exactly, and each active path write advances the sound stream before changing gain.

A retained physical 210/05+VMPEG recording verifies all four routes and steps
0-29.  The measured DVC slope is within 0.0019 dB of an ideal one-decibel line;
the concurrent CDIC measurement remains within 0.30 dB.  This does not establish
the unrecorded high-range coefficient table, exact transition waveform, DSP
accumulator/rounding, or analogue floor.  Philips also documents different ADPCM
attenuator anomalies by player generation.  Those hardware-specific gaps keep
the combined attenuation/quantization area below 100%; the nominal semantics and
known DVC register-path defect are closed without disguising the remaining evidence
limit.

### 11. AUDCTL fidelity

- [x] Map every implemented/read/written AUDCTL bit to documented or measured behavior.
- [x] Identify unknown bits explicitly.
- [x] Add bitwise behavioral tests for gating, mute, channel, and source-selection effects.
- [x] Remove compatibility guesses where hardware evidence contradicts them.

Bit 13 gates ABUF interrupts, bit 11 owns playback, and bit 0 is the `$ff`
termination latch.  The register has no measured mute or channel selector; those
functions belong to the separate SLAVE/attenuation path.  All 65,536 write values,
both termination-latch states, and active/idle source states are covered.  Reset
`$c7fe` and post-write `$d7fe` fixed-bit behavior are retained from service/manual
test paths and Mono-I captures.  The cause of the observed bit-12 transition remains
unknown and is not assigned a fabricated function.  AUDCTL itself meets this matrix
gate; DAC queue/flush behavior controlled through it remains separately open below.

### 12. DAC mute and flush edges

- [ ] Determine sample/frame-boundary semantics on mute, pause, stop, reset, stream replacement, and underrun.
- [ ] Validate whether the last sample is held, zeroed, ramped, or flushed.
- [ ] Add transition captures and regression tests.

### 13. DSP saturation and silicon rounding

- [ ] Identify accumulator/intermediate widths where documentation permits.
- [x] Build adversarial overflow/underflow vectors.
- [ ] Compare candidate rounding/saturation models against hardware/reference captures.
- [x] Centralize arithmetic behavior in testable helpers instead of scattered casts/clamps.

The checked lines currently cover the XA ADPCM path.  MPEG synthesis and attenuation
arithmetic remain separate work, and no checked line implies that CDIC silicon widths
or rounding have been measured.

### 14. De-emphasis

- [x] Identify all CD-i/XA/CDDA emphasis signaling paths.
- [x] Implement the standards-defined de-emphasis response.
- [x] Validate frequency response against the relevant standard.
- [ ] Validate enable/disable transition behavior.
- [x] Add deterministic filter tests.

The standards-visible signal path is now closed.  XA coding bit 6, CD-DA Q-control
bit 0, and each MPEG Layer II frame's two-bit emphasis field independently reach
one shared 50/15-microsecond response.  MPEG reserved/J.17 values and profile-invalid
rates remain diagnostic rather than acquiring an invented CD-i response.  CD-DA
playback and synthesized current/TOC Q data use the image's actual track control
flags.  Filter state and a pending pre-start CD-DA sector's emphasis flag are saved.

The 44.1 kHz biquad is the independently published SoX IEC 60908 fit.  Equivalent
high-shelf fits at the two exact XA rates follow the same continuous-time target.
The deterministic response sweep bounds maximum deviation to 0.06225 dB at
44.1 kHz through 20 kHz, 0.07212 dB at 37.8 kHz through 0.475 Nyquist, and
0.07831 dB at 18.9 kHz through 0.475 Nyquist.  Tests also exhaust all 65,536
unemphasized PCM16 values, all MPEG emphasis/rate activation combinations, all
CD-DA ADR/control bytes, per-frame PL_MPEG emphasis changes, rate changes, reset,
clipping, and exact continuation from copied save state.

The unchecked line is deliberately physical: no reviewed document or capture
establishes whether a particular CDIC/VMPEG/player revision switches an analogue
network, resets digital history, or uses another ramp at an emphasis-bit edge.
MAME continuously primes its digital filter while bypassed, resets it only at an
output-rate/decoder reset boundary, and documents that deterministic transition as
a compatibility model.  Consequently section 14 is not promoted to an overall
100% hardware-fidelity claim, even though no known standards-visible implementation
defect remains.

### 15. CD-DA playback and transport timing

- [ ] Validate play/pause/resume/stop/seek behavior.
- [ ] Validate track/index transitions and lead-in/lead-out handling.
- [ ] Validate sample start/stop alignment and seek-latency model.
- [ ] Exercise mixed-mode discs.

The known CDIC defects in this path are closed: CD-DA PCM bypasses CDIC RAM,
playback waits for bit 11, 588 stereo frames are submitted per 1/75-second sector,
and a buffer/subcode event is delivered for every sector rather than once per second.
The unchecked transport/track/seek edges prevent a 100% claim.

### 16. CD-DA subcode

- [ ] Validate Q-channel position/status behavior.
- [ ] Validate track/index/time updates during play, pause, seek, lead-in, and lead-out.
- [ ] Add synthetic subcode/reference fixtures where feasible.

Mono-I captures place Q data at byte offset `$924` in each alternating data buffer
and show a 75 Hz delivery cadence; MAME now matches those two facts.  Current-sector
and audio-track TOC entries preserve the image's Q control/ADR value, including the
pre-emphasis bit.  Position, track/index, and lead packets remain synthesized, and
R-W, pause, seek, lead-in/lead-out, and multi-session behavior remain unresolved.

### 17. Audio decoder termination, starvation, and stream switching

- [x] Termination at every meaningful software-visible packet/frame boundary.
- [x] Refill after starvation without duplicate/drop.
- [x] Rapid stream-ID changes, cancellation, and current-stream commit.
- [ ] Device-level rapid stop/start transitions.
- [ ] Interactive-FMV branch changes.
- [ ] Simultaneous audio/video state-transition regression.

The CDIC portion has deterministic `$ff`, interrupt-masked abort, immediate
replacement, XA double-buffer starvation/refill, and pre-start CD-DA coverage.
The DVC portion now covers queue drain/starvation/refill, PL_MPEG end-marker
reconstruction, and a parser-level ISO-end latch that cannot be reopened by trailing
input before an abort.  Program-stream packet selection distinguishes all 32 Green
Book audio streams without `Cx`/`Dx` aliasing.  Unselected FMA PES headers retain
their packet boundary so an in-flight selector change stops old-stream ingress
immediately and starts the requested stream at the next accepted frame sync.  The
requested stream changes immediately; the current stream and CSU event commit only
when the decoder accepts that requested header.  A compressed-backend restart drops
partial old-frame bytes but deliberately preserves already decoded PCM.

The software-visible termination and stream-ID transition rows are closed.  Full
section completion remains blocked by device-level stop/start and simultaneous A/V
branch snapshots, physical confirmation of the private `$e03008/$e0300a` mapping
and CSU edge, and the exact VMPEG DAC flush/hold/ramp rule.

## Evidence hierarchy

Prefer evidence in this order:

1. Philips, Motorola, MPEG, Red Book, and Green Book technical documentation where legally accessible.
2. Reproducible measurements from real CD-i hardware.
3. Multiple independent emulator/reference implementations where hardware data is unavailable.
4. Controlled synthetic experiments.
5. Compatibility inference, explicitly labeled as such.

Do not call an item 100% solely because all current games appear to work.

## Required regression gates

Before merging any substantive audio batch:

- DVC audio tests.
- CDIC/XA tests.
- DVC save/replay audio tests.
- SCC68070 DMA tests.
- Live DVC-to-SCC DMA integration fixture.
- All Philips-tagged mametests.
- Complete native mametests before milestone folds.
- `cdivalidate -validate` and `cdivalidate -validate cdimono2` at milestone folds.
- `git diff --check`.
- Targeted Clang/ASan where meaningful, accounting for known Catch 1.7 Windows handler incompatibilities.

## Deliverables

- [x] Source fixes/refactors with no title-specific hacks.
- [x] New deterministic unit/regression fixtures.
- [ ] Runtime telemetry only where needed, removed or reduced after evidence is captured.
- [x] `docs/cdi_audio_fidelity.md` documenting confirmed behavior, inferred models, evidence sources, and unresolved hardware questions.
- [ ] Compatibility matrix for representative XA/CDDA/DVC titles.
- [ ] Final certification report showing every matrix item at 100% or explicitly blocked by unavailable hardware evidence.

## Definition of 100%

For this campaign, 100% does not mean perfection by assertion. It means there is no known unresolved implementation defect in the scoped subsystem, all known edge cases are covered by deterministic regression tests, and hardware-specific behavior is either validated from authoritative evidence or clearly isolated/documented as an evidence limitation rather than silently guessed.
