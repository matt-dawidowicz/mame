# DVC sparse ingress, refill and picture timestamps

Date: 2026-09-07. Branch: `cdi-unified`. Reviewed baseline:
`2bdb486f60d784accc252a8ecccf2e7320220f0a`. Verified code: `47dbd619dfbf2b1ec99bd76a0cef8ebbbef0b350`.
This subsequent documentation-only certification records exact-source CI.
[CI 34167219403](https://github.com/matt-dawidowicz/mame/actions/runs/34167219403) passes on `47dbd619dfbf2b1ec99bd76a0cef8ebbbef0b350`: 12201 assertions / 26 integration cases and 17405336 / 225 helper cases, generated-source freshness and DMA liveness.

## Reproductions and production changes

The original live regression exposed two timing failures on unchanged production:
partial initial audio carrying PTS was completed by a later un-timestamped PES,
and audio started early because only the completing packet's timestamp was used.
Video used its first PTS forever, ignoring later picture timestamps. Before fixes,
the repeated scenarios measured maximum PCM error 25633 and RGB error 255.
The original gap fixture also exposed a test assumption about one-field catch-up
after a full decode-ahead queue; that oracle was corrected by refilling earlier,
before more than one queue of pictures was due. It was not called a decoder bug.

Audio now retains the pending initial timestamp until decoded samples exist,
recomputes the wait against current FMA SCR/DCLK and records that retained PTS in
A/V telemetry. Reset/stream replacement clears the pending state. Existing
45 kHz scheduling granularity and queued-PCM preservation remain unchanged.

Video attributes a PES timestamp to its first picture-start prefix, using saved
per-byte packet attribution for prefixes spanning PES boundaries. Timestamp and
IRQ metadata reorder together with reference and B pictures. An explicit picture
PTS renews the backend-relative presentation anchor; un-timestamped pictures
retain cadence. The fixed save mirror now stores packed interrupt/PTS metadata.
All new scalar/prefix/reference fields are registered for save/load.

The attribution rule is supported by the MPEG systems definition of video PTS in
[ITU-T H.222.0 (2000), section 2.4.3.7](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.222.0-200002-S%21%21PDF-E&lang=e&type=items):
the relevant access unit contains the first picture prefix beginning in the PES.
This cross-check supports timestamp association; it is not a complete MPEG-1
systems or physical VMPEG conformance certification.

## Original reference protocol

Reuse the unchanged original 352x288, 25 Hz, 36-picture I/P/B fixture and 98-frame
44.1 kHz stereo MP2 stream. FFmpeg RGB/PCM reference provenance and hashes are in
the [full-size checkpoint](cdi_capacity_full_av_checkpoint_20260907.md).
The actual closed GOPs in this source contain ten pictures (the final GOP six),
despite the encoder's maximum GOP setting of twelve. The oracle reads each
picture's temporal reference and the known closed-GOP boundary to assign display
position; it does not read production presentation timestamps.

All scenarios split a PES timestamp between 0 and 100 ms, then put the first byte
of the first picture prefix in one PES and its remaining bytes in another at
200 ms. The initial timestamp is 300 ms. DMA fragments are word-aligned, and
only complete PES packets receive any trailing scan padding. Audio's first
timestamped PES contains only 200 compressed bytes; its remainder arrives at
100 ms. Other initial audio frames end at half the reference (56448 stereo
frames), causing genuine silence before the refill header at 1900 ms and its
remaining bytes at 2000 ms.

| Scenario | Video | Empty audio queue refill |
| --- | --- | --- |
| 1 | First ten coded pictures at 200 ms; refill at 1200 ms; hold display picture 7 while the final B/reference await more input | No PTS; starts after the 2000 ms refill |
| 2 | Per-picture PTS; +500 ms at display picture 12, reduced to +300 ms at picture 24 | Late PTS of 1900 ms; starts after the 2000 ms refill |
| 3 | SCR origin 2^33-18000; PTS only on pictures 0/12/24, same steps, inferred cadence across wrap | Future wrapped PTS; waits until 2400 ms |

Only the initial SCR is supplied. Later PTS packets do not reset the stream clock.
The backward timestamp step exercises the existing ordered queue's due-picture
supersession policy. This is a tested emulator scheduling policy, not hardware
frame-drop calibration. Queue telemetry must show 36 decoded pictures, maximum
depth 26, no untimestamped fallback and zero remaining pictures after draining.

MAME's inclusive stream update has already rendered the refill's current sample;
the reference therefore uses the existing next-sample (+1) boundary convention,
also used by explicit reset tests. This is at most one sample of phase, with no
cumulative shift. Future-wait arithmetic was not changed to conceal that boundary.
Whole 768x560 composed output retains exact native CLUT8/matte/cursor pixels and
compares external video to the original RGB reference. Audio samples compare to
the original continuous-synthesis FFmpeg reference across the input gap.

Five saves per scenario target blanking after 50, 150, 1800, 1950 and 2300 ms:
partial PES timestamp, split picture prefix, audio starvation, partial refill PES
and output after refill or during a future wait. Every load repeats the remainder
of the 4400 ms scenario against original field hashes/times, PCM, callback
time/count/order and 1 ms IRQ assertion/status/acknowledgement observations.

## Validation and scope

Local production gates pass. The final three refill variants pass 81 assertions
in one integration case. They check 2,991 complete fields and
5,329,050 channel samples; all 15 saves reproduce
2,346 fields and 4,164,804 channel samples exactly.
Maximum RGB/PCM error is 10/537 and native pixels are exact. The full production
integration run passed 12201 assertions / 26 cases before the final test-only
absent/late audio refill variants; the final targeted run above covers those.
Helpers pass 17405336 assertions / 225 cases. CD-i production build/validity,
DMA liveness, Musashi freshness, include guards and diff checks pass.
[CI 34167219403](https://github.com/matt-dawidowicz/mame/actions/runs/34167219403) passes on `47dbd619dfbf2b1ec99bd76a0cef8ebbbef0b350`: 12201 assertions / 26 integration cases and 17405336 / 225 helper cases, generated-source freshness and DMA liveness.

- Final targeted binary SHA-256: `bbc66fd3bb44c062fbaef0a9160e2a994c7b17f01f5127115e23a261972a4849`.
- Final targeted log SHA-256: `340b7c9e036c4e65bab336e9cf8899f99636b1a99dbef33132ec042f2736f8db`.


Commands: `make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests cdihelpertests mame`,
`./cdiintegrationtests`, `./cdiintegrationtests "[motion-ingress]"`,
`./cdihelpertests`, `./mame -validate`, the DMA liveness audit, regenerated Musashi
comparison, include guard checks and `git diff --check`.

All worksheet weights/grades remain unchanged. Broader scheduling obligations
still include queued-PCM timestamp changes, independent SCR jumps and actual
pending-input/PCM capacity failure paths. This fixture executes live device,
screen and audio timers with the CPU suspended and direct DVC DMA-port ingress;
it does not execute firmware or certify SCC transfer scheduling. Physical clocks,
host DAC/video output, retail gameplay, other rates, malformed/open GOPs and
arbitrary mid-field saves remain unverified here. Existing long-capacity,
30-minute and focused ASan results retain their previous exact-source scope;
this checkpoint does not claim fresh long-playback or full-emulator ASan/UBSan.

Next: queued-audio timestamp discontinuities and independent SCR jumps, using
before/after reference output; then broader malformed/input-capacity and MCD212
interlace/QHY/DYUV/RGB555 scenarios.
