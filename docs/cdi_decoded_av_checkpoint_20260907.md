# Decoded DVC video/audio continuity — 2026-09-07

Development baseline: `85ff4e7560b662a36b5089b628c48f6105e17bdf`, on
`cdi-unified`. Exact code and CI certification are recorded in a subsequent
documentation commit, avoiding a self-referential hash.

## Reproduced failures and fixes

The original live DVC reference fixture presented only 62 of 64 pictures at each
scene end. A standalone ring-buffer decode reproduced 62 pictures with EOF still
false. `plm_buffer_signal_end` recorded the current buffer length, but compaction
removed consumed bytes without moving that end position. Keeping the explicit
ring end aligned restores EOF and the last coded picture: 63 pictures.

A second condition suppressed the held reference picture when the last coded
picture was a B picture. That reference belongs after the final B picture in
display order. Returning the pending reference at EOF regardless of the previous
picture type restores all 64 pictures. The pending flag is cleared so repeated
decode calls cannot emit it twice. Ring writes reopen the dynamic source; an
explicitly ended empty ring latches EOF. File and fixed-memory compaction behavior
is unchanged. Whole-buffer and split/refilled-buffer tests cover end detection,
starvation, reopening and exactly-once final reference delivery.

Scheduled active save/load also reproduced a periodic IRQ phase error: events at
1046, 1056, ... ms in uninterrupted playback shifted to 1047, 1057, ... after a
1037 ms snapshot. The scheduler already restores the DVC timer deadline and
period. Removing its redundant re-arm from successful `save_state_postload`
preserves that phase. Reset/control and failed-restore handling retain their
timer updates. The final fixture compares every polled status/IRQ event exactly.

No audio production fix was warranted. Initial silent output was a fixture
setup error: FMA powers up muted. The final fixture programs straight-through
gain through the real FMA ports. Its sound observer follows MAME's inclusive
current-sample buffer convention (`sound_stream::update`), with fixed absolute
sample positions rather than correlation or an offset search.

## Original references and reproducibility

`scripts/cdi_generate_av_reference.py` creates the complete source fixtures and
independent FFmpeg references. This run used FFmpeg `8.0.1-3ubuntu2`, its MPEG-1
video encoder, native `mp2` encoder and fixed-point `mp2` decoder. Regeneration
with that installation produces an identical checked-in header (SHA-256
`eb90d1e1fb926f15c68be43a26b13e7062ad3090c4b6a6c82ad4b43cab14adbb`).
Other FFmpeg versions may produce different assets; the generator records its
actual version. FFmpeg is needed to regenerate references, not to run the tests.

- Two 32x32, 25 Hz, 64-picture video scenes contain I/P/B pictures. Four flat
  macroblocks change luma independently; scene chroma differs. Encoding uses
  one thread, GOP 12, two B pictures, quantizer 2 and disabled adaptive scene cuts.
  Every reference picture includes all 1024 pixels. RGB conversion uses FFmpeg's
  `neighbor+bitexact` scaler flags.
- An original integer phase/amplitude-modulated stereo waveform produces 98
  distinct MPEG-1 Layer-II frames at 192 kbit/s and 44.1 kHz. Both channels change
  across the scene. Its 112,896 sample pairs span the same 2.56 seconds as the
  video. The final fixture does not repeat one compressed audio frame.
- RGB and signed-16-bit little-endian PCM references are zlib-compressed textual
  arrays in `tests/emu/philips/cdi_dvc_av_reference_data.h`. All media is generated
  original test content; no retail image, ROM, capture or extracted asset is used.

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| VIDEO_0 | 2381 | `7966f91cba9787f8a77e54af93769616e4badb545aa18b5f40292504531c1828` |
| RGB_0_Z | 2075 | `4501d285aace09608c9afaf9442a66b9e3e7d0ab54c32ff1f7ddee24c1e95ef8` |
| VIDEO_1 | 2339 | `ee699cff4449ab3091f71b79bdd05cad2584dabb83cc62c92d21352e555a3b43` |
| RGB_1_Z | 2080 | `56da8605f5a5370d5f9db72e583f40517a6092cd9d557ab773f83b69f66948df` |
| AUDIO | 61440 | `1a14adaf0e4381912897dac25c1c0b27d27a6ec002e63587f621fb38b576a2bd` |
| PCM_Z | 431918 | `fa5d30be00b19365a0b69d52eec45735de50c32b2f5ea6ab7970aabbd85c4e5b` |

## Live path and comparisons

The fixture uses the full Mono-I DVC configuration with an erased test ROM and
suspended CPU. Real FMA/FMV register commands select transfers; original MPEG
pack/PES payloads enter `dma_w`/`dma_done`, reach the production demux/decoders,
queues, timestamp scheduler, DVC sound stream and overlay output. It does not
execute SCC DMA or proprietary firmware. Separate existing fixtures cover the
SCC handshake. Production screen updates drive vblank presentation.

