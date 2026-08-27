# CD-i Audit E1c2: Held-DREQ DMA Re-arm Fix

Validated baseline remains `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`.

RED checkpoint: `cdi-audit-e1c-dvc-dma-handoff` at `a10e2d071b482e4a1d929e9983873d988a5061c6`.

Current behavioral-regression review branch: `cdi-audit-e1c2-dvc-dma-behavioral-regression`.

Native-tested race-fix implementation commit: `ff3adbc880eb6a709b5f7635eb0ee3e6d1f0689a` (`philips/cdi: close DVC DMA abort re-arm race`).

Behavioral-regression implementation commits:

- `a1fead15954704053d9612e853027554a7ef7479` (`philips/cdi: share DVC DMA service transitions`) adds the production-shared transition policy;
- `8b70dfeea0083d48a70f92b3f9adaba1bb6745ea` adds the focused held-DREQ abort/re-arm/exactly-once completion regression to `tests/emu/philips/cdi_hardening.cpp`;
- `89f49869ceaa7ab26f21e3cdcb0da66ab9100b37` (`philips/cdi: exercise shared DVC DMA transition policy`) wires the same transition functions into the production CD-i driver;
- `4c8052213b16332222fe7d4a5126b7dc76d1c10b` removes the temporary one-shot GitHub Actions validator after runner allocation did not occur during the audit session.

## Scope

E1c2 fixes only the Mono-I DVC/SCC68070 held-DREQ liveness problem established by E1c/E1c1.

It intentionally does **not** change:

- the current one-word-per-two-SCC-clock DVC service cadence;
- normal DVC `dma_done()` completion ownership;
- VMPEG command-reassert semantics;
- `m_dvc_dma_mac_mode` cleanup;
- DVC decoder/presentation behavior;
- save-state decoder reconstruction;
- the separate failed-postload SCC/DVC ownership issue;
- Mono-II or DRVDSP behavior.

## Problem

VMPEG DREQ is level-like, but the previous CD-i driver effectively used the DREQ callback as a one-shot start event. If SCC68070 DMA channel 2 became inactive before normal completion, the scheduled service loop cleared `m_dvc_dma_service_active` and stopped. DVC remained DMA-active and kept DREQ asserted. A later SCC repair/re-arm had no DREQ edge to restart service.

The fix must not treat SCC abort as successful DVC completion and must not replace the lost event with indefinite two-clock polling.

## Fix model

E1c2 keeps the existing transfer engine and cadence intact and adds a narrow event-driven re-evaluation path.

### CD-i board-side DREQ level

`cdi_state` stores `m_dvc_dma_req_state`, updated from every DVC DREQ callback. It is reset and save-state registered as board-side input-line state.

The existing `dvc_dma_req_w(ASSERT_LINE)` path remains authoritative for starting service. Its checks remain:

- memory-to-device direction;
- word transfer size;
- valid channel-2 MAC mode;
- non-zero remaining count;
- SCC `dma_channel_external_start(1)` acceptance.

The behavioral-regression branch factors the first four predicates through `cdi_dvc_dma::request_configuration_valid()` so production and test code exercise one shared decision function. SCC `dma_channel_external_start(1)` remains a real SCC-device call in the production driver.

### SCC68070 reconfiguration notification

The SCC68070 device exposes a small `devcb_write8` DMA reconfiguration notification. `dma_w()` emits the zero-based channel index after CPU writes in each DMA channel's register range.

This callback is **integration plumbing**, not a claim that physical SCC68070 hardware exposes a separate reconfiguration signal.

Only DVC-equipped Mono-I machine configurations bind the callback.

### Re-arm policy

`cdi_state::dvc_dma_reconfigure_w()` handles two cases for SCC DMA channel index 1 (hardware DMA channel 2) while VMPEG DREQ remains asserted:

