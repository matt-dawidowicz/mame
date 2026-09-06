# CD-i audio arithmetic checkpoint

Date: 2026-09-06

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

Campaign continuation base: `2b1d26381ad4dc82ed682eecb8b6e371883572d5`

## Scope

This checkpoint follows the standards-derived XA, MPEG Layer II, CD-DA de-emphasis, and fast-CI milestones. It does not redo those changes. It records the evidence and implementation state for attenuation quantization, DSP saturation, and silicon rounding while preserving the rule that documented processor architecture is not automatically equivalent to Philips firmware behavior.

The authoritative completion matrix remains `docs/cdi_audio_fidelity_campaign.md`.

## VMPEG FMA attenuation coefficient recovery

Green Book IV.6.3 supplies the public semantics already implemented by MAME: four independent paths, bit 7 mute, and a seven-bit nominal one-decibel attenuation setting. The retained 210/05 + VMPEG analogue recording covers steps 0-29 and establishes a DVC slope extremely close to the ideal one-decibel line.

The campaign now also has a stronger digital result from the retained genuine VMPEG FMA DSP loader material. The initialization stream is a fully conserved 16,434-byte P/X/Y record stream containing one 512-word P record plus X/Y data records. Within the retained DSP data image, the complete 128-entry attenuation sequence matches, with zero mismatches:

`round(2^22 * 10^(-dB/20))`, for `dB = 0..127`.

Representative entries are:

- 0 dB: `0x400000`;
- 1 dB: `0x390A41`;
- 6 dB: `0x201374`;
- 20 dB: `0x066666`;
- 40 dB: `0x00A3D7`;
- 80 dB: `0x0001A3`;
- 120 dB: `0x000004`;
- 127 dB: `0x000002`.

This resolves the FMA coefficient *representation and full digital transfer table*, including the previously unmeasured high attenuation range. It does not establish the analogue output floor of a particular player or transfer the result to Mono-I CDIC.

`src/mame/philips/cdiaudio.h` now stores the full table literally as `FMA_ATTENUATION_Q22`, so steady-state FMA mixing does not depend on host `libm` or regenerate firmware constants at runtime. Bit 7 still produces exact zero.

`scripts/cdi-fma-dsp-evidence.py` makes the finding reproducible without checking copyrighted firmware into MAME. It can emit the expected curve, scan an extracted 24-bit P/X/Y word file for the exact contiguous 128-word sequence, and classify a supplied DSP56001 disassembly for `MPY`, `MPYR`, `MAC`, `MACR`, `RND`, and scaling-control candidates.

## DSP56001 arithmetic architecture

The VMPEG FMA attenuation control path reaches a Motorola DSP56001. The DSP56000 Family Manual documents the core arithmetic geometry independently of Philips firmware:

- 24-bit X/Y data words;
- a 24 x 24 fractional multiplier producing a 48-bit product;
- 56-bit A/B accumulators with eight extension bits;
- convergent rounding (round-to-nearest-even) through the documented rounding operations;
- saturation by the data shifter/limiter when a full accumulator value cannot be represented in a 24-bit destination.

`src/mame/philips/cdiaudio_dsp56001.h` centralizes these architecture facts and provides pure discrimination helpers for nearest-even reduction and 24-bit limiter behavior. Tests pin positive and negative half-way cases, unscaled accumulator-to-word boundaries, and signed 24-bit saturation extrema.

The retained P/X/Y loader establishes 24-bit program/data transport and the recovered Q22 table. Full instruction-path recovery is still incomplete, so a specific `RND`, `MPYR`, or `MACR` instruction at the attenuation output stage is not claimed as an observed Philips firmware fact.

## FMA fixed-point execution model

The production DVC FMA mixer now executes in an explicit integer domain instead of multiplying host doubles by `pow()`-derived gains on every output frame:

1. recover the exact signed-16-bit decoded PCM value from the DVC normalized sample;
2. multiply each source by its firmware-derived Q22 path coefficient;
3. sum the two path products in signed 64-bit storage;
4. reduce Q22 once with DSP56001 convergent nearest-even semantics;
5. saturate only at the final signed-16-bit PCM output boundary;
6. return the normalized sample to MAME's sound stream.

