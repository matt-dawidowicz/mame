# Philips CD-i non-Mono-II audit ledger

Baseline: `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`

Review branch: `cdi-audit-e1b-vdsc-berr-status`

Mono-II DRVDSP/DSP architecture work is intentionally parked. Findings here must keep functional correctness separate from hardware fidelity and must not introduce title-specific behavior.

## Batch map

| Batch | Scope | Initial risk | Notes |
| --- | --- | --- | --- |
| E1 | Mono-I platform integration: reset vector decode, VDSC bus error, board memory decode/maps | Critical | Highest-value boundary between SCC68070, MCD212 and board glue. |
| E2 | MCD212 remaining rendering/control: matte, cursor, mosaic, transparency, DCA/ICA edge cases, wait states | High | Phase B improved timing/QHY but did not prove all legacy display behavior. |
| E3 | DVC/VMPEG hardware boundary: register constants, MPEG RAM gate, DMA pacing, presentation policy, IRQ | Critical | Current source explicitly contains compatibility/provisional behavior. |
| E4 | Mono-I CDIC/audio: AUDCTL, TOC/subcode, CDDA/XA transitions, attenuation, error/status behavior | High | Phase C intentionally retained several compatibility/unknown behaviors. |
| E5 | Cross-device IRQ/DMA integration: IRQ4 arbitration, SCC channel-1 handoff, acknowledge timing | High | Device-local tests do not prove board-level arbitration/timing. |
| E6 | Save/reset ownership and postload reconstruction | High | DVC decoder reconstruction is explicitly incomplete; cross-device line/timer state needs review. |
| E7 | Input/SLAVE/Quizard integration where hardware-relevant | Medium | Preserve HLE evidence boundaries; avoid expanding into Mono-II SLAVE architecture. |
| E8 | Cleanup: dead/duplicated state, outdated MAME idioms, stale logging/constants/comments | Medium | Do after correctness-sensitive batches so cleanup does not obscure behavior changes. |

## E1a - reset-vector / temporary ROM decode

### Finding E1a-1 - inherited reset-vector fabrication

**Classification:** confirmed hardware-fidelity bug; medium functional risk; possible compatibility dependency.

Current `cdi_state::machine_reset()` copies the first eight bytes of the BIOS into plane-A DRAM. The MCD212 specification instead describes temporary ROM decoding for the first four 680x0 word accesses after reset (SSP and PC fetch), after which addresses `0x000000-0x000007` return to normal DRAM decoding. Reset therefore must not overwrite DRAM bytes 0-7.

Secondary evidence: the independent MiSTer CD-i implementation models boot selection as bus/decode behavior rather than by modifying guest DRAM.

**Required fix shape:** model a reset-armed four-word ROM decode window; do not count debugger reads; preserve/save the remaining access count; remove the reset-time DRAM copy only after validation.

**Required regression coverage:** first four accesses source BIOS; fifth sources DRAM; reset does not mutate DRAM bytes 0-7; reset rearms the window; partial-window save/load restores the remaining count.

## E1b - VDSC bus error

Primary MCD212 behavior: BERR is generated only when CSR1W.BE is enabled and a selected transfer remains unacknowledged for at least one whole video line (approximately 64 microseconds). The VDSC then sets CSR2R.BE. BERR is released when UDS/LDS are released, and reading CSR2R clears BE. Reading CSR2R also clears IT1/IT2.

### Finding E1b-1 - immediate unconditional board-level BERR

**Classification:** confirmed hardware-fidelity bug; potentially functional; compatibility-sensitive.

Mono-I currently overlays a catch-all `bus_error_r/w` across the 24-bit program map. Any unmapped access reaching it immediately pulses the 68k BERR input regardless of CSR1W.BE and without the documented watchdog delay. This bypasses the MCD212 system-controller behavior.

