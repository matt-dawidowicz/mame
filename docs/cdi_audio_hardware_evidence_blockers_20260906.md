# CD-i Audio Hardware Evidence Blockers — 2026-09-06

This document separates the remaining CD-i audio campaign work that cannot be closed responsibly by additional software inference alone. The implementation and regression matrix in `docs/cdi_audio_fidelity_campaign.md` is intentionally conservative: where public documentation, existing retained captures, and independent reference implementations stop short of observable silicon behavior, MAME keeps a deterministic implementation model and records the missing evidence instead of inventing a hardware rule.

These items are **evidence blockers, not permission to add compatibility hacks**. Any future code change against them should be traceable to one of the acquisition procedures below.

## 1. Prohibited DVC in-stream output-rate changes

### Open question

The Green Book Full Motion audio profile fixes MPEG Layer II output to 44.1 kHz. MAME can parse syntactically valid 32/48 kHz MPEG headers and diagnoses them as out of profile, but the exact VMPEG response to an in-stream prohibited rate change is not established.

### Evidence required

Use a controlled MPEG stream that begins with a legal 44.1 kHz sequence and then, without FMA abort/reset, presents otherwise-decodable 48 kHz and 32 kHz headers. Capture at least:

- requested/current FMA stream registers;
- FMA status and interrupt status/enable state;
- FMA DCLK before, during, and after the transition;
- whether compressed input continues to be consumed;
- analogue output waveform and muting duration;
- whether a later legal 44.1 kHz frame recovers without reset.

### Decision rule

Do not infer concealment, hard abort, sample-rate retune, or silent continuation from PL_MPEG behavior. Promote a rule only when a retained VMPEG capture or authoritative Philips document identifies the externally visible response.

## 2. DAC mute / flush / hold / ramp edges

### Open question

The software queues are deterministic, but the physical output edge is not. The reviewed material does not establish whether VMPEG or Mono-I CDIC holds the last sample, emits zero immediately, ramps, drains queued PCM, or flushes at a sample/frame boundary for every control transition.

### Transitions to capture

Measure each transition independently:

1. decoder starvation / underrun;
2. FMA stop/reset;
3. FMA stream replacement;
4. MPEG ISO program end;
5. FMV pause / continue where audio remains active;
6. CDIC playback stop/reset;
7. CD-DA pause / resume / stop;
8. de-emphasis enable/disable transition.

### Capture method

Use a deterministic non-zero waveform with a discontinuity marker immediately before the control event. Record digital/register timing and analogue output from the same run. Required observations:

- last non-zero sample before the event;
- first sample after the event;
- any repeated held sample;
- zero insertion duration;
- ramp length/shape if present;
- whether queued samples reappear after resume;
- exact relation to FMA/CDIC status or interrupt edges.

At least two repetitions per edge should agree before changing the model.

## 3. Exact VMPEG DSP56001 FMA arithmetic path

### Open question

The recovered coefficient table and documented DSP56001 geometry constrain MAME's current Q22 arithmetic model, but they do not reveal the exact Philips P-program instruction sequence. In particular, the retained data image does not prove whether Philips used `RND`, `MPYR`, `MACR`, an unrounded multiply/accumulate, a scaling-mode adjustment, or a move through the 24-bit limiter before the observable PCM boundary.

### Evidence required

Preferred evidence, in order:

1. extracted VMPEG DSP56001 P-program image;
2. disassembly of the FMA mixing routine with entry/exit register state;
3. a reference trace exposing accumulator/output values for adversarial half-way cases;
4. physical output captures designed to distinguish nearest-even, truncation, away-from-zero, and limiter behavior.

### Adversarial vectors

Use coefficient/sample combinations that distinguish the candidate rules rather than ordinary full-scale audio. Include:

- positive and negative exact half-way Q22 reductions;
- one-LSB-below and one-LSB-above each half-way case;
- cross-channel sums near positive/negative PCM saturation;
- maximum coefficients with opposite-sign inputs;
- values whose 24-bit-limiter result differs from direct 56-bit-to-PCM reduction.

## 4. Mono-I CDIC silicon rounding and saturation

### Open question

The XA fixed-point output is independently corroborated at the software/reference level, but the IMS66490/CDIC internal accumulator width and final silicon rounding behavior are not publicly established.

### Evidence required

Construct XA sectors whose predictor state and residual code produce values immediately around the signed-16-bit boundaries and around half-way fixed-point reductions. For every vector retain:

- exact XA sound-group bytes;
- predictor history entering the sample;
- expected outputs under each candidate arithmetic rule;
- hardware-observed PCM or a bit-exact trusted capture.

A physical analogue capture is useful only if the acquisition chain can resolve one-LSB distinctions; otherwise prefer a digital service/debug path if one can be identified.

## 5. De-emphasis transition edge

### Open question

The standards-defined 50/15-microsecond frequency response is implemented and tested. What remains unknown is the transition mechanism on a particular CDIC/VMPEG/player revision: analogue switch, continuous digital history, reset history, or ramped switching.

