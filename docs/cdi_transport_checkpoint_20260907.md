# CDIC transport and digital PCM checkpoint — 2026-09-07

Baseline: `b742233acc64e368e59ffc5bc3f65be11758d99c`. This follow-up resolves the selected seek-handshake evidence
gap and fixes a separately reproduced PCM routing defect. It does not claim new
physical CDIC measurements or a retail/firmware execution result.

## Seek behavior: follow the driver, preserve the production handshake

The [CDIC reverse-engineering notes](https://github.com/cdifan/cdichips/blob/c1cf836421146bc20b2972d8ee7f608da39030a0/ims66490cdic.md)
derive register information from cdap18x/cdapdriv; they explicitly leave register
details incomplete and do not establish autonomous one-sector seek completion.
The supplied Mono-I ROMs were therefore inspected privately. No ROM bytes or
disassembly are included in this source-only change.

| Input | SHA-256 | cdapdriv ROM offset |
| --- | --- | --- |
| cdi200.rom | `24e388c72df21237a89d8d775d41a90208af24c4112c61429b8e47f190ac18c6` | `0x295c8` |
| cdi220b.rom | `fd123e66beadaf844cb220a44166ea33f9fd0d64bafb9e6399febff445429db2` | `0x295c0` |

Both contain the same `0x4716`-byte cdapdriv module, SHA-256
`cc68eb55cebd88dde8bf0734368f307b7dab2d69381db51cc5aa3c0045f7827b`.
Capstone 5.0.9 was used for private static analysis. The following offsets are
relative to that module, not absolute CPU addresses:

- `0x3b64..0x3be8`: seek setup stores the requested time, sets the whole-second
  fraction flag, installs a completion callback, writes command `0x2c` and DBUF
  `0xc000`, acknowledges stale XBUF and returns asynchronously.
- `0x1a88..0x1aec`: interrupt dispatch checks DBUF enable and XBUF ready, reads
  XBUF to acknowledge it, then calls the registered completion routine.
- `0x58..0x9a`, `0x14f8..0x15c6`, `0x3bfe..0x3c86`: the callback checks status,
  obtains ADR-1 Q from SRAM offset `0x924`, and compares its absolute position to
  the saved requested position. It accepts up to 40 frames before the target;
  it also has a post-target BCD-rollover acceptance path that this fixture does
  not exercise. Other Q/status/error paths can keep waiting.
- `0x171c..0x1770`: completion clears DBUF bit 14 and updates the asynchronous
  request bookkeeping. This is software-driven completion, not proof that CDIC
  hardware autonomously stops after one sector.

The live fixture is an original, equivalent register-level consumer of these
observed interactions. It does **not** embed or execute proprietary driver code.
A request for LBA 673 with whole-second rounding starts at 600 and receives
600..633 before reaching the driver's 40-frame window. A backward request for
475 starts at 450 and can complete on its first packet. The fixture then aborts
a pending seek before acquisition and performs a fresh Mode 1 read at 603..604.
All 37 delivered positions, IRQ assertion/acknowledgement, DBUF request/enable bits
and 150 ms without further Q delivery after each software completion/abort are checked. The
existing six-sector compatibility acquisition delay is retained, not promoted
to a physical servo-timing claim. No autonomous seek-stop change was warranted.

## Reproduced defect: data tracks entering CD-DA PCM

Before: `DISC_CDDA` unconditionally handed every readable main-channel sector
to the PCM receiver, including data tracks. This also populated the pre-start
PCM buffer with data. The new fixture generates audio words `0x1111` and data
markers `0x5555`, enables the real CDIC audio path and captures both real DAC
streams through MAME's existing sound hook. The corrected before-fix corpus has
**8 failed assertions / 72 assertions / 2 cases**; the seek case already passes.

The fix permits PCM handoff only for `CD_TRACK_AUDIO`. Q acquisition, position
advancement and IRQ delivery continue on data tracks. It introduces no queue
flush, analogue mute/ramp rule or transport cancellation. This makes a narrow
image-format distinction; it does not infer physical output-edge timing.

Five PCM scenarios cover audio-to-data, direct data-track start, delayed audio
start after the first data Q packet, data-to-audio, and uninterrupted audio.
Afterward, each channel contains exactly 1,176, 0, 0, 3,528 and 4,704 audio samples
respectively. Data-only cases and the seek case are entirely zero; no data marker
reaches either DAC. All 40 PCM-scenario Q positions still arrive. These are
digital DAC-stream observations, not analogue captures or sustained A/V proof.

## Validation

```
make -j6 -C build/projects/sdl/mame/gmake-linux config=release64 cdiintegrationtests cdihelpertests mame
./cdiintegrationtests "[transport]"
./cdiintegrationtests
./cdihelpertests
./mame -validate
python3 scripts/cdi_dvc_dma_liveness_audit.py
```

The initial fixed transport pair passed 72 assertions; tightened exact-count and
all-zero checks pass **78 assertions / 2 cases** in the complete gate. Full suite:
**11,796 assertions / 19 cases PASS**. Helpers: **17,393,781 / 219 PASS**.
Production CD-i build/validity pass (exit 0); DMA liveness GREEN. Musashi sources
regenerated into scratch compare byte-identical to the committed `.h` and `.cpp`.
The new fixture is included in the existing emulator-linked test executable and
CI trigger; no production test hook or proprietary fixture is added.

Grades remain unchanged because the wider command, error, save/restore and
hardware obligations remain partial. Remaining work: active PCM save/load and
sustained decoded video/audio continuity; other firmware/seek error paths;
physical status/servo/output transitions; multisession and additional containers.
No sanitizer, all-system MAME build, retail playthrough or physical-fidelity proof
is claimed by these results.

## Exact-source certification

Code `adf57f583a861e5573e73fa0519e6d5fc76f288a` passes [CI 34080100953](https://github.com/matt-dawidowicz/mame/actions/runs/34080100953):
219 helper cases / 17,393,781 assertions and 19 integration cases / 11,796
assertions, generated-source freshness and DMA liveness. The local production
build and validity gate also pass. This documentation-only update certifies its
code parent without changing production/test behavior.