**Do not change yet:** replacing the immediate exception with an asynchronous ~64 us wait requires careful CPU/bus-cycle integration. Preserve current validated behavior until that mechanism is designed and tested.

### Finding E1b-2 - CSR2R.BE is dead state

**Classification:** confirmed correctness bug with high-confidence primary evidence; low compatibility risk to fix separately.

`CSR2R_BE` is defined but current source never sets it. `csr2_r()` clears only IT1/IT2, although the specification requires a status read to clear BE as well. Thus software cannot observe the documented VDSC bus-error status even when MAME generates a bus error through the compatibility handler.

**Safe partial-fix direction:** while retaining current immediate CPU BERR behavior, latch CSR2R.BE only when CSR1W.BE is enabled and the compatibility bus-error path fires; clear BE together with IT1/IT2 on a side-effecting CSR2R read. Explicitly document that CPU exception timing remains compatibility behavior.

### Finding E1b-3 - bus-error fault address loses byte-lane information

**Classification:** likely correctness bug; needs a focused CPU exception-frame test before code change.

`bus_error_r/w` reports `offset * 2` to `set_buserror_details`. The callbacks do not currently accept `mem_mask`, so an unmapped odd-byte access can only be reported as the preceding even word address. Current MAME examples commonly use the same legacy pattern, but the Musashi API accepts an exact fault address and the CD-i handler should preserve UDS/LDS lane information if the SCC68070 exception frame exposes it.

**Next check:** add a tiny 68070-side regression proving the stacked fault address for even byte, odd byte and word unmapped accesses before changing callback signatures.

## E1c - Mono-I physical memory decode

### Finding E1c-1 - phantom RAM at 0x500000-0x57ffff

**Classification:** confirmed correctness bug; high-confidence removal candidate; observable compatibility change requiring runtime validation.

The current Mono-I map installs generic writable RAM at `0x500000-0x57ffff`. This range is outside the MCD212/VDSC address decoder: primary MCD212 documentation places DRAM in `0x000000-0x3fffff`, ROM/system-I/O/register decoding in `0x400000-0x4fffff`, and for the two-bank 256K x16 (`TD=0`) configuration only `0x000000-0x07ffff` plus `0x200000-0x27ffff` receive DRAM acknowledgement. No DTACK is generated for addresses outside the configured DRAM ranges.

The independent MiSTer CD-i hardware notes identify two 256K x16 DRAM devices and record bus-error holes on a CDI 210/05 from `0x080000-0x1fffff` and `0x500000-0xcfffff`. This is secondary evidence but directly contradicts MAME's writable `0x500000-0x57ffff` block.

Source history also indicates that this block was historically suspicious rather than established hardware: the 2013 MCD212 modernization retained `0x500000-0x57ffff` only as a commented-out RAM mapping and treated the surrounding high range as unmapped/NOP.

**Required fix shape:** delete the generic `map(0x500000, 0x57ffff).ram()` entry so the range falls through to the existing unmapped/bus-error path. Do not combine this with the separate E1b watchdog/BERR-timing correction.

**Validation requirement:** compile plus normal Philips/mametests/cdivalidate coverage, then an explicit Mono-I probe confirming the range is no longer writable RAM and a commercial-title smoke pass to catch any accidental dependency on the phantom block.

## E1d - Mono-I ROM and MCD212 system-I/O decode

Primary MCD212 documentation divides `0x400000-0x4fffff` into system ROM (`0x400000-0x4ffbff`), system I/O (`0x4ffc00-0x4fffdf`), channel-2 registers (`0x4fffe0-0x4fffef`), and channel-1 registers (`0x4ffff0-0x4fffff`). The VDSC generates ROM DTACK itself but only asserts CSIO for system-I/O accesses; the external system-I/O device, if any, must provide DTACK.

Current MAME maps only the 512 KiB BIOS image at `0x400000-0x47ffff`, and maps the MCD212 register window at `0x4fffe0-0x4fffff`. Thus `0x480000-0x4fffdf` currently falls through to the board-level unmapped/BERR path.

