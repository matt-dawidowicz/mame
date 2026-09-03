# Philips CD-i / Digital Video Cartridge upstreaming status

Last reviewed: 2026-09-03

This document is the current engineering status for preparing the Philips CD-i and Digital Video Cartridge work for upstream MAME review. It replaces the older phase-by-phase research log. Historical runtime evidence is still useful, but research checkpoints are not treated as upstream-ready commits.

## Canonical branches and provenance

- `cdi-dvc-reference-20260824` — frozen research reference at `6b55ed1ed611d70a846fd2df311d917f12791991`. Do not rewrite this branch; it preserves the complete validated research implementation and its historical instrumentation.
- `cdi-upstream-scc68070-foundation` — clean SCC68070 prerequisite branch based on then-current upstream MAME `81c45c5bdf00904cf5b20c03e9b89d3a75994cdb`. Current pushed head: `48a0270aaf590d40f3f2bae59a155b162cd1c24c`.
- `cdi-dvc-upstream-cleanup` — subtractive cleanup branch based on the frozen research reference. Cleanup commits through this review are `9927b533f7f6f706feba83b50aa7cecd7c10b72a`, `3639dc7c9cf7b1249f883701bd31198cf966ec77`, `62172912fec1c4f816e56890f296d3da242416dc`, and `83f521d79eee4ce6c8863ef8bebbe2797ecc9be2`.

The old `cdi-dvc-modernization` branch remains a research branch. It should not be used directly as the base of an upstream pull request.

## Evidence labels

- **H** — primary hardware documentation.
- **S** — source code, commit history, or upstream MAME behavior.
- **T** — automated test evidence.
- **R** — emulator, firmware, or title runtime observation.
- **C** — deliberate compatibility model without sufficient hardware proof.
- **U** — unknown or not implemented.

Compatibility models must remain explicitly identified as such. Runtime success is not sufficient evidence to relabel a compatibility model as hardware behavior.

## Cleanup completed

### Removed from the upstream preparation path

#### Generic SDL CD-i pointer hacks

Removed in `9927b533f7f6f706feba83b50aa7cecd7c10b72a`.

Removed project-specific modifications from:

- `src/osd/sdl/osdsdl.cpp`
- `src/osd/sdl/window.cpp`

The research implementation hard-coded CD-i machine names in generic SDL input handling and altered host pointer capture/hiding policy for those machines. This is an OSD layering violation: generic SDL code must not know about individual CD-i drivers. If CD-i pointer behavior later requires a MAME-wide capability, that capability should be generic and requested through normal input/driver interfaces.

#### Global Catch/glibc test-runner workaround

Removed in `9927b533f7f6f706feba83b50aa7cecd7c10b72a`.

`tests/main.cpp` was restored to upstream behavior. The project-local `SIGSTKSZ` override was unrelated to CD-i emulation and must not travel with a CD-i submission. If current MAME needs a Catch/glibc compatibility fix, it belongs in an independent test-infrastructure change.

#### CDIC and SLAVE research tracing

Removed in `3639dc7c9cf7b1249f883701bd31198cf966ec77`.

The final research commit added unconditional CDIC sector/DBUF/command trace output and enabled SLAVE input logging by default. Those changes collected bring-up evidence but did not alter guest-visible behavior. Normal MAME execution should not carry project-specific always-on traces.

#### Mono-II structural experiment

Removed from this cleanup branch in `62172912fec1c4f816e56890f296d3da242416dc`.

Removed:

- `src/mame/philips/cdimono2.h`
- `tests/emu/philips/cdimono2.cpp`
- Mono-II wiring in `cdi.cpp` and `cdi.h`
- Mono-II test registration

Mono-II is a separate platform effort and is not required to add Digital Video Cartridge support to Mono-I. It should return only as an independently reviewable series with ROM/runtime evidence.

#### Standalone DVC audio save-replay regression

Removed in `83f521d79eee4ce6c8863ef8bebbe2797ecc9be2`.

`tests/emu/philips/cdidvc_audio_replay.cpp` specifically validated reconstruction of opaque PL_MPEG state from a replay journal. DVC machines do not currently advertise save-state support, so this regression is not part of the initial upstream target.

## SCC68070 prerequisite status

The SCC68070 work has been reconstructed separately on `cdi-upstream-scc68070-foundation` rather than copied wholesale from the research branch.

Current SCC cleanup includes:

