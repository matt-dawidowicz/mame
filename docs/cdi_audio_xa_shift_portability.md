# CD-i XA arithmetic shift portability

Date: 2026-09-05

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

## Purpose

The XA ADPCM compatibility model uses arithmetic floor division by powers of two in two places: sign-extended sample-range scaling and the predictor numerator after the retained `+128` rounding bias. Historically the implementation expressed both operations with signed C++ right shifts.

On the compilers MAME normally targets, negative signed right shift behaves as an arithmetic shift. That matches the independently corroborated XA compatibility model, but the language expression leaves the intended negative-value rule dependent on implementation behavior rather than stating it directly.

This checkpoint makes the software rule explicit without changing the model or promoting it to a CDIC-silicon claim.

## Defined software rule

`cdic_hle::floor_shift_right` implements:

`floor(value / 2^shift)`

for signed 32-bit values.

Important retained cases include:

- `-3 / 2 -> -2` for an XA range shift;
- `+7.5 -> +8` after the predictor's `+128` bias and `/256` stage;
- `-7.5 -> -7` after the same biased predictor stage.

The predictor coefficients, `+128` bias, final PCM16 saturation, and clipped-history feedback are unchanged.

## Regression scope

The arithmetic regression compares the production helper against an independent quotient/remainder floor-division reference over:

- every signed 16-bit input for shifts 0 through 15, covering the complete domain used by sign-extended XA codes;
- representative signed 32-bit boundaries, including `INT32_MIN` and `INT32_MAX`, for shifts 0 through 31;
- the already retained predictor tie and clipping vectors.

Existing XA reference fixtures and deterministic PCM comparisons remain the end-to-end waveform gate.

## Evidence boundary

This change proves the **MAME compatibility model** is explicit and compiler-independent. It does not establish the physical IMS66490/CDIC accumulator width, internal truncation stages, or hardware tie rule. Those section-13 evidence gates remain open until discriminating hardware or authoritative implementation evidence is available.
