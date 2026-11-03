# Integrated research roadmap

The roadmap separates ideas from claims. It is deliberately possible to stop after any stage and retain useful results. No capability here is established until its named implementation and tests exist.

## Stage 0 — source and target inventory

Record research papers and artifact versions, licenses, original MimirAlloc requirements, actual Mojo/ARM failure, target device capabilities and physical budgets. **Exit:** reproducible blocker report and evidence ledger, with unknowns retained.

## Stage 1 — deterministic allocator and accounting

Create a namespaced allocator or bounded arena with tested contracts, alignment, overflow/failure, ownership and concurrency. Add low-overhead bounded statistics. **Exit:** contract tests on named physical devices and meaningful baseline comparisons.

## Stage 2 — workload and evidence store

Capture privacy-reviewed experiment records with exact versions, time, methods, outcomes and access scope. Start with simple structured search. **Exit:** stale, contradictory, missing and unauthorized records behave as documented.

## Stage 3 — optional advisor in shadow mode

Compare deterministic rules, site counters, a small predictor and a budgeted small LLM. Collect hypothetical proposals and validate them without modifying the allocator. **Exit:** total cost and prediction quality measured on held-out traces, with advisor-off operation.

## Stage 4 — one guarded adaptive knob

Allow a bounded cache-retention setting only after a deterministic guard validates current state and safe change. Use rate limits, hysteresis and rollback. **Exit:** correct behavior under wrong predictions and pressure, and a measured net improvement in named workloads.

## Stage 5 — one model-state adapter

Choose a runtime with a documented integration API. Coordinate exact-compatible KV or state objects with safe reference/completion and scoped budgets. **Exit:** output/state verification, memory and latency evidence, and an explicit incompatibility path.

## Stage 6 — retrieval experiments

Test advisor RAG against structured/lexical baselines on a versioned authorized corpus. Separately budget the RAG engine's own indexes and buffers. **Exit:** better grounded decisions or documented negative result, with recall and access-control tests.

## Stage 7 — heterogeneous managed objects

Add an explicit CPU/device object abstraction for a single supported backend pair. Preserve raw-pointer behavior; implement completion-aware transfer and physical-capacity accounting. **Exit:** failure injection and performance versus direct application-managed buffers.

## Stage 8 — distributed and neural-computer research

Introduce remote objects with idempotency, generation, authentication, consistency and authoritative fencing. Add a neural-computer endpoint only via a verified host bridge and separately account guest and host resources. **Exit:** tested partitions, crash recovery, stale handles and comparative cost.

## Stage 9 — long-range integration

Research joint budgets across reclaimable KV and retrieval indexes, state-aware placement, latency/cost forecasts and model-assisted diagnostics. Any policy change must preserve domain contracts. Evaluate energy and whole-application outcomes, not just allocated bytes or benchmark scores.

## Decision discipline

Each research ticket states: hypothesis, cited evidence, target, implementation boundary, baseline, allowed changes, safety constraints, measurements, exit gate and negative-result path. If a tiny LLM loses to deterministic rules, record it as a useful conclusion. If a device has no supported memory export, report the limitation and try an explicit copy or decline the feature.

## Source anchors

- [LLAMA](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [PagedAttention](https://arxiv.org/abs/2309.06180)
- [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html)
- [Linux CXL](https://cxl.docs.kernel.org/)
