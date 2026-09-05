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
2. PL_MPEG rejected a conventional Layer II bitrate change between adjacent
   frames.  Frame size and allocation tables are selected by each frame header,
   so the backend now permits bitrate and channel-mode changes while retaining
   its per-instance sample-rate lock.  This is decoder capability, not permission
   to change bitrate inside a CD-i audio sequence: Green Book IX.5.3.2.1 requires
   a presentation-time sequence boundary for that change.
3. The DVC header helper now returns the parsed bitrate/sample-rate indices,
   frame size, CRC presence, padding, channel mode, mode extension, emphasis,
   private bit, and an MPEG-syntax acceptance/rejection classification.  Free
   format (bitrate index zero) is distinguished from the reserved bitrate index,
   and the reserved emphasis value is accepted for robust decoding but labelled
   explicitly rather than silently promoted to a legal Full Motion profile value.

The focused audio gate at this checkpoint is 1,046,230 assertions in nine test
cases.  The combined DVC/parser/save-replay/reference test binary is 2,235,334
assertions in 32 test cases.  The broader standalone Philips gate is 2,332,516
assertions in 104 test cases, including the unchanged CDIC/XA baseline of 69
assertions in eight test cases.

## Full Motion profile, PCM queue, and audio-save checkpoint (2026-09-05)

This batch separates three layers that were previously easy to conflate: legal
MPEG-1 Layer II syntax, the narrower CD-i Full Motion authoring profile, and
VMPEG behavior on input that violates that profile.

1. Green Book IX.5.3.2 mandates 44.1 kHz, private bit zero, no emphasis or
   50/15-microsecond emphasis, no free format, and a mode-dependent Layer II
   bitrate matrix.  It also holds ID, layer, bitrate index, and sampling frequency
   constant within an audio sequence.  The profile checker exhausts every
   combination of 14 indexed bitrates, three syntactic sample rates, four channel
   modes, both private-bit values, and all four emphasis values.  Independent
   violation flags make compound malformed cases deterministic.  The ordinary
   parser still recognizes a syntactically decodable initial 32/48 kHz or other
   out-of-profile header and records a diagnostic; silently inventing the
   VMPEG's error/concealment response would not be justified by the specification.
2. Green Book IX.5.4.3.1 requires the decoder to mute while no decoded audio frame
   is available and to remain in MPEG-audio playback mode at an ISO end code.
   MAME's current output model now passes through one production-used pure helper:
   scheduled silence wins first, only complete stereo pairs are consumed, queue
   exhaustion emits zero, and a later refill resumes without replay or loss.  A
   retained transition fixture produces FNV-1a `fdc1c8bc`.  Save-stateable
   starvation counters are diagnostic only; no VMPEG underflow IRQ edge is
   fabricated from host-queue occupancy.
3. PL_MPEG stores `signal_end` and `has_ended` inside an opaque buffer.  The replay
   journal did not preserve either, so a state saved at ISO end reconstructed a
   decoder with different termination state.  Both states are now registered.
   Postload replays bytes and the exact decoded-frame count before reapplying the
   marker; it performs the terminal failed decode only if the live backend had
   already done so.  Regressions cover pre-header, exact-boundary, partial-frame,
   starvation, observed/unobserved end, refill after end, and the existing
   bit-identical mid-frame continuation.

The optimized and unoptimized focused audio gates pass 1,135,145 assertions in
14 cases, as does the ASan/UBSan build with LeakSanitizer disabled.  The combined
DVC/parser/timing gate passes 2,413,670 assertions in 55 cases.  The standalone
Philips gate passes 5,847,426 assertions in 126 cases, and the extended pure
selection including the repository's unchanged `rgbutil` test passes 5,847,700
assertions in 127 cases.  Exact VMPEG DAC hold/ramp edges, first-frame optional
muting, profile-error signaling, complete stream switching, active A/V save/load,
and long continuation remain outside this checkpoint and are still open in the
campaign matrix.

## MPEG stream-ID and PES routing checkpoint (2026-09-05)

Green Book IX.5.3.1.5 permits every MPEG audio Stream ID from `C0` through `DF`,
and IX.8.2.4 defines the selected audio stream number as 0-31.  MAME previously
masked the FMA selector to four bits.  That made stream `D0+n` indistinguishable
from `C0+n`, even though the system specification assigns the fifth stream-number
bit meaning.

