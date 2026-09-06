# SCC68070 MMU translation checkpoint — 2026-09-06

This checkpoint covers the first five translation-model gates requested for the SCC68070 MMU campaign. It is intentionally not a claim that the complete MMU is finished; CPU-visible bus-error generation, access attributes, supervisor/read/write/execute enforcement, stack-segment semantics, live MSR reporting, and instruction/data intention integration remain separate gates.

## Evidence basis

The implementation is constrained by the Philips SCC68070 hardware documentation and contemporary Philips technical material:

- MCR bit 7 (`EN`) enables address translation/protection.
- MCR bit 6 (`SN`) selects mode 1 (`0`: 8 logical segments, 11-bit displacement) or mode 2 (`1`: 128 logical segments, 7-bit displacement).
- Logical addresses use a 10-bit offset within a 1 KiB block.
- Descriptor `F` is bit 7 of the segment-number byte; `F=0` invalidates the descriptor and `F=1` makes the segment number eligible for the associative comparison.
- Mode 1 uses an 11-bit maximum block displacement, giving 1–2048 KiB segment sizes.
- Mode 2 uses the seven most-significant bits of the stored 11-bit length field, giving 1–128 KiB segment sizes.
- The 14-bit descriptor base is the physical base in 1 KiB blocks.
- Only off-chip CPU addresses are MMU-translated; on-chip peripheral addresses and DMA accesses bypass the MMU.
- The eight on-chip descriptors form a fully associative CAM. The documentation describes simultaneous comparison and does not define priority for duplicate active segment numbers, so the software oracle reports duplicate matches as `multiple_match` rather than inventing a winner.

## Closed gates in this checkpoint

1. **MMU disabled identity/external-bus behavior**
   - The pure translation oracle returns the original external 24-bit address when `EN=0`.

2. **Every one of the eight descriptor slots independently**
   - Tests populate each slot in turn and prove the selected segment/base/displacement mapping.

3. **Descriptor selection / overlap**
   - Flushed descriptors never participate.
   - A single active matching descriptor is selected regardless of slot.
   - Duplicate active matches are surfaced explicitly as `multiple_match`; no undocumented slot priority is fabricated.

4. **Segment boundaries**
   - First byte and last byte of a segment translate.
   - The first block beyond the programmed maximum displacement reports `length_violation`.
   - An address in a different, uncached logical segment reports `not_present`.

5. **Minimum and maximum segment lengths**
   - Mode 1: encoded length 0 covers one 1 KiB block; 0x7ff covers 2048 blocks (2 MiB).
   - Mode 2: encoded effective length 0 covers one block; 0x7f covers 128 blocks (128 KiB), with low four stored length bits ignored by mode-2 geometry.

## Code organization

The address geometry now lives in `src/devices/machine/scc68070_helpers.h` as a side-effect-free, constexpr-capable translation oracle. The device MMU descriptor type aliases the shared descriptor representation so register emulation and tests cannot drift into different field layouts.

This is deliberately the foundation layer. The CPU callback path will consume the typed result only after the bus-error/status/attribute machinery is implemented, so an unmapped or out-of-range access cannot silently fall through to physical memory during an intermediate commit.

## Remaining MMU gates

- CPU-path integration for valid translations.
- Bus-error exception injection for not-present/length/attribute violations.
- MSR `N/ST/L/A/S/E/R/W` live reporting.
- Supervisor/read/write/execute permission checks.
- Stack-segment downward-growth behavior.
- Instruction-fetch vs data-read vs data-write intention handling.
- Cross-boundary multi-byte access behavior.
- Save/load while MMU mappings are actively in use.
- Full-device and CD-i regression coverage.
