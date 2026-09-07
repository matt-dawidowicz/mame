# Moving decoded A/V checkpoint — completed 2026-09-07

The moving-video regression and full 30-minute uninterrupted playback gate pass.
No production emulator change was needed in this batch. Initial fixture failures
came from incorrect geometry, palette and queued-audio assumptions and are not
reported as emulator defects.

Baseline: `a5b2ff9050352461da5d323cc550767d379e8197`, branch `cdi-unified`.
Code: `eb4fbe922c06d79eae0e48d08d3d2ae4356e091b`. The local duration run and
[CI 34144638170](https://github.com/matt-dawidowicz/mame/actions/runs/34144638170) test `d07ba8c1768ad6814f042696c00ab24fbf72ff76`,
its documentation-only child. Production and test trees are identical between
these commits. This subsequent documentation certification records their results.

## Implemented coverage

- `scripts/cdi_generate_motion_reference.py` creates original textured, panning
  MPEG-1 video with moving patches and chroma gradients. No retail assets or ROMs.
- `tests/emu/philips/cdi_dvc_motion_reference_data.h` retains encoded streams,
  compressed complete FFmpeg RGB references, picture types and steady PCM.
- `tests/emu/philips/cdi_dvc_motion_integration.cpp` runs the real DVC and MCD212
  screen composition in a full test machine with a suspended CPU and erased ROM.
  Real FMA/FMV commands and `dma_w`/`dma_done` are exercised; this fixture does
  not execute firmware or the SCC DMA engine.
- `cdi_dvc_dma_test_support.cpp` registers the fixture and sound observer.

| Profile | Size | Rate | Pictures |
| --- | --- | --- | ---: |
| 0 | 64 x 48 | 25/1 | 50 |
| 1 | 80 x 64 | 30000/1001 | 60 |
| 2 | 96 x 48 | 24000/1001 | 48 |

Each original clip contains I/P/B pictures, closed GOPs of 12 and two B pictures.
FFmpeg 8.0.1-3ubuntu2 generates independent full RGB references with
`neighbor+bitexact`. The existing original changing stereo MP2 fixture supplies
98 frames, 112896 sample pairs, 44.1 kHz and 192 kbit/s. Repeated decoding supplies
a separate steady PCM reference for uninterrupted playback.

Reference regeneration is byte-identical. Header size: 14586265 bytes; SHA-256:
`24757392eaee7db30523276b61622a19a66e7c085db016c03df89e9bc0423415`.
Individual array hashes are retained in the header. FFmpeg is only needed for
regeneration, not normal test execution.

Every complete visible 768 x 560 composed bitmap is checked. The fixture uses
one CLUT8 plane, weight, matte boundary, external DVC window and native cursor.
Native pixels must be exact; decoded RGB permits maximum error 12 and an integer
mean squared channel error at most 4 over decoded pixels. Field cadence follows
the actual screen period, with at most one 90 kHz tick of rounding. PCM maximum
error is 600 and integer mean squared error is at most 22500. These software
decoder tolerances are not silicon fidelity measurements.

The 4.4-second scenarios exercise all three initial formats, video pause/resume,
format/stream changes, rejected old stream IDs and ten scheduled file-backed
save/load checkpoints. Captured picture phases include I, P, B, paused output,
the cleared scene boundary and the final-picture region. Complete field hashes
and times, PCM, sound callback time/count/order, and 1 ms IRQ/status observations
must repeat exactly after restore.

The fixture preserves the current model: FMV pause holds video while audio and
the clock continue; resume catches up to the due picture. Audio stream selection
retains already queued PCM before playing the new stream. Thus a video branch
does not imply an immediate synchronized audio branch. Hardware pause/flush
semantics and seamless interactive branching remain unverified.

## Completed local validation

| Gate | Result |
| --- | --- |
| Full `./cdiintegrationtests` | PASS: 11990 assertions, 22 cases |
| Full `./cdihelpertests` | PASS: 17394016 assertions, 221 cases |
| `./mame -validate` | PASS, exit 0; production code unchanged |
| DVC DMA liveness | GREEN |
| Full 1800-second `[motion-long]` run | PASS: 26 assertions |
| Exact-source CD-i fast CI | PASS: helpers, integration, Musashi freshness, DMA liveness |
| Reference regeneration | Byte-identical |
| Standalone production PL_MPEG ASan probe | PASS: all 158 original pictures |

The short fixture initially passed 69 assertions; subsequent native-pixel,
decoded-error and cadence checks bring its contribution to 78, included in the
full 11990-assertion gate above. The prior detailed run captured 1581 restored
fields and 2795940 restored channel samples across ten saves/loads. No behavior
changed when the extra checks were added.

Independent standalone whole-picture decoder comparison measured max/RMS RGB
errors of 11/1.55567, 11/1.57085 and 10/1.56321 for profiles 0, 1 and 2. ASan scope
is this standalone decode, not the full emulator; no UBSan or physical/retail
validation is claimed.

## Completed uninterrupted-playback gate

Command: `./cdiintegrationtests "[motion-long]" -s`, with `CDI_MOTION_SECONDS` unset.
The test completes at 1800.3 seconds of machine time, providing 1800 seconds of
active output after the initial 300 ms timestamp. It repeats the original 25 Hz
clip and changing audio without decoder resets, pause, stream branches or saves.
Other sizes/rates are covered by the short scenarios, not this duration gate.

- Started UTC: `2026-09-07T16:13:41.522551+00:00`.
- Finished UTC: `2026-09-07T16:43:42.051539+00:00`; wall duration 1800.465 seconds.
- Exit 0; all 26 assertions pass.
- Complete composed fields: 90,154; compared visible pixels: 38,773,432,320.
- Compared channel samples: 158,786,462, including the initial timeline prefix.
- Maximum decoded RGB error 9; decoded-channel RMS 1.557458; native pixel error 0.
- Maximum PCM error 537; all PCM mean-square, field cadence and IRQ/status checks pass.
- Binary SHA-256: `7c75dad96a6276e27bb5281d0d756f9bf21532f694cf518dd7b7c738a2a2ae82`.
- Complete log SHA-256: `220bba888519d7231d96846c5c58b95f27bd648e47820f58ed2d731c1fd28e9a`.

The prior run was intentionally stopped for shutdown at its 960-second progress
report. This completed run restarted from zero and supersedes that incomplete
duration evidence. The normal repository binary was used. The 30-minute case is
hidden from normal CI; default CI ran all 22 short integration cases separately.

## Scoped assessment changes

Weights and the established 0-4 rubric remain unchanged. Moving decoded output,
independent frame references and diverse snapshot phases now provide meaningful
verification; the completed duration run closes the previously absent long-run
software evidence. No grade reaches 4 from this work.

| Subsystem | Previous raw / rounded | Current raw / rounded |
| --- | ---: | ---: |
| MCD212 display | 65 / 65% | 66.25 / 65% |
| DVC overall | 70 / 70% | 75 / 75% |
| MPEG video decode and presentation | 58.75 / 60% | 66.25 / 65% |
| A/V synchronization | 51.25 / 50% | 61.25 / 60% |
| Save states | 57.5 / 60% | 62.5 / 65% |
| Cross-system video | 53.75 / 55% | 60 / 60% |

All other grades are carried with updated evidence where relevant. There is no
overall average across overlapping rows and no hardware/compatibility percentage.

## Remaining work and exact next batch

Reproduce active save/load just before and after the 8 MiB audio / 32 MiB video
replay-journal limits. Passing uninterrupted playback does not prove saveability
after overflow. Define the continuation/error policy from a live trace before
changing it. Ten short snapshots are certified; long-duration snapshots are not.

Continue full-size/GOP/error/underflow references, wider long-run rates and broader
MCD212 native/interlace/QHY configurations. This fixture tests one display setup.
Reference-backed synchronized branch latency, host-output drift, physical board
calibration and retained retail gameplay remain open. Firmware execution and SCC
DMA ingress are outside this fixture's scope; existing separate DMA gates remain.

Build command: `make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests`.
No additional production build is required for this documentation-only completion.
