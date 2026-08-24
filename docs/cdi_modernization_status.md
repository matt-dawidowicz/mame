# Philips CD-i modernization status

Last reviewed: 2026-08-23

Branch: `cdi-dvc-modernization`

Phase-A checkpoint: `63fbd31f6934cbd60956b1a0899d75ea5c31a871`

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
| CDIC | `[######----] 60%` | Existing CD-i peripheral tests and SCC-owned channel-1 DMA client. | Trace command/sector/DMA completion order under real firmware. |
| SLAVE/HLE | `[#####-----] 50%` | Pointer, transport, response-ready, and hardening tests. | Replace HLE behavior only with MCU/firmware trace evidence. |
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

Phase C requires separate authorization and must begin from the unresolved evidence items above.
