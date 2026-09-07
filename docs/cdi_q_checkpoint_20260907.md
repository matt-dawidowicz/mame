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
At that baseline, generic `get_track()` selects the preceding track until INDEX 01;
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

## Stored raw Q — 2026-09-07

Baseline `d73fd1500f27bbc583b895dad554f5d8a03016ff`. The raw-subcode fixture
reproduces six lost INDEX 02 observations before the fix. CDIC now extracts bit 6
of each RW_RAW symbol and accepts Q modes 1–3 only with a matching recorded CRC.
Its twelve bytes then replace the synthetic fallback. No generic disc code changes.
Cooked RW is packed R-W, not a deinterleaved Q block, as documented by
[cdrdao](https://github.com/cdrdao/cdrdao/blob/master/dao/cdrdao.man).
No-Q and corrupt-CRC raw images retain the metadata fallback. This is an explicit
HLE policy; physical CDIC CRC-error flags/retry behavior remain unknown.

Five live fixture variants (metadata only, cooked R-W, valid raw Q with INDEX 02,
corrupt raw Q CRC, and absent raw Q) each inspect 33 SRAM packets. Raw fixture
non-Q bits are set to expose incorrect channel extraction. Full integration:
**2,199 assertions / 12 cases PASS**, using the same incremental build/run commands
above. All five Q cases contribute 2,175 assertions. Existing helper scope is
unchanged. No percentage rises: higher-index preservation strengthens the current
grade-3 position package, while generic CUE index metadata and wider formats remain open.

## TOC completion update — 2026-09-07

The live TOC fixture now verifies every track point and A0/A1/A2 over a complete
45-packet cycle. It reproduces 165 failed assertions before the fix and passes
546 afterwards. Full integration passes **2,745 assertions / 13 cases**.
Track pointers and A2 use generic logical LBAs +150, including audio tracks and
gaps, excluding storage padding. Track 12 is BCD 0x12; data copy flags survive.
The running minute field is BCD rather than the invalid 0xa0 placeholder.
The existing A0 0x10 data-disc policy is retained as Philips HLE behavior, not a
universal CD-ROM disc-type claim. No physical lead-in address is modeled.

Recalculated grades: Q TOC 1→3 gives 61.25→71.25 raw (60→70%); CDIC TOC 2→3
gives 67.5→71.25 raw (70% unchanged); disc TOC 2→3 gives 56.25→60 (55→60%).
CD-DA remains 65%. All retain Low/Medium confidence and their wider gaps.
The next task at that checkpoint was generic CUE index/file/pregap coverage,
completed in the follow-up below. Seek-only completion and data-track PCM
handoff evidence remain open.

## Remaining work

- Validate stored mode-2/3 packets and additional raw-subcode image containers.
- Wider live TOC inputs: audio-only, data-only, CHD images, multiple sessions and physical lead-in captures.
- Multisession and additional image containers; CUE index/file/pregap fixes are recorded below.
- Broader seek/error semantics, physical lead-out signaling and servo timing; the driver-controlled seek handshake is verified in the transport checkpoint.
- CD-DA data-track PCM handoff is fixed and digitally captured in the transport checkpoint; physical output transitions remain open.
- Audible PCM continuity, retail playthroughs and physical alignment are untested.

Percentages use the existing weights: position/transition packages 1→3, CRC 1→3,
synthetic disc reference 0→2, mixed-mode runtime 0→1, and partial TOC/subcode 1→2.
They are scoped engineering estimates, not hardware or game compatibility rates.

## Final production gate

Source `ee18320370b6c7e0abfd901bf0deafa543da0d6b` built locally as the production CD-i emulator:

```
make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 mame
./mame -validate
./cdihelpertests
python3 scripts/cdi_dvc_dma_liveness_audit.py
```

Build and validity: PASS, exit 0. Helpers: 17,393,781 assertions / 219 cases.
Integration: 2,745 assertions / 13 cases. DMA liveness: GREEN. Musashi regeneration
produces no tracked-source diff. This is a CD-i-targeted production build, not all
MAME systems, a sanitizer run or a firmware/retail playthrough.

Final Q/TOC CI [34075842095](https://github.com/matt-dawidowicz/mame/actions/runs/34075842095)
also passed on the exact source above: 219 helper cases / 17,393,781 assertions,
13 integration cases / 2,745 assertions, generated-source freshness and DMA liveness.

## Generic CUE and CHD follow-up — 2026-09-07

Baseline `39885b7d304ccd7947be45e643a37d7294b84062`. The initial three reproduction cases failed **210 / 3,001
assertions** on unchanged production code. This exposed file-relative INDEX
positions compared against track-relative time; pregaps assigned to the previous
track; an extra pregap added to loose-file physical extraction; a FILE-boundary
offset carried from the previous BIN into a new multi-track BIN; and truncated
payload/subcode reads returning success. The FILE-boundary error produced an
invalid huge lead-out through unsigned length subtraction. Fixing the mapping
and explicit read failures made that parser defect independently visible.

Logical lookup now selects the upcoming track at INDEX 00 and returns a storage
offset from its actual start (INDEX 00 if stored, INDEX 01 otherwise). Virtual
gaps take the existing zero-fill path. Physical extraction uses stored offsets
without a second pregap adjustment. CUE index comparisons add the file-relative
INDEX 01 origin to elapsed track time; formats lacking higher-index metadata
continue to report INDEX 01 after the gap. A new BIN inherits no previous BIN
offset. Short reads return I/O failure; offsets multiply in 64 bits. CDIC uses the
generic owner/index, encodes higher indexes as BCD, and retains valid stored-Q
priority and the existing CRC logic.

Generated fixture layouts: shared/stored, separate/stored, separate/virtual,
shared/virtual. All retain the same logical twelve-track TOC. Payload byte pairs
encode sector identity to expose wrong offsets despite audio endian conversion.
The generic case checks boundaries, INDEX 02/11/12, lead-out, raw subcode alignment,
virtual silence and physical reads. Live CDIC cases inspect 33 SRAM packets per
run, across metadata/raw-subcode variants; intentionally different stored Q proves
it still takes priority. Shared-file cooked/bad-CRC/no-Q cases also retain fallback
coverage. Two files truncated after opening verify failed payload/subcode reads.

An additional generic CHD case creates uncompressed images directly from generated
bytes, with explicit CHT2 metadata and 0xd7 track-padding bytes. It checks stored
and virtual gaps, logical versus physical data/subcode reads, and track-boundary
padding removal. This is not a chdman compression/extraction round-trip, a claim
of higher-index preservation in CHD metadata, or live CHD/controller coverage.
Generic cases share the emulator-linked test executable but instantiate the real
`cdrom_file` directly, without the CDIC fixture or a fake mapper.

```
make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests cdihelpertests mame
./cdiintegrationtests "[cue]"
./cdiintegrationtests
./cdihelpertests
./mame -validate
python3 scripts/cdi_dvc_dma_liveness_audit.py
python3 src/devices/cpu/m68000/m68kmake.py src/devices/cpu/m68000/m68k_in.lst src/devices/cpu/m68000/m68kops.h src/devices/cpu/m68000/m68kops.cpp
git diff --exit-code -- src/devices/cpu/m68000/m68kops.h src/devices/cpu/m68000/m68kops.cpp
```

After: CUE reproduction **3,436 assertions / 3 cases PASS**. Expanded full suite
**11,718 assertions / 17 cases PASS**; helpers **17,393,781 / 219 PASS**.
Production CD-i build and validity exit 0; DMA liveness GREEN; generated sources
unchanged. Existing worksheet grades/percentages are retained: the broader format,
controller and physical obligations remain open. No sanitizer or full-system
MAME build, retail playthrough, measured PCM output or hardware fidelity is claimed.

Next: establish seek-only completion and data-track PCM handoff from controller
and output evidence. Multisession, first-track special gaps, postgaps, mixed-sector
pregap types, malformed CUEs and other image containers need separate fixtures.

Exact code certification: `fe5aabb5089aedb6cbb3a9dc8eac587886cf33e2`. CI [34077601590](https://github.com/matt-dawidowicz/mame/actions/runs/34077601590)
passes the same 219 helper cases / 17,393,781 assertions and 17 integration cases /
11,718 assertions, generated-source freshness and DMA liveness. This document-only
certification uses its code parent; it does not change production or test code.

## Transport follow-up

The [transport checkpoint](cdi_transport_checkpoint_20260907.md) supersedes the
seek-handshake and data-track PCM next tasks above: driver-controlled seek passes,
and data-track PCM leakage is reproduced and fixed with live DAC sample evidence.
Broader physical/error behavior and active A/V save continuity remain open.
