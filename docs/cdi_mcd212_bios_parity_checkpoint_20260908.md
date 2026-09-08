# MCD212 BIOS field-parity regression checkpoint — 2026-09-08

## Scope

A real Philips CD-i Mono-I BIOS run on the unified branch progressed beyond the
previous SCC68070 UART regression but remained on the cyan startup screen. A
bounded debugger probe showed normal early RAM scans, then a stable firmware loop
at `0x0041c738`–`0x0041c754` polling MCD212 Control/Status Register 1 at
`0x004ffff1`:

```text
0041C738: btst    #$7, $4ffff1.l
0041C740: bne     $41c738
0041C742: btst    #$7, $4ffff1.l
0041C74A: beq     $41c742
0041C74C: btst    #$5, $4ffff1.l
0041C754: beq     $41c738
```

Bit 7 is Display Active (`CSR1R_DA`) and bit 5 is field parity (`CSR1R_PA`).
The trace proved that Display Active changed state and IRQ 6 continued to fire,
but PA never advanced, so firmware restarted the wait indefinitely.

## Root cause

The dynamic MCD212 timing modernization configured the MAME screen's visible
rectangle to the active video region. The PA toggle still lived in
`screen_update()` behind:

```cpp
if (scanline == (m_total_height - 1))
    m_csrr[0] ^= CSR1R_PA;
```

For the tested PAL profile, the visible region ends before the final blanking
line. MAME therefore never called the renderer for the source line that owned the
parity toggle. A field-timing status bit had accidentally become dependent on
whether an invisible raster row was rendered.

## Fix

Code commit `4b70e032b11a514a046075dcc07fdb2f301574b7`
(`fix(mcd212): advance field parity at field boundary`) moves PA advancement to
`mcd212_device::ica_tick()`, immediately before Display Active is cleared at the
field boundary, and removes the renderer-owned toggle.

This keeps parity tied to the MCD212 field scheduler rather than the visible clip
rectangle. It also preserves the existing PA use in ICA selection, interlace field
phase and composed output.

Regression commit `bbc96e45bb0eda560b4e9d2547fd547c91387766`
(`test(mcd212): guard BIOS field parity wiring`) adds
`scripts/cdi_mcd212_field_parity_audit.py` to the fast CD-i gate. The audit requires
PA advancement in `ica_tick()` before the DA clear and rejects any return of that
advancement to `screen_update()`.

## Runtime verification

The production CD-i executable was rebuilt from the locally patched
`cdi-unified` tree with:

```text
make -j4 \
  SUBTARGET=cdi \
  SOURCES=src/mame/philips/cdi.cpp,src/mame/philips/cdidvc_plmpeg.cpp
```

The rebuild recompiled `src/mame/philips/mcd212.cpp`, archived `libmame_cdi.a`,
and linked `cdi` successfully.

The real Mono-I BIOS was then run with:

```text
./cdi cdimono1 \
  -rompath ~/src/mame/roms \
  -log
```

It advanced past the cyan initialization screen into the actual CD-i BIOS UI.
The run reported 100.01% average speed over 17 seconds. The two Mono-I SERVO/SLAVE
MCU images remain marked `ROM NEEDS REDUMP` because the current driver borrows
those dumps from the cdi910 set; that warning was present before this fix and did
not prevent the BIOS from reaching its UI.

The preceding 60-second diagnostic run had already shown that the old SCC68070
UART unmapped-access storm was gone. This MCD212 fix closes the next observed live
firmware blocker.

## Evidence boundary

This is direct firmware-runtime evidence for MCD212 DA/PA field-boundary behavior
on the tested Mono-I PAL BIOS path. It is not physical waveform validation and it
does not certify every interlace/QHY/timing combination or any retail title.

The MCD212 engineering grade remains **70% (Medium confidence)**: the boot result
materially strengthens firmware-visible timing evidence but does not close the
broader display-mode and hardware-validation obligations already recorded in the
master matrix. Compatibility remains **Not estimated** until retained retail-title
runtime evidence is collected.

At the time this checkpoint was written, GitHub Actions run `34181211181`
(`CI (CD-i fast)`) for regression commit `bbc96e45...` had started; its final
result must be recorded before treating the automated source guard as certified.
