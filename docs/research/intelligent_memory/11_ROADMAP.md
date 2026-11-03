# Roadmap: from deterministic allocation to intelligent memory federation

The roadmap proposes independent experiments. It does not establish current support, performance, security or an implementation. The original Mojo/ARM failure investigation remains a prerequisite for any claim that MimirAlloc fixes that issue.

## Phase 0 — Evidence and constraints

Inventory the real target devices, OS and ABI; reproduce the motivating failure; record current allocator/runtime behavior, source licenses and accurate physical memory budgets. Define required semantics and acceptable tradeoffs. **Exit:** reproducible target matrix and baseline.

## Phase 1 — Correct allocator core

Implement a namespaced host allocator with checked arithmetic, explicit ownership, alignment, error behavior and bounded configurations. Compare system allocator. **Exit:** contract, concurrency, fault and native-device tests for named environments; advisor unavailable by default.

## Phase 2 — Instrumentation and baseline policy

Add privacy-aware bounded counters, sampled event IDs and deterministic caps. Make metrics recursion-safe; document overhead and missing data. **Exit:** repeatable real workloads and phase-change tests with no correctness regression.

## Phase 3 — Optional advisor in shadow mode

Evaluate static rules, small classifier, specialized predictor and small LLM on held-out workloads. Build a separate budgeted worker and typed, bounded policy proposals. Retrieval remains optional. **Exit:** recorded predictions, invalid-output handling, drift tests and full cost; no actual allocator action required.

## Phase 4 — Guarded adaptive policies

Permit one reversible setting such as cache retention under strict bounds, epoch validation, hysteresis and rollback. Preserve ownership and all hard limits. **Exit:** controlled A/B tests demonstrate net system benefit, stability and deterministic fallback on actual targets.

## Phase 5 — RAG integration

Build a versioned authorized research/experience index and evaluate whether it improves advisory decisions or human diagnostics. Separately define budgets and placement for application RAG indexes. **Exit:** recall and provenance evaluation, scoped ACL, invalidation, injection-resistant control boundary and a useful result versus simple lookup.

## Phase 6 — KV-cache cooperation

Choose one inference runtime with a documented integration point. Support explicit block handles and budgets; validate compatibility keys, safe reuse, device completion, eviction and recovery. **Exit:** end-to-end model output validation, measured TTFT/decode and memory, and no false cross-tenant cache hits.

## Phase 7 — Local memory federation

Offer an opt-in managed-object API across CPU and one supported accelerator or file-backed tier. Keep raw-pointer allocator semantics separate. **Exit:** correct explicit acquisition/release, device synchronization, failure and physical-capacity accounting with measured cost.

## Phase 8 — Remote and multi-device pools

Introduce authenticated object operations, quotas, generations, idempotency, defined consistency and failure recovery. Test with a conventional remote process first; add neural-computer workers only via a real host adapter. **Exit:** fault-injection and isolation tests plus real workload comparison with local and existing systems.

## Phase 9 — Experimental intelligence and future systems

Investigate predictive prefetch, lifetime grouping, compression decisions, dynamic tiering, capacity markets or a federated agent coordinator. These are optional hypotheses. Require measurable value and guardrails. Research CXL or device-to-device paths only where actual hardware, firmware, drivers and APIs support them.

## Prioritized decisions

| Decision | Start with | Move forward when |
|---|---|---|
| LLM vs small predictor | deterministic/no-model baseline | total resource-adjusted benefit exceeds cost |
| RAG vs structured lookup | versioned searchable facts | recall or decision quality justifies index/model overhead |
| local vs distributed | explicit local objects | application benefits after network/failure costs |
| offload vs recompute | measure both | engine can restore state and wins on target workload |
| shared pool vs private memory | shared immutable object | synchronization and permissions demonstrably correct |

## Cross-cutting release gates

For every target publish API contracts, supported environment, provenance, reproducible tests, failure modes, performance methodology and limitations. Distinguish research, prototype, host-tested, physical-device-tested and integration-proven. Ship a model-free configuration and deterministic fallback.

## Sources

- [LLAMA research](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [vLLM PagedAttention](https://arxiv.org/abs/2309.06180)
- [LMCache](https://github.com/LMCache/LMCache)
- [Linux CXL](https://cxl.docs.kernel.org/)
