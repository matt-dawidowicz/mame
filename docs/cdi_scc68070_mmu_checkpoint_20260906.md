# SCC68070 MMU translation checkpoint — 2026-09-06

This checkpoint records the original MMU milestone. The [fresh source/test audit](cdi_verified_status_20260906.md#f2--scc-mmu-restartable-fault-path-is-not-enabled-audio-branch) reopens broad architectural certification: translation and query/save fixtures pass, but the restartable fault path is not enabled and executed-CPU fault tests are missing. Exact silicon timing remains a separate evidence gap.

## Certification

The MMU milestone is certified at commit:

- `0c433789c840c116aa4ad31ccbefa9daa1f8fd2d`

The maintained CD-i fast workflow completed green in GitHub Actions run:

- `34051815017` (`CI (CD-i fast) #42`)

That run completed all of the maintained MMU-relevant gates successfully:

- CD-i helper-test build;
- CD-i helper-test execution;
- CD-i integration-test build;
- CD-i integration-test execution.

Two test-harness corrections were required while reaching the certified run. Neither changed MMU semantics:

1. The full-machine MMU fixture originally called `m_maincpu->memory().translate(...)`, which is ambiguous because `scc68070_device` exposes `memory()` through both `device_t` and `device_memory_interface`. The fixture now binds the CPU explicitly to `device_memory_interface` before calling `translate()`.
2. An existing DVC DMA edge regression sampled the SCC68070 IPL line in the same scheduler callback that asserted or cleared it. The observed line transition becomes visible on the following scheduler turn, so the fixture now samples the assertion and acknowledgement edges after synchronization rather than treating the scheduler propagation delay as a controller failure.

The fresh audit identifies a concrete source defect and an executed-CPU verification gap. Repair and validate fault delivery before treating this milestone as architecturally complete.

## Evidence basis

The implementation is constrained by the Philips SCC68070 hardware documentation and contemporary Philips technical material:

- MCR bit 7 (`EN`) enables address translation/protection.
- MCR bit 6 (`SN`) selects mode 1 (`0`: 8 logical segments, 11-bit displacement) or mode 2 (`1`: 128 logical segments, 7-bit displacement).
- Logical addresses use a 10-bit offset within a 1 KiB block.
- Descriptor `F` is bit 7 of the segment-number byte; `F=0` invalidates the descriptor and `F=1` makes the segment number eligible for the associative comparison.
- Mode 1 uses an 11-bit maximum block displacement, giving 1–2048 KiB segment sizes.
- Mode 2 uses the seven most-significant bits of the stored 11-bit length field, giving 1–128 KiB segment sizes.
- The 14-bit descriptor base is a physical 1 KiB block address.
- Descriptor attributes expose supervisor (`S`), execute (`E`), read (`R`), write (`W`), and stack (`ST`) semantics. Philips states that `E`, `R`, and `W` permit their corresponding access class, `S` requires supervisor privilege, and stack segments grow from high to low addresses.
- Only off-chip CPU addresses are MMU-translated; on-chip peripheral and interrupt-acknowledge accesses bypass the MMU. DMA remains owned by the SCC DMA controller and does not pass through CPU MMU translation.
- The eight on-chip descriptors form a fully associative CAM. The documentation describes simultaneous comparison and does not define priority for duplicate active segment numbers, so the software model reports duplicate matches as `multiple_match` rather than inventing a winner.
- MMU violations generate a CPU bus error and expose fault class / descriptor attributes through the MMU Status Register (MSR).

## Architectural implementation and verification scope

### 1. MMU disabled / external addressing

When `EN=0`, CPU external accesses retain the existing SCC68070 24-bit address-bus behavior. The MMU performs neither address translation nor attribute enforcement.

### 2. All eight descriptors / associative selection

Every descriptor slot can independently translate a matching logical segment. Flushed descriptors are excluded from comparison. A unique matching active descriptor wins regardless of slot.

Duplicate active matches are treated as an explicit invalid state (`multiple_match`) and fault conservatively; no undocumented slot priority is fabricated.

### 3. Mode-1 and mode-2 segment geometry

The shared translation oracle covers:

- mode-1 logical segment and 11-bit block displacement;
- mode-2 logical segment and 7-bit block displacement;
- first and last legal bytes;
- first block past the programmed limit;
- minimum 1 KiB segments;
- maximum 2 MiB mode-1 segments;
- maximum 128 KiB mode-2 segments;
- mode-2 use of only the seven effective stored length bits.

### 4. Live CPU-path translation

The SCC68070 Musashi front end now classifies real CPU operations as:

- instruction fetch;
- data read;
- data write.

Those callbacks invoke the live SCC68070 translation/protection hook before touching the physical address space. Successful translations then use the translated physical address.

The debugger/disassembler `memory_translate()` path uses the same live mapping and access-class model with side effects suppressed, so inspection cannot mutate MSR state or inject an exception.

### 5. Execute/read/write and supervisor protection

Descriptor `E`, `R`, and `W` permission bits independently gate instruction fetch, data read, and data write. `S=1` rejects an otherwise permitted user-mode access and admits the corresponding supervisor-mode access.

Permission checks are bypassed with address translation when `EN=0`.

### 6. Live MSR reporting

Fault results are converted to the documented MSR classes:

- `N` for no matching descriptor; the conservative duplicate-match model also enters this class;
- `L` for segment-length violation;
- `A` for access/attribute violation;
- `ST/S/E/R/W` reflect the matched descriptor attributes where a descriptor exists.

The existing MSR/MCR word-lane behavior remains intact.

### 7. Restartable CPU bus error

A live MMU violation does not fall through to the untranslated physical address. The SCC68070 records the logical fault address, read/write state, and current function code through Musashi's `set_buserror_details(..., rerun=true)` external-MMU path.

The original claim that this schedules a restartable fault was too broad. Musashi only raises its pending MMU fault when `m_can_instruction_restart` is true. The reviewed SCC68070 path never calls `set_emmu_enable`, and initialization/reset leave restart disabled. The metadata call alone does not activate the required exception route. This is source-traced; a full executed-CPU reproduction is still required. Do not add a second bus-error pulse blindly: the intended enabled path explicitly forbids double injection.

Exact SCC68070 bus-error cycle timing and stack-frame timing remain hardware-evidence questions rather than claims of this checkpoint.

### 8. Stack-segment direction

`ST=1` uses the documented high-to-low stack direction. The effective logical displacement is measured from the top of the selected segment and physical block progression moves downward from the descriptor base.

Tests cover minimum and multi-block mode-1 stacks and effective mode-2 stack lengths. The direction is documentation-constrained; exact silicon edge timing or any undocumented wrap behavior still requires hardware capture.

### 9. Multi-byte boundary behavior

Word and longword CPU accesses preflight the translation/protection result for **every byte of the operand before any physical bus access occurs**.

This prevents a write whose later byte crosses a length/protection boundary from partially modifying memory before the MMU fault is raised. Contiguous successful mappings retain native word/dword physical accesses; only genuinely discontinuous translated operands are split into byte accesses.

### 10. Active-MMU save/load

The SCC68070 already registers the complete persistent MMU state with MAME's save manager:

- MSR status;
- MCR control;
- all eight descriptor attribute fields;
- all eight descriptor lengths;
- all eight descriptor segment/valid bytes;
- all eight descriptor bases.

The translation engine introduces no hidden cache or derived state that must be separately serialized.

A full Mono-I integration fixture now programs the **real internal MMU registers**, enables translation, verifies fetch/read/write translation through `device_memory_interface::translate()`, captures a real in-memory MAME `ram_state`, changes the descriptor and disables the MMU, reloads the snapshot, and verifies that both the register contents and active translation are reconstructed.

## Code organization

`src/devices/machine/scc68070_helpers.h` contains the side-effect-free, constexpr-capable geometry, permission, stack, and MSR-fault model.

The live device keeps its existing register-layout `mmu_desc_t`, including its unused/documented byte lane. The helper templates consume the live structure's functional fields directly; there is no reinterpret cast and no replacement of the register/save-state layout.

`src/devices/cpu/m68000/scc68070.cpp` owns CPU access classification and complete-operand preflight. `src/devices/machine/scc68070.h` owns the live device policy: internal-address bypass, descriptor translation, protection, MSR update, and restartable bus-error request.

## Automated coverage

Focused helper coverage includes:

- disabled identity translation;
- all eight descriptor slots;
- flushed and duplicate CAM matches;
- first/last/out-of-range boundaries;
- minimum/maximum mode-1 and mode-2 lengths;
- independent `S/E/R/W` permissions;
- MSR fault encoding;
- mode-1 and mode-2 stack direction;
- cross-boundary operand preflight;
- compatibility with the live padded descriptor structure.

Full-machine integration coverage includes:

- programming the real Mono-I SCC68070 MMU register aperture;
- enabled fetch/read/write translation;
- changing a live descriptor base;
- disabling translation;
- real MAME save-state capture/load with the MMU active;
- restoration of MCR/MSR/descriptor contents and resulting translation.

## Remaining evidence-only / silicon-fidelity questions

These are deliberately **not** closed by inference:

- exact SCC68070 MMU lookup and bus-error cycle timing;
- exact exception-stack timing and any silicon-specific retry edge beyond Musashi's restartable-MMU facility;
- physical behavior if software creates duplicate active CAM entries for one logical segment;
- hardware capture of stack-segment boundary/wrap corner cases;
- any undocumented interaction between MMU faults and concurrent external bus arbitration.

The items in this section are hardware-fidelity questions. They are additional to the open software fault-delivery and executed-CPU validation work described in section 7 and the fresh audit; they are not the complete remaining-work list.
