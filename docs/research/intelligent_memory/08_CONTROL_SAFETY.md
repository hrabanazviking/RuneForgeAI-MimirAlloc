# Control, correctness and safe autonomy

## A policy can be intelligent and bounded

Let optional models reason over telemetry, propose workload categories and choose among explicitly allowed strategies. Keep validity checks in code that can run even if the model, RAG index or network is unavailable. The purpose is to preserve the allocator's contract while allowing experimental policies to improve performance.

## Non-negotiable invariants

1. Every live allocation remains valid, aligned, owned and non-overlapping according to its API contract.
2. A model prediction cannot free or move a live non-movable object, authorize a user, override a quota or bypass device completion.
3. An active KV block is not reclaimed until the engine has confirmed it is safe or can be reconstructed under the agreed contract.
4. A RAG cache cannot bypass source-document access control or serve another tenant's content.
5. Cross-device buffers are reused only after all relevant operations and visibility requirements are satisfied.
6. A remote object has explicit generation, authority, consistency, retry and failure semantics.
7. A user can disable the advisor and recover a fully functional deterministic baseline.

## Control loop

**Observe:** bounded counters, state and health collected in preallocated or otherwise recursion-safe paths. **Summarize:** aggregate over a defined time/operation window; label missing and stale signals. **Propose:** a policy or tiny LLM produces a typed action with epoch and expiry. **Validate:** static range checks, hard invariants, authorization, compatibility and non-LLM policy. **Apply:** at a defined safe point and rate, using supported backend operations. **Measure:** compare outcome against defined limits. **Rollback:** restore prior safe setting on failure, timeout, resource pressure or drift.

Control loops can oscillate: aggressive release increases faults, a fast advisor restores caches, then pressure triggers release again. Use hysteresis, minimum dwell times, change limits and asynchronous decisions; evaluate step responses under repeatable load. Advisor confidence is not calibrated unless tested and cannot override hard constraints. A latency-sensitive allocation must never wait indefinitely for model inference.

## Resource self-interference

The advisor's weights, KV cache, retrieval index, worker, logs and intermediate buffers consume RAM and compute. Under critical pressure, unloading or pausing the advisor may be preferable. Reserve a small, bounded diagnostic path and define who owns it; avoid circular dependencies where the allocator needs a model whose runtime itself needs the same allocator to start or recover. A sidecar isolates some failures but still shares system resources.

## Prompt and data threats

A retrieved document, stack symbol, user-provided workload label or remote telemetry value may contain adversarial instructions. Treat all as data, use allowlisted output operations and deterministic checks, and prohibit arbitrary shell execution through policy output. Segregate private tenants; a hash is not a substitute for access control. Scrub credentials and payloads from diagnostic records before collection, and make remote inference opt-in according to the deployment's data policy.

## Rollout states

`disabled → observe → shadow → guarded canary → enabled` with a user-accessible rollback at each stage. State changes require an explicit authorization configuration, not a model-generated request. A canary may alter a bounded cache cap but should not alter object lifetimes or security rules. Keep a decision log: action, source, input version, guard result, applied setting and measured result.

## Validation matrix

- Invalid JSON, unknown actions, numeric overflow, NaN, stale epochs, contradictory requirements and missing telemetry.
- Advisor crash, long latency, exhausted context, unavailable RAG, expired model, bad config and output flood.
- Phase transition, variable temperature, memory pressure, high concurrency and device reset.
- Cross-tenant cache keys, stale ACL, poisoned retrieval and unauthorized source.
- Repeated pressure/recovery cycles for oscillation, work amplification and leaks.
- Baseline still functions when model and all optional services are disabled.

## Sources

- [Linux PSI](https://docs.kernel.org/accounting/psi.html)
- [Linux cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [HALP learned eviction](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/)
- [vLLM security guidance](https://docs.vllm.ai/en/latest/usage/security/)
