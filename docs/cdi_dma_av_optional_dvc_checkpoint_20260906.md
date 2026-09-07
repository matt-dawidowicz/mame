# CD-i DVC DMA, A/V-clock, transition, and optional-presence checkpoint — 2026-09-06

> Historical audio-branch checkpoint. Unified START/MTC=0 (65536 operands), held-request re-arm and immediate abort behavior supersede the zero-count/abort descriptions below; see [the unified audit](cdi_unified_verified_status_20260907.md) and [merge record](cdi_branch_consolidation_20260906.md).

Branch: `audio/cdi-fidelity-100-campaign-20260905`

## Scope

This checkpoint closes software-visible DVC DMA and transition regressions that can be proven from the current SCC68070/DVC implementation, strengthens the deterministic A/V clock campaign, and makes the optional physical presence of the Digital Video Cartridge an explicit tested machine property. It deliberately does **not** convert unresolved silicon/DAC behavior into guessed emulator semantics.

It also supersedes three stale open-source-fix statements in the 2026-09-05 audit documents. The current production source already contains those remediations:

1. MCD212 ICA/DCA control fetches use the wrapped `mcd212_control::command_words()` path, so the second word and subsequent control address cannot index one word past the 512 KiB plane array.
2. DVC `mpeg_schedule_packet()` compares PTS/DTS with `current_mpeg_clock90(target)`, the DCLK-advanced MPEG clock, rather than the last parsed SCR alone.
3. CDIC RAM word access uses the endian-explicit `cdicdic_memory.h` helpers rather than host-native `uint16_t *` aliasing.

Those defects are therefore not reimplemented in this batch; the audit text was stale relative to branch source.

## Optional Digital Video Cartridge

The DVC is a physical optional cartridge, so absence is represented as actual device absence rather than a run-time mute/disable bit.

The driver already has the correct split:

- `cdimono1` uses `cdimono1_mem`, leaves `0xd00000-0xefffff` unpopulated, does not instantiate `cdi_dvc_device`, and filters DVC-required software with `!DVC`.
- `cdimono1dvc` and `cdimono1dvc_ntsc` install the cartridge RAM/register map and instantiate `CDI_DVC` with its audio, interrupt, DMA-request, and external-video callbacks.
- `cdi_state::m_dvc` remains an `optional_device`, so code shared by these configurations can distinguish true absence safely.

`tests/emu/philips/cdi_dvc_edge_integration.cpp` now boots synthetic full-machine fixtures with both configurations and verifies that the base Mono-I machine has no DVC subdevice while the DVC-equipped machine does. This is the supported choice for emulating a player with or without the cartridge; no fake electrically-present-but-disabled device mode is added.

## Live SCC68070-to-DVC DMA edge campaign

The existing integration fixture already proved a normal four-word SCC68070 channel-2 transfer into the DVC. The new edge fixture uses the same real register map, SCC device, driver service timer, and DVC FMA command path to cover the state transitions that were previously missing.

### Zero count

The SCC68070 specification/history leaves the exact physical zero-count interpretation unresolved. MAME's current compatibility policy is therefore tested as policy rather than promoted to a silicon fact:

- a DVC request with DMA count zero may latch at the DVC;
- the CD-i driver does not arm SCC channel 2;
- no source address advances;
- no completion IRQ is fabricated;
- the DVC request remains pending because no transfer completed;
- programming a non-zero count and restrobing the request recovers cleanly.

### Partial transfer and abort

The fixture then starts a three-word transfer, allows exactly one production service event, and asserts SCC software abort before the next word. It verifies:

- one word and only one word crossed the SCC-to-DVC boundary;
- MTC and MAC describe the remaining two-word suffix exactly;
- SCC CA clears and COC+ERR assert;
- the configured DMA interrupt asserts;
- the driver service remains scheduled until it observes the SCC abort;
- observing the abort does not consume another word and does not call the DVC completion transition;
- the DVC FMA request bit remains set because the transfer did not complete.

After W1C acknowledgement, the fixture restarts at the first untransferred word. The request remains asserted after the penultimate word and clears only when the exact final word reaches the DVC and SCC MTC reaches zero. This pins partial-transfer conservation, restart, and the completion boundary without claiming cycle-level DREQ or bus arbitration timing.