### Evidence required

Use a continuous deterministic tone/noise sequence with emphasis toggled at a known sector/frame boundary. Capture:

- several milliseconds before and after the bit transition;
- both enable and disable directions;
- repeated transitions after different preceding signal histories;
- the corresponding XA/MPEG/CD-DA emphasis signaling source.

Compare candidate responses from the current continuously-primed model, a reset-history model, and an ideal instantaneous analogue shelf switch. Do not select between them from subjective listening.

## 6. CD-DA transport timing and Q subcode

### Open questions

The known CDIC delivery cadence is modeled, but play/pause/resume/stop/seek, exact sample alignment, track/index transitions, lead-in/lead-out, multi-session/mixed-mode behavior, and synthesized Q-channel position packets remain incompletely validated.

### Reference disc

Build or obtain a reproducible disc/image with:

- at least two audio tracks;
- a pregap and non-trivial INDEX 00/01 transition;
- one pre-emphasized audio track;
- a data track for mixed-mode transition coverage;
- known sector-aligned impulses or timestamp-coded PCM at track/seek boundaries;
- if possible, a multi-session layout.

### Measurements

For play, pause, resume, stop, forward/backward seek, track transition, lead-in and lead-out, capture:

- requested LBA and first/last audible sector;
- delay from command to first/last audible sample;
- 75 Hz CDIC buffer/subcode events;
- Q control/ADR, track, index, relative MSF and absolute MSF;
- behavior while paused;
- transition through audio/data boundaries;
- whether pre-emphasis state changes before, at, or after the audible boundary.

Synthetic-disc tests may close software packet-generation bugs, but physical seek/servo latency must remain a model unless measured.

## 7. Physical long-run A/V drift

### Current software result

The software clock-domain arithmetic is now bounded independently of perceptual judgment: exact-rate 25 Hz fixtures remain exact and 30000/1001 boundaries remain within the one-tick 90 kHz / one-tick 45 kHz quantization budget. That proves absence of arithmetic accumulation in the HLE model, not physical oscillator/servo/host-output drift.

### Evidence required

Use a continuous MPEG title/fixture of at least 30 minutes with known SCR, audio PTS, video PTS/DTS and a stable external reference. Record:

- DVC SCR/PTS/DCLK telemetry;
- presented video-frame timestamps;
- emitted audio sample count or externally recorded audio clock;
- wall/reference clock;
- beginning, periodic, and ending A/V offset.

Separate at least these terms in analysis:

- MPEG timestamp arithmetic error;
- VMPEG/CDIC oscillator difference;
- drive/sector-delivery variation;
- host audio-device resampling/buffering;
- video-present scheduling latency.

Do not tune one term to compensate for another without identifying the source.

## 8. Interactive FMV branch transition

### Open question

Generic pause/continue/stop/play and save/load reconstruction are now regression-tested, but those command cycles are not equivalent to a real branching MPEG presentation with meaningful timestamp discontinuities and decode dependencies.

### Evidence required

Create or retain a controlled branching Full Motion fixture (or a title sequence with a reproducible branch) containing:

- at least two branch destinations;
- distinct SCR/PTS epochs or an intentionally documented discontinuity;
- audio and video packets on both branches;
- repeated back-to-back branch choices;
- enough post-branch material to expose cumulative drift.

For each branch record requested/current stream state, SCR/PTS/DCLK before and after, first decoded/presented video frame, first audible audio frame, discarded pre-branch material, and any interrupt/status transitions. Repeating the branch sequence must not accumulate a monotonic timing error.

## 9. Attenuation transition and Mono-I CDIC quantizer

### Open question

The VMPEG FMA digital coefficient table is recovered exactly and low-range physical attenuation has been measured, but the high-range analogue floor/transition waveform and Mono-I CDIC coefficient quantization remain unresolved.

### Evidence required

- VMPEG: sweep stable input through the full 0-127 dB range plus mute, with sufficiently low-noise acquisition to distinguish the final coefficient steps from the analogue floor; capture abrupt setting changes as well as steady plateaus.
- Mono-I CDIC: repeat an equivalent exhaustive or strategically discriminating coefficient sweep and retain raw measurements, not only dB summaries.

Do not copy the VMPEG Q22 table into CDIC merely because the nominal public attenuation API is similar.

## Exit criteria

An evidence blocker can be closed only when all of the following are true:

1. the capture/document identifies the relevant hardware revision and procedure;
2. raw evidence or a reproducible extraction path is retained;
3. the observation distinguishes the implemented rule from plausible alternatives;
4. production code and regression tests are updated together;
5. `docs/cdi_audio_fidelity_campaign.md` records whether the result is hardware-confirmed, standards-derived, independently corroborated, or still an implementation model;
6. the maintained CD-i regression gate is green on the exact code commit.

Until then, the current deterministic implementation remains preferable to an unverified "more realistic" guess.
