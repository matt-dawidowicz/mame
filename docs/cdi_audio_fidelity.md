# CD-i Audio Fidelity Evidence Ledger

This document records what the audio campaign can claim, why it can claim it,
and where the evidence stops.  The completion matrix and definition of 100%
remain in `docs/cdi_audio_fidelity_campaign.md`.

## Evidence vocabulary

- **Hardware-confirmed**: reproduced on a physical CD-i/VMPEG system with a
  retained procedure or capture.
- **Standards-derived**: required by the relevant MPEG, CD-ROM, or audio
  standard, without claiming an identical silicon mechanism.
- **Independently corroborated**: two or more trustworthy implementations or
  references agree on externally visible behavior.
- **Current implementation model**: deterministic MAME behavior that is needed
  for emulation but is not attributed to physical hardware.
- **Unresolved inference**: plausible behavior that must not be promoted to a
  specification or a 100% claim.

## MPEG audio parser/decode checkpoint (2026-09-05)

The first campaign batch closes two concrete PL_MPEG defects and makes the
accepted header profile explicit.

1. A failed frame-sync scan at an exact input boundary could move the bit index
   one byte beyond the buffer.  On a later streaming refill that out-of-range
   index could underflow the remaining-length calculation and the discard path.
   The scan now stops exactly at the buffer end.  Deterministic tests cover a
   three-byte header fragment, header completion, one-byte-short frame,
   exact-frame refill, end signalling, and no duplicate output.
2. PL_MPEG rejected a legal change of Layer II bitrate between adjacent frames.
   Frame size and allocation tables are selected by each frame header, so the
   decoder now permits bitrate and channel-mode changes while retaining its
   per-instance sample-rate lock.  The regression changes bitrate, padding, and
   all four channel modes across four adjacent frames.
3. The DVC header helper now returns the parsed bitrate/sample-rate indices,
   frame size, CRC presence, padding, channel mode, mode extension, emphasis,
   and an acceptance/rejection classification.  Free format (bitrate index
   zero) is distinguished from the reserved bitrate index, and the reserved
   emphasis value is accepted for robust decoding but labelled explicitly
   rather than silently promoted to a legal profile value.

The focused audio gate at this checkpoint is 1,046,230 assertions in nine test
cases.  The combined DVC/parser/save-replay/reference test binary is 2,235,334
assertions in 32 test cases.  The broader standalone Philips gate is 2,332,516
assertions in 104 test cases, including the unchanged CDIC/XA baseline of 69
assertions in eight test cases.

## Independent PCM reference checkpoint (2026-09-05)

`tests/emu/philips/cdidvc_audio_reference_data.h` retains three independently
encoded, non-silent Layer II frames: stereo, joint stereo, and mono.  The dual
channel vector uses the stereo payload with the independently valid dual-channel
mode header because Layer II uses the same two-channel allocation layout for
those two modes.  Each frame is repeated three times to exercise synthesis
filter history and exact frame transitions.

The source was a deterministic pair of unequal sine mixtures at 44.1 kHz.  The
fixtures were encoded at 192 kbit/s with FFmpeg 6.1.1/libtwolame and decoded to
reference PCM with FFmpeg's separate fixed-point `mp2` decoder.  The retained
reference consists of 64 evenly spaced sample pairs per mode plus whole-output
absolute-energy, square-energy, and peak metrics.  PL_MPEG's floating-point
synthesis is compared with explicit tolerances; this catches wrong allocation,
scale, channel routing, or synthesis output without assuming that two different
numeric implementations must be bit-identical.  No title or game data is
retained.

The retained-frame SHA-256 values are:

- stereo: `1f8013b128e6c9244240d2271421e19ffe0851461ae990bd3bca379ab7c74deb`;
- joint stereo: `a64954ddd5ff0fb91cfed87dc1e0a1f1d98f4d70d73fbba386ff00d9b55cdb82`;
- mono: `f6e9bcaccadb0311f80bf912c17a1e66073f5df78b30f58d883ac4e3fbdad8cf`.

The complete three-frame FFmpeg PCM used to derive landmarks/metrics hashed to
`f92641e164ce0696a843700c7a21266fb64f708a2937aed9356c6c678fb64f32`
(stereo),
`f0237ad3fbfae83390944afc541e082351ec73b9f88fb0e56576e9af8bb1066b`
(joint stereo), and
`094c4e02c8f05efb563a2e1dab80f4c86e4f757e0f6ef47942e648d22065376e`
(mono, one channel).

Across all 3,456 decoded samples per channel, the measured PL_MPEG-to-FFmpeg
maximum/RMS differences were 1,114/389 for stereo and dual channel, 1,388/225
for joint stereo, and 464/217 for mono.  Exact cross-host PCM hashes are not yet
claimed: compiler floating-point contraction can legitimately perturb a low
output bit.  The production float-to-PCM conversion is now centralized and has
exact clipping, symmetric-rounding, infinity-range, and NaN-safe tests.

