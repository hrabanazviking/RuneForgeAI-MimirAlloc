# Learned allocation: lifetimes, placement and practical baselines

## Why object lifetime matters

Suppose many short-lived and a few long-lived objects share a slab. The short-lived objects are freed, but one surviving object can prevent the slab from being returned. If the application or a predictor can distinguish likely lifetimes, grouping similar objects may reduce page retention. Incorrect predictions can instead worsen fragmentation. An allocator must remain correct regardless of the prediction and must not silently relocate objects referenced by ordinary raw pointers.

The [LLAMA study](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) reports lifetime classification using a neural language model over symbolized calling contexts and a lifetime-aware allocator on particular server workloads. Its reported fragmentation reduction was workload-specific and involved huge pages; it does not establish that a chat model or the same policy benefits mobile devices.

## Prediction pipeline

1. Sample events with an application/site identifier, size bucket, timestamp or allocation epoch and eventual free. Use a bounded sampler and avoid raw data or sensitive stack symbols unless explicitly permitted.
2. Correlate allocation and free using safe internal IDs rather than dereferencing stale pointers. Thread/process exit and missing events need explicit handling.
3. Aggregate lifetime classes by site and workload phase. Expire profiles when binary, runtime, model or environment changes.
4. Train or evaluate simple heuristics first: default unknown, majority class, exponential moving average or bounded counters. Only then compare a learned model.
5. Map the result to a small number of preallocated or bounded arenas/size classes. The same allocation and free contracts apply whichever arena was chosen.
6. Periodically compare outcomes, with guardrails against unbounded arena proliferation or aggressive tuning.

Offline analysis must handle **right-censoring**: an allocation still alive when observation ends has a lifetime longer than the observed interval, not the exact duration observed. Missing frees, sampling bias and workload changes can mislead a predictor. Divide train/test by run, version, workload phase and device; randomly splitting near-identical events from the same run can leak information and overstate performance.

## Features, labels and privacy

Potential features include size class, normalized allocation site, thread role, previous lifetime statistics, workload phase, application-provided lifetime hints, current pressure and policy version. Avoid putting private prompt text, model contents, raw memory or network credentials into telemetry. Symbolized call stacks can reveal application structure, so treat their handling as configurable and privacy-sensitive.

Labels can be coarse lifetime buckets defined in wall time, allocation count or application phase. A threshold optimized for a server may not fit a mobile model-load cycle. Evaluate calibration and cost-weighted misclassification: assigning one long-lived object to an otherwise short-lived page may cost more than the opposite mistake.

## A policy that fails safely

- Unknown, low confidence or out-of-distribution contexts use a standard arena.
- Predictions are hints. Metadata and freeing remain fully correct regardless of bucket.
- Bound arenas and per-class caches by bytes, thread count and pressure policy.
- Never defer freeing live-owned or asynchronously used memory based on a prediction of future value.
- Tune class boundaries at safe points; existing pointers retain stable semantics.
- Allow opt-out, immutable configuration snapshots and automatic fallback if telemetry becomes stale.

## Beyond lifetime prediction

**Workload-aware cache retention:** learn a likely reuse score for *already free* memory, then retain or release within a hard cap. **Placement:** rank feasible tiers for *movable explicitly managed* objects; validate location and access requirements. **KV policy:** predict prefix reuse and compare recompute versus transfer. **RAG:** predict which index partitions to keep warm, without deleting the authoritative corpus. All depend on measurable workload benefit and independently verified safety.

## Shadow-mode evaluation

Record the advisor's hypothetical decision with policy version, input window, timestamp and baseline decision. Do not assume an offline trace can perfectly predict changed placement: changing a policy changes future cache hits, I/O and admission. Use discrete-event simulation for exploration, then actual controlled A/B tests. Report errors and limitations of counterfactual simulation, plus end-to-end performance and power on physical targets.

## Related sources

- [LLAMA lifetime allocation research](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [MiniMalloc: static accelerator memory planning](https://research.google/pubs/minimalloc-a-lightweight-memory-allocator-for-hardware-accelerated-machine-learning/)
- [HALP learned eviction](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/)
- [TCMalloc fleet characterization](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/)
