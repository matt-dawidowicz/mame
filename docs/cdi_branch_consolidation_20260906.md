# CD-i branch consolidation — 2026-09-06

## Active development branch

`cdi-unified` is the single active project branch, as requested by Matt. It combines
all 32 project branches present in the fork at inventory time. At consolidation,
`master` was unchanged and source branches were retained as historical references.
The user subsequently requested their deletion; see the
[cleanup record](cdi_branch_cleanup_20260907.md) for the complete later inventory,
ancestry checks and staging recovery details. Routine project work goes to
`cdi-unified`; the audio campaign is a subsystem ledger on this branch.

The specifically named local experiment `dvc/fmv-clock-underflow-trace-20260905`
was absent from both the remote enumeration and available worktrees. No access to
the Windows checkout or unpushed local work is claimed. Import that branch when it
is made available; this consolidation covers the enumerated repository branches.

## Inventory and ancestry

The JSON companion retains all exact source tips, merge bases and independent-tip
classification. Rewritten history was deepened until real common ancestors were
available. Duplicate branch names pointing to the same commit are counted in the
32-branch inventory. Thirteen independent tips cover all 32 source branches.
The published merge uses the audio tip as its first parent and the other twelve
independent tips as additional parents; the tree is the reviewed result of the
sequential local merges and conflict resolutions below.

| Source branch | Assessed tip | Coverage |
| --- | --- | --- |
| `archive/cdi-dvc-modernization-pre-rewrite-20260905` | `6b55ed1ed611d70a846fd2df311d917f12791991` | Ancestor of an included parent |
| `archive/cdi-live-dma-dev-history-20260905` | `d6dfd665ae5f9d35d850989ad73b050eda16a057` | Independent merge parent |
| `audio/cdi-fidelity-100-campaign-20260905` | `213d886190e0f52457f324f1d31278e22d392f22` | Independent merge parent |
| `build/cdi-fast-iteration-20260903` | `f0d78dfbda5c7fbbdb11c3fc1ba25a58e731146f` | Ancestor of an included parent |
| `cdi-audio-completion` | `afc4643afe22ca087ce2c582e2427e822a498f85` | Independent merge parent |
| `cdi-audit-e1b-vdsc-berr-status` | `8e2ed6f260a8e1d48c130426c130a7538cc67528` | Independent merge parent |
| `cdi-audit-e1b-vdsc-berr-status-2` | `45d895f11700a427012ecf50dcc11ed6946ba388` | Independent merge parent |
| `cdi-audit-e1c-dvc-dma` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | Ancestor of an included parent |
| `cdi-audit-e1c-dvc-dma-handoff` | `a10e2d071b482e4a1d929e9983873d988a5061c6` | Ancestor of an included parent |
| `cdi-audit-e1c2-dvc-dma-behavioral-regression` | `1c55a2b244c2f29f293b21f727b538197ca5f2da` | Independent merge parent |
| `cdi-audit-e1c2-dvc-dma-rearm` | `4f78511be6dc9eeb5bb5938f305c34a818476ec5` | Ancestor of an included parent |
| `cdi-audit-e1c2-dvc-dma-rearm-2` | `2ae445314d95312910d3e647c31c70821be594a1` | Independent merge parent |
| `cdi-audit-e1c2-dvc-dma-rearm-3` | `e5156d9505c2b3fb3038c32c1809c006d27d7e72` | Ancestor of an included parent |
| `cdi-audit-e2b-display-enable` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | Ancestor of an included parent |
| `cdi-core-modernization` | `c25e70ee11efddf9416135d99b9658c4dc6256ad` | Independent merge parent |
| `cdi-dsp56001-lle` | `712386e56f399bcdf071ff30fba856ae860f426a` | Ancestor of an included parent |
| `cdi-dvc-modernization` | `dd55c7ba57e51239322411657b06548da7f25c10` | Independent merge parent |
| `cdi-dvc-reference-20260824` | `6b55ed1ed611d70a846fd2df311d917f12791991` | Ancestor of an included parent |
| `cdi-dvc-upstream-cleanup` | `072265b08f27a9b4c006c7933b9c1839340dbcbd` | Ancestor of an included parent |
| `cdi-dvc-upstream-completion` | `01df3929ceab7f792ab0d51981f21b13b5f25b6e` | Ancestor of an included parent |
| `cdi-hardening-ci-base` | `d96b4ff4894759ad7a10934e20b67843e2a3987a` | Ancestor of an included parent |
| `cdi-hardening-ci-run` | `069fc7f87792be04f762c932b61bee3e5cd18a47` | Independent merge parent |
| `cdi-hardening-slave-100` | `e2f5c1cc9c771c7574708bd093f4fa0a7e2a898c` | Ancestor of an included parent |
| `cdi-project-audit-20260826` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | Ancestor of an included parent |
| `cdi-project-audit-batch3-review-20260826` | `1df0e4dcd8d6aada3acc9f7743f70948fd3f6871` | Ancestor of an included parent |
| `cdi-project-audit-batch4-review-20260826` | `9477eefec01a0e8ba77b5b51e3c3fa41da63cf6f` | Ancestor of an included parent |
| `cdi-project-audit-batch5-review-20260826` | `9e620b677396f22e29b17bb6e45b34a4b18b3839` | Ancestor of an included parent |
| `cdi-project-audit-morning-validation-20260826` | `d39e31ff866bc6cc49f5d280fdef286bdd2d80e7` | Independent merge parent |
| `cdi-project-audit-promotion-20260826` | `a014f032c23c67222c5f6c79634ad89ef2937bf3` | Ancestor of an included parent |
| `cdi-upstream-scc68070-completion` | `7039d8e2dcc9742a7fc5bc4539b51379dc7890dc` | Independent merge parent |
| `cdi-upstream-scc68070-foundation` | `48a0270aaf590d40f3f2bae59a155b162cd1c24c` | Ancestor of an included parent |
| `dvc/architecture-cleanup` | `bc60c05a9a85dd3ad81e1280e94fe10c1149afd6` | Independent merge parent |

