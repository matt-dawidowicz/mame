# Active CD-DA save/load continuity — 2026-09-07

Baseline: `6d8b0704491edced64656e4a460206261612a7e5`.
The new emulator-linked regression passes against the existing production code.
No CDIC, DAC or shared sound implementation change was warranted by these results.

## Method and evidence boundary

The original synthetic CUE/BIN generator now supports a changing stereo pattern
and a pre-emphasized track. Left and right channels differ, and every sector
contains changing samples. No retail media, ROM bytes or captured assets are used.
Existing constant-PCM and Q/container scenarios retain their previous inputs.

The fixture loads a disc into the real Mono-I CDIC, writes PLAY CDDA/DBUF/AUDCTL,
and consumes Q through the same memory-mapped SRAM and XBUF acknowledgement path
as the existing transport fixture. The CPU is suspended; this is a live device
and register-consumer test, not execution of proprietary firmware.

After at least one sector of nonzero audio has reached the real DAC sound hooks,
the fixture schedules a file-backed save through MAME's normal save lifecycle.
It advances uninterrupted playback to 400 ms, then schedules a load outside the
timer callback and repeats the saved continuation to 400 ms. Saving and loading
inside a running timer with `ram_state` would expose the callback's old time to
postload callbacks, so this test deliberately uses the normal scheduled path.
Postload verifies restored machine time, consumer position and the production
driver's saved CDIC interrupt line. Test result vectors and run number are not saved.

Comparisons include every stereo DAC sample, callback time and buffer length;
each 1 ms polling tick, XBUF, DBUF, interrupt before/after acknowledgement and
IRQ owner; all twelve Q words on each ready packet; and decoded disc positions.
Independent checks reject skipped/repeated LBAs, incorrect track/ADR/emphasis,
missing active audio, a constant or silent comparison, and missing save/load calls.
The first sound-hook block includes the saved, unconsumed buffer prefix before
the snapshot instant; both runs compare that same complete buffer without trimming
or shifting samples to manufacture agreement.

| Scenario | Start LBA | Save time | Snapshot IRQ | Compared samples per channel | Nonzero samples per channel |
| --- | ---: | ---: | --- | ---: | ---: |
| Changing stereo audio | 450 | 143 ms | Acknowledged | 11,466 | 11,466 |
| Pending Q packet | 450 | 134 ms | Asserted | 12,348 | 12,348 |
| Audio into data track at LBA 600 | 590 | 143 ms | Acknowledged | 11,466 | 3,820 |
| Pre-emphasized stereo audio | 450 | 143 ms | Acknowledged | 11,466 | 11,466 |

All **93,492 compared channel samples match exactly**. No tolerance, offset
search or ignored leading silence is used. At least nineteen Q packets are
compared in each continuation. This covers short active CD-DA continuation,
buffered DAC output, filter history, pending Q and the audio-to-data transition.
It does not prove analogue output, host audio-device latency, sustained decoded
MPEG A/V, other active CDIC modes, firmware playthrough or physical timing.

## Comparison sensitivity

A temporary test-only postload probe disabled/re-enabled both DACs, deliberately
discarding their restored queues. It produced **8 failed / 96 assertions**:
every scenario and channel detected a PCM mismatch, even though sample counts
were unchanged. First differences were at sample 132 (143 ms saves) or 617
(134 ms save). The probe was removed, the exact fixture restored and rebuilt,
and all 96 assertions passed again. These injected failures demonstrate the
comparison's sensitivity; they are not evidence of a production defect.

## Validation

```
make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests
./cdiintegrationtests "[pcm][save]"
./cdiintegrationtests
./cdihelpertests
./mame -validate
python3 scripts/cdi_dvc_dma_liveness_audit.py
git diff --check
```

- Focused regression: **96 assertions / 1 case**, four scenarios, PASS.
- Complete emulator-linked suite: **11,892 assertions / 20 cases**, PASS.
- Helpers: **17,393,781 assertions / 219 cases**, PASS.
- Existing unchanged production emulator validity: exit 0; DMA liveness GREEN.
- Test translation unit rebuilt after removal of the sensitivity probe.
- Existing CI path filters include the new test; it is included by the established
  integration support translation unit and registered in its sorted driver list.

Scoped grades remain unchanged: one short CD-DA mode comparison does not close
the broader CDIC active-mode, transport-error or system save-state obligations.
Next: sustained decoded pictures and audio, scene/stream transitions and their
save/load continuity; broader CDIC command/error and active peripheral scenarios.
No fresh sanitizer or all-system MAME build is claimed.