## Evidence register

| ID | Class | Source | Supported claim | Limit |
| --- | --- | --- | --- | --- |
| MPEG-STD-001 | Standards-derived | [RFC 3003](https://www.rfc-editor.org/rfc/rfc3003), referring normatively to ISO/IEC 11172-3:1993 | MPEG elementary audio is a sequence of independently headed frames and may be interspersed with non-MPEG data, so deterministic resynchronization is required. | RFC 3003 is not a VMPEG hardware specification. |
| MPEG-REF-001 | Independently corroborated | [FFmpeg `mpegaudiodecheader.c` at `ef5929f`](https://github.com/FFmpeg/FFmpeg/blob/ef5929f4ebb158ae689845055513a5725f5de28c/libavcodec/mpegaudiodecheader.c) | Indexed Layer II frame size is recalculated from each frame's bitrate, sample rate, and padding; free format is represented separately when a size cannot be derived from one header. | FFmpeg behavior is reference-decoder evidence, not VMPEG evidence. |
| MPEG-REF-002 | Independently corroborated | [mpg123 `mpeghead.h` at `f6c19f4`](https://github.com/madebr/mpg123/blob/f6c19f46031088efc8d0e5b83305a6f36ceceb65/src/libmpg123/mpeghead.h) and [parser](https://github.com/madebr/mpg123/blob/f6c19f46031088efc8d0e5b83305a6f36ceceb65/src/libmpg123/parse.c) | The ordinary compatible-header mask excludes bitrate, while the parser rejects bitrate index 15 and handles index zero as free format. | mpg123 supports profiles beyond CD-i Full Motion. |
| MPEG-REF-003 | Current implementation model | [PL_MPEG upstream at `c871f2b`](https://github.com/phoboslab/pl_mpeg/blob/c871f2be022ece7ef4f64230b4fb8e1fb9eb6023/pl_mpeg.h) | Documents the inherited decoder architecture and the upstream constant-header restriction from which MAME's streaming fixes diverge. | Upstream PL_MPEG is not an independent hardware model. |
| MPEG-REF-004 | Controlled synthetic experiment | `tests/emu/philips/cdidvc_audio_reference.cpp` and its retained data header | Non-silent mono, dual-channel, stereo, and joint-stereo PL_MPEG output remains within measured bounds of FFmpeg 6.1.1's independent fixed-point Layer II decoder. | This compares software decoders; it does not establish VMPEG DSP rounding. |
| DVC-HW-001 | Hardware-confirmed | [CDi_FMVTest FMA playback-delay capture at `991b9cb`](https://github.com/Slamy/CDi_FMVTest/tree/991b9cb22905942d969a6d3219f89c5e941a7741/fma_playback_delay) | On the recorded 210/05 + VMPEG system, `MA_TRIG_DEC` occurs close enough to analogue sample output to serve as a software timing marker; the retained analysis bounds any additional decode delay to roughly 4 ms after accounting for encoder silence. | One hardware/configuration and one fixture; it does not establish long-duration drift or underflow edges. |
| DVC-RE-001 | Independently corroborated | [MiSTer CD-i DVC notes at `1d0d29b`](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/doc/dvc.md) | FMA DCLK is treated as 45 kHz and the audio clock is the driver-visible link to MPEG SCR timing. | Reverse-engineering notes, not a Philips register specification. |

## Explicit implementation boundaries

- **Free format:** ISO-family reference decoders recognize bitrate index zero as
  free format.  MAME classifies it as `unsupported_free_format`; PL_MPEG cannot
  derive its length by searching for a following compatible header.  Whether
  the CD-i Full Motion profile or VMPEG silicon accepts it remains unproven.
- **Sample-rate changes:** the current PL_MPEG instance locks sample rate so
  MAME's sound stream is never changed underneath queued samples.  A rate change
  currently requires an explicit decoder restart; simple stream selection does
  not yet provide that transition.  Exact VMPEG behavior for an in-stream rate
  change is unknown.
- **CRC:** protection and CRC length are parsed, but PL_MPEG does not validate
  the Layer II CRC.  Error signalling/concealment on VMPEG is unknown.
- **Emphasis:** the two header bits are exposed for evidence gathering.  The
  reserved value has its own `accepted_reserved_emphasis` classification; this
  checkpoint does not call it standards-valid and does not implement MPEG
  de-emphasis.
- **Resynchronization:** accepting a structurally valid indexed header after
  junk is a software streaming model.  The exact VMPEG false-sync and error-event
  policy has not been measured.

## Completion status

No audio area is declared 100% at this checkpoint.  The indexed MPEG-1 Layer II
header space, explicit rejection classes, malformed resynchronization, legal
bitrate/channel-mode transitions, backend exact/partial-frame termination, and
independent non-silent decoder comparison now have deterministic coverage.
Remaining blockers include CRC policy, VMPEG-specific header/update events,
stream-switch edges, and the hardware-dependent matrix areas listed in the
campaign specification.
