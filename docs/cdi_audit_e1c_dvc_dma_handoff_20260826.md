# CD-i Audit E1c: DVC DMA Handoff

Validated baseline: `cdi-project-audit-20260826` at `a014f032c23c67222c5f6c79634ad89ef2937bf3`.

Scope: Mono-I VMPEG/DVC request -> driver service loop -> SCC68070 DMA channel 1 -> DVC ingress/completion. Mono-II/DRVDSP architecture is out of scope.

## Result summary

### E1c-1: DMA pacing remains a compatibility model

The driver services one 16-bit word at a time and schedules the next service event after `DVC_DMA_SERVICE_CLOCK_TICKS == 2` SCC68070 clocks. There is no primary-hardware evidence in this audit establishing that cadence as the physical REQ/ACK/RDY/DTC timing.

Classification: hardware-fidelity uncertainty. Preserve current behavior pending stronger evidence.

### E1c-2: `m_dvc_dma_mac_mode` is duplicated/dead driver state

The driver queries SCC68070 channel-1 MAC state before starting the request, mirrors it into `m_dvc_dma_mac_mode`, saves/resets that mirror, and uses it only in diagnostics. Actual address advancement is performed inside `scc68070_device::dma_channel_transfer()` from the SCC68070 sequence-control register.

Classification: dead/duplicated state; low-risk cleanup candidate. Removing the mirror changes logging only if the log value is not instead kept as a local snapshot.

### E1c-3: VMPEG request state and driver service state can diverge

There are two independent active-state concepts:

- DVC `m_dma_active`: VMPEG request/ingress state.
- Driver `m_dvc_dma_service_active`: scheduled host service-loop state.

Normal completion converges them: the SCC transfer reaches zero, the driver clears service-active, calls `cdi_dvc_device::dma_done()`, and the DVC clears `m_dma_active` and deasserts DREQ.

Abort/error paths do not converge them.

If SCC channel 1 becomes inactive while DREQ remains asserted, `dvc_dma_service_tick()` clears only `m_dvc_dma_service_active` and returns. It does not call `dma_done()` and the DVC does not deassert DREQ. Because the driver reacts to DREQ callback transitions rather than owning a persistent line-level handshake, a later SCC restart does not automatically re-arm the service loop while DREQ is already high.

Classification: architectural liveness bug / missing integration behavior. Do not fix by blindly treating abort as successful DMA completion.

### E1c-4: transfer failure after external start can wedge the same way

`dma_channel_transfer()` can return false while the DVC request is still active (for example, if the live SCC channel state is changed to an invalid/reserved transfer configuration). The driver then clears its service-active flag and returns without completing or cancelling the DVC request.

Classification: correctness bug in exceptional/reprogramming paths; current commercial compatibility exposure unknown.

### E1c-5: normal completion calls `dma_done()` once

On the ordinary path the last successful `dma_channel_transfer()` clears SCC `CSR_CA` when the transfer counter reaches zero. The driver observes the inactive channel immediately after forwarding that final word, clears its own service-active flag, invokes `m_dvc->dma_done()`, and returns. `dma_done()` then clears DVC request state and emits DREQ clear; because driver service-active is already false, the clear callback cannot trigger a second completion.

Classification: normal-path completion ownership is internally consistent.

### E1c-6: DREQ reassert while service is active is not a new SCC transfer

If VMPEG emits another ASSERT callback while `m_dvc_dma_service_active` is already true, the driver logs a reassert and returns. The DVC command path can nevertheless reset its DVC-side DMA counters/target before issuing that ASSERT. This means command rewrites during an active DMA can change DVC bookkeeping without restarting/revalidating the SCC transfer.

Classification: suspicious command/reassert semantics; hardware behavior not established. Add regression/trace coverage before changing.

### E1c-7: normal mid-transfer save/load is structurally preserved, but failure recovery is incomplete

The SCC68070 saves channel status/control/counters/address state. The driver saves its service-active flag and diagnostics, and its `emu_timer` is scheduler-owned. The DVC saves request/ingress state. A successful load therefore has the pieces required to resume a normal in-flight service operation.

However, DVC save-state restore failure explicitly clears DREQ/DVC DMA state while there is no corresponding SCC-side cancellation. A restored SCC channel can therefore remain active after the DVC has abandoned the transfer.

Classification: save-state failure-path integration gap. Successful in-flight save/load still needs an explicit regression test.

## State ownership audit

### Behaviorally required driver state

- `m_dvc_dma_service_active`: current service-loop ownership/state.
- `m_dvc_dma_timer`: cadence implementation.

### Diagnostic-only driver state

- `m_dvc_dma_initial_words`
- `m_dvc_dma_service_events`
- `m_dvc_dma_transfer_serial`
- `m_dvc_dma_request_clock`
- `m_dvc_dma_first_clock`

These are useful validation telemetry but should not be mistaken for hardware state.

### Duplicated state candidate

- `m_dvc_dma_mac_mode`

The SCC68070 sequence-control register is authoritative for MAC behavior.

## Regression coverage required before a behavioral fix

1. Normal N-word DVC DMA transfers exactly N words and calls DVC completion once.
2. DREQ deassert during service stops further word ingress without fabricating a successful completion.
3. SCC software abort while DREQ is held does not permanently lose the request if firmware subsequently re-arms the DMA channel.
4. Transfer failure from a reprogrammed/invalid SCC channel cannot leave an unserviceable high DREQ forever.
5. A second VMPEG DMA command while a request is active has explicitly defined/tested behavior rather than silently resetting only DVC-side counters.
6. Save/load between DMA words resumes with the same remaining count, memory address, target, next word, and exactly one final completion.
7. Save-state reconstruction failure leaves SCC and DVC DMA ownership in a mutually consistent inactive/cancelled state.

## Recommended change order

1. Safe cleanup: remove `m_dvc_dma_mac_mode` as stored/saved driver state; retain a local log snapshot if desired.
2. Add focused integration regression coverage for normal completion and SCC abort/re-arm under held DREQ.
3. Only then change request/service ownership. Prefer modeling DREQ as persistent line state or otherwise providing an explicit SCC/DVC re-arm notification rather than polling every two clocks indefinitely.
4. Keep the two-clock service cadence unchanged until hardware evidence supports a timing change.