The FMA selector and readback now retain five bits; the FMV selector correctly
retains four.  A production-used pure classifier owns the complete byte after an
MPEG start-code prefix: pack start, program end, selected PES, or skipped packet.
The regression independently enumerates every byte for every one of the 32 FMA
and 16 FMV selectors, explicitly proves the 16 former `Cx`/`Dx` alias pairs are
distinct, exhausts 16-bit register normalization, and traverses every first
PES-header-byte classification for every selected audio stream.  The optimized,
unoptimized, and ASan/UBSan combined DVC gates pass 2,569,450 assertions in 57
cases; the audio selection passes 1,286,825 assertions in 16 cases.  The standalone
Philips binary passes 6,003,206 assertions in 128 cases, and the extended selection
including the unchanged `rgbutil` source passes 6,003,480 assertions in 129 cases.
The native C++20 `release64` CDIC/DVC production archive also compiles.  Linking
the generated SDL `mametests` target remains blocked at the known container
prerequisite, missing `SDL2/SDL.h`; the standalone gates above compile and execute
the changed production-used helpers and all Philips test sources.

This closes program-stream ID/PES selection, not full stream switching.  The
Green Book distinguishes a requested stream from the stream actually being decoded
and defines a change-of-stream event.  Available reverse-engineered register names
identify `$e03008` as the planned stream and tentatively identify `$e0300a` as the
current stream, while the independent MiSTer implementation explicitly labels its
own switching behavior inaccurate.  MAME still mirrors the requested value at both
addresses and does not fabricate a CSU timing edge.  The following checkpoint closes
the separate elementary-stream access-point gap.

## Direct MPEG-audio access checkpoint (2026-09-05)

Green Book IX.5.4.3.2 requires an MPEG Audio Pointer to identify either the first
byte of a pack start code or the first byte of an audio-frame synchronization word.
The prior parser could recognize only `00 00 01`, so a conforming pointer placed
directly at a Layer II frame was scanned past and never reached PL_MPEG.

FMA reset now enters an explicit access-routing state.  One production-used pure
transition detects a system start-code prefix as soon as its third byte arrives or
a complete syntactically valid Layer II header on the fourth byte.  For the latter,
the four held header bytes are delivered exactly once and the header-derived frame
length bounds every subsequent payload byte.  Start-code scanning resumes only at
the exact frame boundary.  This both permits consecutive directly accessed frames
and prevents a coincidental `00 00 01` inside compressed audio from being mistaken
for a system-layer boundary.  Once a real system prefix is encountered, the normal
pack/PES parser takes ownership again.

The transition's complete state—prefix window, header window/count, and remaining
frame bytes—is registered for save states.  Tests exhaust both protection-bit values
and all 65,536 lower header-field combinations against an independent legal-index
oracle, cover pack-prefix and rolling-scan boundaries, retain an embedded start-code
pattern as frame payload, route four consecutive mode-varying frames byte-for-byte,
decode those frames through PL_MPEG, resume at a following pack prefix, and compare
every future transition after four mid-header/mid-frame snapshot points.

Optimized, unoptimized, and ASan/UBSan focused DVC gates each pass 3,036,627
assertions in 61 cases.  The complete standalone Philips binary passes 6,470,383
assertions in 132 cases; its permanent `[dvc][audio]`, `[dvc][mpeg]`, `[save]`,
and `[scc68070][dma]` selections pass 1,754,002/20, 619,363/11, 72,923/8,
and 8/1 respectively.  The extended pure gate including `rgbutil` passes
6,470,657/133, and the native C++20 `release64` production archive compiles.

This completes the scoped DVC MPEG-1 Layer II parser at the standards/software
boundary.  The private VMPEG response to prohibited/malformed input remains an
explicit evidence limitation, while desired/current stream-switch and CSU timing
remain separate section-17 behavior rather than parser defects.

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

## XA sound-group and ADPCM checkpoint (2026-09-05)

The fourth campaign batch replaces the CDIC path's implicit sound-parameter choice
and reserved-range coercion with one validated, testable group decoder.

