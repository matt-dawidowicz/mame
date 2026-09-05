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
- [ ] Document any VMPEG-specific behavior that differs from conventional MPEG expectations.

100% gate: deterministic parser behavior over the legal profile space plus adversarial malformed vectors, with no known unmodeled VMPEG-specific behavior.

### 2. MPEG Layer II decode path

- [ ] Compare PL_MPEG output against at least one independent MPEG Layer II decoder for synthetic/legal fixtures.
- [x] Validate mono, dual-channel, stereo, and joint-stereo transitions.
- [x] Validate decoder termination at exact frame boundary, partial frame, and end-of-stream.
- [ ] Validate clipping and rounding assumptions and document backend-vs-hardware uncertainty.

### 3. DVC PCM buffering/output

- [ ] Validate queue refill/starvation behavior.
- [ ] Validate exact output-rate changes and stream restart behavior.
- [ ] Validate mute/unmute, flush, and decoder-reset transitions.
- [ ] Add deterministic PCM hashes for representative transition fixtures.

### 4. DVC audio save states

- [ ] Keep current mid-frame replay equivalence test.
- [ ] Add snapshots at frame boundary, partial frame, starvation, decoder flush, sequence end, and stream switch.
- [ ] Add active simultaneous A/V save/load runtime regression.
- [ ] Validate long post-load continuation hashes/timestamps.

### 5. DVC DMA audio ingress

- [ ] Preserve certified live SCC68070 channel-2 integration fixture.
- [ ] Add abort/restart/zero-count/partial-transfer cases.
- [ ] Validate audio decoder feed boundary across DMA completion.

### 6. XA sector routing and coding validation

- [ ] Exhaustive Mode-2 sector routing matrix for file/channel/submode combinations.
- [ ] Exhaustive supported/unsupported XA coding combinations.
- [ ] Validate EOF/EOR and audio/data mixed-sector transitions.
- [ ] Add malformed/contradictory group signaling cases.

### 7. XA ADPCM decode

- [ ] Exhaust predictor/filter/range combinations.
- [ ] Compare decoder output against independent XA decoder/reference vectors.
- [ ] Validate clipping, predictor-history reset, channel interleave, 4-bit/8-bit modes, mono/stereo, and 18.9/37.8 kHz.
- [ ] Resolve exact rounding/saturation rules where observable.

### 8. XA/CDIC timing and buffering

- [ ] Validate double-buffer sequence and IRQ/termination boundaries under sustained audio.
- [ ] Validate sector cadence for every legal coding mode.
- [ ] Validate restart timing after stop/reset/update/read transitions.
- [ ] Measure long-run drift against sector timestamps.

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

- [ ] Map every implemented/read/written AUDCTL bit to documented or measured behavior.
- [ ] Identify unknown bits explicitly.
- [ ] Add bitwise behavioral tests for gating, mute, channel, and source-selection effects.
- [ ] Remove compatibility guesses where hardware evidence contradicts them.

### 12. DAC mute and flush edges

- [ ] Determine sample/frame-boundary semantics on mute, pause, stop, reset, stream replacement, and underrun.
- [ ] Validate whether the last sample is held, zeroed, ramped, or flushed.
- [ ] Add transition captures and regression tests.

### 13. DSP saturation and silicon rounding

- [ ] Identify accumulator/intermediate widths where documentation permits.
- [ ] Build adversarial overflow/underflow vectors.
- [ ] Compare candidate rounding/saturation models against hardware/reference captures.
- [ ] Centralize arithmetic behavior in testable helpers instead of scattered casts/clamps.

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

### 16. CD-DA subcode

- [ ] Validate Q-channel position/status behavior.
- [ ] Validate track/index/time updates during play, pause, seek, lead-in, and lead-out.
- [ ] Add synthetic subcode/reference fixtures where feasible.

### 17. Audio decoder termination, starvation, and stream switching

- [ ] Termination at every meaningful packet/frame boundary.
- [ ] Refill after starvation without duplicate/drop.
- [ ] Rapid stop/start and stream-ID changes.
- [ ] Interactive-FMV branch changes.
- [ ] Simultaneous audio/video state-transition regression.

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

- [ ] Source fixes/refactors with no title-specific hacks.
- [ ] New deterministic unit/regression fixtures.
- [ ] Runtime telemetry only where needed, removed or reduced after evidence is captured.
- [ ] `docs/cdi_audio_fidelity.md` documenting confirmed behavior, inferred models, evidence sources, and unresolved hardware questions.
- [ ] Compatibility matrix for representative XA/CDDA/DVC titles.
- [ ] Final certification report showing every matrix item at 100% or explicitly blocked by unavailable hardware evidence.

## Definition of 100%

For this campaign, 100% does not mean perfection by assertion. It means there is no known unresolved implementation defect in the scoped subsystem, all known edge cases are covered by deterministic regression tests, and hardware-specific behavior is either validated from authoritative evidence or clearly isolated/documented as an evidence limitation rather than silently guessed.
