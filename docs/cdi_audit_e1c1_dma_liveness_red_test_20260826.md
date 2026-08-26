# CD-i Audit E1c1: DVC DMA Held-Request RED Test

Baseline under audit: `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`.

Parent audit branch: `cdi-audit-e1c-dvc-dma-handoff`.

## Scope

This checkpoint tests only the Mono-I DVC/SCC68070 DMA liveness finding from E1c. It does not change emulator behavior and does not alter the validated branch.

The production ownership sequence under test is:

1. VMPEG asserts DREQ and the driver starts scheduled channel-1 service.
2. SCC68070 channel 1 becomes inactive before normal transfer completion, for example through software abort or another error/reprogramming path.
3. `cdi_state::dvc_dma_service_tick()` clears only `m_dvc_dma_service_active` and returns.
4. The DVC does not receive `dma_done()`, therefore its normal completion path does not clear `m_dma_active` or DREQ.
5. No service timer is re-armed, so a level-held request has no retry path.

## Test

`scripts/cdi_dvc_dma_liveness_audit.py`

This is intentionally a source-audit RED gate rather than the final runtime regression. It parses the actual production functions that currently own the handoff:

- `cdi_state::dvc_dma_req_w()`
- `cdi_state::dvc_dma_service_tick()`
- `cdi_dvc_device::dma_done()`

It exits `1` only while the SCC-inactive path simultaneously drops driver service ownership, returns without DVC completion, and does not re-arm service.

It exits `0` once that specific orphan pattern is absent.

A parser/infrastructure error exits `2` and must not be mistaken for either RED or GREEN.

## Result against the validated source behavior

Expected and observed result:

```text
RED: DVC DMA held-request liveness bug reproduced
  SCC channel inactive -> driver clears service-active and returns
  DVC dma_done() is not called -> normal DVC completion is skipped
  no service timer is re-armed -> the request has no retry path
  DREQ reassert while servicing ignored: 1
  dma_done clears DVC active/DREQ: 1/1
```

Return code: `1`.

This establishes a concrete pre-fix RED checkpoint.

## Mutation control

The audit gate was also run against a local test fixture derived from the same authoritative production functions with one deliberate mutation: the SCC-inactive branch was changed from dropping service ownership to re-arming the service timer.

Observed result:

```text
GREEN: held DVC request is no longer silently orphaned by the SCC-inactive path
  drops_service=0 returns=1
  completes_dvc=0 reschedules=1
```

Return code: `0`.

The mutation control demonstrates that the gate is detecting the liveness pattern rather than merely demanding the current source text remain unchanged.

## Limitations

This RED gate proves the source-level ownership bug but does not prove the exact hardware-correct recovery policy. In particular, it does not establish that polling/retrying every two SCC clocks is the physical behavior. The two-clock service cadence remains a compatibility model.

Before promotion, the eventual fix still needs a behavioral regression covering held DREQ plus SCC abort/re-arm, followed by the normal Philips/MAME validation stack. The source-audit gate should then be retained only if useful as a hardening check, or replaced by the stronger behavioral regression.
