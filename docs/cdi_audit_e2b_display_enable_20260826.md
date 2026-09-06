# CD-i audit E2b - MCD212 Display Enable and ICA/DCA mode gating

Baseline: `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`

Review branch: `cdi-audit-e1b-vdsc-berr-status`

Scope is deliberately narrow: DCR1.DE and its interaction with display fetches, CSR1R.DA, ICA, and DCA. Mono-II DSP/DRVDSP remains out of scope.

## Primary hardware behavior

The MCD212 DCR1 `DE` bit is the global Display Enable. The hardware documentation states that DE enables display access to DRAM and synchronization outputs.

The same documentation defines `CSR1R.DA` as vertical Display Active information: DA is high while the display controller is fetching information from video memory.

Section 5.6.5 further specifies that ICA and DCA are also gated by DE. The legal modes are:

| DE | IC | DC | ICA | DCA |
| --- | --- | --- | --- | --- |
| 1 | 0 | x | No | No |
| 1 | 1 | 0 | Yes | No |
| 1 | 1 | 1 | Yes | Yes |
| 0 | x | x | No | No |

Thus DCA cannot run merely because DC is set: both DE and IC must also be set.

## Current MAME behavior

`screen_update()` enters the active-display path solely from raster position. It does not test DCR1.DE before:

- setting `CSR1R.DA`;
- calling `process_vsr<0>()` and `process_vsr<1>()`;
- advancing the video start registers;
- mixing the two display planes;
- drawing the hardware cursor;
- updating interlace/external-video cached output.

`ica_tick()` executes each ICA based only on that path's IC bit. It does not test the global DE bit.

`ica_tick()` also reloads each runtime DCA pointer from DCP based only on DC.

`dca_tick()` executes each DCA based only on DC. It tests neither DE nor IC.

The existing RAM-DTACK code does test DE, so the device is currently internally inconsistent: with DE clear, CPU plane-RAM accesses are treated as uncontended while the rendering and command engines can still consume/advance DRAM-backed state.

## Finding E2b-1 - display fetch and VSR progression ignore DE

**Classification:** confirmed functional correctness bug and hardware-fidelity bug.

When DE is clear, the display controller must not perform display DRAM fetches. Current MAME can nevertheless continue decoding plane data and advancing VSR on every active source line. This means disabling the display does not freeze the display-file fetch state as the hardware definition requires.

A visible blanking difference is secondary to the more important state difference: guest software can later re-enable DE and observe VSR/display progression that should never have occurred while display access was disabled.

## Finding E2b-2 - CSR1R.DA can assert while DE is clear

**Classification:** confirmed status-register correctness bug.

DA is defined as indicating that the display controller is fetching video memory. Current `screen_update()` sets DA whenever the raster lies inside MAME's active display region, regardless of DE. Therefore software polling CSR1R can observe Display Active even though the global display engine is disabled.

The exact sub-line transition timing when software changes DE during an active line remains a finer hardware-timing question. The high-confidence requirement is simpler: DE=0 must never produce display-fetch DA activity.

## Finding E2b-3 - ICA execution ignores global DE

**Classification:** confirmed functional correctness bug.

The hardware mode table explicitly disables ICA whenever DE=0. Current `ica_tick()` tests IC only, so an ICA can execute and modify MCD212 state while the hardware would leave it inactive.

This can affect CLUTs, transparency/matte state, display parameters, VSR/DCP control flow, and generated interrupts depending on the command stream.

## Finding E2b-4 - DCA execution uses DC alone and ignores both DE and IC

**Classification:** confirmed functional correctness bug; potentially higher impact than the visible-display symptom.

The documented DCA enable equation is effectively `DE && IC && DC`. Current `dca_tick()` uses DC alone. Current `ica_tick()` also reloads the live DCA address from DCP using DC alone.

Consequently the currently impossible hardware combinations `DE=0,DC=1` and `DE=1,IC=0,DC=1` can still execute DCA commands in MAME.

## E2b implementation boundary

Do not turn this into a broad MCD212 rewrite. A reviewable correction should centralize the documented mode predicates and use them consistently:

- display fetch enabled: `DE`;
- ICA path enabled: `DE && IC[path]`;
- DCA path enabled: `DE && IC[path] && DC[path]`.

Use those predicates to gate only the relevant engines. Do not stop the MAME raster/timer infrastructure itself merely because the physical MCD212 synchronization pins would be disabled; synchronization-output pin fidelity is a separate issue.

For the rendering path, DE=0 must at minimum prevent VSR/display-memory progression and DA assertion. The exact analogue output level while synchronization is disabled should not be invented to justify additional behavior.

## Required regression coverage

1. DE=0 with valid nonzero VSR/ICM: rendering callbacks do not advance either VSR and DA remains clear.
2. DE=0, IC=1, DC=1: neither ICA nor DCA executes and DCA runtime pointers are not reloaded as an enabled command stream.
3. DE=1, IC=0, DC=1: neither ICA nor DCA executes.
4. DE=1, IC=1, DC=0: ICA executes, DCA does not.
5. DE=1, IC=1, DC=1: ICA and DCA retain current validated execution behavior.
6. Re-enable DE after a disabled interval: display fetch resumes from the unadvanced VSR state rather than from a fictitiously progressed address.
7. CSR1R polling with DE held clear never reports DA solely because the MAME raster is in its active region.

## Validation policy

Any source fix should live on a dedicated review branch/commit and receive focused compile/unit coverage first. It must then join the later consolidated `mametests`, Philips validation, `cdivalidate`, and representative commercial-title smoke run before promotion to the authoritative validated branch.

No title-specific compatibility exception is justified for these mode predicates: they are explicit chip-level behavior.
