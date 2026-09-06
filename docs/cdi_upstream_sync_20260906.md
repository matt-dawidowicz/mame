# CD-i upstream synchronization — 2026-09-06

## Scope and ancestry

- Destination: `cdi-unified` in `matt-dawidowicz/mame`.
- Project parent: `d23cc2554ffc640259f9e2fe71a7acc81a26c7fa`.
- Upstream parent: `1fb001f9bfab5cf0148fdfe8755659c4a869831b` in `mamedev/mame`.
- Common ancestor: `eff67ffd33d012f978282ba457a3b1cdc151dd92`.
- Before merge: 433 project-only commits and 60 upstream-only commits.
- All 32 historical project branch references and fork `master` are retained.
  The upstream repository is not modified. The existing branch inventory remains
  the recovery/ancestry record.

Git merged the two histories cleanly. Of 906 upstream changed paths, the only
path also changed by the project was `src/mame/mame.lst`; its independent driver
list additions merged automatically. No manual source conflict resolutions or
new CD-i behavior changes were required. CD-i sources, SCC/DSP changes, MPEG
backend and project tests retain their unified versions.

## Relevant upstream changes

The upstream batch includes other systems and software-list maintenance, plus
shared runtime and utility changes. Commit `4bb0fa2bbc7a` removes text-writing
methods from `core_file`/`emu_file`, migrates callers to utility streams, and
changes log-file ownership. Other commits add standard header includes and
remove the archiver dependency on `zutil.h`. These are build/integration risks
for the CD-i target even though Philips device sources did not change.

No uses of the removed text-writing interfaces were found in the Philips source
or integration tests. The focused workflow now also triggers for shared emulator,
utility, frontend and build-script changes so future upstream merges receive the
production gate. Existing tests are reused; this merge creates no new modeled
hardware behavior requiring invented unit tests.

## Verification

Local helpers are rebuilt from 29 translation units with GCC 13.3.0, C++17 and
`-O1`, using the command in [the consolidation record](cdi_branch_consolidation_20260906.md).
The DVC DMA source liveness audit passes. Fresh helper and emulator-linked CI
results will be recorded against the published code merge after execution.
This is a focused CD-i integration gate, not an all-system MAME build, retail-disc
compatibility demonstration or hardware-fidelity certification. The previous
percentage reassessment obligations remain open. CDIC SRAM bounds remains the
next substantive engineering task.

## Default branch setting

The requested default is `cdi-unified`, but the connected GitHub tools expose no
repository-settings mutation. Automatic approval review rejected browser access
as a workaround for that missing operation. The default remains `master` until
changed through GitHub repository Settings > General > Default branch. This is
an administration limitation, not a merge or code-access failure.