The arithmetic envelope is important. With two signed-16-bit inputs and coefficients no greater than `0x400000`, the worst possible pre-reduction magnitude is exactly `2^38`. That is far below a signed 56-bit DSP56001 accumulator. Therefore an accumulator-overflow saturation event is mathematically unreachable in this HLE matrix. Applying a fabricated 24-bit accumulator clamp would be less accurate, not more accurate.

The DSP's 24-bit data shifter/limiter remains architecture-real, but whether Philips' complete FMA program moves this attenuation accumulator through a limiter before final output remains an instruction-stream question. The HLE therefore does not invent an intermediate 24-bit clip. It enforces the observable signed-16-bit PCM boundary explicitly.

## Rounding status

Three distinct policies remain regression-visible:

- VMPEG FMA Q22 reduction: convergent nearest-even, matching the documented DSP56001 rounded arithmetic family;
- MAME filtered PCM host conversion: nearest with half-way values away from zero;
- XA predictor compatibility arithmetic: `+128` followed by arithmetic-floor shift and final PCM saturation.

The new FMA regression pins positive and negative exact-half Q22 values so a future instruction-level DSP trace can distinguish the current convergent model from truncation or nearest-away immediately.

The coefficient table is firmware evidence. The exact *instruction* selecting convergent reduction is still not recovered; consequently the code and documentation call this an architecture-constrained FMA execution model rather than falsely claiming an observed `RND/MACR` instruction.

## CDIC remains separate

Mono-I CDIC attenuation is not routed through the VMPEG DSP56001 and does not inherit the FMA Q22 table. Its public attenuation semantics remain Green Book-backed, while its internal coefficient width, accumulator geometry, and silicon rounding are still unknown.

The XA decoder remains on its independently corroborated fixed-point compatibility model: Green Book predictor coefficients scaled by 256, `+128` before the eight-bit arithmetic-floor shift, followed by final PCM16 saturation. Existing adversarial tests pin signed shift semantics, tie behavior, clipping, clipped-history feedback, and the exact software intermediate envelope.

The compatibility-model predictor numerator spans `-21,888,688` through `+21,888,692`, requiring at least 26 signed bits for that software formulation; the pre-clip decoded result spans `-118,271` through `+118,269`, fitting signed 18 bits. These are mathematical properties of MAME's current model, not a statement of CDIC datapath widths.

## Regression additions

The arithmetic campaign now covers:

- the complete 128-entry FMA Q22 coefficient table against the nominal formula;
- representative literal constants from 0 through 127 dB;
- all 128 mute-bit encodings;
- straight-through channel routing;
- high-range coefficients that remain non-zero through 127 dB;
- positive and negative convergent half-way cases;
- the exact worst-case two-input Q22 accumulator envelope;
- positive and negative final PCM saturation;
- exhaustive signed-16-bit round-trip through the normalized DVC wrapper;
- documented DSP56001 nearest-even and 24-bit limiter behavior;
- MAME host PCM nearest-away rounding and saturation;
- XA compatibility predictor arithmetic and bounds.

## Remaining evidence gates

The implementation defects that can be closed from the currently retained evidence have been addressed. The remaining items are evidence limits rather than known HLE arithmetic bugs:

- recover the exact Philips FMA program instruction path from coefficient fetch through output;
- identify its scaling-mode state and confirm the exact rounded/unrounded multiply/accumulate opcode sequence;
- confirm whether a full-accumulator move invokes the DSP56001 24-bit limiter on that path;
- obtain an analogue/high-dynamic-range capture if the physical output floor and transition waveform are to be attributed to a specific VMPEG/player revision;
- determine Mono-I CDIC accumulator/intermediate widths and coefficient quantization from CDIC-specific evidence;
- determine physical DAC hold/flush/ramp behavior at stop, mute, replacement, and underrun edges.

These cannot be manufactured from the Green Book or the DSP56001 architecture manual. The repository now contains the code and probe needed to discriminate them as soon as an instruction dump or hardware capture is supplied.
