# CD-i upstream synchronization — 2026-09-06

## Scope and ancestry

- Destination: `cdi-unified` in `matt-dawidowicz/mame`.
- Starting project HEAD: `d23cc2554ffc640259f9e2fe71a7acc81a26c7fa`.
- CI/documentation preparation: `05f611f0a9dfff5b87185e37a58a36050b37f628` (first merge parent).
- Published merge: `ade0f78abf4367f07c9d2fd3d383f448110c4cb2`, via [PR #3](https://github.com/matt-dawidowicz/mame/pull/3).
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
Local results: **17,393,781 assertions / 219 cases PASS**; the DVC DMA source
liveness audit is **GREEN**.

[CI run 34066705811](https://github.com/matt-dawidowicz/mame/actions/runs/34066705811)
on 2026-09-06 passed the generated helper target (**17,393,781 assertions / 219
cases**), DVC liveness audit, production build and emulator-linked integration
(**21 assertions / 6 cases**). CI checked out the PR test merge
`3f3acf4d9c42aa9d73db863106b733e9ff433a71`. Its tree,
`d0392992e6081c11d6f9a2227334e4a1faed4f40`, exactly matches both the reviewed local
merge and the published merge `ade0f78abf4367f07c9d2fd3d383f448110c4cb2`.
The final verification commit only updates documentation.

The upstream snapshot is an ancestor of the merged branch (zero commits behind
that snapshot). All 32 source branch tips match the consolidation inventory and
fork `master` remains `25ae98b69b2ed4f4cd3015c65d4b48a635525741`. Workflow YAML,
relative documentation links and task documentation diffs pass validation.
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
