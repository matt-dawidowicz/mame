# CD-i DVC A/V sample-clock checkpoint

Date: 2026-09-05

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

## Scope

Section 9 requires the DVC audio sample clock to be compared against MPEG SCR/PTS and VMPEG DCLK before a long-run A/V drift threshold can be certified. The DVC implementation already maintains the necessary timing domains independently:

- MPEG SCR and packet PTS/DTS parsing;
- a 45 kHz FMA/FMV DCLK;
- SCR-to-DCLK anchoring through `mpeg_clock_from_dclk`;
- emitted stereo audio frame accounting in `m_audio_output_frames`;
- current output sample rate;
- audio/video PTS cross-stream telemetry.

This checkpoint adds the missing deterministic conversion and comparison math. It does not change scheduling or claim a hardware servo algorithm.

## Audio sample clock

`cdi_dvc::audio_sample_clock90(anchor90, emitted_frames, sample_rate)` converts the cumulative emitted stereo-frame count into the MPEG 90 kHz timestamp domain.

The calculation splits whole seconds and remainder frames instead of repeatedly adding a rounded per-sample period. As a result, the measurement clock itself cannot accumulate the kind of rounding drift it is intended to detect.

For exact-rate streams:

- 44,100 emitted frames advance exactly 90,000 MPEG ticks;
- 48,000 emitted frames advance exactly 90,000 MPEG ticks;
- 30 minutes at either rate advances exactly 162,000,000 MPEG ticks in the measurement model.

## Unified observation

`cdi_dvc::observe_audio_clock` returns one measurement record containing:

- emitted-sample clock in the 90 kHz MPEG domain;
- sample clock minus SCR-derived clock, in 90 kHz ticks;
- sample clock minus current audio PTS, in 90 kHz ticks;
- sample clock minus raw VMPEG DCLK, in 45 kHz ticks.

Positive deltas mean the emitted audio sample clock is ahead of the reference. Negative deltas mean it is behind. Existing `clock90_delta_microseconds` and `sample_delta_microseconds` helpers can convert captured differences for reports without changing the underlying integer evidence.

## Regression gate

`tests/emu/philips/cdidvc_timing.cpp` now verifies:

- exact agreement at one-second boundaries for 44.1 and 48 kHz;
- zero arithmetic drift after 30 minutes at both rates;
- simultaneous zero delta against SCR, PTS and DCLK when all clocks agree;
- explicit sign/magnitude behavior when the sample clock leads each reference;
- deterministic handling of a zero sample rate;
- existing 33-bit MPEG timestamp and 32-bit DCLK wrap semantics remain separate tested primitives.

## Remaining section-9 work

This checkpoint supplies the measurement primitive, but the campaign's first section-9 checkbox remains deliberately open until the production device emits the unified observation during a real DVC run. The existing runtime device already owns all required inputs, so the remaining integration is a measurement-only hook rather than a new timing model.

After that hook is retained, the evidence campaign still requires:

1. a continuous MPEG A/V run of at least 30 minutes;
2. repeated interactive FMV scene transitions;
3. a drift acceptance threshold derived from the MPEG/CD-i timing model rather than visual judgment;
4. reset, seek, pause/continue, and stream-change runs proving that no monotonic drift accumulates across discontinuities.

No section-9 hardware-fidelity checkbox is promoted solely by the synthetic zero-drift arithmetic test.
