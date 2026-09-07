# CD-i branch cleanup — 2026-09-07

At the user's explicit request, deleted all 35 obsolete branches from
`matt-dawidowicz/mame` and the two obsolete local branches in
`/home/thematt523/src/mame`. Only `cdi-unified` remains in that fork and clone.
The upstream `mamedev/mame` repository was not modified. Other clones and the
previously unavailable local-only FMV experiment were outside this inventory.

## Audit and preservation

- Audited against unified commit `3e322e8095c28fce3f574817df4b77b5b70f9ddc`.
- All 34 non-staging remote tips and both obsolete local tips are ancestors of
  unified: zero unique commits. Their exact history remains reachable on unified.
- `cdi-mmu-exec-staging-20260906` has five unique commits limited to two temporary
  patch scripts and their candidate-validation workflow. These scaffolding files
  were not integrated into the active tree. Its candidate tip is already an ancestor.
- Saved and verified those five commits in `cdi-retired-mmu-staging.bundle` in the
  task's outputs directory before deleting the staging branch. This incremental
  bundle requires `f330c0901c7d1aaf5581f50735ee03c6b8aeaa1f`,
  which remains reachable on unified. Bundle SHA-256: `c873f1d19c7409077501d91a50dbb75df0572bccc6f635f2a55f088618bf2b01`.
- GitHub confirmed `cdi-unified` as default; the fork had no open PRs and upstream
  had no open PRs by this fork owner. All listed staging workflow runs were completed.
- The sole registered worktree used `cdi-unified` and was clean.
- Remote deletion was atomic, with an expected-tip lease for each deleted ref.
  Local deletion used `git branch -d`. Ref enumeration then confirmed only unified,
  and `origin/HEAD` was refreshed to unified.
- No emulator code changed. Verification covered ancestry, bundle integrity,
  remote/local refs, documentation links and whitespace; emulator tests were not rerun.

## Deleted remote branches

| Branch | Audited tip | Commits outside unified |
| --- | --- | ---: |
| `archive/cdi-dvc-modernization-pre-rewrite-20260905` | `6b55ed1ed611d70a846fd2df311d917f12791991` | 0 |
| `archive/cdi-live-dma-dev-history-20260905` | `d6dfd665ae5f9d35d850989ad73b050eda16a057` | 0 |
| `audio/cdi-fidelity-100-campaign-20260905` | `213d886190e0f52457f324f1d31278e22d392f22` | 0 |
| `build/cdi-fast-iteration-20260903` | `f0d78dfbda5c7fbbdb11c3fc1ba25a58e731146f` | 0 |
| `cdi-audio-completion` | `afc4643afe22ca087ce2c582e2427e822a498f85` | 0 |
| `cdi-audit-e1b-vdsc-berr-status` | `8e2ed6f260a8e1d48c130426c130a7538cc67528` | 0 |
| `cdi-audit-e1b-vdsc-berr-status-2` | `45d895f11700a427012ecf50dcc11ed6946ba388` | 0 |
| `cdi-audit-e1c-dvc-dma` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | 0 |
| `cdi-audit-e1c-dvc-dma-handoff` | `a10e2d071b482e4a1d929e9983873d988a5061c6` | 0 |
| `cdi-audit-e1c2-dvc-dma-behavioral-regression` | `1c55a2b244c2f29f293b21f727b538197ca5f2da` | 0 |
| `cdi-audit-e1c2-dvc-dma-rearm` | `4f78511be6dc9eeb5bb5938f305c34a818476ec5` | 0 |
| `cdi-audit-e1c2-dvc-dma-rearm-2` | `2ae445314d95312910d3e647c31c70821be594a1` | 0 |
| `cdi-audit-e1c2-dvc-dma-rearm-3` | `e5156d9505c2b3fb3038c32c1809c006d27d7e72` | 0 |
| `cdi-audit-e2b-display-enable` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | 0 |
| `cdi-core-modernization` | `c25e70ee11efddf9416135d99b9658c4dc6256ad` | 0 |
| `cdi-dsp56001-lle` | `712386e56f399bcdf071ff30fba856ae860f426a` | 0 |
| `cdi-dvc-modernization` | `dd55c7ba57e51239322411657b06548da7f25c10` | 0 |
| `cdi-dvc-reference-20260824` | `6b55ed1ed611d70a846fd2df311d917f12791991` | 0 |
| `cdi-dvc-upstream-cleanup` | `072265b08f27a9b4c006c7933b9c1839340dbcbd` | 0 |
| `cdi-dvc-upstream-completion` | `01df3929ceab7f792ab0d51981f21b13b5f25b6e` | 0 |
| `cdi-hardening-ci-base` | `d96b4ff4894759ad7a10934e20b67843e2a3987a` | 0 |
| `cdi-hardening-ci-run` | `069fc7f87792be04f762c932b61bee3e5cd18a47` | 0 |
| `cdi-hardening-slave-100` | `e2f5c1cc9c771c7574708bd093f4fa0a7e2a898c` | 0 |
| `cdi-mmu-exec-candidate-20260906` | `f330c0901c7d1aaf5581f50735ee03c6b8aeaa1f` | 0 |
| `cdi-mmu-exec-staging-20260906` | `1c3621286000416b5da663fe9a60459c4b09ace8` | 5 |
| `cdi-project-audit-20260826` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | 0 |
| `cdi-project-audit-batch3-review-20260826` | `1df0e4dcd8d6aada3acc9f7743f70948fd3f6871` | 0 |
| `cdi-project-audit-batch4-review-20260826` | `9477eefec01a0e8ba77b5b51e3c3fa41da63cf6f` | 0 |
| `cdi-project-audit-batch5-review-20260826` | `9e620b677396f22e29b17bb6e45b34a4b18b3839` | 0 |
| `cdi-project-audit-morning-validation-20260826` | `d39e31ff866bc6cc49f5d280fdef286bdd2d80e7` | 0 |
| `cdi-project-audit-promotion-20260826` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | 0 |
| `cdi-upstream-scc68070-completion` | `7039d8e2dcc9742a7fc5bc4539b51379dc7890dc` | 0 |
| `cdi-upstream-scc68070-foundation` | `48a0270aaf590d40f3f2bae59a155b162cd1c24c` | 0 |
| `dvc/architecture-cleanup` | `bc60c05a9a85dd3ad81e1280e94fe10c1149afd6` | 0 |
| `master` | `25ae98b69b2ed4f4cd3015c65d4b48a635525741` | 0 |

Deleted local branches: `cdi-project-audit-20260826` and `master`, at the same tips listed above.

## Staging recovery

The local bundle is deliberately outside the source repository. From a clone containing unified history, recover the staging ref if needed:

```sh
git fetch /path/to/cdi-retired-mmu-staging.bundle refs/remotes/origin/cdi-mmu-exec-staging-20260906:refs/heads/recovered-mmu-staging
```