### Finding E1d-1 - upper MCD212 ROM-select range is incompletely represented, but returned data is not yet proven

**Classification:** confirmed decode-model gap; functional behavior in `0x480000-0x4ffbff` remains uncertain; do not change yet.

The VDSC unquestionably asserts CSROM throughout `0x400000-0x4ffbff`, so treating `0x480000-0x4ffbff` as though the VDSC did not select ROM is architecturally wrong. However the Mono-I service documentation identifies a 512 KiB system ROM and separate board glue signals for ROM chip select and ROM DTACK. The evidence reviewed so far does not establish whether accesses in the upper part of the VDSC ROM-select aperture alias the 512 KiB ROM, are further qualified by board glue, or otherwise return no device data.

The independent MiSTer implementation follows the full MCD212 CSROM aperture, but its SDRAM backing is an implementation choice and is not sufficient primary evidence for physical Mono-I aliasing.

**Required next evidence:** inspect the Mono-I ROM/glue schematic or hardware capture closely enough to determine which CPU address lines reach the physical system ROM and how `CSROMON/CSROMN` are qualified. Only then decide whether MAME should mirror the BIOS or use a narrower board-level decode beneath the VDSC CSROM window.

### Finding E1d-2 - system-I/O aperture is correctly a separate hardware question, not an MCD212 register extension

**Classification:** no confirmed MAME bug yet; keep explicitly unmapped pending board evidence.

The MCD212 does not internally acknowledge `0x4ffc00-0x4fffdf`; it only asserts CSIO. The reviewed Mono-I service-manual signal listing identifies chip selects for ROM, NVRAM, SLAVE, CDIC, VDSC, DSP/glue and related devices, but no `CSIO`/system-I/O consumer has yet been identified. Therefore it would be speculative to add a device or generic storage here. If no board device responds, the eventual result should be the documented VDSC watchdog/BERR path from E1b rather than today's immediate unconditional compatibility BERR.

## E1e - Mono-I SLAVE address decode and byte lane

The reverse-engineered SLAVE interface has four channel registers at relative addresses `1`, `3`, `5`, and `7`. It is connected only to main-CPU data lines D7-D0, so the registers are odd-address byte accesses. Historical MAME hardware notes additionally state that A2/A1 are wired to MCU port C bits 1/0, D7-D0 to port A, R/W to port D bit 7, and /DTACK comes from port B bit 6.

Current MAME maps `0x310000-0x317fff` to a 16-bit HLE handler without a lane mask, then maps `0x318000-0x31ffff` as `noprw`. The HLE handler itself treats its word offset as a channel index and only accepts offsets 0-3.

### Finding E1e-1 - SLAVE is mapped on the wrong CPU byte lane

**Classification:** confirmed hardware-interface correctness bug; likely low risk to fix separately but requires validation.

Because the address-map entry lacks `umask16(0x00ff)`, accesses using the upper 68000 byte lane can invoke the SLAVE HLE even though the physical SLAVE is wired only to D7-D0 and its documented registers are at odd addresses.

**Required fix shape:** make SLAVE accesses explicitly lower-byte-only. Do not combine this with any mirroring/decode-range correction so lane semantics can be validated independently.

**Required regression coverage:** odd-byte accesses to `0x310001/3/5/7` reach channels A-D; even/upper-byte byte accesses do not mutate or consume SLAVE transport state; word accesses preserve the physical lower-byte semantics rather than exposing a fictitious 16-bit peripheral.

### Finding E1e-2 - the 0x317fff/0x318000 split is not supported by the visible MCU wiring

**Classification:** strong likely decode/mirroring bug; board-level ATTEX aperture still needs primary confirmation before code change.

The known MCU wiring exposes only A2/A1 for channel selection, not the higher address bits represented by the current 32 KiB split. The independent MiSTer implementation decodes the entire `0x31xxxx` range as SLAVE chip select and supplies only A2/A1 to the MCU, which would naturally mirror the four channel registers throughout that board-selected aperture.

