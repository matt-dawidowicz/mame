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
- [ ] Verify stream-ID and PES routing interaction for audio packets.
- [x] Document Full Motion profile behavior that differs from conventional MPEG expectations.

The May 1994 Green Book profile is now kept separate from generic MPEG syntax.
An exhaustive profile oracle covers the Layer II mono/stereo bitrate table,
44.1 kHz-only sampling, reserved private bit, and the two allowed emphasis values.
It also records that free format is forbidden and bitrate/sample frequency cannot
change inside one audio sequence.  MAME diagnoses an out-of-profile initial header
but continues to decode because the physical VMPEG error response is not specified;
that compatibility policy is not presented as hardware behavior.

100% gate: deterministic parser behavior over the legal profile space plus adversarial malformed vectors, with no known unmodeled VMPEG-specific behavior.

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
- [ ] Validate exact output-rate changes and stream restart behavior.
- [ ] Validate mute/unmute, flush, and decoder-reset transitions.
- [x] Add deterministic PCM hashes for representative transition fixtures.

The production output path now uses the same pure stereo-frame transition helper
as the regression.  Timestamp silence has priority, incomplete pairs are retained,
starvation produces deterministic zero, draining compacts exactly once, and refill
resumes at its first sample.  The retained transition fixture hashes to `fdc1c8bc`.
Green Book IX.5.4.3.1 requires muted output while no decoded frames are available;
the exact VMPEG DAC edge and underflow interrupt timing remain open.

### 4. DVC audio save states

- [x] Keep current mid-frame replay equivalence test.
- [x] Add snapshots at frame boundary, partial frame, starvation, and backend flush/end-marker boundaries.
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
- [ ] Validate restart timing after stop/reset/update/read transitions.
- [ ] Measure long-run drift against sector timestamps.

Mono-I captures establish first/second CD-fed XA buffers `$2800`/`$3200`, the
per-buffer sound-map ABUF boundary, read-to-clear termination, and interrupt-masked
abort behavior.  The save-stateable transport tests cover initial wait, alternating
refill, starvation, restart, `$ff`, and abort state.  For all 16 coding-field-valid
bytes, the decoded samples and 37.8/18.9 kHz clock produce exactly the corresponding
2/4/8/16 sector periods at 75 Hz.  Timer phase relative to sector arrival, all command
transitions, and long-run timestamp drift remain open, so this area is not 100%.

### 9. A/V synchronization and decoder clock

- [ ] Instrument audio sample clock against SCR/PTS/DCLK.
- [ ] Run at least a 30-minute continuous MPEG A/V fixture/title with drift telemetry.
- [ ] Run repeated interactive FMV scene transitions.
- [ ] Establish an acceptable drift threshold from the MPEG/CD-i timing model rather than visual judgment.
- [ ] Prove no monotonic drift accumulation across resets, seeks, pause/continue, and stream changes.

### 10. Attenuation and quantization

- [ ] Identify authoritative attenuation register semantics.
- [ ] Derive the exact transfer function or lookup table.
- [ ] Verify every register step with hardware measurements or a documented formula.
- [ ] Add exhaustive amplitude test vectors.
- [ ] Verify channel independence and transition behavior.

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

- [ ] Identify all CD-i/XA/CDDA emphasis signaling paths.
- [ ] Implement the correct de-emphasis response.
- [ ] Validate frequency response against the relevant standard or hardware.
- [ ] Validate enable/disable transition behavior.
- [ ] Add deterministic filter tests.

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
and show a 75 Hz delivery cadence; MAME now matches those two facts.  Its Q contents
are still synthesized from one-track assumptions, and R-W, track/index transitions,
lead-in/lead-out, pause, and seek behavior remain unresolved.

### 17. Audio decoder termination, starvation, and stream switching

- [ ] Termination at every meaningful packet/frame boundary.
- [x] Refill after starvation without duplicate/drop.
- [ ] Rapid stop/start and stream-ID changes.
- [ ] Interactive-FMV branch changes.
- [ ] Simultaneous audio/video state-transition regression.

The CDIC portion has deterministic `$ff`, interrupt-masked abort, immediate
replacement, XA double-buffer starvation/refill, and pre-start CD-DA coverage.
The DVC portion now covers queue drain/starvation/refill and PL_MPEG end-marker
reconstruction without duplicate/drop.  Packet-level termination, rapid stream
selection, interactive branching, and exact DAC flush rules remain open.

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
