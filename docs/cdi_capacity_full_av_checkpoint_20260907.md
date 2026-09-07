# DVC capacity, full-size composition and synchronized branches

Date: 2026-09-07. Branch: `cdi-unified`. Reviewed baseline:
`c9d4027d69d80c76c55a09c431cc782d7b2f8296`. Verified code:
`7d3b17b91d25d31ea98e810a79464de91e25d9d0`. [CI 34164448976](https://github.com/matt-dawidowicz/mame/actions/runs/34164448976)
passes on `30246e5f846ff13a44b9c261051081f273ee0f94`, with production and tests identical to the code commit.
The final-source local long-capacity gate also passes. This subsequent certification
changes documentation only; the code commit contains the implementation and tests.

## Reproduced failures and changes

1. The unmodified production path loses playback on restore after the 8 MiB audio
   journal limit. The before run restores the earlier snapshot, then reports
   different IRQ/status at 348664 ms and exits with 2 failed assertions. History
   overflow marked the entire snapshot invalid; postload cleared both decoders.
2. A full-size first picture spans PES packets. The first packet carried PTS
   27000, but no picture could yet be returned. Later packets cleared packet PTS,
   so output started before its intended 300 ms time. The first wrong composed
   pixel at clock 9169 was x=96,y=82 (RGB 16767686 instead of black).

After replay history reaches 8 MiB audio, 32 MiB video or 16384 pump events,
presave uses current decoder state in the existing fixed save buffers. DVA1/DVV1
images encode scalar values, ring bytes/bit position/EOF, audio synthesis history,
quantizer table indices and video plane-slot permutations. Host pointers, callbacks,
allocator capacity and C++ padding are excluded. Images have explicit little-endian
scalar encoding and a version marker; malformed sizes/versions, invalid geometry,
unusable sample rates and truncated data fail restoration. Unused initial video
reference planes are zeroed before snapshots can copy them. Reads reconstruct
fresh decoders and check new input allocation.

Below the history bounds, existing replay remains in use. After direct restoration,
the direct-snapshot mode remains selected because original history is unavailable.
This is a save implementation policy, not a VMPEG hardware buffer or flush model.
Excessive actual pending input, PCM, presentation queues or unsupported geometry
can still invalidate a save; this work does not claim unlimited malformed/backlog
support or cross-version MAME save compatibility.

The first selected video PES timestamp is now latched at payload acceptance and
retained until the first decoded picture supplies its backend-relative anchor.
Reset clears it; MAME save/load registers the pending flag. A live test supplies
only 32 bytes, saves at blanking after 50 ms, supplies the remainder at 100 ms,
then verifies correctly timed output and an exact restored continuation.

## Independent original references and composed output

`scripts/cdi_generate_motion_reference.py --full-size` generates original YUV
patterns, MPEG-1 video and independent FFmpeg 8.0.1-3ubuntu2 RGB output. All three
profiles contain I/P/B pictures, closed GOPs of 12 and two B pictures; encoder
target/maxrate is 1150 kbit/s with a 327680-bit buffer. This is codec/reference
evidence, not a complete CD-i media-conformance claim.

| Original profile | Pictures | Whole-picture max RGB error | RGB RMS |
| --- | ---: | ---: | ---: |
| 352x288, 25 Hz | 36 | 10 | 1.18093 |
| 352x240, 30000/1001 Hz | 45 | 10 | 1.21410 |
| 384x288, 25 Hz | 36 | 10 | 1.17387 |

All 117 pictures are independently compared, including pixels outside the composed
window. Full-size data is zlib-compressed RGB and base64-encoded in an original
source fixture. Both the small and full-size generators reproduce their headers
byte-for-byte. Source header SHA-256 values:

- Small: `24757392eaee7db30523276b61622a19a66e7c085db016c03df89e9bc0423415`.
- Full: `be630a4eefc71c6865b1df620b6414fc7d8f4a4a965b10e4d276162650008ff1`.

Complete 768x560 output is checked in CLUT8, CLUT4, RL7 and CLUT4 hold-three mosaic.
The original native pattern specifies four palette colors and weights, a matte
opening at x=96 and a white cursor. Native pixels are exact. External video is
placed at x=64,y=20 at 2x scale, so full-height/rightmost video is clipped at the
visible edge; standalone references separately cover the complete decoded picture.
This does not certify all MCD212 modes or full PAL/NTSC board timing.

The new short scenarios make 15 saves/loads, repeating
2,234 full fields and 3,958,416
channel samples exactly, including sound callback time/count/order and 1 ms IRQ
assertion/status/acknowledgement observations. Maximum composed RGB error is 10,
PCM error 537 and native error 0. Existing tolerances remain RGB max 12 and
mean-square at most 4 (integer quotient), PCM max 600 and mean-square 22500.

Two synchronized branches occur in blanking at 1618 and 3216 ms. The fixture
explicitly issues FMA stop/reset and FMV clear, rejects stale stream IDs, then
feeds both requested streams with a shared future PTS of 9000 (100 ms). References
check old PCM removal, scheduled silence, fresh synthesis and video presentation.
Six saves repeat these transitions. These are scheduled command-model observations,
not physical controller latency. Stream selection alone retains queued PCM as
previously tested; this batch does not change that policy or video-only pause.

## Capacity protocol

The hidden `[motion-capacity]` fixture plays the original small 25 Hz clip and
changing stereo audio for 1336 active seconds. It saves before and after each
actual replay-byte limit, checking logged audio/video overflow flags. All four
snapshots are captured on one uninterrupted baseline before any load: restoring
earlier would clear replay journals and move the later crossing. Each snapshot
then replays 500 ms against baseline field hashes/times, PCM, callback timing and
IRQ/status/acknowledgement events. The pump-event limit is also reached on that
baseline. The final-source capacity run passes all 26 assertions.

## Completed capacity evidence

The 1336-active-second baseline plus four 500 ms continuations passes on the
production code above. All snapshots are valid; the initial one uses replay and
the latter three use DVA1/DVV1 images. Audio/video overflow flags are explicitly
checked as 0/0, 1/0, 1/0 and 1/1. The 16384-event pump limit is also reached.

| Snapshot | Audio history overflow | Video history overflow | Saved audio bytes | Saved video bytes |
| --- | ---: | ---: | ---: | ---: |
| 1 | 0 | 0 | 8355840 | 8790600 |
| 2 | 1 | 0 | 24024 | 27519 |
| 3 | 1 | 0 | 24024 | 15018 |
| 4 | 1 | 1 | 24024 | 32916 |

Byte lengths after the first snapshot are serialized current state, not historical
stream length. The last snapshot's small size is expected after history overflow.
The run checks 67,017 complete fields and 118,038,062
channel samples. All 100 restored fields, 176400 channel samples, callback
time/count/order and timed IRQ/status/ack observations equal baseline exactly.
Maximum RGB/PCM errors are 9/537; native error
is zero. Both input and presentation remain bounded throughout this fixture.

- Binary SHA-256: `05da09607bccd2d362029ee44b2186dd72779beecb50c8e6bc575f7ed01600b8`.
- Complete log SHA-256: `1ff655510ccbb67e4c4990af3a03bc058f6cd8f776564168ca61cad78e48e496`.
- Command: `/tmp/cdi-capacity-certified "[motion-capacity]" -s`, an unchanged copy
  of the repository integration binary. The temporary copy was removed after the
  run; the retained repository binary still matches the recorded launch hash.
  Production-source hashes also match the committed files.

An earlier corrected-capacity run also passed but preceded the PTS and checked
allocation changes. The final-source run above supersedes it for certification.

## Local gates

| Gate | Result |
| --- | --- |
| Full cdiintegrationtests | PASS: 12120 assertions / 25 cases |
| Full cdihelpertests | PASS: 17405336 assertions / 225 cases |
| Decoder-state unit cases | PASS: 11320 assertions / 4 cases |
| Production CD-i build and mame -validate | PASS, exit 0 |
| DVC DMA liveness | GREEN |
| Regenerated Musashi / include guards / diff check | PASS |
| Focused snapshot ASan with leak detection | PASS: 11320 assertions / 4 cases |
| Full-size standalone PL_MPEG ASan | PASS: all 117 pictures |
| Small/full reference regeneration | Byte-identical |
| Final-source capacity and CI | PASS: 26 capacity assertions; [CI 34164448976](https://github.com/matt-dawidowicz/mame/actions/runs/34164448976) |

The ASan snapshot harness compiles production decoder/bridge code with the actual
snapshot test source. Catch 1.7 needs a harness-local constant SIGSTKSZ=65536 with
current glibc; repository Catch is unchanged. This is focused ASan/leak evidence,
not full-emulator ASan, UBSan, an all-system build or Windows/macOS certification.

Build: `make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests cdihelpertests mame`.
Other commands: `./cdiintegrationtests`, `./cdihelpertests`, `./mame -validate`,
`python3 scripts/cdi_dvc_dma_liveness_audit.py`, and the generator commands above.

## Assessment and remaining scope

Established weights and the 0-4 rubric are preserved. Exact multi-mode native pixels,
composition and corresponding short saves justify limited grade-2-to-3 credit:
MCD212 66.25 to 71.25 raw (65 to 70 rounded), MPEG video 66.25 to 68.75 (65 to 70),
and cross-system video 60 to 65. Save-state capacity/error policy also gains grade
2 to 3 (raw 62.5 to 63.75, still 65% rounded). Other grades remain unchanged;
no grade reaches 4 from this work. No overall/hardware/compatibility percentage
is calculated.

Next: sparse ingress and starvation/refill, discontinuous/arbitrary PTS and actual
queue limits; additional MCD212 interlace/QHY/DYUV/RGB555 and geometry; active
peripheral/mid-field saves; host output and physical/retail evidence. The previous
[30-minute gate](cdi_motion_av_checkpoint_20260907.md) remains prior exact-source
evidence, not a new 30-minute run on this code. Full-size and synchronized scenarios
are short tests. CPU firmware execution, SCC DMA ingress, physical VMPEG behavior
and retained commercial-title gameplay are outside this fixture's scope.

## CI runtime budget correction

The first exact-source attempt, [CI 34163549673](https://github.com/matt-dawidowicz/mame/actions/runs/34163549673),
was cancelled by the configured 12-minute job limit. GitHub's check annotation
explicitly reports that limit. Generated-source freshness, helper build and all
17405336 assertions / 225 helper cases passed; integration compilation finished
and the integration suite ran until the job was terminated. No assertion failure
was reported, but this is not a passing CI result.

The job now permits 18 minutes for the expanded suite. Tests, production code,
compiler settings and cache policy are unchanged. The final local 1336-second
capacity run also passes all 26 assertions on the code commit; the rerun [CI 34164448976](https://github.com/matt-dawidowicz/mame/actions/runs/34164448976) now passes on `30246e5f846ff13a44b9c261051081f273ee0f94` with identical production/tests.
