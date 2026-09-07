# Moving decoded A/V checkpoint — paused 2026-09-07

The user requested saving progress for shutdown. The new moving-video regression
passes the full local integration/helper gates. The uninterrupted run was stopped
at the user's request after its last complete progress report at **960 seconds
(16 minutes)**. This is **not a completed 30-minute gate**. New work is saved
locally; publication and exact-source CI remain pending.

Baseline: `a5b2ff9050352461da5d323cc550767d379e8197`, branch `cdi-unified`.
Code checkpoint: `eb4fbe922c06d79eae0e48d08d3d2ae4356e091b`. This subsequent documentation-only save records the exact local source; it does not certify pending CI or the interrupted duration gate.
No production emulator changes were needed in this batch. Initial fixture
failures came from incorrect geometry, palette and queued-audio assumptions;
they are not reported as emulator defects.

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
| Six-second `[motion-long]` smoke | PASS: 26 assertions |
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

## Interrupted uninterrupted-playback gate

The hidden `[motion-long]` case defaults to 1800 seconds of active playback after
the 300 ms initial timestamp. It repeats the original 25 Hz clip and changing
audio without decoder resets, pause, stream branches or save/load. The other
two formats are covered by the short scenarios, not this duration gate.

Last complete report before intentional termination:

```text
MOTION_LONG seconds=960 fields=48071 max_rgb=9 max_pcm=537 samples=84672002
```

No failure was reported before interruption. This progress line includes the
initial 300 ms timeline prefix; it does not certify 960 seconds of active output
or a completed test. The process has stopped and must start from zero tomorrow.
Long-run decoder replay capacity and saving after journal overflow remain open.
Do not infer long-run saveability from uninterrupted playback.

## Resume next

1. Read this checkpoint and inspect branch/HEAD/worktree status. Keep the current
   source; the older scratch fixture drafts are stale.
2. Run the full uninterrupted gate from the repository root:
   `./cdiintegrationtests "[motion-long]" -s` with `CDI_MOTION_SECONDS` unset.
   Record final assertions, fields, samples, errors and completion. Do not promote
   the interrupted 16-minute progress report to a 30-minute pass.
3. Review the final source and update affected weighted worksheet obligations.
   All current percentages are deliberately carried unchanged pending completion
   and review. Broad MCD212 modes, host output, physical calibration and retail
   compatibility remain open.
4. Verify origin is the authorized `matt-dawidowicz/mame` fork and publish the
   coherent commits to `cdi-unified`; wait for exact-source CD-i fast CI.
5. Record the code hash and actual CI/long-run results in a subsequent
   documentation-only certification, then push that certification.

Local build command used:
`make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests`.
An optional `/tmp` binary with only the test observer built at `-O2` passed a
six-second smoke but offered no useful speed improvement. The interrupted long
run used the normal repository binary. No optimized binary is required to resume.