## Conflict resolutions

- **Canonical/status:** retain the latest audio campaign details and the shared
  fresh audit. Canonical's older review date does not replace the newer ledger.
- **Pre-rewrite/DSP lineage:** archived `6b55ed1e` and rewritten `8ff9c703` match in
  the relevant CD-i device/helper/test implementation, except the newer Mono-II
  plane callbacks. Preserve those callbacks and subsequent audio work. Import the
  unique DSP56001 host/bootstrap interpreter and later decoder/loop/wrap fixes.
- **DSP morning audit:** retain partial genuine instruction execution, immediate
  MOVEP correction, zero-count DO semantics, bootstrap address state, attribution
  corrections and native tests. Keep the Mono-II DSP disabled and host window
  unmapped: complete firmware execution is still unverified.
- **DVC DMA:** retain the behavioral branch's shared request/reconfigure/transfer
  policy and held-DREQ state. It supersedes the earlier rearm-2 callback that did
  not handle an abort before the next service tick. Merge the distinct audit docs.
- **Archived live-DMA harness:** the current live fixture already includes the
  archived implementation. Preserve seven synthetic drivers and later MMU/audio/
  edge/save cases, rather than reverting to the archive's single-driver shim.
- **Old audio-completion candidate:** preserve current audio, de-emphasis, saved
  decoder reconstruction, Mono-II and pointer capture. The older branch staged
  header-only state and intentionally removed save/board paths; its build-error
  record confirms it was unfinished. Its unique `cdicdic_audio.h` and associated
  tests remain explicitly historical comparison material, outside the active test
  targets. Its scripts and branch-specific CI remain historical, not current gates.
- **Core modernization:** its functional patches are already represented in the
  rewritten lineage; preserve the later unified reset and plane-access callbacks.
- **DVC architecture:** backend/device/save helpers match original import
  `3936fed5ab`; preserve all later audio/video fixes. Retain the legacy timing
  comparison header and RGB test assertion improvements. Keep current region
  configuration and Catch signal-stack handling.