The observer reads DVC overlay scanlines with an explicit all-true external mask,
then compares every pixel against the appropriate independent reference. This
tests the DVC overlay output; it does not validate the complete MCD212 matte,
composition, cursor or host display. Reference order, full-frame hashes and
presentation times reject missing or out-of-order changed pictures. A picture
must arrive from its timestamp through 21 ms later, allowing one vblank and the
observer's 1 ms polling interval.

Twelve scheduled scenes alternate the two video streams; each resets both
decoders and restarts the changing audio sequence. Playback starts at 200 ms and
advances to 31 seconds. The sound hook compares all active PCM against the
independent reference at fixed absolute sample positions. The first reset occurs
after the current sound sample has already rendered: sample 121716 is explicitly
checked as silence, and subsequent scenes begin one sample later. That one-sample
boundary offset does not accumulate. It is a characterized software reset edge,
not proof of seamless hardware switching.

A normal scheduled, file-backed save at 1037 ms captures active decoded pictures,
queues and audio. Playback continues to 31 seconds, loads the snapshot and repeats
the continuation to 31 seconds. Test result vectors remain outside saved state.
The restored run checks exact picture hashes/times, every channel sample, sound
callback time/count/order and 1 ms IRQ/status observations. Enabled interrupt
status must agree with assertion and clear on acknowledgement.

| Observation | Result | Required bound |
| --- | ---: | --- |
| Baseline presented pictures | 768 | All 64 per scene, 12 scenes |
| Restored pictures | 747 | Exact saved continuation |
| Maximum RGB channel error vs FFmpeg | 2 | 3 |
| Maximum picture lateness | 19 ms | 21 ms |
| Active reference channel-sample comparisons | 5,346,682 | More than 5,000,000 |
| Maximum PCM error vs FFmpeg | 537 | 600 signed-16-bit units |
| PCM RMS error vs FFmpeg | Approximately 128.37 | 150 |
| Exact restored channel-sample comparisons | 2,644,236 | No difference |
| Exact restored sound callbacks | 1,499 | Same time, count and order |
| Scheduled saves / loads | 1 / 1 | Exactly 1 / 1 |

The PCM tolerance bounds the difference between these software decoders; it is
not bit-exact MPEG audio or physical DSP/DAC certification. Save/load comparison
itself has no tolerance. Repeated scenes extend queue/reset/replay observation;
they do not multiply the number of unique reference streams.

## Validation

```
python3 scripts/cdi_generate_av_reference.py
make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests cdihelpertests mame
./cdiintegrationtests "[decoded]" -s
./cdihelpertests "[eof]"
./cdiintegrationtests
./cdihelpertests
./mame -validate
python3 scripts/cdi_dvc_dma_liveness_audit.py
git diff --check
```

- Focused decoded A/V: 20 assertions / 1 case, PASS.
- Focused ring/EOF regressions: 235 assertions / 2 cases, PASS.
- Complete integration suite: 11,912 assertions / 21 cases, PASS.
- Complete helper suite: 17,394,016 assertions / 221 cases, PASS.
- Production CD-i build and validity: PASS / exit 0; DVC DMA liveness GREEN.
- Reference regeneration is byte-identical. A standalone 64-picture ring decode
  compiled with AddressSanitizer returns 64 pictures and EOF without an ASan
  finding. This is a focused decoder check, not full-emulator ASan or UBSan.
- The existing CD-i CI path filters and integration registration include the
  new fixture. Production EOF defects and timer-phase mismatch supplied failure
  evidence before the fixes; intermediate audio setup/indexing errors are not
  presented as production defects.

## Remaining obligations

This closes a bounded original-reference development batch. It is 31 seconds of
baseline playback plus replay, not the separate 30-minute uninterrupted decoded
gate. Flat macroblocks provide limited AC/motion coverage. Varied textures,
motion vectors, dimensions/rates, malformed video, interactive pause/branch/stream
selection, snapshots at more codec/scene phases and replay-capacity recovery need
additional fixtures. Full MCD212 composition, host-device drift, analogue output,
physical board timing and retail-title gameplay remain unverified here. No
Dragon's Lair gameplay fix is claimed.

MPEG video's limited independent-reference obligation gains grade 1, moving its
weighted estimate from 55% to 60%. Continuous decoded A/V gains limited grade 1,
moving A/V synchronization from 45% to 50%. Other grades remain unchanged with
updated evidence. Hardware fidelity and compatibility are not assigned invented
percentages.

## Exact-source certification

Exact-source [CI 34085809119](https://github.com/matt-dawidowicz/mame/actions/runs/34085809119)
passes for `ee2a74166d8ffa54bf7602bbf4cb47889e43eb62`: 21 integration cases / 11,912
assertions and 221 helper cases / 17,394,016 assertions, generated Musashi freshness
and DVC DMA liveness. The local production CD-i build/validity and focused decoder
ASan check also pass. This subsequent documentation-only certification changes no
production or test behavior; full all-system/Windows/macOS and physical/retail
validation are outside this gate.
