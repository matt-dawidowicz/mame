# CD-i audio arithmetic checkpoint

Date: 2026-09-05

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

Base certified commit for this checkpoint: `bc353e635098595cfb39fa47650943f1698b0188`

## Scope

This checkpoint follows the standards-derived XA, MPEG Layer II, and CD-DA de-emphasis milestone. It does not redo that work. It narrows the remaining attenuation-quantization, DSP-saturation, and silicon-rounding questions without inventing undocumented CDIC or VMPEG arithmetic.

The authoritative completion matrix remains `docs/cdi_audio_fidelity_campaign.md`. No open hardware-evidence checkbox in sections 10 or 13 is promoted to complete by this checkpoint.

## Attenuation coefficient quantization

Green Book IV.6.3 supplies the public semantics already implemented by MAME: four independent paths, bit 7 mute, and a seven-bit nominal one-decibel attenuation setting. The retained 210/05 + VMPEG recording covers steps 0-29 and establishes a DVC slope extremely close to the ideal one-decibel line, but it does not reveal the high-range coefficient table or its internal fixed-point width.

`cdi_audio::quantize_nominal_attenuation_gain` is therefore a parameterized candidate-grid helper, not production hardware attribution. It lets tests ask what a hypothetical Q-format would do while the production mixer continues to use the standards-visible nominal curve.

The deterministic candidate tests establish:

- Q15 is non-zero through 96 dB and quantizes to zero beginning at 97 dB.
- Q23 and Q31 remain non-zero at the public 127 dB endpoint.
- Over the existing measured 0-29 dB interval, the worst Q15 deviation from the ideal line is about 0.003212 dB.
- Over the same interval, the worst Q23 deviation is about 0.000012994 dB.

Those errors are too small for the existing capture to identify a unique coefficient width. A useful follow-up capture must therefore exercise the high attenuation range, especially the region around 96-127 dB, with enough analogue dynamic range to distinguish a quantized floor from the ideal curve. Until that exists, MAME must not replace the nominal transfer with an asserted Q15/Q23/Q31 lookup table.

## Host PCM saturation and rounding

The floating-point de-emphasis path already required a deterministic conversion back to signed PCM16. That boundary is centralized as:

- `cdi_audio::saturate_pcm16`: saturates an integer intermediate to `[-32768, 32767]`.
- `cdi_audio::quantize_pcm16_nearest_away`: nearest conversion with exact half-way cases rounded away from zero, plus deterministic NaN/infinity handling.
- `quantize_deemphasized_pcm16` delegates to the shared helper so the existing standards-derived filter behavior is unchanged.

This is explicitly a MAME host-output policy. It is not evidence that CDIC, VMPEG, the DSP56001 path, or a physical DAC uses the same accumulator width, saturation point, or half-way rule.

## XA predictor rounding and saturation

The XA decoder remains on its independently corroborated fixed-point compatibility model: Green Book predictor coefficients scaled by 256, `+128` before the eight-bit arithmetic shift, followed by final PCM16 saturation. Adversarial tests pin previously implicit boundary behavior:

- an exact `+7.5` predictor tie becomes `+8`;
- an exact `-7.5` predictor tie becomes `-7`;
- a negative odd encoded value shifted by one bit follows arithmetic-floor semantics (`-3 >> 1` behaves as `-2` in the model);
- over-range predictor results saturate before being stored back into predictor history.

The tests make the existing FFmpeg-correlated expectations explicit and reviewable. Production XA arithmetic still uses signed right shift to express the intended arithmetic-floor operation, so compiler portability of that expression is a separate implementation concern; the tests ensure a toolchain that disagrees with the retained model fails rather than silently changing audio. This does not identify the IMS66490/CDIC accumulator width or prove that physical silicon rounds at the same internal stage.

### Software intermediate envelope

The compatibility model now also has a closed mathematical envelope. Evaluating every Green Book predictor coefficient pair at the four PCM16 history corners gives a predictor numerator range of:

- minimum: `-21,888,688`;
- maximum: `+21,888,692`.

Therefore a signed 25-bit intermediate is insufficient for the current software model, while a signed 26-bit intermediate is sufficient before the `/256` predictor stage. Combining the rounded predictor with the full sign-extended XA code gives a pre-clip decoded range of `-118,271` through `+118,269`, which fits a signed 18-bit software intermediate before final PCM16 saturation.

These are mathematical requirements of MAME's compatibility model only. They must not be rewritten as claims that CDIC contains a 26-bit predictor accumulator or an 18-bit post-predictor datapath. A narrower physical datapath with staged truncation or saturation could produce the same ordinary vectors and would need hardware discrimination near the constructed boundaries.

## Regression additions

`tests/emu/philips/cdi_audio_arithmetic.cpp` covers four focused areas:

- `[audio][attenuation][quantization]` for Q-format candidate discrimination and the measured-range ambiguity;
- `[audio][rounding][saturation]` for exhaustive PCM16 identity plus clipping and half-way boundaries;
- `[audio][xa][rounding][saturation]` for predictor ties, signed range shifts, clipping, and clipped-history feedback;
- `[audio][xa][rounding][saturation][bounds]` for the exact compatibility-model predictor and pre-clip intermediate envelope.

## Remaining evidence gates

The following campaign items remain deliberately open:

- exact high-range attenuation transfer function or lookup table;
- hardware verification of every attenuation register step;
- attenuation transition waveform and channel-transition timing;
- CDIC and VMPEG accumulator/intermediate widths;
- physical rounding/saturation stage and tie rule;
- MPEG synthesis arithmetic relative to VMPEG silicon;
- hardware/reference captures that discriminate candidate arithmetic models.

The next hardware-facing attenuation experiment should prioritize high-range steps rather than collecting more points inside 0-29 dB. The next silicon-rounding experiment should use vectors intentionally constructed around half-LSB predictor/attenuator boundaries and clipping thresholds, with a digital or sufficiently low-noise analogue reference capture.