1. The Green Book's 128-byte sound group contains 16 parameter bytes followed by
   112 sample-data bytes.  In 8-bit mode each of four sound-unit parameters occurs
   four times; in 4-bit mode each of eight parameters occurs twice.  MAME reports
   per-unit masks for contradiction, selected-copy reserved values, and invalid
   values confined to redundant copies (ranges 9-15 for 8-bit, 13-15 for 4-bit).
2. Subsequent Mono-I hardware measurements resolve which redundant parameters the
   CDIC actually consumes: bytes 12-15 in 8-bit mode, and bytes 4-7 plus 12-15 in
   4-bit mode.  MAME now decodes those selected copies even when another copy differs.
   Only an invalid selected value receives duration-preserving silence with held
   history; that response remains an explicit concealment model.  No undocumented
   physical CDIC status or IRQ is fabricated.
3. Sign extension, predictor arithmetic, final 16-bit clipping, channel interleave,
   and history updates now live in the pure helper used by production.  Device reset
   and audio-map start use the same tested history-reset helper.  Reserved ranges are
   rejected at the group boundary instead of being silently clamped to 12.
4. An independent integer oracle exhausts 813,888 legal combinations: all four
   filters, every legal range for each width, every 4-bit or 8-bit code, and 81
   adversarial pairs of predictor-history endpoints.  Whole-group cases cover 4/8-bit,
   mono/stereo, both 37.8/18.9 kHz coding values, two consecutive groups, clipping,
   output bounds, channel order, and reset.
5. A retained synthetic reference procedure wraps 36 deterministic groups in two
   raw Mode 2/Form 2 stereo sectors.  FFmpeg 6.1.1's separate fixed-point `adpcm_xa`
   decoder produced 16,128 interleaved little-endian PCM bytes, exactly matching MAME.
   Each 2,352-byte sector is zero-initialized, carries the standard 12-byte sync and
   Mode 2 byte, repeats subheader `01 00 24 01`, places the generated payload at byte
   24, and leaves the final 24 bytes zero; the payload algorithm is retained in the
   reference test.
   The raw sectors hash to
   `2e0bde2df9977d8daccd4cbdcf582ac2900c77ecd87a835c345886bec5023a88`;
   the PCM hashes to
   `47a6ee3539f4694d1e4ac0c04f91e30b180efbfb870f64774031445c795acc2a`.
   The regression retains 15 PCM landmarks and the byte-order-independent FNV-1a
   value `19f5a1a69dbe9187`.

FFmpeg corroborates 4-bit XA layout, clipped-history feedback, exact rational
coefficients, and the current add-half/arithmetic-shift predictor term.  Pinned jPSXdec
source independently corroborates both 4-bit and 8-bit group layouts and recognizes
all redundant parameter copies.  Its majority recovery and floating-point history
are extraction policies, not CDIC evidence, and were deliberately not copied.

The focused CDIC gate now passes 2,710,835 assertions in 15 cases; its XA subset is
2,449,377 assertions in six cases.  Optimized, unoptimized, and
AddressSanitizer/UndefinedBehaviorSanitizer builds pass (LeakSanitizer is disabled in
the ptraced campaign container).  The standalone Philips gate passes 5,043,282
assertions in 111 cases, and the extended pure mametests gate passes 5,044,352
assertions in 118 cases.  The generated native MAME C++20 `release64` CD-i production
archive also compiles after the refactor.

## CDIC audio transport and AUDCTL checkpoint (2026-09-05)

The fifth campaign batch replaces inherited CDIC playback shortcuts with a
save-stateable model anchored to reproducible CD-i 210/05 captures.

1. AUDCTL bit 11, not bit 13, owns playback.  Bit 13 only gates completed
   sound-map-buffer interrupts, and bit 0 is the read-to-clear `$ff` termination
   latch.  A full-word `$2800` write begins a CPU sound map at fixed buffer `$2800`;
   the observed `$fffe` readback is status and must not be reused as an address.
   Sound maps alternate `$2800`/`$3200`, set ABUF on each completed prior buffer,
   clear bit 11 on `$ff`, and finish an in-flight interval without an IRQ when the
   guest aborts by writing zero.
2. Service test paths consistently distinguish reset readback `$c7fe` from the
   `$d7fe` value observed after software writes zero.  MAME models both values but
   does not assign a function to their unexplained bit-12 difference.  Every 16-bit
   write value, both termination-latch states, and active/idle source states are
   covered by the pure register/action regression.
