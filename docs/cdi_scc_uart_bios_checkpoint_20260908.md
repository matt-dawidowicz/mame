# SCC68070 UART BIOS regression checkpoint — 2026-09-08

## Scope

A real CD-i BIOS boot attempt produced a log of roughly 1 GB because firmware
entered a tight UART polling loop. The captured beginning and end both showed the
same unmapped low-byte accesses in the SCC68070 internal register block:

- write `0x80002016` (`UCR` lane),
- write `0x80002014` (`UCS/UCSR` lane),
- write `0x80002010` (`UMR` lane),
- read `0x8000201a` (`RHR` lane),
- repeated read `0x80002012` (`USR` lane).

For the 68070's 16-bit bus, MAME maps these byte-wide low-byte handlers on the
corresponding odd addresses. The firmware-visible UART order is therefore:

| Register | Handler address | Aligned low-byte bus access |
| --- | ---: | ---: |
| UMR | `0x80002011` | `0x80002010` |
| USR | `0x80002013` | `0x80002012` |
| UCS/UCSR | `0x80002015` | `0x80002014` |
| UCR | `0x80002017` | `0x80002016` |
| THR | `0x80002019` | `0x80002018` |
| RHR | `0x8000201b` | `0x8000201a` |

## Root cause and fix

The modernization branch had replaced that production CD-i firmware layout with
a different preliminary/document-derived register ordering. The helper constants
then drove the live SCC68070 address map, and a unit test asserted the incorrect
layout, so the regression was internally self-consistent while the BIOS could no
longer reach the expected registers.

Code commit `c4d6d0ce93ae633df5831bb51bca7fa571750a83`
(`fix(scc68070): restore firmware UART register map`) restores the firmware-visible
addresses while retaining the newer UART status, timing, break and FIFO behavior.

Regression commit `a7c66c4dfc77d497e2eb568e8fdd1a3e0a493ce3`
(`test(scc68070): cover CD-i BIOS UART access sequence`) replaces the obsolete
address assertion and encodes the exact five-address BIOS sequence above. This
specifically prevents the observed unmapped polling loop from being reintroduced
by another register-map reshuffle.

## Verification

GitHub Actions run `34172619049` (`CI (CD-i fast)`) passes on
`a7c66c4dfc77d497e2eb568e8fdd1a3e0a493ce3`:

- generated Musashi source freshness: PASS,
- SCC/CD-i helper build: PASS,
- helper suite: **17,405,341 assertions / 225 test cases**, PASS,
- DVC DMA liveness audit: **GREEN**,
- emulator-linked CD-i integration build: PASS,
- integration suite: **12,201 assertions / 26 test cases**, PASS.

The linked integration build compiles the live SCC68070 and Philips CD-i driver
sources, so the restored map is compile- and regression-clean against that project
gate.

The required post-fix BIOS rerun has now also been performed on the corrected
branch. With `-log`, the Mono-I BIOS no longer emits the `0x80002010`–`0x8000201a`
unmapped UART storm and advances through its RAM tests into later MCD212 firmware
initialization. A subsequent debugger trace isolated a different blocker: firmware
was polling MCD212 CSR1 DA/PA state at `0x004ffff1`. After the independent MCD212
field-parity fix in `4b70e032b11a514a046075dcc07fdb2f301574b7`, the same real BIOS
advances through the cyan startup screen into the actual CD-i BIOS UI. See
[`cdi_mcd212_bios_parity_checkpoint_20260908.md`](cdi_mcd212_bios_parity_checkpoint_20260908.md).

## Evidence boundary

The original BIOS-blocking UART register-map regression is therefore closed by
both automated and real-firmware evidence. The successful boot does **not** prove
complete SCC68070 UART behavior: live mode changes, break, overrun, electrical
waveforms, active-peripheral save/restore and wider timer/UART event combinations
remain open.

No hardware-fidelity or retail-title compatibility claim is made here. The
SCC68070 engineering completion grade remains **75% (Medium confidence)** because
the new boot evidence closes the specific map/regression obligation but not those
wider live peripheral-verification items.