## Stream-change, end-of-program, and save-visible transition state

`tests/emu/philips/cdidvc_state_transitions.cpp` adds four deterministic gates around the scalar stream-control state that the production device saves:

- all 32 initial streams × all 32 requested streams are snapshotted while a stream change is pending and must commit identically after restoration;
- all 32 streams are snapshotted after ISO program end; descriptor writes cannot reopen the ended input, while explicit abort releases the current stream and admits a following ISO stream;
- 100,000 rapid request/commit/end/abort transitions continue identically from a mid-run snapshot and finish with the same deterministic state hash;
- all 65,536 system-command words are exhausted to prove that play, pause, continue, step, stop, FIFO clear, decoder on/off, and DMA bits are decoded independently.

This closes control-state determinism. It does **not** establish whether physical VMPEG hardware holds, drains, flushes, zeros, or ramps already-decoded PCM at those transitions; those remain section-12 hardware-capture questions.

## A/V clock campaign

The stale-SCR scheduling defect is already absent from production: packet scheduling uses the DCLK-advanced current clock. Existing tests retain a vector in which comparing against stale SCR would produce +100/+80 90-kHz ticks while the live anchored clock correctly produces +10/-10.

The deterministic clock campaign is now stronger:

- every one-second observation from 0 through 1,800 seconds at the Full Motion 44.1 kHz audio rate is compared against independently specified SCR, PTS, and DCLK references;
- sample-clock minus SCR, PTS, and DCLK is zero at every observation, not merely at the 30-minute endpoint;
- 128 passes through twelve adversarial segment lengths exercise reset/seek/pause/branch-style discontinuities, including segment lengths that do not land on integral 90-kHz sample periods;
- each discontinuity obtains a new authoritative MPEG/DCLK anchor instead of feeding an old sample-clock rounding residue into the next segment.

This proves that the measurement and re-anchoring arithmetic itself has no monotonic drift.

### Remaining A/V evidence boundary

The production DVC already owns SCR/PTS/DTS, DCLK, output-frame accounting, and cross-stream telemetry, but the unified `observe_audio_clock()` record is still a test/checkpoint primitive rather than a retained production-run trace hook. Therefore the following remain open and must not be marked as hardware/runtime complete from the synthetic long-run test alone:

- a retained production observation stream comparing emitted audio against SCR/PTS/DCLK;
- a continuous real MPEG A/V fixture or title run of at least 30 minutes;
- repeated interactive FMV branch/scene changes under that instrumentation;
- reset, seek, pause/continue, and stream-change runtime discontinuity traces;
- an acceptance threshold derived from the timing model and verified against actual runtime behavior.

## Save-state boundary

DVC save states reconstruct opaque PL_MPEG decoder state from fixed replay mirrors: currently 8 MiB for audio replay and 32 MiB for video replay, plus bounded PCM/picture mirrors. The new transition continuation tests prove deterministic control-state continuation after a snapshot; existing decoder replay tests cover pre-header, exact-frame, partial-frame, starvation, end, and refill-after-end states.

A save taken after an arbitrarily long uninterrupted compressed stream is a separate architectural problem because the fixed replay journal is deliberately bounded. This checkpoint does not call the replay-storage architecture infinite-history or complete for arbitrary-duration snapshots.

## Completion statement

Software-visible results closed by this checkpoint, subject to a green CI run from the same tree:

- verified that the three formerly reported production defects are already fixed in current source;
- certified true DVC-present versus DVC-absent machine configurations;
- added live SCC68070/DVC zero-count, partial-transfer, abort, acknowledgement, restart, and exact-completion integration coverage;
- added exhaustive/long-run stream-control snapshot and rapid-transition coverage;
- extended deterministic A/V arithmetic to every second of a 30-minute Full Motion run and repeated discontinuity re-anchoring.

Still evidence-blocked or runtime-open:

- exact SCC68070 zero-count silicon behavior and bus-cycle/DREQ timing;
- decoder/DAC queue disposition at physical transitions;
- production-run unified A/V telemetry and a real 30-minute MPEG/title trace;
- active simultaneous A/V save/load of a real long-running stream beyond the bounded replay-history architecture.