- **SCC completion:** integrate Timer 1/2 match/capture/count, UART mode/framing/
  break/receive handling, DMA retained counters, masked address writes, zero-MTC
  semantics and error/device-termination APIs. Collapse its eight staging `.inc`
  fragments into the normal `scc68070.cpp`, retaining current MMU translation and
  the DVC DMA reconfiguration callback. The source branch's UART reserved-bit
  read-zero model replaces the older read-one helper; this merge does not claim a
  new independent hardware verification of that discrepancy.

## Integration changes required by the merge

SCC START now owns CA. DREQ gates an already started controller rather than
implicitly starting it. MTC=0 on an active controller represents 65536 operands.
The DVC remaining-count interface and initial-count telemetry are widened to
32 bits so this count is not mistaken for an empty request. Raw guest MTC remains
16 bits. CDIC compatibility wrappers continue to use the SCC-owned transfer API.

Live tests program SCC START explicitly. The edge fixture now checks DREQ before
START, activation from a held request without another DVC edge, immediate abort
notification, exact partial-transfer restart, and an entire 65536-word transfer.
These are required interface reconciliation tests, not title compatibility claims.

The unified fast gate includes the three DSP test translation units and the SCC
peripheral suite, alongside existing MMU/audio/video tests. Its path filters now
include machine SCC sources and DSP sources/tests, closing audit finding F7.
Platform-wide builds remain available manually; unified branch pushes use the
focused gate. The old optimization auto-writer is constrained to its historical
audio branch so branch creation cannot replay source patches on unified development.

## Verification and remaining work

- Local final merged helper suite: **PASS, 17,393,781 assertions / 219 cases**,
  GCC 13.3.0, C++17, `-O1`, including the widened-count regression.
- DVC source liveness audit: **GREEN**; both board bindings and the held-request
  path remain connected, with one completion site and unchanged service cadence.
- Unified code commit: `e2d6fffb0853e54fde1be1429c6f12b01f4045a5`.
- [Unified CI run 34065213340](https://github.com/matt-dawidowicz/mame/actions/runs/34065213340), job `101572661665`: **PASS**.
  Generated helper target: **17,393,781 assertions / 219 cases**; emulator-linked
  production build and integration target: **21 assertions / 6 cases**. This run
  includes explicit START, held-request re-arm, immediate abort and the complete
  65536-word transfer regression. The MMU case still tests translation queries and
  state restoration, not executed faulting instructions.
- Include guards, XML/JSON validation, UI translations and documentation build
  passed. Full Linux/macOS/Windows platform builds were intentionally skipped;
  the focused Linux CD-i gate compiled the combined production implementation.
- No new retail-disc, physical hardware, or full Mono-II firmware validation.

Known CDIC SRAM bounds, MMU executed-fault delivery, Q/TOC and actual decoded A/V
coverage gaps from the fresh audit remain open. The SCC/DMA/DSP implementations
have changed materially, so previous percentages are not silently promoted to new
certification. Reassess those affected rows against the combined branch after the
integration gates (now passing). The next source-defect task remains CDIC SRAM bounds.

Reproduce helpers from the repository root:

```python
import re, subprocess
from pathlib import Path
block = Path('scripts/src/tests.lua').read_text().split('project("cdihelpertests")')[1].split('-- Full-machine')[0]
files = [p for p in re.findall(r'MAME_DIR \.\. "([^"]+\.cpp)"', block) if p.startswith('tests/')]
subprocess.run(['g++', '-std=c++17', '-O1', '-pthread',
    '-I3rdparty/catch/single_include', '-Isrc/mame/philips',
    '-Isrc/devices/machine', '-Isrc/devices/cpu/dsp56000', '-Isrc/lib/util',
    *files, '-o', '/tmp/cdi-unified-helpers'], check=True)
subprocess.run(['/tmp/cdi-unified-helpers'], check=True)
```
