# CD-i modernization workflow

These instructions apply to Philips CD-i / DVC project work. They do not require
auditing unrelated MAME systems. User instructions take precedence.

Start with `docs/CDI_MASTER_STATUS.md`. The repository is the source of truth;
conversation history is a handoff hint, not certification evidence.

## Request modes

- **Status** (`status`, `status report`, `percentages`, `what's left`, `what remains`):
  read the master branch index, matrix and next-batch section, then report the latest recorded state, confidence, gaps and
  next actions concisely. Do not re-audit implementation or rerun tests. If a row is
  clearly stale, reconcile that row against its linked evidence once.
- **Verified status** (`verified status`, `audit`, `re-audit`, `check the current
  branch`, `recalculate the percentages`): inspect relevant implementation, tests,
  history, TODOs and compatibility evidence. Update the affected master rows and
  subsystem documents. Separate implementation, test coverage, specification
  verification, hardware verification and demonstrated title compatibility.
- **Development** (`continue`, `continue the project`, `fix this`, `implement the
  next batch`, `work on ...`): inspect branch, HEAD and worktree status; read the
  master index, relevant subsystem document and recent relevant commits. Resume
  the next documented substantive task. Make code changes, meaningful tests,
  appropriate validation, documentation updates and coherent commits; push when
  authorized and possible. Do not stop at a proposed plan.

## Branch and worktree discipline

- Canonical branch: `cdi-dvc-modernization`.
- Audio branch: `audio/cdi-fidelity-100-campaign-20260905`; its
  `docs/cdi_audio_fidelity_campaign.md` controls the detailed audio matrix.
- Keep unrelated experimental DVC/FMV work isolated. A master row referencing
  another branch does not mean those changes exist in the current checkout.
- Preserve unrelated modifications. Do not reset, discard, stash or overwrite
  intentional local work automatically. In the Windows worktree this specifically
  includes `3rdparty/catch/single_include/catch.hpp`,
  `tests/emu/video/rgbutil.cpp`, `docs/cdi_fast_build.md`, and
  `scripts/cdi-build.ps1`.
- Stage exact task paths, prefer small coherent commits, and never force-push
  merely to refresh project status. Verify remote advancement before publishing.
- Do not redo de-emphasis, recovered VMPEG Q22 attenuation, or closed MMU/model
  gates unless current evidence identifies a real defect. Read the current ledger
  before accepting an older next-task recommendation.

## Evidence and documentation

Every substantive code change updates its subsystem document and affected master
rows: what changed, why, modeled behavior, validating tests and remaining uncertainty.
Record exact code commit, verification date, commands/results and evidence scope.
Use the code parent as the baseline for a subsequent documentation-only commit;
do not create self-referential certification hashes.

Percentages are conservative scoped engineering estimates, not assertion counts,
code coverage or game compatibility. Keep functional and hardware-fidelity values
separate. Use **Not estimated** when evidence or a denominator is missing; never
manufacture numbers or average overlapping subsystem rows. Label carried historical
estimates and stale values. Confidence must be High, Medium or Low and explained.
100% requires no known technically defensible work remaining in the stated scope;
hardware-blocked work stays open for any scope that includes hardware fidelity.

Use the master index as the fast entry point. Reconcile only changed/stale rows;
retain historical evidence as historical evidence. Apply the documented subsystem
regression gates to substantive code changes. Documentation-only maintenance needs
link/path, evidence-reference and diff checks, not a fresh emulator build.