3. CD-fed XA receipt is now separate from playback.  The first two selected sectors
   fill `$2800`/`$3200` and report DBUF low nibbles 4/5.  Playback waits for the
   guest's `$0800` write, consumes the halves in order, preserves the expected half
   during starvation, and resumes from a later refill without replaying a consumed
   buffer.  The timing state is included in save states.
4. The duration of every coding-field-valid XA sector is an exact rational identity:
   the samples per channel at `CLOCK2/512` or `CLOCK2/1024` occupy exactly
   2, 4, 8, or 16 periods of the 75 Hz sector clock.  This removes duplicated
   arithmetic but does not claim the unresolved phase between MAME's two timers or
   prove long-duration disc/timestamp drift.
5. CD-DA now submits all 588 stereo sample frames from each physical sector and
   exposes a CPU buffer/subcode event every 1/75 second.  Its PCM bypasses CDIC RAM;
   the currently synthesized Q bytes are placed at the captured byte offset `$924`.
   Playback waits for `$0800`.  Retaining the first sector received before that write
   is a deterministic HLE bridge between the measured first IRQ and playback start,
   corroborated by another emulator, not a claim about the CDIC's hidden queue depth.
6. The parameter-corruption capture also reports that coding `$14` does not play on
   the tested 210/05.  MAME still classifies it according to the Green Book coding
   fields because the capture does not establish whether the restriction applies to
   related emphasis/stereo values or what status/timing the chip exposes.  This is a
   named unresolved hardware limitation, not generalized into a guessed rejection.

The new pure states cover sustained alternation, startup, refill, starvation,
interrupt-masked abort, `$ff`, immediate replacement, and all CD-DA receive-gating
combinations.  The focused optimized and unoptimized gates each pass 3,425,276
assertions in 20 cases; the same focused gate passes under AddressSanitizer and
UndefinedBehaviorSanitizer with LeakSanitizer disabled in the ptraced campaign
container.  The standalone Philips gate passes 5,757,723 assertions in 116 cases,
and the extended pure gate passes 5,758,793 assertions in 123 cases.  Its permanent
selections pass as follows: `[audio]` 1,702,246/14, `[dvc]` 2,324,790/54,
`[dvc][audio]` 1,046,230/9, `[xa]` 2,507,802/6, `[save]` 63/4,
`[scc68070][dma]` 8/1, `[timing][audio]` 618/2, and `[cdda]` 8/1.

The native C++20 `release64` CDIC/DVC production archive compiles from this tree.
The container cannot link the live integration executable because its SDL OSD
dependency stops at the unavailable `SDL2/SDL.h`; neither `sdl2-config` nor the
header exists in the standard include paths.  This is a recorded host prerequisite,
not an audio-source or test failure.  The previously certified Windows live
DVC-to-SCC DMA fixture (three assertions in one case) remains an unchanged mandatory
local gate.  The extended pure gate needed C++20 plus warning demotions for the
unrepaired scratch copy of `rgbutil.cpp`; the user's intentional Windows repair is
preserved and no RGB source is part of this checkpoint.

## Evidence register