- deterministic initial values for DMA transfer/address counters while preserving MT/MAC/DAC across SCC RESET, matching the Philips register reset table;
- correct masked partial writes to the 24-bit DMA memory and device address counters;
- removal of the DVC-specific `dma_channel_external_start()` behavior from the SCC-only prerequisite;
- restoration of the existing `dma()` interface used by current CDIC code;
- internal `/4` UART baud-clock handling with optional XCKI source;
- focused regression tests for reset preservation, DMA byte lanes, IRQ priority, UART clock selection, I2C side effects, and MMU masks;
- updated source status text describing the SCC implementation as partial rather than a skeleton.

The SCC branch is statically audited but is **not yet build-certified after reconstruction**. No GitHub CI status is attached to its current head in this fork. A native `mametests`/CD-i build is required before it should become an upstream PR.

The final upstream history should probably split the current SCC foundation further so DMA/register correctness, interrupt arbitration, UART behavior, and unrelated peripheral fixes can be reviewed independently.

## DVC implementation retained

The research DVC device contains substantial useful implementation work and should not be discarded merely to reduce diff size. Retained areas include:

- VMPEG FMA/FMV register model;
- shared interrupt generation and acknowledge behavior;
- SCC68070 DMA ingress model;
- MPEG-1 program-stream parsing;
- SCR/PTS/DTS extraction and clock scheduling;
- MPEG audio/video decoding through PL_MPEG;
- audio stream output;
- decoded-video queue and presentation scheduling;
- DVC-to-MCD212 external-video composition;
- PAL/NTSC DVC machine definitions;
- explicit compatibility labeling for uncertain registers and MPEG-RAM visibility.

These areas still need extraction into a smaller upstream series; retention here does not imply that every current implementation detail is hardware-proven.

## Code still to remove or refactor before the first DVC PR

### DVC save-state reconstruction — deferred removal/refactor

The production DVC source still contains a large save-state reconstruction mechanism. This was present from the initial DVC import and later became interwoven with MPEG parser and presentation fixes, so it cannot be safely removed by reverting a historical commit.

The current mechanism reserves fixed mirrors including approximately:

- 8 MiB audio elementary-stream replay;
- 32 MiB video elementary-stream replay;
- 1,048,576 PCM values;
- storage for up to 64 decoded 384x288 video frames;
- presentation, picture-event, and replay-pump mirrors.

It also reconstructs opaque PL_MPEG decoder state by replaying elementary-stream data after state load.

This is not a first-DVC-PR requirement because DVC machines do not advertise `MACHINE_SUPPORTS_SAVE`. The intended upstream extraction should remove the replay mirrors, pre/post-save reconstruction callbacks, replay journals, and save-only tests, while preserving ordinary reset/runtime state. That change must be performed as a source-level refactor rather than a whole-file historical rollback.

### Research telemetry in DVC hot paths

The DVC research source still contains instrumentation that should not remain active in normal builds, including:

- sequence/RAM-gate tracing enabled by default;
- per-audio-sample FNV-style output hashing;
- per-composited-pixel video hashing;
- bootstrap register counters;
- long-session A/V synchronization counters;
- shutdown telemetry summaries;
- title/regression-specific trace commentary.

These measurements were useful for validation. They should either be removed from the upstream extraction or gated behind disabled diagnostic logging so they impose no normal hot-path cost.

### PL_MPEG implementation leakage into guest-visible state

The current VMPEG `GEN_PICTURES_IN_FIFO` approximation examines PL_MPEG's internal `has_reference_frame` member. That couples guest-visible hardware behavior to a private implementation detail of the host decoder.

The upstream design should maintain VMPEG input/output-picture accounting explicitly in the DVC model. PL_MPEG should be treated as a decoder backend: feed elementary-stream bytes in and receive decoded frames/samples out. A future decoder replacement should not change VMPEG register semantics.

### SCC/DVC DMA ownership

The research DVC integration exposes several SCC DMA query/transfer helpers and has `cdi_state` schedule individual transfer service events. This proved the concept but leaves the system driver participating in DMA-controller state ownership.

Preferred direction:

1. define a generic SCC68070 peripheral DMA/DREQ interface;
2. migrate the existing CDIC consumer to it first;
3. add DVC as the second consumer;
4. keep counters, CA/COC status, address updates, transfer size, and completion ownership in the SCC device;
5. keep the peripheral responsible only for request/data semantics.

Do not reintroduce the research `dma_channel_external_start()` compatibility shortcut into the SCC prerequisite.

### E03018 MPEG-RAM visibility