This is strong secondary corroboration but not enough to assert the ATTEX's exact SLAVE chip-select equation as primary fact. The current `.noprw()` upper half should therefore be treated as inherited unexplained behavior, not validated hardware behavior.

**Required next evidence:** recover the ATTEX/Mono-I decode equation or a hardware address probe for `0x31xxxx`. If full-byte `0x31` decoding is confirmed, replace the arbitrary split with four lower-byte registers mirrored according to the unconnected higher address lines.

## E2a - MCD212 ROM/DRAM DTACK and arbitration

The current machine handlers call `eat_cycles()` for every Mono-I plane-RAM and system-ROM access, with the delay supplied by `mcd212_device::ram_dtack_cycle_count<Path>()` or `rom_dtack_cycle_count()`.

### E2a ROM result - current programmable ROM delay is consistent with the MCD212 timing tables

**Classification:** no bug found in this pass; preserve current behavior.

The MCD212's DD/DD1/DD2 table specifies progressively longer ROM acknowledgement windows of approximately 3-4, 5-6, 7-8, and 9-10 VDSC CLK periods when programmable delay is enabled, and approximately 11-12 or more CLK periods with DD cleared. With the SCC68070/VDSC clock relationship used by CD-i, the existing return values `{2, 3, 4, 5}` SCC68070 cycles and the 7-cycle DD=0 case are consistent with those documented ranges. Do not churn this code without contrary hardware timing evidence.

### Finding E2a-1 - IC1/IC2 incorrectly gate display-decoder DRAM contention

**Classification:** confirmed timing-model correctness bug; potentially software-visible; fix requires a bounded arbitration model rather than a one-line deletion.

The MCD212 documentation lists Display Decoder 1, Display Decoder 2, ICA/DCA Controller 1, and ICA/DCA Controller 2 as distinct DRAM masters. DCR1.DE enables display access to DRAM. IC1/IC2 enable the ICA mechanisms (and gate whether DCA can be enabled); they do not enable or disable the display decoders themselves.

Current `ram_dtack_cycle_count<Path>()` returns the minimum two SCC68070 cycles immediately whenever `DCR_ICA_BIT` for the path is clear. During an active display fetch region this incorrectly removes contention from the display decoder simply because ICA is disabled.

The tempting fix of merely deleting the IC check is not correct either: outside active display, ICA and DCA ownership depends on IC/DC state, refresh periods still reserve slots, and documented free-run periods can accept system accesses immediately. The correct small fix should explicitly classify the current raster position as display-fetch, ICA, DCA, refresh, or free-run and then apply the documented 11-CLK channel / 5-CLK system-slot arbitration only when a video-related master actually owns the path slot.

### Finding E2a-2 - focused DTACK regression coverage is missing

**Classification:** regression-coverage gap.

Existing MCD212 unit coverage concentrates on timing profiles, ICA/DCA pointers and limits, interrupt masking, external-video eligibility, and QHY helpers. There is no focused test that holds DE/IC/DC and raster phase independently and verifies the RAM-delay result. Because the current function depends directly on live screen/machine phase, a testable pure arbitration helper is preferable to reproducing device internals in tests.

**Required regression vectors:** DE=0 free-run; active-display DE=1 with IC=0 still contended; ICA window with IC=0 free-run versus IC=1 contended; DCA window with IC/DC combinations; final/free-run region; all 16 slot phases showing the 11-CLK channel and 5-CLK system portions.

## Current stop point

No validated behavior has been promoted. The authoritative branch remains untouched. E1 findings are documented, and E2a has identified one confirmed DRAM-arbitration bug plus missing focused regression coverage. No source fix has been staged because the correct repair must distinguish display, ICA, DCA, refresh, and free-run ownership rather than removing a single condition.