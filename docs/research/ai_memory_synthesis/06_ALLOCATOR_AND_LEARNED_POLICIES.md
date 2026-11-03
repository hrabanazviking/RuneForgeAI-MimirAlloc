# Allocators, learned predictions and safe policy

## What an allocator controls

A CPU allocator services calls by managing regions, size classes, free lists, metadata and reclamation within its target ABI. It must preserve alignment, ownership, overflow checks, concurrency and platform-specific failure behavior. A prediction can choose among safe placements; it cannot alter the meaning of a pointer or release a live allocation.

For accelerators, a generic heap often does not own device memory. The model runtime and GPU/NPU API may allocate separate tensors and buffers. An optional managed-buffer layer can cooperate through public backend interfaces, but cannot assume universal interception of driver allocation.

## Lifetime-aware research

[LLAMA](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) studied a learned language model over calling contexts to predict allocation lifetimes, combined with a lifetime-aware allocator on particular C++ server workloads. Grouping objects with similar lifetimes can reduce pages pinned by stragglers; an incorrect class can worsen fragmentation. The implementation must remain correct for every classification. The paper's results are not evidence that an arbitrary chat LLM or a mobile device will see the same benefit.

[MiniMalloc](https://research.google/pubs/minimalloc-a-lightweight-memory-allocator-for-hardware-accelerated-machine-learning/) addresses static memory planning for specialized ML accelerators. It is a useful contrast: a graph compiler may know tensor lifetimes in advance, while a general heap sees dynamic requests and partial information.

## Design choices

1. Gather bounded, privacy-reviewed size/site/lifetime statistics. Avoid logging payloads and be careful with sensitive call-stack data.
2. Begin with no prediction, explicit application hints and simple per-site counters. Compare a tiny classifier, sequence model and general small LLM only when there is a meaningful decision to predict.
3. Keep a default class for unseen or low-confidence contexts. Preserve the same correctness rules regardless of prediction.
4. Bound arena count and total retained bytes; otherwise lifetime grouping can create more half-empty pages.
5. Treat objects alive at the end of a trace as **right-censored** rather than assigning a false exact lifetime.
6. Separate training and test by process run, workload phase, version and physical target. A random split of closely related events can overstate generalization.

## Optional tiny-LLM advisor

A separate worker may summarize workload phases, review versioned experiment outcomes or propose a bounded cache cap at a slow cadence. It must not run synchronously in `malloc` or `free` as a requirement, or introduce recursive initialization. Its weights, runtime, context, retrieval index and device use count against the total memory and power budget. Use typed allowlisted outputs, policy epochs, hard clamps, rate limits and deterministic fallback. Test model-off mode as a first-class product configuration.

[HALP](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/) reports a learned-plus-heuristic cache policy in a content-delivery setting. It is evidence that learning can help a particular cache decision; it is not a proof of benefit for a heap or KV cache.

## Experiments

Compare baseline, application hints, simple heuristic, specialized small predictor and optional LLM on the same traces and real workloads. Report live/retained memory, fragmentation definition, p99 latency, throughput, controller overhead and energy. Evaluate phase changes, cache churn, missing telemetry and mispredictions. An observed policy win must remain a win after accounting for the controller and index; otherwise prefer the simpler policy for that target.

## Sources

- [LLAMA allocation](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [MiniMalloc](https://research.google/pubs/minimalloc-a-lightweight-memory-allocator-for-hardware-accelerated-machine-learning/)
- [HALP](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/)
- [TCMalloc characterization](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/)