| ID | Class | Source | Supported claim | Limit |
| --- | --- | --- | --- | --- |
| MPEG-STD-001 | Standards-derived | [RFC 3003](https://www.rfc-editor.org/rfc/rfc3003), referring normatively to ISO/IEC 11172-3:1993 | MPEG elementary audio is a sequence of independently headed frames and may be interspersed with non-MPEG data, so deterministic resynchronization is required. | RFC 3003 is not a VMPEG hardware specification. |
| MPEG-STD-002 | Standards-derived | [Philips/Sony CD-i Green Book, May 1994 Release 2](https://archive.org/download/cdi_may94_r2/cdi_may94_r2.pdf), Chapter IX sections 5.3.1, 5.3.2, 5.4.3, and 8.2.4 | Defines Full Motion audio Stream IDs `C0`-`DF`, selected stream numbers 0-31, packet/access-point limits, the Layer II bitrate-by-mode table, 44.1 kHz/private/emphasis constraints, audio-sequence invariants, muted output under starvation, stream-switch acceptance, and ISO-end playback continuity. | It permits optional first-frame muting and does not specify the private VMPEG register layout, response to malformed/profile-invalid input, or exact DAC/IRQ edges. |
| MPEG-REF-001 | Independently corroborated | [FFmpeg `mpegaudiodecheader.c` at `ef5929f`](https://github.com/FFmpeg/FFmpeg/blob/ef5929f4ebb158ae689845055513a5725f5de28c/libavcodec/mpegaudiodecheader.c) | Indexed Layer II frame size is recalculated from each frame's bitrate, sample rate, and padding; free format is represented separately when a size cannot be derived from one header. | FFmpeg behavior is reference-decoder evidence, not VMPEG evidence. |
| MPEG-REF-002 | Independently corroborated | [mpg123 `mpeghead.h` at `f6c19f4`](https://github.com/madebr/mpg123/blob/f6c19f46031088efc8d0e5b83305a6f36ceceb65/src/libmpg123/mpeghead.h) and [parser](https://github.com/madebr/mpg123/blob/f6c19f46031088efc8d0e5b83305a6f36ceceb65/src/libmpg123/parse.c) | The ordinary compatible-header mask excludes bitrate, while the parser rejects bitrate index 15 and handles index zero as free format. | mpg123 supports profiles beyond CD-i Full Motion. |
| MPEG-REF-003 | Current implementation model | [PL_MPEG upstream at `c871f2b`](https://github.com/phoboslab/pl_mpeg/blob/c871f2be022ece7ef4f64230b4fb8e1fb9eb6023/pl_mpeg.h) | Documents the inherited decoder architecture, the upstream constant-header restriction from which MAME's streaming fixes diverge, and the dynamic buffer's separate `total_size`/`has_ended` state: `signal_end` fixes the former and a later write clears both. | Upstream PL_MPEG is not an independent hardware model; these fields explain required save reconstruction rather than VMPEG state. |
| MPEG-REF-004 | Controlled synthetic experiment | `tests/emu/philips/cdidvc_audio_reference.cpp` and its retained data header | Non-silent mono, dual-channel, stereo, and joint-stereo PL_MPEG output remains within measured bounds of FFmpeg 6.1.1's independent fixed-point Layer II decoder. | This compares software decoders; it does not establish VMPEG DSP rounding. |
| DVC-HW-001 | Hardware-confirmed | [CDi_FMVTest FMA playback-delay capture at `991b9cb`](https://github.com/Slamy/CDi_FMVTest/tree/991b9cb22905942d969a6d3219f89c5e941a7741/fma_playback_delay) | On the recorded 210/05 + VMPEG system, `MA_TRIG_DEC` occurs close enough to analogue sample output to serve as a software timing marker; the retained analysis bounds any additional decode delay to roughly 4 ms after accounting for encoder silence. | One hardware/configuration and one fixture; it does not establish long-duration drift or underflow edges. |
| DVC-RE-001 | Independently corroborated | [MiSTer CD-i DVC notes at `1d0d29b`](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/doc/dvc.md) | FMA DCLK is treated as 45 kHz and the audio clock is the driver-visible link to MPEG SCR timing. | Reverse-engineering notes, not a Philips register specification. |
| DVC-RE-002 | Independent implementation | [MiSTer VMPEG/FMA RTL at `1d0d29b`](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/rtl/vmpeg.sv) and [audio path](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/rtl/mpeg/fma/mpeg_audio.sv) | Keeps DSP-reported underflow and stream-change events distinct from output-FIFO occupancy; its compatibility output starts half-full and slowly ramps a held sample toward zero after empty. | The source labels stream-change handling probably inaccurate, and its FIFO/ramp policy is RTL compatibility behavior rather than a hardware capture.  MAME therefore does not copy these exact edges. |
| XA-STD-001 | Standards-derived | [Philips/Sony CD-i Green Book, May 1994 Release 2](https://archive.org/download/cdi_may94_r2/cdi_may94_r2.pdf), Chapter II sections 4.5/4.9 and Appendix II | Defines double-written subheaders, channel ranges, legal Audio/Video/Data and Form combinations, empty/message restrictions, audio-mask routing, and every audio coding field. | A media/system specification does not identify the private CDIC register response to malformed input. |
| XA-STD-002 | Standards-derived | [Philips/Sony CD-ROM XA System Description, May 1991](https://archive.org/download/xa-10-may-1991/CD-ROM%20XA%20Specification%20May%201991%20-%20print%20to%20pdf%20in%20chrome.pdf), Chapter II sections 4.3 and 6.2 | Trigger is not channel-allocated; EOF/EOR are channel-allocated. The decoder model uses CIRC reliability flags to select trustworthy duplicated subheader bytes. | MAME's raw image interface does not expose those per-byte CIRC flags. |
| XA-REF-001 | Independently corroborated | [jPSXdec duplicated-subheader and coding parser at `fd54036`](https://github.com/m35/jpsxdec/blob/fd5403629ac81aaca0feff0dc89e6cadd6353b26/jpsxdec/src/jpsxdec/cdreaders/CdSectorXaSubHeader.java) | An independent XA extraction tool flags unequal duplicated fields as corruption and recognizes the same reserved coding subfields. | Its confidence-based recovery is a PlayStation media-extraction compatibility policy, not CDIC hardware behavior, so MAME does not copy that guess. |
| XA-STD-003 | Standards-derived | [Philips/Sony CD-i Green Book, May 1994 Release 2](https://archive.org/download/cdi_may94_r2/cdi_may94_r2.pdf), Appendix II audio coding and decoder sections | Defines the 128-byte sound-group layouts for 4/8-bit and mono/stereo, redundant parameter positions, legal filters/ranges, exact predictor coefficients, 16-bit output, and clipping. | It specifies neither private CDIC malformed-group signaling nor observable accumulator/rounding circuitry. |
| XA-REF-002 | Independently corroborated | [FFmpeg `adpcm.c` at `ef5929f`](https://github.com/FFmpeg/FFmpeg/blob/ef5929f4ebb158ae689845055513a5725f5de28c/libavcodec/adpcm.c) and [PSX STR demuxer](https://github.com/FFmpeg/FFmpeg/blob/ef5929f4ebb158ae689845055513a5725f5de28c/libavformat/psxstr.c) | Independently corroborates 4-bit group/channel layout, rational predictor coefficients, clipped-history feedback, add-half/arithmetic-shift rounding, and exact PCM for the retained two-sector fixture. | Supports 4-bit XA only and does not validate redundant parameter copies; software agreement is not CDIC silicon proof. |
| XA-REF-003 | Independently corroborated | [jPSXdec `XaAdpcmDecoder` at `fd54036`](https://github.com/m35/jpsxdec/blob/fd5403629ac81aaca0feff0dc89e6cadd6353b26/jpsxdec/src/jpsxdec/adpcm/XaAdpcmDecoder.java) and [`XaAdpcmSoundUnit`](https://github.com/m35/jpsxdec/blob/fd5403629ac81aaca0feff0dc89e6cadd6353b26/jpsxdec/src/jpsxdec/adpcm/XaAdpcmSoundUnit.java) | Independently corroborates both width layouts and explicitly collects redundant sound parameters. | Its majority selection, invalid-filter repair, double-precision predictor history, and final rounding are extraction/compatibility choices rather than hardware facts. |
| XA-HW-001 | Hardware-confirmed | [CDIC Black Box Analyzer parameter-corruption test at `e861f76`](https://github.com/Slamy/CDIC_BlackBoxAnalyzer/blob/e861f76ece477b3aff0e9c4c70f5c6ba1715e60a/src/test_audiomap.c) | The tested CD-i 210/05 consumes parameter bytes 12-15 in 8-bit mode and bytes 4-7 plus 12-15 in 4-bit mode. | One player revision; `$14` failed, but related coding values and invalid-selected-parameter signaling were not characterized. |
| XA-MODEL-001 | Current implementation model | `cdic_hle::decode_xa_group` and `[cdic][xa][group][malformed]` | A redundant-copy disagreement is diagnostic and the hardware-selected copy is decoded.  An invalid selected value retains its sample duration as silence without changing predictor history. | Silence/held-history concealment and physical error/status signaling remain unmeasured. |
| CDIC-HW-001 | Hardware-confirmed | [CDIC register observations](https://github.com/Slamy/CDIC_BlackBoxAnalyzer/blob/e861f76ece477b3aff0e9c4c70f5c6ba1715e60a/doc/registers.md), [sound-map captures](https://github.com/Slamy/CDIC_BlackBoxAnalyzer/blob/e861f76ece477b3aff0e9c4c70f5c6ba1715e60a/src/test_audiomap.c), and [usage manual](https://github.com/Slamy/CDIC_BlackBoxAnalyzer/blob/e861f76ece477b3aff0e9c4c70f5c6ba1715e60a/doc/cdic_manual.md) | On the measured 210/05, AUDCTL bits 13/11/0 control ABUF IRQ, playback, and `$ff` termination; CPU maps start at `$2800`, alternate with `$3200`, and exhibit the captured normal/abort/termination readback and IRQ edges. | Does not reveal the internal PCM queue, accumulator, or all fixed-bit causes. |
| CDIC-HW-002 | Hardware-confirmed | [XA playback capture](https://github.com/Slamy/CDIC_BlackBoxAnalyzer/blob/e861f76ece477b3aff0e9c4c70f5c6ba1715e60a/src/test_xa_play.c) and [CD-DA playback/R-W capture](https://github.com/Slamy/CDIC_BlackBoxAnalyzer/blob/e861f76ece477b3aff0e9c4c70f5c6ba1715e60a/src/test_cdda_play.c) | CD-fed XA first reports buffers 4/5 at `$2800`/`$3200`; XA and CD-DA require `$0800`; CD-DA produces per-sector buffer/subcode events and its PCM is not stored in CDIC RAM. | Captures use one player and discs; exact DAC start sample, track/lead transitions, and complete P-W semantics remain unresolved. |
| CDIC-REF-001 | Independently corroborated | [MiSTer CD-i CDIC RTL at `1d0d29b`](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/rtl/cdic.sv), [audio decoder](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/rtl/audiodecoder.sv), and [audio player](https://github.com/MiSTer-devel/CDi_MiSTer/blob/1d0d29b164a05d11db0094564feacf0f66c1d4e4/rtl/audioplayer.sv) | Independently implements bits 13/11/0, `$2800`/`$3200` buffering, and the same measured 4/8-bit parameter offsets. | The RTL incorporates reverse-engineering evidence and is corroboration, not an IMS66490 specification. |
| CDIC-MODEL-001 | Current implementation model | [E-Di CDIC model at `b98537f`](https://github.com/whatever-industries/edi_emulator/blob/b98537fddf3b6199554b2d289840a6704c344c81/crates/cdi-core/src/cdic.rs) | A separately maintained emulator uses distinct sound-map/realtime states, two ready XA halves, and one pending pre-start CD-DA sector; this cross-checks MAME's state decomposition. | It cites the same Mono-I evidence and therefore is not independent hardware confirmation. |

## Explicit implementation boundaries

- **Free format:** ISO-family reference decoders recognize bitrate index zero as
  free format, but Green Book IX.5.3.2.5 explicitly excludes it from CD-i Full
  Motion.  MAME classifies it as `unsupported_free_format`; PL_MPEG cannot derive
  its length by searching for a following compatible header.  The VMPEG response
  to prohibited input remains unmeasured.
- **Sample rate and sequence continuity:** the Full Motion profile permits only
  44.1 kHz and holds bitrate/sample frequency constant within an audio sequence.
  The conventional parser deliberately distinguishes syntactically valid 48/32
  kHz input and reports it as out of profile.  PL_MPEG locks each decoder instance's
  sample rate, while stream selection does not yet model a PTS-defined sequence
  boundary or rate restart.  Prohibited-input behavior remains unknown.
- **CRC:** protection and CRC length are parsed, but PL_MPEG does not validate
  the Layer II CRC.  Error signalling/concealment on VMPEG is unknown.
- **Private bit and emphasis:** Full Motion reserves the private bit as zero and
  permits only no emphasis and 50/15-microsecond emphasis.  Independent profile
  flags expose every violation.  The conventional parser still distinguishes the
  reserved emphasis code, and MAME does not yet implement MPEG de-emphasis or
  claim a VMPEG malformed-input response.
- **Resynchronization:** accepting a structurally valid indexed header after
  junk is a software streaming model.  The exact VMPEG false-sync and error-event
  policy has not been measured.
- **DVC stream selection:** program-stream routing now honors all 32 Green Book
  audio Stream IDs.  The `$e03008`/`$e0300a` desired/current interpretation comes
  from reverse-engineering labels rather than a public VMPEG register manual;
  MAME currently mirrors them and does not claim the exact switch/CSU edge.  Direct
  access at an MPEG audio frame sync is now implemented with header-length-bounded
  payload routing; the exact physical false-sync/error response remains unmeasured.
- **DVC PCM starvation:** Green Book requires muted output until another decoded
  frame is ready.  MAME's zero-output, whole-stereo-pair queue rule is deterministic
  and production-tested, but host-queue exhaustion is not asserted to be the exact
  VMPEG FIFO threshold or underflow-event edge.  Hold/ramp behavior at the analogue
  boundary remains unresolved.
- **DVC audio save reconstruction:** the opaque PL_MPEG input-end marker and its
  observed-ended latch are now replayed exactly at the software boundary.  This
  proves deterministic backend continuation; it is not a physical VMPEG state or
  cross-version save-format promise.
- **Malformed XA signaling:** deterministic MAME behavior rejects contradictory
  duplicated subheaders and invalid CD-i sector/coding combinations.  For sound
  groups it follows the measured Mono-I copy selection, treating disagreement in a
  redundant copy as diagnostic.  An invalid selected parameter keeps its time slot
  as silence and does not alter predictor history.  Which CDIC status bit, interrupt,
  or concealment response physical hardware exposes remains unknown, so this policy
  is not mislabeled as hardware error signaling.
- **XA rounding/saturation:** the Green Book fixes the predictor coefficients, output
  width, and final clipping.  FFmpeg independently agrees bit-for-bit with MAME's
  current fixed-point result over the retained 4-bit fixture and the exhaustive oracle.
  CDIC accumulator width, intermediate overflow behavior, and tie rounding have not
  been captured; software agreement is not a hardware claim.
- **EOF termination:** XA establishes that EOF is channel-allocated.  Stopping the
  current HLE read after delivery of a selected EOF remains the current implementation
  model; exact CDIC command-completion signaling is not established by the media
  specification.
- **8-bit/18.9 kHz:** the Green Book coding-bit table permits the field combination,
  while its named quality overview lists only Level A (8-bit/37.8), Level B
  (4-bit/37.8), and Level C (4-bit/18.9).  MAME accepts the field combination but
  makes no claim that it has a fourth named quality level.  A direct 210/05 capture
  reports that mono/no-emphasis coding `$14` does not work; the exact response and
  scope across related coding values remain unresolved.
- **AUDCTL fixed bits:** service tests report `$c7fe` on reset and `$d7fe` after an
  initialization path; Mono-I writes/captures corroborate `$d7fe`, `$dffe`, `$fffe`,
  and `$f7ff` state changes.  Bit 12's cause is unknown.  MAME preserves the observed
  readback distinction without assigning that bit an invented feature.
- **CDIC DAC/queue boundary:** AUDCTL gating and transfer-completion events are
  measured, but the exact first/last audible sample, internal queue depth, and
  hold/zero/ramp/flush response on stop, underrun, replacement, or reset are not.
- **CD-DA subcode:** Q location and 75 Hz delivery are captured.  MAME still
  synthesizes Q from simplified disc assumptions and does not reproduce captured
  R-W, multi-track/index, lead-in/lead-out, pause, or seek transitions.

## Completion status

AUDCTL fidelity and the DVC MPEG-1 Layer II parser are now narrowly scoped campaign
areas declared at a defensible 100%.  All behavior MAME exposes from AUDCTL is tied
to the retained 210/05
captures or service-test readback, all 65,536 writes and both latch/source states are
deterministic, the inherited bit-13-as-playback and readback-as-address defects are
removed, and the unexplained bit-12 transition is explicitly isolated rather than
assigned a function.  This certification does not include the separately listed DAC
mute/flush edge area.

The parser certification covers generic/Green Book header classification,
malformed/truncated resynchronization, all 32 audio Stream IDs, PES routing, and
both permitted MPEG Audio Pointer entry forms.  It does not promote unknown VMPEG
CRC/profile-error signaling, decoder arithmetic, or stream-switch event timing into
parser claims.

No other incomplete matrix area is promoted to 100% by this checkpoint.  Remaining
blockers include VMPEG CRC/profile-error and stream-transition behavior, DVC
rate/reset/active-A/V save and clock runtime gates, CDIC invalid-coding/error
signaling, XA/CD-DA de-emphasis, silicon rounding and attenuation, exact DAC sample
edges, long-duration A/V drift, and full CD-DA Q/R-W/track/seek behavior.
