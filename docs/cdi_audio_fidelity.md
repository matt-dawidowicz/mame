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

## XA routing and coding checkpoint (2026-09-05)

The third campaign batch closes concrete standards-visible routing defects without
claiming an undocumented CDIC error response.

1. CD-ROM XA makes Trigger independent of channel allocation, but makes EOF and
   EOR channel-allocated.  The previous helper treated all three bits as global,
   allowing an unselected EOF or EOR sector to be delivered and an unselected EOF
   to stop a read.  The decision now exposes Trigger, selected-channel EOR, and
   selected-channel EOF independently.  An audio sector reaches the audio processor
   only when its channel is selected in both the 32-bit main mask and the 16-bit
   audio mask; a selected audio sector omitted from the audio mask remains host data.
2. Sector conformance is checked before audio allocation.  Channels above 31,
   audio channels above 15, simultaneous Audio/Video/Data bits, audio in Form 1,
   data in Form 2, real-time video in Form 1, malformed empty/message metadata,
   and reserved audio coding values are classified and dropped rather than silently
   routed.  The classifications follow the Green Book's allowed type/form table.
3. One pure coding-byte decoder now owns validation, channel count, sample width,
   sample-clock divisor, emphasis exposure, and playback duration.  Every one of
   the 256 byte values is tested.  Sixteen values are coding-field-valid: the
   Cartesian product of emphasis on/off, 4/8-bit samples, 37.8/18.9 kHz, and
   mono/stereo.  The Green Book names only Levels A, B, and C, so 8-bit/18.9 kHz
   is described as coding-field-valid rather than promoted to a named quality level.
4. Mode 2 subheader metadata is double-written for integrity.  MAME previously
   retried descrambling after a contradiction, then continued with the second copy
   even when both attempts remained invalid.  The XA decoder model requires CIRC
   reliability information to choose a copy; raw-image reads do not provide those
   per-byte flags.  MAME now drops that unresolved Mode 2 sector while allowing the
   physical 75 Hz read position to advance.  It does not fabricate a CDIC status bit
   or interrupt whose hardware mapping is unknown.
5. Headerless CD-DA PCM is now explicitly excluded from Mode 1/2 header validation
   and descrambling.  This removes a data-dependent path that could reinterpret
   ordinary audio samples as a coincidentally valid sector header.

The focused CDIC gate is now 263,516 assertions in 11 test cases.  It compares
67,108,864 routing states against an independent literal-bit oracle, checks all
256 coding bytes, and checks all 261,120 single-field unequal byte pairs across
the four duplicated subheader fields (plus equal and combined-mismatch cases).
Optimized, unoptimized, and AddressSanitizer/UndefinedBehaviorSanitizer builds pass.
The broader standalone Philips gate passes 2,595,963 assertions in 107 test
cases, including the unchanged 1,046,230-assertion DVC audio gate.  MAME's
generated native C++20 `release64` project also compiles the complete focused
CD-i production archive, including CDIC and DVC.  The generated `mametests`
wrapper could not be linked in the campaign container because its OSD support
requires unavailable SDL2 development headers; the same Philips test sources
were compiled and run through the standalone Catch harness above.

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
| XA-STD-001 | Standards-derived | [Philips/Sony CD-i Green Book, May 1994 Release 2](https://archive.org/download/cdi_may94_r2/cdi_may94_r2.pdf), Chapter II sections 4.5/4.9 and Appendix II | Defines double-written subheaders, channel ranges, legal Audio/Video/Data and Form combinations, empty/message restrictions, audio-mask routing, and every audio coding field. | A media/system specification does not identify the private CDIC register response to malformed input. |
| XA-STD-002 | Standards-derived | [Philips/Sony CD-ROM XA System Description, May 1991](https://archive.org/download/xa-10-may-1991/CD-ROM%20XA%20Specification%20May%201991%20-%20print%20to%20pdf%20in%20chrome.pdf), Chapter II sections 4.3 and 6.2 | Trigger is not channel-allocated; EOF/EOR are channel-allocated. The decoder model uses CIRC reliability flags to select trustworthy duplicated subheader bytes. | MAME's raw image interface does not expose those per-byte CIRC flags. |
| XA-REF-001 | Independently corroborated | [jPSXdec duplicated-subheader and coding parser at `fd54036`](https://github.com/m35/jpsxdec/blob/fd5403629ac81aaca0feff0dc89e6cadd6353b26/jpsxdec/src/jpsxdec/cdreaders/CdSectorXaSubHeader.java) | An independent XA extraction tool flags unequal duplicated fields as corruption and recognizes the same reserved coding subfields. | Its confidence-based recovery is a PlayStation media-extraction compatibility policy, not CDIC hardware behavior, so MAME does not copy that guess. |

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
- **Malformed XA signaling:** deterministic MAME behavior now rejects contradictory
  duplicated subheaders and invalid CD-i sector/coding combinations.  Which CDIC
  status bit or interrupt physical hardware exposes is still unknown, so rejection
  is not mislabeled as hardware error signaling.  Contradictory sound-group
  parameter-copy behavior remains open in the campaign matrix.
- **EOF termination:** XA establishes that EOF is channel-allocated.  Stopping the
  current HLE read after delivery of a selected EOF remains the current implementation
  model; exact CDIC command-completion signaling is not established by the media
  specification.
- **8-bit/18.9 kHz:** the Green Book coding-bit table permits the field combination,
  while its named quality overview lists only Level A (8-bit/37.8), Level B
  (4-bit/37.8), and Level C (4-bit/18.9).  MAME accepts the field combination but
  makes no claim that it has a fourth named quality level.

## Completion status

No audio area is declared 100% at this checkpoint.  The indexed MPEG-1 Layer II
header space, explicit rejection classes, malformed resynchronization, legal
bitrate/channel-mode transitions, backend exact/partial-frame termination, and
independent non-silent decoder comparison now have deterministic coverage.
Remaining blockers include CRC policy, VMPEG-specific header/update events,
stream-switch edges, XA sound-group redundancy and independent PCM comparison,
and the hardware-dependent matrix areas listed in the campaign specification.
