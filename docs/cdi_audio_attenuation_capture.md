# CD-i high-range attenuation capture protocol

Date: 2026-09-05

Campaign branch: `audio/cdi-fidelity-100-campaign-20260905`

## Purpose

The retained CD-i 210/05 + VMPEG attenuation recording establishes the public four-path behavior and an approximately ideal one-decibel slope for steps 0-29.  It does **not** discriminate the internal coefficient width or the high-range floor.  This protocol targets the smallest set of measurements that can reject candidate fixed-point families without changing production MAME behavior first.

`scripts/cdi-attenuation-probe.py` emits deterministic CSV predictions from the Green Book nominal curve for candidate unsigned Q-format grids.  The script is evidence tooling only.  Its candidates are hypotheses to test, not hardware attribution.

## Primary discriminator: first zero coefficient

Nearest-quantized candidate grids produce a six-decibel staircase in the first attenuation value whose coefficient becomes zero:

| Candidate | First zero |
| --- | ---: |
| Q12 | 79 dB |
| Q13 | 85 dB |
| Q14 | 91 dB |
| Q15 | 97 dB |
| Q16 | 103 dB |
| Q17 | 109 dB |
| Q18 | 115 dB |
| Q19 | 121 dB |
| Q20 | 127 dB |
| Q21-Q24 | no zero through 127 dB |

This makes the upper half of the register range much more informative than another dense measurement sweep below 30 dB.

## Recommended measurement sequence

1. Use a stable full-scale or otherwise precisely characterized sinusoidal source and record an unattenuated reference through each route being tested.
2. Measure coarse discriminator points at 78/79, 84/85, 90/91, 96/97, 102/103, 108/109, 114/115, 120/121, and 126/127 dB.
3. Around any observed floor or discontinuity, repeat the adjacent register values and collect enough samples to separate deterministic output from the analogue noise floor.
4. Repeat at least one straight-through route and one cross route.  If the inferred threshold differs by route, stop treating the four paths as one shared quantizer model.
5. Preserve raw captures, source level, analogue gain, interface range, sample rate, player revision, VMPEG/DVC revision, and exact register-write sequence.
6. Do not infer a zero coefficient merely because the measured output falls below the acquisition system's noise floor.

## Dynamic-range warning

With a hypothetical 2 Vrms full-scale analogue output, an ideal 96 dB attenuation leaves only about 31.7 microvolts RMS and 127 dB leaves about 0.89 microvolts RMS.  Ordinary consumer capture hardware may therefore be unable to distinguish a real non-zero coefficient from its own input noise at the most valuable discriminator points.

If the analogue path cannot supply the required dynamic range, prefer a digital/internal observation point or a calibrated low-noise measurement chain rather than declaring the first inaudible step to be the silicon floor.

## Reproducible vector generation

Example:

```text
python scripts/cdi-attenuation-probe.py --verify --start-db 72 --end-db 127 > attenuation-probe.csv
```

The default candidate set is Q12-Q24 plus Q31.  Each row includes the ideal gain and full-scale PCM peak plus the candidate coefficient, reconstructed gain, and reconstructed PCM peak for every selected width.

## Completion rule

Section 10's exact-transfer and every-step hardware gates remain open until a retained capture or authoritative formula selects a model across the complete public range.  A measured first-zero threshold can eliminate many Q-format hypotheses, but it is not by itself proof that the physical implementation is a simple nearest-quantized power-of-two coefficient table.  Transition waveform/timing and channel independence remain separate evidence requirements.