- if driver service is idle, it re-runs the existing DREQ-start path and therefore the existing SCC direction/size/MAC/count/start validation;
- if driver service is still marked active but the SCC channel has already become inactive, it stops only the driver-local service loop and cancels the pending service timer. DREQ remains held, so a subsequent SCC channel-2 register change can re-run the start validation.

The behavioral-regression branch routes that transition decision through `cdi_dvc_dma::reconfigure()`. The helper can return only `ignore`, `stop_service`, or `retry_request`; it does not own the SCC channel, DVC device, timer, or completion callback.

The second case closes the race where an SCC-side software abort/reconfiguration notification can arrive before the next scheduled DVC service tick observes that channel 2 became inactive.

SCC abort itself is still not converted to `dma_done()`, and the inactive service path still stops rather than polling. Later firmware changes/re-arm of channel 2 re-evaluate the still-high DREQ against the existing SCC configuration checks.

### Final-word policy

After a successful SCC word transfer and `m_dvc->dma_w(data)`, the behavioral-regression branch calls `cdi_dvc_dma::post_transfer()` with the real SCC channel-active state.

- an active channel returns `schedule_next`, leaving service active and preserving the validated two-SCC-clock inter-word timer;
- an inactive channel returns `complete`, clears only driver-local service-active state, and reaches the existing single `m_dvc->dma_done()` call.

The helper does not invoke `dma_done()` itself. Completion therefore remains owned by the production driver and DVC device exactly as before.

## Preserved invariants

Source review confirms:

1. `DVC_DMA_SERVICE_CLOCK_TICKS` remains `2` and is still the inter-word service delay.
2. The SCC-inactive service path still does not call DVC `dma_done()`.
3. The SCC-inactive service path still does not schedule a retry timer.
4. The normal final-word path still contains exactly one `m_dvc->dma_done()` call.
5. DVC `dma_done()` remains responsible for clearing DVC-side DMA-active state and deasserting DREQ.
6. PAL and NTSC DVC machine configurations both bind the SCC reconfiguration notification.
7. The abort-before-service-tick race stops only driver-local service and preserves the held DREQ for later re-evaluation.
8. The behavioral regression and production driver now consume the same request/reconfigure/post-transfer decision functions rather than duplicating a test-only transition model.
9. SCC transfer state, memory access, transfer count, CA/COC status and external-start acceptance remain owned by `scc68070_device`.

The production-wiring commit `89f49869ceaa7ab26f21e3cdcb0da66ab9100b37` was diff-reviewed against its parent. The only changes in `src/mame/philips/cdi.cpp` are the helper include, shared request-validation call, shared reconfiguration transition, and shared post-transfer completion decision. Unrelated reconstruction attempts were rejected and removed from the review branch before this clean commit was retained.

## Audit gate

`scripts/cdi_dvc_dma_liveness_audit.py` was upgraded from the E1c1 RED detector into a RED-to-GREEN contract gate.

The gate requires the event-driven held-request contract, including:

- DREQ level member/tracking/reset/save coverage;
- preservation of the existing SCC DMA direction/size/MAC/count/start validation;
- CD-i reconfiguration callback declaration and request/channel guards;
- no `dma_done()` fabrication in the re-arm path;
- SCC callback accessor/member/construction and channel-2 notification;
- exactly two DVC machine bindings (PAL and NTSC);
- unchanged two-clock cadence;
- exactly one normal service-loop `dma_done()` site;
- no polling added to the SCC-inactive abort path.

The optional reconfiguration-function parser deliberately treats the pre-fix source as RED rather than as a test-infrastructure error.

## Mutation validation

A local synthetic source fixture exercising the same audit-contract decisions produced:

```text
pre_fix_missing_reconfigure: RED
fixed: GREEN
missing_scc_notify: RED
missing_retry: RED
lost_request_validation: RED
cadence_changed: RED
duplicate_completion: RED
fabricated_abort_completion: RED
abort_polling_added: RED
```

A separate regex/path mutation check also produced GREEN for the complete fixed fixture and RED when either the SCC channel-2 notification, DREQ retry, or two-clock cadence invariant was removed.

