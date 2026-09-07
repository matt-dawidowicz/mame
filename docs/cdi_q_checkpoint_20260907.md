# CDIC Q and CD-DA transport checkpoint — 2026-09-07

## Metadata-derived position delivery

Source baseline: `3a9d22a445` (MMU certification). The generated shared BIN/CUE
contains twelve tracks: track 1 audio at LBA 0, track 2 stored pregap at 300,
INDEX 01 at 450, data tracks 3–12 at 600,675,...,1275, lead-out at 1350.
Every data track carries the copy-permission flag. No retail media is used.

`tests/emu/philips/cdi_q_integration.cpp` loads the image through the real image
device. Guest-visible writes select CD-DA/read-mode1 commands and BCD time.
The real 75 Hz CDIC timer delivers Q; the fixture acknowledges XBUF and reads
the 12 zero-extended SRAM words at byte offset 0x924 in alternating buffers.
It exercises 33 sectors: first/inside/last track sectors, sequential pregap
and INDEX 01 boundaries, data/audio flags, track 12 BCD, absolute continuity,
relative reset, backward repositioning and the final readable sector.
At lead-out it checks no new XBUF/Q delivery for 45 ms. This is the bounded
image-read policy, not a claim of physical lead-out status or readable AA packets.

Before the fix: 61 failed / 435 assertions. After: 435 / 435 pass. Full integration:
459 assertions / 8 cases pass. Commands:

```
make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests
./cdiintegrationtests "[q]"
./cdiintegrationtests
```

MAME `get_track_start()` and TOC supply logical INDEX 01 LBAs and pregap lengths.
Generic `get_track()` deliberately selects the preceding track until INDEX 01;
CDIC Q selects the upcoming track during the TOC-defined pregap. Stored pregap
payload in this shared file remains accessible through the generic sector path.
The fallback relative clock counts down toward INDEX 01; absolute time stays
LBA+150. Exact pause end-frame convention has no physical Philips capture.
`get_track_index()` compares relative LBAs with unnormalized CUE file offsets;
it cannot be used as an authoritative later-track index oracle. Higher indexes
are not claimed by the TOC-only fallback. Generic MAME behavior is unchanged.

The [ECMA-130 standard](https://dev.ecma-international.org/wp-content/uploads/ECMA-130_2nd_edition_june_1996.pdf),
sections 22.3.3 and 22.3.6, constrains track/index/time fields and Q CRC; its
audio-specific details defer to IEC 908. The test uses a separate bitwise
ten-byte, inverted-CRC calculation. The existing twelve-byte table-based loop
with FF CRC placeholders passes this oracle; its unusual form is not a proven
bug and was deliberately preserved. Philips SRAM placement is retained from
the existing measured HLE model, not derived from the generic disc standard.

## Remaining work

- Preserve authoritative stored Q (RW and RW_RAW) including higher indexes.
- Repair TOC A2 total length/audio-track address origin and missing data entries.
- Generic CUE index normalization, separate-file/virtual pregaps and multisession.
- Seek-only completion, read errors, physical lead-out signaling and servo timing.
- CD-DA command crossing into data: the existing PCM handoff may play data sectors;
  no DAC assertion or sufficiently specific controller evidence yet supports a fix.
- Audible PCM continuity, retail playthroughs and physical alignment are untested.

Percentages use the existing weights: position/transition packages 1→3, CRC 1→3,
synthetic disc reference 0→2, mixed-mode runtime 0→1, and partial TOC/subcode 1→2.
They are scoped engineering estimates, not hardware or game compatibility rates.
