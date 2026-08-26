# Philips CD-i modernization status

Last reviewed: 2026-08-26

Project branch lineage: `cdi-dvc-modernization`
Audit promotion target: `cdi-project-audit-20260826`

Phase-A checkpoint: `63fbd31f6934cbd60956b1a0899d75ea5c31a871`
Phase-B checkpoint: `2b05d1314a3fd2de2c5d03216b9a732f376ba6c9`
Phase-C checkpoint: `a639c7135ddb9ef1b2bdc770e759fe2ef4d6576e`
Phase-D checkpoint: `eff3cd54b38b0db6023ea488d0908f254a802d13`

This is a living engineering record. Percentages are conservative implementation-confidence estimates derived from the current source, focused tests, and available runtime evidence. They are not compatibility ratings and do not imply that every title or firmware path has been exercised.

## Evidence labels

- **H** — SCC68070 product specification or other primary hardware documentation.
- **S** — current source, git history, or an upstream MAME implementation change.
- **T** — automated test coverage in this checkout.
- **R** — observed emulator/firmware/runtime behavior tied to a named configuration.
- **C** — deliberate compatibility model; useful behavior without adequate hardware proof.
- **U** — unknown or not implemented; no inferred behavior should be added without evidence.

Primary SCC68070 reference: Philips SCC68070 product specification, April 1993, archived by the [International CD-i Association](https://www.icdia.co.uk/docs/). The archive warns that surviving CD-i documentation can contain inaccuracies or inconsistencies; conflicts are recorded below rather than silently resolved.

## Phase A checkpoint

Scope: stabilize and test the SCC68070 internal-peripheral model while retaining SCC ownership of DMA state and transfers. This checkpoint does not add game-specific exceptions and does not move DVC DMA state into the DVC device.

Implemented in this checkpoint:

- one deterministic reset path for device reset and the CPU RESET callback, including DMA/MMU state and emulation queues;
- documented interrupt arbitration order encoded in a testable helper;
- I2C IDR read/write side effects: set PIN, clear AL/AAS, clear the pending I2C interrupt, and suppress debugger-read mutations;
- UART fixed USR bit, self-clearing miscellaneous commands, receive/transmit reset interrupt handling, and receive-queue drain state;
- Timer 0 reload writes no longer re-phase the live free-running timer;
- channel-2 DMA MAC/DAC sequencing, 24-bit wrap, and reserved-mode rejection before a partial transfer;
- MMU status/control word lane composition and documented control, length, and base masks;
- focused SCC68070 helper tests covering DMA, IRQ, I2C, UART, and MMU boundaries.

## Phase B checkpoint

Scope: modernize the MCD212 display pipeline from primary MCD212 and May 1994 Green Book evidence while preserving the doubled-raster half-line model and DVC external-video handoff. This checkpoint does not contain title-specific exceptions and has no commercial-title runtime claim.

Implemented in this checkpoint:

- Extended Case QHY (`CM1=1111`) decoding for plane B when paired with normal-resolution DYUV in plane A;
- the specified QHY single-pair, finite-run, and zero-run-to-end-of-line representations, including malformed-stream bounds;
- separable horizontal/vertical linear interpolation of the normal DYUV Y/U/V components before matrixing, followed by independent signed RGB QHY deltas;
- unmasked 8-bit QHY levels in the lowest eight entries of CLUT bank 2 while retaining the MCD212 six-MSB CLUT output model;
- distinct 50 Hz 280-line, 50 Hz `ST=1` 240-line, and 60 Hz 240-line active windows;
- integral non-interlace totals and 312.5/262.5-line interlace totals, plus PA-dependent half-line DCA phasing;
- ICA at field start, documented odd/even ICA pointers, 32/64-byte DCA windows, active-line-only DCA scheduling, and DI1/DI2 interrupt gating;
- longword-aligned CPU DCP access and documented masks for CSR, DCR, DDR, image-control, cursor, matte, mosaic, and weight fields;
- deterministic reset/save coverage for QHY line state and an explicit cursor-blink reset;
- focused tests for timing profiles, ICA/DCA/IRQ control, QHY stream vectors, 2-D interpolation, 8-bit quantizer deltas, and external-video eligibility.

## Phase C checkpoint

Scope: make the Mono-I CDIC HLE an explicit, auditable command/sector/buffer/audio/IRQ state model without attempting an unsupported MCU LLE rewrite. No title-, LBA-, BIOS-, or checksum-specific path was added.

Evidence boundary: no public IMS66490 data sheet was found. Board-level interrupt, DMA, clock, SRAM, and audio-DSP topology is supported by the Philips CD-i 210/20/25 service manual. Register observations, first-buffer order, command traces, and AUDCTL/IRQ behavior are independently recorded in the MiSTer CD-i core at revision `8894b795f0b85956127372e5172b73278aea3655`; those are **R/S**, not a substitute for a chip specification. The six-sector seek delay and synthesized TOC remain explicit **C** behavior.

Implemented in this checkpoint:

- a testable command descriptor separating start, immediate stop, and stop-after-next-physical-sector effects;
- explicit idle/seeking/reading transport state with save/reset coverage;
- Mode 2 file/channel/message/EOF/EOR/TRIG filtering as a pure decision, separate from physical LBA advancement;
- independent data and audio double-buffer selectors: first data delivery uses buffer 1, while first audio delivery uses buffer 0;
- CPU-visible buffer completion separated from audio decode/playback;
- XBUF IRQ gated by DBUF enable, ABUF IRQ gated by AUDCTL bit 13, and read-to-clear source acknowledgements;
- AUDCTL bit 0 changed from a synthetic read toggle to decoder-termination read-to-clear status;
- failed sector reads terminate the HLE command instead of delivering a fabricated zero sector;
- XA predictor arithmetic and supported coding duration moved to tested pure helpers without changing the established coefficient model;
- DSEL and new transport/buffer selectors receive deterministic reset values.

### CDIC command audit

| Command | Implementation | Timing / completion / IRQ | Evidence and remaining boundary |
| --- | --- | --- | --- |
| `0x23` Reset Mode 1 | Partial: does not start a read; while active, the live command stops after the next physical sector. | Stop is checked after physical arrival even when filtering suppresses delivery. | R, S, T (`[cdic][command]`, `[cdic][reset]`). Exact command-complete status and idle behavior are **U**. |
| `0x24` Reset Mode 2 | Same modeled stop semantics as `0x23`. | Same physical-sector boundary. | R, S, T. Exact distinction from `0x23` outside an active read is **U**. |
| `0x27` Fetch TOC | Partial: enters TOC operation and synthesizes Q packets from MAME TOC metadata. | Uses the compatibility seek delay; XBUF follows visible buffer completion. | S, C. Packet ordering, lead-in cadence, mixed-mode descriptors, and stored subcode use are not hardware-proven. |
| `0x28` Play CDDA | Partial: reads raw sectors, decodes CDDA each sector, exposes a CPU buffer once per second. | 75 Hz physical scheduling; visible XBUF at fraction zero. | R, S. Exact CDDA start/stop and subcode timing remain **U/C**. |
| `0x29` Read Mode 1 | Implemented HLE read at BCD MSF, with data double buffering. | Six-sector startup delay is **C**; each successful physical read advances LBA. | S, T. Error/status and servo seek completion are **U**. |
| `0x2a` Read Mode 2 | Implemented HLE read with file/channel filtering and separate audio-buffer selection. | Filtered sectors advance physically without a visible buffer or IRQ; EOF stops after accepted delivery. | R, S, T (`[cdic][sector]`, `[cdic][buffer]`). Filter edge behavior lacks an IMS66490 specification. |
| `0x2b` Stop CDDA | Cancels the current disc operation immediately. | No modeled completion IRQ. | S. Whether it also flushes all audio pipeline state is **U**. |
| `0x2c` Seek | Compatibility behavior: starts the same Mode 1 read path as `0x29`. | Same fixed delay and visible sector completion as Mode 1. | C. A true seek-only command-complete state is **U**. |
| `0x2e` Update | Accepted and command-strobe bit cleared; no other effect. | No timing or IRQ. | U/C fixed no-op. |
| Other values | Bounded unknown command; no fabricated response or transport mutation. | DBUF command-strobe bit still self-clears. | U, T. |

### CDIC register audit

| Register | Read/write and reset | Side effects / IRQ / timing | Evidence and remaining boundary |
| --- | --- | --- | --- |
| COMMAND `0x3c00` | 16-bit R/W; reset zero. | DBUF bit 15 accepts the command; reset commands are also observed live at the physical-sector boundary. | R, S, T. Unsupported commands are logged/ignored. |
| TIME `0x3c02-05` | 32-bit word-lane R/W; reset zero. | BCD MSF converts to logical LBA; fraction bit 7 requests whole-second positioning. | S, T (`[cdic][command]`). Invalid BCD handling is **U**. |
| FILE `0x3c06` | 16-bit R/W; reset zero. | High byte filters Mode 2 file number. | R, S, T (`[cdic][filter]`). Low byte is retained but unexplained. |
| CHANNEL `0x3c08-0b` | 32-bit word-lane R/W; reset all ones. | Safely selects Mode 2 channels 0-31. | R, S, T. Reset mask is existing **C** behavior. |
| AUDIO CHANNEL `0x3c0c` | 16-bit R/W; reset all ones. | Selects audio channels 0-15 after the data-channel filter. | R, S, T. Reset mask is existing **C** behavior. |
| DSEL `0x3c80` | 16-bit R/W; now deterministic zero reset. | No modeled effect. | U/C fixed storage. |
| ABUF `0x3ff4` | 16-bit R/W; reset zero; bit 15 read-to-clear. | IRQ source only when AUDCTL bit 13 enables it. | R, S, T (`[cdic][irq]`). Remaining low bits are unexplained storage. |
| XBUF `0x3ff6` | 16-bit R/W; reset zero; bit 15 read-to-clear. | Set on CPU-visible sector completion; IRQ requires DBUF bit 14. | R, S, T (`[cdic][irq]`). Remaining low bits are unexplained storage. |
| DMACTL `0x3ff8` | 16-bit R/W; reset zero. | Address selects CDIC SRAM operand; SCC68070 channel 1 owns memory cycles and completion. | H, S, prior SCC tests. Address masks and request-edge timing remain incomplete. |
| AUDCTL `0x3ffa` | 16-bit R/W; reset zero; bit 0 read-to-clear. | Bit 13 gates ABUF IRQ in this HLE; bit 0 reports `0xff` sound-map termination. Existing sound-map arm/address interpretation is retained. | R, S, T. Bit 11/13 playback ownership and low-bit read behavior still need direct traces. |
| IVEC `0x3ffc` | 16-bit R/W; compatibility reset `0x000f`. | Low byte is returned on IACK; no acknowledge side effect. | H for daisy-chain topology, S. Reset value and IACK edge timing are **C/U**. |
| DBUF `0x3ffe` | 16-bit R/W; reset zero. | Bit 15 accepts a command then clears; bit 14 enables reading/XBUF IRQ; bits 2/0 identify audio/data and buffer; clearing bit 14 cancels transport and resets selectors. | R, S, T (`[cdic][command]`, `[cdic][buffer]`, `[cdic][irq]`, `[cdic][reset]`). Other bits are unexplained storage. |

### CDIC sector, audio, and state audit

| Area | Current result | Evidence and tests | Remaining boundary |
| --- | --- | --- | --- |
| Physical versus visible progress | Physical 75 Hz reads and LBA advancement are owned by the transport; filters decide only CPU-visible delivery. Reset-after-sector is outside the delivery path. | S, T (`[cdic][sector][filter]`). | Servo seek/track-loss timing and command-complete pins are not modeled. |
| Mode 1 | Raw sector is copied without Mode 2 filters into alternating data buffers. | S, T (`[cdic][buffer]`). | Mode mismatch, EDC/ECC status, and error flags are not modeled. |
| Mode 2 filters | File must match; EOF/EOR/TRIG bypass applicability/channel checks after file match; message sectors are skipped; otherwise channel mask selects. | R, S, T (`[cdic][filter]`). | Conflicts need hardware traces rather than title exceptions. |
| Buffer completion | Data and audio each alternate independently and report the just-completed buffer through DBUF. XBUF asserts only for delivered sectors. | R, S, T (`[cdic][buffer]`, `[cdic][irq]`). | CPU/DSP/SRAM contention is not cycle-accurate. |
| XA ADPCM | 4/8-bit, mono/stereo, 18.9/37.8 kHz supported combinations retain the established predictor; reserved encodings are rejected. | S, T (`[cdic][xa]`). | De-emphasis, malformed-group signaling, and silicon rounding captures are missing. |
| CDDA/XA transitions | A new disc audio sector stops sound-map decoding; sound-map termination status is explicit. | S, T. | DAC queue flush/mute edges and exact AUDCTL playback gating remain **U**. |
| TOC/subcode | Existing Q synthesis is retained and audited as **C**. | S, C. | No claim of exact lead-in packet order, P-W delivery, CRC polynomial initialization, or multi-session behavior. |
| End of disc | Failed image reads stop the operation without delivering zero data. | S, T at helper/state boundary. | Hardware error/status/IRQ response is **U**. |
| Attenuation | Four cross-mix attenuation bytes remain logarithmic float scaling. | S. | Quantization, mute timing, and DSP saturation are not hardware-captured. |

CDIC confidence after this checkpoint:

- Functional: `[#######---] 70%`
- Hardware fidelity: `[#####-----] 50%`
- Evidence: service-manual topology, third-party hardware observations, independent RTL, source history, synthetic tests, and compatibility models explicitly marked above.
- Remaining: authoritative register documentation; seek/command completion; TOC/subcode; EDC/ECC/error status; exact AUDCTL/audio timing; cycle-level DMA/IRQ behavior.

## Phase D checkpoint

Scope: retain the Mono-I SLAVE HLE while replacing ad-hoc nested parsing with an explicit, bounded command model. This checkpoint preserves the established response-readiness, replacement, pointer, IRQ2, and reset behavior. It does not attempt MCU LLE, add title-specific paths, or invent responses for commands whose protocol is unknown.

Evidence boundary: the Philips CD-i 210/20/25 service manual verifies that revision command `0xf0` returns the command echo, the SLAVE firmware release, and the CD-processor firmware release before a dummy read. It also identifies the SLAVE as the owner of system reset and audio-control signals. The supplied Mono-I SLAVE dump has SHA-1 `56d0acd7caad51c7de703247cd6d842b36173079`, but the current SCC68070 `/DTACK` limitation prevents using it as a live LLE oracle. Command names and mailbox paths inherited only from source history remain **S**, **C**, or **U** rather than hardware facts.

Implemented in this checkpoint:

- a pure command descriptor covering every accepted opcode, request length, response channel/length, timing class, and evidence classification;
- a bounded shared-mailbox parser with explicit collecting, complete, and rejected transitions;
- exact rejection of unknown initiating opcodes, invalid channels, wrong-channel continuations, corrupt parser state, and requests larger than the 17-byte transport buffer;
- retention of the historical channel-2-to-channel-1/2 LCD continuation path while other multi-byte commands require their initiating channel;
- one centralized command executor rather than four nested channel/index switches;
- the documented three-byte `0xf0` revision response, exposing both firmware-release bytes already present in the HLE;
- parser-origin save/reset state and reuse of the existing response, delayed-IRQ, output-bounds, input-bounds, pointer-wrap, and reset helpers;
- explicit no-response behavior for unknown commands and the unimplemented disc-base request.

### SLAVE command map

| Input channel | Command | Length | Classification | Modeled behavior and boundary |
| --- | --- | ---: | --- | --- |
| 0 | `0x83` pointer enable | 1 | **STRONGLY INFERRED** | Enables asynchronous pointer packets and immediately queues the current four-byte position packet on channel 0. Exact enable acknowledgement is not hardware-captured. |
| 0 | `0x84` pointer disable | 1 | **STRONGLY INFERRED** | Disables asynchronous pointer packets; no response is fabricated. |
| 0 | `0xc0-0xff` pointer position | 3 | **STRONGLY INFERRED** | Decodes the established packed 10-bit X/Y representation. Pointer arithmetic and wrapped host deltas have exhaustive synthetic coverage. |
| 1 | none | — | **UNKNOWN** | No command may start on channel 1. It is retained only as an accepted continuation channel for a channel-2 LCD write. Exact mailbox wiring is unknown. |
| 2 | `0x80` keyboard events | 1 | **UNIMPLEMENTED** | Records enable state, but no keyboard input source or event packet generator exists. |
| 2 | `0x82` / `0x83` mute / unmute | 1 | **STRONGLY INFERRED** | Controls both DMADAC volumes. Reset persistence and transition timing are not hardware-proven. |
| 2 | `0x8a` CPU reset | 1 | **STRONGLY INFERRED** | Pulses the main-CPU reset callback. SLAVE reset ownership is documented; pulse width remains **C**. |
| 2 | `0xc0-0xcf` attenuation | 5 | **STRONGLY INFERRED** | Forwards four attenuation bytes to the CDIC mixer callback. Opcode-low-nibble meaning and serial-DAC timing remain unknown. |
| 2 | `0xf0` LCD write | 17 | **STRONGLY INFERRED** | Copies 16 panel bytes after the opcode; completion may arrive through channel 1 or 2 as retained source behavior. |
| 3 | `0x80` / `0x81` memory set / clear | 4 | **UNIMPLEMENTED** | Consumes the known request footprint and performs no effect or reply. Memory target and semantics are unknown. |
| 3 | `0xb0` disc status | 4 | **COMPATIBILITY** | Returns fixed `b0 00 02 15` on channel 3 after 250 ms. Payload and delay are not connected to the SERVO/CD processor. |
| 3 | `0xb1` disc base | 4 | **UNIMPLEMENTED** | Consumes the request and deliberately emits no response. The former zero-response idea remains disabled. |
| 3 | `0xf0` revision | 1 | **VERIFIED** structure | Returns `f0`, SLAVE release, and CD-processor release on channel 2. The three-byte structure is service-manual-backed; fixed `32 31` values and 100 us IRQ delay are **C/S**. |
| 3 | `0xf3` pointer type | 1 | **COMPATIBILITY** | Returns fixed `f3 01` on channel 2 after 100 us. Device-discovery wiring is not modeled. |
| 3 | `0xf4` test plug | 1 | **STRONGLY INFERRED** | Returns `f4` plus the configured test-plug input on channel 2 after 100 us. |
| 3 | `0xf6` video standard | 1 | **STRONGLY INFERRED** | Returns `f6 01` for NTSC or `f6 02` for PAL on channel 2. Immediate readability without IRQ remains **C**. |
| 3 | `0xf7` / `0xfe` developer mode | 1 | **UNIMPLEMENTED** | Stores armed/disarmed state only; no developer/test protocol is invented. |
| 3 | `0xfa` X-bus IRQ enable | 1 | **UNIMPLEMENTED** | Stores enable state only; X-bus-to-SERVO routing and event generation are absent. |
| any | all other opcodes | — | **UNKNOWN** | Rejected immediately, parser reset, no response or side effect. |

### SLAVE transport and state audit

| Area | Current result | Evidence and tests | Remaining boundary |
| --- | --- | --- | --- |
| Command assembly | One shared parser records origin channel, opcode, byte index, and exact command length. LCD retains its historical cross-channel continuation; all other continuations are origin-channel-local. | S, T (`[slave][command]`, `[slave][malformed]`). | Exact 68070-to-SLAVE mailbox channel wiring needs firmware/bus traces. |
| Input bounds | Writes are checked before indexing the 17-byte buffer; descriptor lengths are bounded; malformed state resets atomically. | S, T (`[transport]`, `[malformed]`). | Hardware behavior on abandoned partial commands is unknown. |
| Response replacement | Each of four output channels owns one pending response. A newly prepared response replaces that channel only and forces immediate IRQ re-evaluation. | S, T (`[readiness]`, `[irq2]`). | Hardware queue depth and overwrite/overrun status are unknown. |
| Readiness and IRQ2 | Delayed data is unreadable and cannot assert IRQ2 before its deadline. Ready interrupting responses arbitrate across channels; consuming the final ready source clears IRQ2. | S, T (`[readiness]`, `[irq2]`). | The 100 us and 250 ms delays are compatibility values; IACK/bus-edge timing is not modeled. |
| Pointer input | Host counters use signed modular deltas; device coordinates clamp to the established 768x560 doubled-raster range; asynchronous packets replace channel-0 output. | S, T (`[pointer]`). | 60 Hz host polling is **C** and not a serial-bit timing model. Keyboard packets remain unimplemented. |
| Reset | Cancels delayed IRQ, clears all input/output/parser state, disables pointer events, and resets stored developer/X-bus/LCD/input state. | S, T (`[transport]`, `[irq2]`). | DMADAC mute/attenuation reset state and physical reset pulse width remain unknown. |
| Save state | Output deadlines, readiness/IRQ flags, parser origin/index/count, command bytes, and retained protocol flags are registered. | S, build validation. | No active-partial-command round-trip fixture exists yet. |

SLAVE confidence after this checkpoint:

- Functional: `[#######---] 65%`
- Hardware fidelity: `[####------] 45%`
- Evidence: Philips service/manual behavior, board-level signal ownership, current source/history, supplied firmware identity, firmware-only boot smoke test, and synthetic command/transport/pointer tests.
- Remaining: keyboard event source/format; live SERVO disc status/base; developer and X-bus protocols; hardware mailbox timing; response queue depth; reset/mute/attenuation timing; SCC68070 `/DTACK` support needed for trustworthy LLE experiments.

## Phase E Mono-II foundation

Scope: establish an auditable `cdimono2` board skeleton without reviving the historical asynchronous `/DTACK` shortcut or fabricating a DSP command/register HLE. Phase E uses the roadmap labels **A** (primary hardware/service evidence), **B** (firmware, source history, or independent inference), **C** (compatibility model), and **D** (unknown or blocked).

Implemented in this checkpoint:

- one shared header for the Mono-II clocks, mapped/blocked address regions, DRVDSP host-register addresses, IRQ2 line conversion, and explicit implementation boundaries;
- the SLAVE MCU port-B bit-5 active-low IRQ output connected to SCC68070 `IN2`, with IRQ2 cleared deterministically at machine reset;
- a disabled DSP56001 device at the documented 27 MHz board clock, representing the component without pretending that the current MAME core executes DSP code;
- DRVDSP and SLAVE host ranges deliberately left unmapped until their real device interfaces exist;
- five structural test cases covering board-map ordering, machine clocks/startup descriptors, IRQ/reset behavior, DSP register addressing, and unsupported-bus boundaries.

### Mono-II board map

| Range / component | Current representation | Evidence | Remaining boundary |
| --- | --- | --- | --- |
| SCC68070 and MCD212 at 30.2098 MHz | Configured and linked to the existing plane RAM, video interrupt, screen, and MCD212 map. | **A/B** | No new cycle-level bus or video claim. |
| `0x000000-0x07ffff` plane A | 512 KiB mapped RAM. | **B** | Bus arbitration with video fetch is not cycle-accurate. |
| `0x200000-0x27ffff` plane B | 512 KiB mapped RAM. | **B** | Same boundary as plane A. |
| `0x300000-0x30000f` DRVDSP host interface | Address/register descriptors only; intentionally unmapped. | **B/D** | Current DSP56001 core has no host interface or instruction execution. |
| `0x310000-0x317fff` SLAVE host mailbox | Topology recorded; intentionally unmapped. | **A/D** | SCC68070 cannot yet suspend a bus cycle for MCU-controlled asynchronous `/DTACK`. |
| `0x320000-0x323fff` timekeeper NVRAM | Existing high-byte map retained. | **B** | Exact byte-lane wording should be checked against a board schematic. |
| `0x400000-0x47ffff` boot ROM | Existing ROM window retained. | **B** | Runtime proof requires a legal matching Mono-II ROM set. |
| `0x4fffe0-0x4fffff` MCD212 | Existing device submap retained. | **A/B** | Covered by Phase B structural tests, not Mono-II hardware captures. |
| SERVO and SLAVE MC68HC05C8 at 4 MHz | Existing dumped-ROM MCU devices retained. | **A/B** | Their present CPU core exposes ports and register storage but no external SPI pins. |
| DSP56001/DRVDSP at 27 MHz | Present as a disabled device. | **A/D** | LEMM, DSP code execution, host port, local memory, DMA, and interrupt routing are absent. |

### Mono-II signal and reset audit

| Signal/path | Result | Classification |
| --- | --- | --- |
| SLAVE port B bit 5 to SCC68070 `IN2` | Connected as an active-low line; all 256 port values are covered by the structural test. | **B**, implemented |
| SCC68070 IRQ2 at reset | Explicitly clear before MCU firmware can assert it. | **C**, deterministic reset safety |
| SERVO-SLAVE SPI clock/data/select | Board topology is documented, but no driver connection is made because the current MC68HC05 core lacks pin-level SPI I/O. | **A/D**, blocked |
| SCC68070-SLAVE address/data/RW and `/DTACK` | No driver-local latch or suspended-cycle approximation restored. | **A/D**, blocked |
| DSP host registers | Odd byte addresses `0x300001`, `03`, `05`, `07`, `0b`, `0d`, and `0f` are recorded for future implementation. | **B**, structural only |
| DSP/LEMM execution, DMA, and interrupt | No command HLE, fake ready bit, register array, or audio bypass added. | **D**, blocked |
| DSP reset | Device remains disabled; no executable/reset-state claim is made beyond structural presence. | **D** |

Mono-II confidence after this checkpoint:

- Structural foundation: `[######----] 60%`
- Functional runtime: `[##--------] 20%`
- Hardware fidelity: `[###-------] 30%`
- Proven: linked machine configuration, deterministic IRQ2 reset/translation, stable map descriptors, and deliberate unsupported-range boundaries.
- Not proven: firmware boot, SERVO-SLAVE traffic, host-mailbox transfers, DSP execution, digital audio, CD playback, controller input, or commercial-title behavior.

## MCD212 display-pipeline audit

| Area | Current result | Evidence and tests | Remaining boundary |
| --- | --- | --- | --- |
| Coding modes and plane combinations | OFF, CLUT8, CLUT7/RL7, dual CLUT7, DYUV, CLUT4/RL3, RGB555, and Extended Case QHY values are represented. QHY is accepted only on plane B and consumes plane-A DYUV as its base. | H, S, T (`[mcd212][qhy]`) | Reserved coding combinations are bounded but have no silicon-behavior claim. |
| ICA/DCA commands | STOP, NOP, DCP/VSR reload variants, interrupt, display-parameter reload, and register loads match the documented tables. Odd/even ICA selection, DCA fetch size, DCP alignment, and interrupt disables were corrected. | H, S, T (`[mcd212][control]`) | Memory-fetch contention is slot-level, not a cycle-accurate control-program engine. |
| CLUT and QHY levels | CLUT output retains six implemented MSBs; QHY bank-2 levels retain all eight bits per component. | H, S, T (`[mcd212][qhy]`) | No hardware capture covers special-graphics unequal RGB QHY levels. |
| Plane order, transparency, masks, external video | Documented field masks are enforced. External video is eligible only when enabled and both image planes are transparent; cursor pixels clear eligibility. | H, S, T (`[mcd212][overlay]`, `[dvc]`) | There is no direct device-to-device overlay fixture or post-change gameplay capture. |
| Matte, weight, mosaic, cursor, backdrop | Register fields and existing source algorithms were audited; reserved bits are discarded and reset is deterministic. | H, S, T | Region-edge, weighting, cursor blink, and mosaic output still need register-correlated frame captures. |
| DYUV and QHY reconstruction | Ordinary DYUV remains separate. QHY expands cached normal-resolution Y/U/V with the specified 2-D filter, matrices to RGB, then adds signed per-channel deltas. | H, S, T (`[mcd212][qhy]`) | The documents do not define odd-sum rounding or cold first-even-field edge behavior; full-frame hardware evidence is required. |
| PAL/NTSC/interlace timing | Active start/height, integral or half-line totals, PA phase, ICA start, and DCA line range are derived from MCD212 tables 5-5 through 5-7. | H, S, T (`[mcd212][timing]`) | HSYNC/VSYNC pin waveforms and bus/IRQ assertion edges are not externally captured. |

## SCC68070 register audit

| Register/group | Read/write and reset | Side effects / IRQ / timing | Evidence and tests | Remaining gap |
| --- | --- | --- | --- | --- |
| LIR | Levels read from bits 6:4/2:0; pending bits are write-one-to-clear and read as zero; reset zero. | INT1 precedes INT2 at equal level. IACK clears the selected latched pending bit, not the input line. | H, S, T (`[scc68070][irq]`) | External-pin assertion timing is not cycle-accurate. |
| IDR | Byte read/write; data reset zero. | Any CPU access sets PIN and clears AL/AAS; an enabled read advances receive ACK/NAK state. Debugger reads are non-mutating. | H, S, T (`[i2c]`) | Slave-mode data flow remains incomplete. |
| IAR | Byte read/write; retained until reset or rewritten. | No modeled timing effect. | H, S | General-call/slave addressing has no complete bus test. |
| ISR | Byte read/write; PIN set at reset. | Drives START/repeated START/STOP and pending interrupt state. | H, S | Arbitration, slave mode, and multi-master timing are incomplete. |
| ICR | Byte read/write; reset zero. | Disabling ESO releases the bus and stops the local timer. | H, S | SEL behavior is not modeled fully. |
| ICCR | Low five bits writable; high three bits read one; reset zero. | Programs the I2C clock divider. | H, S | Divider code zero is illegal in hardware but currently has a one-cycle **C** fallback. |
| UMR | Byte read/write; bit 5 reads one; reset zero. | Programs framing/mode. | H, S, T (`[uart]`) | Echo/loopback/parity/framing are not fully modeled. |
| USR | Read-only status; bit 1 reads one. | RX/TX callbacks update readiness and error state. | H, S, T (`[uart]`) | TXEMT forced high and TXRDY-at-reset remain explicit **C** behavior. |
| UCSR | Byte read/write; bit 3 reads one; reset zero. | Internal clock is SCC clock/4; XCKI can be configured externally. | H, S | Baud/framing is timer-level, not bit-level; missing XCKI stops UART timers. |
| UCR | Low control bits persist; miscellaneous commands self-clear; bit 7 reads one. | Receiver/transmitter reset clears the matching queue interrupt. | H, S, T (`[uart]`) | Start/stop break is logged but unimplemented. |
| UTH/URH | Holding registers reset zero. | TX write queues a byte; RX read returns the current byte, advances the queue, and clears RXRDY when empty. | H, S | 32 KiB software queues are **C**, not the one-byte hardware depth. |
| TSR/TCR | Byte lanes implemented; status is write-one-to-clear. | Timer interrupt is represented by one pending latch. | H, S | Timer 1/2 match, capture, and event-counter inputs are **U**. |
| RR/T0 | 16-bit read/write; reset zero. | T0 advances at CLKOUT/96. RR takes effect at the next overflow and does not re-phase T0. | H, S | Readback is timer-derived rather than cycle-by-cycle state. |
| PICR1/PICR2 | Priority fields read/write; pending bits write-one-to-clear and read zero. | Same-level order is timer, UART RX, UART TX, then I2C after INT1/INT2. I2C uses PICR1. | H, S, T (`[irq]`) | No bus-level IACK timing test. |
| DMA CSR/CER | Status/error byte lanes; COC/NDT/ERR write-one-to-clear. | SO starts only from clear status; SA sets COC+ERR/soft-abort; completion clears CA and sets COC. | H, S, T (`[dma]`) | NDT and all bus-error sources are incomplete. |
| DMA DCR/OCR/SCR/CCR | Implemented masks and fixed read bits; channel 1 has fixed device type/size coupling. | Reserved operand/address modes reject a transfer before memory mutation. | H, S, T (`[dma]`) | Burst/cycle-steal, request gating, chain mode, and cycle timing are **U**. |
| DMA MTC/MAC/DAC | 16-bit count; 24-bit addresses; channel-2 MAC/DAC fixed or increment by operand size. | Successful transfer advances enabled counters and sets completion at zero. | H, S, T (`[dma]`) | A zero initial count and DMA RESET counter retention are documentation conflicts/unknowns. |
| DMA CPR | Channel 1 reads zero; channel 2 reads one; writes ignored. | None. | C, S | Compatibility-only placeholder, not a hardware claim. |
| MMU MSR/MCR | Status is the high byte, control the low byte; MCR stores EN/SN only; reset zero. | No translation effect yet. | H, S, T (`[mmu]`) | Exception/status generation is **U**. |
| MMU descriptors | Attribute/length/segment/base storage; length is 11 bits and base is 14 bits. | No translation effect yet. | H, S, T (`[mmu]`) | Attribute reserved-bit mask and all live address translation are **U**. |

## Subsystem confidence

| Subsystem | Confidence | Current evidence | Next evidence-driven step |
| --- | ---: | --- | --- |
| SCC68070 internal peripherals | `[######----] 60%` | Register audit plus focused DMA/IRQ/I2C/UART/MMU tests. | Implement Timer 1/2 only after input routing is identified; trace MMU-enabled firmware before translation. |
| MCD212 video | `[#######---] 70%` | Primary-document audit, QHY vectors, timing/control tests, and linked CD-i validation target. | Capture QHY, cursor, transparency, region, and interlace field output from Extended Case hardware. |
| CDIC | `[#######---] 70%` functional / `[#####-----] 50%` fidelity | Command/register audit, independent state/filter/buffer/IRQ/XA tests, and SCC-owned DMA client. | Capture seek completion, AUDCTL, TOC/subcode, and error/IRQ behavior on hardware. |
| SLAVE/HLE | `[#######---] 65%` functional / `[####------] 45%` fidelity | Complete classified command map, bounded parser, documented revision response, and pointer/transport/readiness/IRQ2 tests. | Add keyboard and SERVO/X-bus behavior only from MCU, firmware, or bus-trace evidence. |
| SERVO/MCU | `[###-------] 30%` | Mostly existing HLE/integration behavior. | Capture command/response timing from a known firmware/disc pair. |
| DVC | `[#####-----] 50%` | Broad native DVC tests and SCC-owned DMA path; prior runtime vertical-slice evidence. | Compare PES/DMA/status/IRQ traces at the first failing scene; no title-specific bypasses. |
| Mono-I / Mono-II glue | `[######----] 60%` | Machine configurations and validation build. | Add clean-boot checkpoints for representative firmware revisions. |
| Audio | `[#####-----] 50%` | CDIC/DVC decode paths and unit coverage. | Runtime A/V clock and underflow trace comparison. |
| Video | `[######----] 60%` | MCD212 QHY/timing/control tests, DVC suite, and external-video mask regression coverage. | Frame/scanline captures tied to register and overlay traces. |
| Input | `[#####-----] 50%` | Existing machine input paths; pre-existing local edits are outside this checkpoint. | Validate pointing-device range and button behavior on Mono-I/II. |
| IRQ/DMA integration | `[######----] 60%` | SCC arbitration tests, channel register semantics, CDIC/DVC clients. | Bus-level IACK/DREQ timing and error injection. |
| Save states | `[######----] 60%` | SCC fields and queues registered; existing DVC save-state helpers/tests. | Round-trip active I2C/UART/DMA operations and document cross-version limits. |

## Compatibility models and unknowns

- `dma_channel_external_start()` is an explicit peripheral-DREQ compatibility bridge: it clears stale COC and asserts CA so existing DVC/CDIC clients can enter an SCC-owned transfer. It is not proof of the SCC68070 hardware start sequence.
- The DVC remains a functional vertical slice, not a cycle-accurate MPEG board model. Runtime success in one scene must not be generalized to later sequences.
- UART TXEMT is forced high for Magicard compatibility and TXRDY is set at reset. Both require firmware trace comparison before removal.
- ICCR divider code zero uses a one-cycle fallback even though the hardware document marks the code illegal.
- DMA priority-register values are placeholders. DMA NDT, request modes, bus errors, zero-count behavior, and reset retention require more evidence.
- The MMU stores masked registers but does not translate CPU accesses or raise segment exceptions.
- Timer 1/2 match, capture, and event-counter modes are not implemented because the external signal ownership is not yet established.
- I2C arbitration and slave/multi-master behavior are incomplete.
- QHY is an Extended Case Green Book facility and is not described by the base MCD212 data sheet. The implemented combination and stream format are specification-backed, but chip-revision ownership is not proven.
- QHY first-even-field boundary reuse and interpolation rounding for odd sums remain explicit unknowns pending hardware/full-frame evidence.
- The native overlay regression covers MCD212 eligibility and the DVC suite independently; it is not a live combined-device or gameplay proof.
- DCP/DDR reset retention of the six pointer MSBs is ambiguous in the MCD212 wording; this checkpoint retains deterministic zero reset.
- No IMS66490 data sheet is available in the reviewed sources. CDIC command/register semantics beyond board topology are classified from firmware, hardware observations, independent implementations, tests, or compatibility behavior rather than promoted to hardware fact.
- CDIC seek uses a six-sector fixed delay because the HLE has no servo position/feedback model. This is a compatibility value, not a hardware timing claim.
- CDIC `0x2c` Seek remains equivalent to starting Mode 1 delivery. A seek-only completion state must not be invented without firmware/hardware evidence.
- CDIC Fetch TOC remains synthesized from image metadata. Packet order, mixed-mode/multi-session behavior, P-W subcode, and exact completion signaling are unresolved.
- CDIC end-of-disc now terminates without a fabricated sector, but the hardware error/status/IRQ response is unknown.
- CDIC DSEL, IVEC reset, unused ABUF/XBUF/DBUF bits, and some AUDCTL fields remain stored or fixed compatibility values.
- Mono-II SERVO-SLAVE SPI is documented but not connected because the current MC68HC05 core has no external SPI-pin interface.
- Mono-II SLAVE host-mailbox LLE remains blocked on asynchronous SCC68070 `/DTACK`; the historical driver-local wait-state shortcut was not restored.
- The Mono-II DSP56001 remains disabled and its host register window remains unmapped at the board level. The MAME DSP56001 core now exposes a host interface and a partial genuine instruction interpreter, with native tests covering host/bootstrap behavior, decoder rejection boundaries, 16-bit wraparound, DO-loop semantics, MOVEP peripheral addressing, and bootstrap register state. Full Philips firmware execution, board-level HREQ/interrupt/DMA integration, and commercial-title compatibility are not claimed, and no command HLE was added.
- No matching `cdimono2` ROM files are present in this checkout, so firmware startup is an explicit unpassed runtime gate.

## Phase A validation gates

The checkpoint may be committed only when all of the following pass from the same tree:

1. focused SCC68070 tests;
2. existing CD-i peripheral and DVC DMA/IRQ tests;
3. complete native `mametests` suite;
4. `cdivalidate -validate`;
5. CD-i validation/emulator build with `-j2`;
6. `git diff --check` and exact-path staging only.

Latest result (2026-08-23):

- focused SCC68070: **PASS**, 796 assertions in 6 cases;
- all Philips tests: **PASS**, 1,283,656 assertions in 73 cases;
- complete native suite: **PASS**, 1,284,738 assertions in 90 cases;
- regenerated `cdivalidate` build (`TESTS=1`, `-j2`): **PASS**;
- `cdivalidate -validate`: **PASS** (exit zero, no diagnostics);
- exact-path `git diff --check`: **PASS**.

There is no direct DVC-to-SCC DMA fixture in the current test tree. The present integration gate is the SCC DMA suite plus all DVC/Philips tests and a linked CD-i target; a live cross-device transfer fixture remains follow-up work and should not be described as already covered.

## Phase B validation gates

The checkpoint may be committed only when all of the following pass from the same tree:

1. focused MCD212 timing, control, QHY, and external-video tests;
2. complete DVC and Philips native test selections;
3. complete native `mametests` suite;
4. CD-i validation/emulator build with `-j2` and warnings enabled;
5. `cdivalidate -validate`;
6. `git diff --check` and exact-path staging only.

Latest result (2026-08-23):

- focused MCD212: **PASS**, 69 assertions in 7 cases;
- DVC selection: **PASS**, 1,279,657 assertions in 48 cases;
- all Philips tests: **PASS**, 1,283,725 assertions in 80 cases;
- complete native suite: **PASS**, 1,284,807 assertions in 97 cases;
- regenerated `cdivalidate` build (`TESTS=1`, `-j2`): **PASS**;
- `cdivalidate -validate`: **PASS** (exit zero, no diagnostics);
- exact-path `git diff --check`: **PASS**.

No commercial title was required or run for this phase. There is no post-change QHY hardware capture, no post-change DVC gameplay result, and no direct combined MCD212-to-DVC overlay fixture. These are follow-up evidence requirements, not passed gates.

## Phase C validation gates

The checkpoint may be committed only when all of the following pass from the same tree:

1. focused CDIC command/state and BCD-position tests;
2. sector filtering and independent buffer tests;
3. XA predictor/coding tests;
4. IRQ acknowledgement/gating and reset tests;
5. complete Philips and native `mametests` suites;
6. warnings-enabled CD-i validation/emulator build with `-j2`;
7. `cdivalidate -validate`;
8. `git diff --check` and exact-path/hunk staging that excludes pre-existing traces.

Latest result (2026-08-24):

- focused CDIC: **PASS**, 69 assertions in 8 cases;
- all Philips tests: **PASS**, 1,283,794 assertions in 88 cases;
- complete native suite: **PASS**, 1,284,876 assertions in 105 cases;
- regenerated warnings-enabled `cdivalidate` build (`TESTS=1`, `-j2`): **PASS**;
- `cdivalidate -validate`: **PASS** (exit zero, no diagnostics);
- exact-path `git diff --check`: **PASS**.

No commercial title is required for this phase. BrainDead 13 and other title-specific regressions remain parked. The gates establish deterministic HLE behavior and regression safety; they do not establish cycle-accurate IMS66490 behavior.

## Phase D validation gates

The checkpoint may be committed only when all of the following pass from the same tree:

1. classified SLAVE command-map and parser tests;
2. response-readiness and IRQ2 arbitration tests;
3. transport bounds and response-replacement tests;
4. pointer encoding, wrap, movement, and clamping tests;
5. malformed/wrong-channel/overlength command tests;
6. complete Philips and native `mametests` suites;
7. warnings-enabled CD-i validation/emulator build with `-j2`;
8. `cdivalidate -validate`;
9. `git diff --check` and exact-path/hunk staging that excludes pre-existing input tracing.

Latest result (2026-08-24):

- focused SLAVE command/parser: **PASS**, 3,262 assertions in 5 cases;
- response-readiness/IRQ2: **PASS**, 13 assertions in 5 cases;
- transport selection: **PASS**, 183 assertions in 18 cases;
- pointer selection: **PASS**, 3,903 assertions in 21 cases;
- malformed command selection: **PASS**, 11 assertions in 1 case;
- all Philips tests: **PASS**, 1,287,056 assertions in 93 cases;
- complete native suite: **PASS**, 1,288,138 assertions in 110 cases;
- regenerated warnings-enabled `cdivalidate` build (`TESTS=1`, `-j2`): **PASS**;
- `cdivalidate -validate`: **PASS** (exit zero, no diagnostics);
- five-second `cdimono1 -bios pcdi220` firmware-only smoke test: **PASS**; expected REDUMP warnings remain;
- exact-path `git diff --check`: **PASS**.

No commercial title was required or run. The gates prove deterministic parser, transport, response, pointer, IRQ2, and reset behavior in this HLE; they do not prove complete SLAVE firmware semantics, exact mailbox timing, or LLE equivalence.

## Phase E validation gates

The checkpoint may be committed only when all of the following pass from the same tree:

1. focused Mono-II board-map/startup/IRQ/reset/DSP-boundary tests;
2. complete Mono-I regression selections and all Philips tests;
3. complete native `mametests` suite;
4. regenerated CD-i validation/emulator build with `-j2`;
5. `cdivalidate -validate cdimono2` and device enumeration;
6. address-sanitized focused, Philips, and complete native test selections;
7. `git diff --check` and exact-path/hunk staging that excludes pre-existing traces and input changes.

Latest result (2026-08-24):

- focused Mono-II structure: **PASS**, 327 assertions in 5 cases;
- focused SLAVE regression: **PASS**, 3,275 assertions in 10 cases;
- transport regression: **PASS**, 183 assertions in 18 cases;
- all Philips tests: **PASS**, 1,287,383 assertions in 98 cases;
- complete native suite: **PASS**, 1,288,465 assertions in 115 cases;
- regenerated native and AddressSanitizer `cdivalidate`/`mametests` builds (`TESTS=1`, `NOWERROR=1`, `-j2`): **PASS**;
- `cdivalidate -validate cdimono2`: **PASS** (exit zero, no diagnostics);
- device enumeration: **PASS**, including SCC68070, MCD212, two MC68HC05C8 devices, and a DSP56001 at their declared clocks;
- AddressSanitizer focused Mono-II: **PASS**, 327 assertions in 5 cases;
- AddressSanitizer focused SLAVE: **PASS**, 3,275 assertions in 10 cases;
- AddressSanitizer all Philips: **PASS**, 1,287,383 assertions in 98 cases;
- AddressSanitizer complete suite: **PASS**, 1,288,465 assertions in 115 cases;
- native and AddressSanitizer global `cdivalidate -validate`: **PASS**;
- native and AddressSanitizer five-second `cdimono1 -bios pcdi220` firmware-only smoke: **PASS**; expected REDUMP and ALSA warnings remain;
- exact-path `git diff --check`: **PASS**.

Runtime limitation:

- `cdivalidate -verifyroms cdimono2` reports `romset "cdimono2" not found` because this checkout has no matching Mono-II ROM set;
- therefore no Mono-II firmware boot, CD playback, input, audio, or commercial-title result is claimed;
- the existing Mono-I firmware-only smoke gate remains required to protect the established machine family while Mono-II runtime media is unavailable.

## DSP56001 audit validation

Scope: validate the current partial DSP56000/DSP56001 interpreter and host/bootstrap behavior independently of Mono-II board mapping. This checkpoint does not map the Mono-II DSP host window, add command HLE, claim complete DSP56001 instruction coverage, or establish commercial-title compatibility.

Validated integration commit: `d39e31ff866bc6cc49f5d280fdef286bdd2d80e7`.

The clean promotion commit `523dca4db56e365ca117d5cdf41d3b952dd39af5` reused the exact validated integration tree (`ae9cb5dcef208072fdf1cb8da21657597367497a`) while removing review-branch merge history and the reverted six-bit CVR experiment.

Latest result (2026-08-26):

- static `git diff --check`: **PASS**;
- regenerated `cdivalidate` and `mametests` build (`TESTS=1`, `-j2`): **PASS**;
- focused `[emu][cpu][dsp56000]` tests: **PASS**;
- all `[emu][philips]` tests: **PASS**;
- complete native `mametests` suite: **PASS**;
- `cdivalidate -validate`: **PASS**;
- `cdivalidate -validate cdimono2`: **PASS**;
- DSP-specific `cdidsp` build: **PASS**;
- `cdidsp -validate`: **PASS**;
- final `git diff --check`: **PASS**.

GCC 11.4.0 compatibility note: the inherited `tests/emu/video/rgbutil.cpp` uses volatile-assignment idioms that GCC diagnoses with `-Wvolatile`, and MAME normally promotes that warning to an error. The validated test build therefore used `ARCHOPTS_CXX=-Wno-error=volatile`; other warning classes remained errors. `rgbutil.cpp` is outside the CD-i/DSP audit delta and was not modified.

Validated DSP behavior in this checkpoint includes stricter partial-decoder masks, non-speculative extension-word reads, reserved JCLR bit rejection, last-word DO-loop termination, architectural zero-count DO behavior, 16-bit execution/address wraparound tests, DSP56001 bootstrap R0/R1/R2 state preservation, and corrected immediate-MOVEP peripheral-address decoding.

Remaining architecture boundaries include migration from temporary private P/X/Y execution backing to correctly mapped device address spaces, board ownership of external Y-space behavior, HREQ/interrupt/DMA integration, broader instruction coverage and legality checks, and instruction-accurate timing. The Mono-II DSP therefore remains disabled/unmapped in the machine configuration until those integration boundaries are addressed.
