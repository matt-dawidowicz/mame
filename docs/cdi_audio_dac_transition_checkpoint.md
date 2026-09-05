# CD-i DAC transition checkpoint

Date: 2026-09-05

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

## Scope

Section 12 asks what the physical CD-i audio path does at mute, pause, stop, reset, stream replacement, and underrun.  Available evidence does not yet establish whether a given CDIC/VMPEG/player revision holds its last sample, outputs zero immediately, ramps, or flushes queued data.  MAME must therefore keep those hardware questions open.

This checkpoint makes the **current emulator policies** deterministic and regression-visible so future hardware captures can be compared against a precise baseline rather than an implicit implementation.

## DVC/VMPEG output policy currently implemented

`cdi_dvc::take_audio_output_frame` is the production-used queue transition helper.

- Timestamp-scheduled silence has priority over queued decoded PCM and emits a zero stereo frame without consuming PCM.
- A complete stereo pair is consumed atomically.
- An empty queue emits a zero stereo frame classified as starvation.
- An incomplete stereo pair also emits starvation zero and retains the unmatched sample.
- Once its partner arrives, the retained sample is emitted as the first channel of the resumed stereo frame.
- A compressed stream replacement recreates the decoder backend but deliberately preserves PCM that was already decoded because no physical VMPEG DAC/FIFO flush edge has been established.
- A full audio-output reset clears the decoded queue and wait state after synchronizing the MAME sound stream; this remains an emulator reset policy rather than a measured DAC waveform.

The regression in `tests/emu/philips/cdi_audio_arithmetic.cpp` now pins scheduled-silence priority, complete-pair drain, empty/incomplete starvation zero, and exact refill continuity.

## CDIC realtime policy currently implemented

`cdic_hle::stop_realtime_audio` currently disables realtime consumption and clears the in-flight period counter, but it does **not** erase ready double-buffer halves or change `next_play`.  Re-enabling playback can therefore resume from the retained next ready half.

The new regression pins that retention behavior.  It is useful for detecting accidental queue loss while investigating the physical stop edge, but it must not be interpreted as proof that the analogue DAC holds a sample or that CDIC silicon preserves an internal FIFO identically.

## What remains unmeasured

The following physical transition questions are still open:

- attenuation bit-7 mute assertion/deassertion: exact sample edge and hold/zero/ramp behavior;
- DVC pause/continue: whether queued decoded PCM is held, drained, or flushed;
- DVC stop/reset: exact relationship among compressed decoder reset, decoded PCM queue, DSP state, and DAC output;
- DVC stream replacement: whether already-decoded PCM survives on hardware;
- DVC underrun: whether the last sample is held, zero is inserted, a ramp occurs, or an underflow status edge precedes the audible transition;
- CDIC AUDCTL stop/start: whether ready halves, predictor history, de-emphasis state, or a hidden DAC latch are preserved;
- CDIC command resets and replacement reads: exact DAC/predictor clearing boundary;
- CD-DA pause/stop/seek transitions at the sample boundary.

## Capture matrix

A hardware capture intended to close section 12 should use a non-zero, phase-known waveform immediately before each transition and record enough pre/post samples to classify the boundary unambiguously.  At minimum, capture these cases independently:

| Source | Transition | Current MAME queue policy | Hardware result needed |
| --- | --- | --- | --- |
| DVC MPEG | natural underrun | zero frames until refill | hold / zero / ramp / other |
| DVC MPEG | stream ID replacement | preserve decoded PCM | preserve / flush / partial |
| DVC MPEG | decoder reset | clear output queue | exact sample edge |
| DVC MPEG | pause / continue | control path pending evidence | exact sample edge + queue disposition |
| DVC MPEG | attenuation mute / unmute | gain path changes at stream update | transition waveform |
| XA/CDIC | AUDCTL stop / start | ready halves retained | queue + DAC disposition |
| XA/CDIC | command reset | transport boundary known, hidden DAC edge open | DAC + predictor disposition |
| CD-DA | pause / resume / stop | transport model incomplete | exact sample/sector boundary |

For each case retain raw PCM capture, command/register timestamp, player revision, DVC revision where applicable, source waveform, and the emulator trace generated from the same transition sequence.

## Completion status

This checkpoint does not mark section 12 complete.  It closes a prerequisite: MAME's currently implemented transition behavior is now explicit enough to compare sample-for-sample against future captures.  The three section-12 hardware checkboxes remain open until those captures establish the physical edge semantics.