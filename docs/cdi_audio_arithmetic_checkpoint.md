# CD-i audio arithmetic checkpoint

Date: 2026-09-06

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

Current campaign continuation base: `2b1d26381ad4dc82ed682eecb8b6e371883572d5`

## Scope

This checkpoint follows the standards-derived XA, MPEG Layer II, CD-DA de-emphasis, and fast-CI milestones. It does not redo those changes. It narrows the remaining attenuation-quantization, DSP-saturation, and silicon-rounding questions while preserving the rule that documented processor architecture is not automatically equivalent to Philips firmware behavior.

The authoritative completion matrix remains `docs/cdi_audio_fidelity_campaign.md`.

## Attenuation coefficient quantization

Green Book IV.6.3 supplies the public semantics already implemented by MAME: four independent paths, bit 7 mute, and a seven-bit nominal one-decibel attenuation setting. The retained 210/05 + VMPEG recording covers steps 0-29 and establishes a DVC slope extremely close to the ideal one-decibel line, but it does not reveal the high-range coefficient table or its internal fixed-point width.

`cdi_audio::quantize_nominal_attenuation_gain` remains a parameterized candidate-grid helper, not production hardware attribution. Q15 is non-zero through 96 dB and zero from 97 dB; Q23 and Q31 remain non-zero at 127 dB. The existing capture cannot distinguish those families because their deviations over 0-29 dB are far below the measurement spread.

The next decisive evidence remains a high-range capture or an authoritative Philips coefficient table. MAME must not replace the nominal transfer with an asserted fixed-point lookup until that exists.

## DSP56001 arithmetic architecture

The VMPEG FMA attenuation control path is known to reach a Motorola DSP56001. The DSP56000 Family Manual documents the core arithmetic geometry independently of Philips firmware:

- 24-bit X/Y data words;
- a 24 x 24 fractional multiplier producing a 48-bit product;
- 56-bit A/B accumulators with eight extension bits;
- convergent rounding (round-to-nearest-even) through the documented rounding operations;
- saturation by the data shifter/limiter when a full accumulator value cannot be represented in a 24-bit destination.

`src/mame/philips/cdiaudio_dsp56001.h` now centralizes these documented architecture facts and provides pure discrimination helpers for nearest-even reduction and 24-bit limiter behavior. Tests pin positive and negative half-way cases, unscaled accumulator-to-word boundaries, and signed 24-bit saturation extrema.

This closes the matrix question "identify accumulator/intermediate widths where documentation permits" for the DSP56001 core itself. It does **not** prove that the Philips FMA attenuation firmware uses a particular 24-bit coefficient format, RND/MPYR/MACR instruction, scaling mode, accumulator move, or saturation stage. Those remain firmware/hardware evidence questions.

## Host PCM saturation and rounding

The floating-point de-emphasis path still uses the deterministic MAME host boundary:

- `cdi_audio::saturate_pcm16`: signed PCM16 saturation;
- `cdi_audio::quantize_pcm16_nearest_away`: nearest conversion with exact half-way cases rounded away from zero.

This differs intentionally from DSP56001 convergent rounding. The distinction is now regression-visible rather than implicit. Neither policy is attributed to CDIC silicon or the physical DAC without evidence.

## XA predictor rounding and saturation

The XA decoder remains on its independently corroborated fixed-point compatibility model: Green Book predictor coefficients scaled by 256, `+128` before the eight-bit arithmetic-floor shift, followed by final PCM16 saturation. Existing adversarial tests pin signed shift semantics, tie behavior, clipping, clipped-history feedback, and the exact software intermediate envelope.

The compatibility-model predictor numerator spans `-21,888,688` through `+21,888,692`, requiring at least 26 signed bits for that software formulation; the pre-clip decoded result spans `-118,271` through `+118,269`, fitting signed 18 bits. These are mathematical properties of MAME's current model, not a statement of CDIC datapath widths.

## Regression additions

`tests/emu/philips/cdi_audio_arithmetic.cpp` now distinguishes three arithmetic families explicitly:

- candidate attenuation coefficient quantization;
- documented DSP56001 nearest-even rounding and 24-bit limiter behavior;
- MAME host PCM nearest-away rounding and saturation;
- XA compatibility predictor arithmetic and bounds.

The separation matters because all three can agree on ordinary samples while differing exactly at half-LSB and clipping boundaries.

## Remaining evidence gates

The following remain deliberately open:

- exact high-range attenuation transfer function or lookup table;
- hardware verification of attenuation settings 30-127;
- attenuation transition waveform and channel-transition timing;
- the Philips FMA firmware coefficient format and exact DSP instruction sequence;
- whether/where that firmware invokes convergent rounding or the DSP limiter;
- CDIC accumulator/intermediate widths;
- MPEG synthesis arithmetic relative to VMPEG silicon;
- physical DAC rounding/saturation/hold behavior;
- retained hardware/reference captures that discriminate the candidate arithmetic models.

The next useful silicon experiment should target samples and coefficients that land exactly around DSP nearest-even half-LSB boundaries and 24-bit limiter thresholds, while the attenuation experiment should continue to prioritize the high register range.
