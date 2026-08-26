# CD-i Audit E1c2: Held-DREQ DMA Re-arm Fix

Validated baseline remains `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`.

RED checkpoint: `cdi-audit-e1c-dvc-dma-handoff` at `a10e2d071b482e4a1d929e9983873d988a5061c6`.

Fix/review branch: `cdi-audit-e1c2-dvc-dma-rearm-3`.

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

`cdi_state` now stores `m_dvc_dma_req_state`, updated from every DVC DREQ callback. It is reset and save-state registered as board-side input-line state.

The existing `dvc_dma_req_w(ASSERT_LINE)` path remains authoritative for starting service. Its existing checks are retained:

- memory-to-device direction;
- word transfer size;
- valid channel-2 MAC mode;
- non-zero remaining count;
- SCC `dma_channel_external_start(1)` acceptance.

### SCC68070 reconfiguration notification

The SCC68070 device exposes a small `devcb_write8` DMA reconfiguration notification. `dma_w()` emits the zero-based channel index after CPU writes in each DMA channel's register range.

This callback is **integration plumbing**, not a claim that physical SCC68070 hardware exposes a separate reconfiguration signal.

Only DVC-equipped Mono-I machine configurations bind the callback.

### Re-arm policy

`cdi_state::dvc_dma_reconfigure_w()` retries the existing DREQ-start path only when:

- the notification is for SCC DMA channel index 1 (hardware DMA channel 2);
- VMPEG DREQ is still asserted;
- the driver is no longer servicing the request.

This means SCC abort itself is not converted to `dma_done()`, and the inactive service path still stops rather than polling. Once firmware subsequently changes/re-arms channel 2, the still-high DREQ is re-evaluated against the existing SCC configuration checks.

## Preserved invariants

Source review confirms:

1. `DVC_DMA_SERVICE_CLOCK_TICKS` remains `2` and is still the inter-word service delay.
2. The SCC-inactive service path still does not call DVC `dma_done()`.
3. The SCC-inactive service path still does not schedule a retry timer.
4. The normal final-word path still contains exactly one `m_dvc->dma_done()` call.
5. DVC `dma_done()` remains responsible for clearing DVC-side DMA-active state and deasserting DREQ.
6. PAL and NTSC DVC machine configurations both bind the SCC reconfiguration notification.

## Audit gate

`scripts/cdi_dvc_dma_liveness_audit.py` was upgraded from the E1c1 RED detector into a RED-to-GREEN contract gate.

The gate now requires all of the following for event-driven GREEN:

- DREQ level member/tracking/reset/save coverage;
- preservation of the existing SCC DMA direction/size/MAC/count/start validation;
- CD-i reconfiguration callback declaration and request/channel/idle guards;
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

## Validation status

**Source-audit status: GREEN.**

**Native MAME/mametests/runtime status: UNVALIDATED.**

No authoritative branch has been changed. E1c2 must remain on the review branch until it receives at minimum:

1. a native compile;
2. `mametests` / existing SCC68070 and Philips unit coverage;
3. the E1c2 audit gate against the real checkout;
4. targeted DVC PAL/NTSC runtime smoke coverage;
5. the project's consolidated Philips/CD-i validation before promotion.

A future stronger runtime regression should exercise an actual DVC request across SCC abort, status/config repair, re-arm, resumed word ingress, and exactly-once final completion. Until that exists, this source-level fix should not be described as fully validated hardware behavior.