The E03018 bit-0 MPEG-RAM gate remains an explicit **C** compatibility mechanism. Runtime validation shows that it provides a useful visibility boundary, but its physical hardware meaning is not established. Keep the behavior isolated and clearly labeled; do not rewrite comments to imply hardware certainty.

## Test strategy

The research suite accumulated very high assertion counts, especially through exhaustive pure-helper tests. Those remain useful for bit-field and parser invariants, but they do not replace integration coverage.

The next tests should prioritize cross-device behavior:

1. program SCC DMA channel 2, assert a DVC request, transfer real words, and verify address/count/CA/COC/IRQ transitions;
2. drive DVC interrupt state through shared CDIC/DVC IRQ4 arbitration and verify IACK vector ownership;
3. feed a minimal MPEG fixture through DVC decode and presentation, then verify external-video composition through MCD212;
4. cover PAL and NTSC DVC machine timing with the same integration path;
5. retain small parser/command helper tests where they protect documented boundary behavior.

A few high-value integration fixtures are more useful now than increasing the raw pure-helper assertion count.

## Other CD-i modernization tracks

The research branch also contains significant MCD212, CDIC, and SLAVE-HLE work. These should remain separate review tracks rather than prerequisites bundled into one DVC pull request.

### MCD212

Retained research includes QHY/Extended Case handling, PAL/NTSC active-window timing, ICA/DCA scheduling, field timing, register masks, and external-video eligibility. Much of this work has stronger primary-document grounding than the DVC compatibility models and can be submitted independently.

### CDIC HLE

The CDIC work improves command, sector, buffer, audio, and interrupt state handling, but authoritative IMS66490 register documentation remains unavailable. Compatibility timing, synthesized TOC behavior, error/status semantics, and exact audio control remain areas of uncertainty. Keep compatibility assumptions explicitly marked.

### SLAVE HLE

The SLAVE work replaces ad-hoc command parsing with bounded command/transport state and improves pointer behavior. It remains HLE; the existing SCC `/DTACK` limitation prevents treating the dumped SLAVE firmware as a complete live LLE oracle. Submit protocol/parser improvements separately from DVC.

## Proposed upstream sequence

### Series 1 — SCC68070 correctness

Small, independently justified SCC changes based on current upstream MAME and Philips documentation. Compile/test this series before using it as a dependency.

### Series 2 — baseline CD-i correctness

Submit MCD212, CDIC, and SLAVE improvements in separate PRs or narrowly coupled groups. Do not make all of them mandatory for the DVC review unless an actual dependency exists.

### Series 3 — DVC core

Add the DVC device, machine definitions, shared IRQ4 integration, register/clock model, MPEG-RAM mapping, and a generic SCC peripheral-DMA connection. Exclude Mono-II, SDL changes, global test-runner changes, research tracing, and save-state reconstruction.

### Series 4 — MPEG decode and presentation

Add the decoder backend, audio output, video scheduling, and MCD212 external-video composition. Keep guest-visible VMPEG state independent of PL_MPEG private fields.

### Later series

Only after the basic DVC implementation is upstream and stable:

- hardware-backed refinement of compatibility registers and E03018 behavior;
- save-state support, if DVC machines are explicitly marked as supporting saves;
- Mono-II support with its own hardware/runtime evidence;
- any generic pointer/input capability demonstrated to be useful outside a machine-name-specific SDL hack.

## Historical validation retained as research evidence

The frozen 2026-08-24 reference branch recorded extensive local validation, including focused SCC, MCD212, CDIC, SLAVE, DVC, PAL/NTSC, native, and sanitizer runs. Those results establish that the research implementation reached a useful functional state.

They do **not** certify the reconstructed SCC branch or the subtractive cleanup branch. Every reconstructed upstream candidate must be rebuilt and retested after extraction.

## Immediate next actions

1. Compile and run `mametests` for `cdi-upstream-scc68070-foundation`.
2. Reconstruct SCC changes into reviewer-sized commits if the build passes.
3. Continue subtractive cleanup of DVC research telemetry without reverting later functional MPEG/presentation fixes.
4. Refactor save-state reconstruction out of the first DVC candidate rather than historically rolling back `cdidvc.cpp`.
5. Introduce a generic SCC peripheral-DMA interface and prove it first with CDIC, then DVC.
6. Add SCC-to-DVC and IRQ4 integration fixtures.
7. Re-run PAL/NTSC runtime smoke tests after each extracted series.

The objective is no longer to add more CD-i behavior to the research branch. The objective is to preserve the validated behavior while reducing scope, coupling, unsupported features, instrumentation, and review risk until each upstream change has one defensible purpose.