These are test-harness controls, not a substitute for compiling/running MAME.

## Existing native validation

The race-corrected implementation at `ff3adbc880eb6a709b5f7635eb0ee3e6d1f0689a` was validated in a detached WSL worktree with the following results:

```text
mametests-build: 0
scc-tests:       0
cdi-tests:       0
cdi-build:       0
source-audit:    0
diff-check:      0
```

The corresponding commands were:

```text
make -j2 REGENIE=1 NOWERROR=1 TESTS=1 EMULATOR=0
./mametests "[emu][machine][scc68070]"
./mametests "[emu][philips][cdi]"
make -j2 SUBTARGET=cdiaudit SOURCES=src/mame/philips/cdi.cpp
python3 scripts/cdi_dvc_dma_liveness_audit.py
git diff --check
```

`REGENIE=1 NOWERROR=1` was required for the test build because the existing GCC 11 environment otherwise promoted unrelated volatile-deprecation warnings in `tests/emu/video/rgbutil.cpp` to errors. The CD-i-only build had already compiled and linked E1c2 successfully even when that unrelated mametests build issue was present.

That checkpoint establishes that the race fix itself compiles and links and does not regress the repository's pre-existing SCC68070 or Philips/CD-i unit coverage.

## Behavioral regression

The new focused regression is:

`CD-i DVC held DMA request survives SCC abort, re-arms, and completes once`

with tags:

`[emu][philips][cdi][dvc][dma][hardening]`.

It exercises the production-shared transition policy in this order:

1. valid memory-to-device word DMA with remaining data;
2. active driver service followed by SCC channel-2 inactivity/abort;
3. `stop_service` without completion;
4. held request surviving unrelated-channel and deasserted-request guards;
5. idle held request becoming `retry_request` after SCC repair/reconfiguration;
6. resumed transfer returning `schedule_next` while SCC remains active;
7. final transfer returning `complete` only once when SCC becomes inactive.

The exact helper and sequence were also compiled independently as C++17 with warnings promoted to errors:

```text
g++ -std=c++17 -Wall -Wextra -Werror -pedantic ...
E1C2_STANDALONE_GREEN
```

This standalone result validates the constexpr transition policy and focused state sequence only. It is **not** a substitute for compiling the current MAME branch or running the Catch2 regression through `mametests`.

A temporary GitHub Actions validator was queued to run the full native command set against the review work, but no hosted runner was assigned during the audit session. The temporary workflow was then removed so the review branch would not retain self-modifying CI infrastructure. No native result is inferred from the queued run.

## Validation status

**Race-fix source-audit status at `ff3adbc880eb6a709b5f7635eb0ee3e6d1f0689a`: GREEN.**

**Existing native build/unit-test status at `ff3adbc880eb6a709b5f7635eb0ee3e6d1f0689a`: GREEN.**

**Production-shared behavioral regression implementation: COMPLETE on `cdi-audit-e1c2-dvc-dma-behavioral-regression`.**

**Standalone transition-policy regression: GREEN.**

**Native build and `mametests` execution of the production-shared behavioral-regression branch: PENDING.**

**Hardware-fidelity status for exact SCC/VMPEG request/ack timing: UNVALIDATED.**

No authoritative validated branch has been changed. E1c2 remains on the review branch until the production-shared regression receives native MAME validation and the project's later consolidated Philips/CD-i validation is complete.

The required native validation remains:

```text
make -j2 REGENIE=1 NOWERROR=1 TESTS=1 EMULATOR=0
./mametests "[emu][machine][scc68070]"
./mametests "[emu][philips][cdi]"
make -j2 SUBTARGET=cdiaudit SOURCES=src/mame/philips/cdi.cpp
python3 scripts/cdi_dvc_dma_liveness_audit.py
git diff --check
```

Do not describe E1c2 as fully native-validated or hardware-timing-validated until those gates are satisfied on the production-shared regression branch.
