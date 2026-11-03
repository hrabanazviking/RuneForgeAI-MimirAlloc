# Tiny language models as optional memory-policy advisors

## Why a model might help

A small model might summarize workload phases, interpret a versioned configuration, find similar past performance incidents, or suggest policy changes using a bounded set of telemetry features. A sequence model may learn relationships between allocation contexts and future lifetimes. These are hypotheses or adaptations of existing research. The [LLAMA study](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) used a specialized learned language model for lifetime prediction; it did not demonstrate that a general-purpose chat LLM should execute synchronously on every `malloc`.

A tiny model is not free: it requires weights, runtime, context, scratch memory, CPU/GPU time and possibly another KV cache. On a constrained machine, that overhead can exceed the savings. Compare against no advisor, static rules, counters, heuristics, a small decision tree and a simple learned classifier. Select a tiny LLM only when its measured net utility justifies its full cost.

## Candidate policy tasks

| Task | Candidate input | Proposed output | Safety boundary |
|---|---|---|---|
| Phase recognition | sampled counters and feature windows | `interactive`, `loading`, `idle`, or unknown | Label affects only allowable preset selection |
| Lifetime hint | anonymized call-site/site history and size bucket | short/long/unknown class and confidence | Misprediction affects placement, never object validity |
| Cache cap suggestion | live/retained bytes, pressure and reuse history | value within a configured interval | deterministic clamp, rate limit and rollback |
| Managed-object tier selection | backend capabilities, size, mobility and locality | ranked feasible backends | hard requirements checked independently |
| KV budget recommendation | known context/session counts, recent demand | bounded per-session or global budget | runtime refuses invalid eviction or unsafe reuse |
| Incident explanation | retrieved versioned logs and measurements | human-readable summary and cited hypotheses | no direct mutation or secret disclosure |

Memory reclamation, free, pointer arithmetic, concurrent access, key revocation, page protection and authorization are deterministic mechanisms. A model cannot make an invalid free or a stale handle safe by predicting it is safe.

## Where the advisor runs

**Fast path:** deterministic allocation and local cache rules only. Record bounded, sampled telemetry without allocation recursion. **Slow path:** an optional sidecar or independently provisioned worker gathers windows and proposes changes at explicit epochs. **Validation:** a separate policy guard validates typed output, model/version identity, freshness, resource limits and applicable device contracts. **Application:** change only supported knobs at safe points, preserving current allocations and leases. **Fallback:** on timeout, missing data, invalid output, crash, OOM or user disable, use a known deterministic configuration.

Avoid having the advisor use the very allocator path it is controlling without a bootstrap/isolation plan: allocator initialization can recursively allocate and deadlock. Candidate approaches include a separately provisioned process with its own allocator and strict budget, preallocated ring buffers, and explicit IPC. A second process still competes for physical memory and can fail. Never require it for process startup or correct cleanup.

## Tiny model candidates — evaluation, not recommendations of performance

- [SmolLM2-135M-Instruct](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct), and other SmolLM2 sizes, are compact model candidates to test for schema adherence and operational advice. Check the exact model license and runtime.
- [Qwen3-0.6B](https://huggingface.co/Qwen/Qwen3-0.6B) is a candidate the A.E.S.I.R. ecosystem may already study; its actual memory and speed vary by quantization, context, hardware and runtime.
- A tiny specialized classifier or sequence model can be much smaller than either general language model. The correct baseline may not be generative at all.

Raw parameter storage is only a lower bound: approximately `parameter_count × bits_per_weight / 8`, plus quantization metadata, embeddings, runtime, context/KV, scratch, aligned allocations and possible duplicate copies. For illustration, 135 million parameters at an idealized 4 bits each are about 67.5 million bytes ≈ 64.4 MiB *for weights alone*. This is neither an available file-size promise nor the actual resident footprint. Measure cold and warm peaks, tokenization and resident shared pages.

## Typed proposals

```json
{
  "schema_version": 1,
  "telemetry_epoch": 42,
  "action": "set_cache_cap",
  "target": "cpu_small_object_pool",
  "value_bytes": 1048576,
  "reason_codes": ["sustained_pressure"],
  "expires_after_ms": 5000
}
```

This is a design example, not an implemented API or recommended cap. Validate allowed action and target against a schema and per-target capability; reject stale epochs, out-of-range values, unknown fields, excessive change rate and attempts to change hard ownership or tenant policy. Model-supplied confidence and explanations are not proofs. An advisor never sends arbitrary shell commands or privileged device configuration.

## Safe rollout

1. **Observation:** collect privacy-reviewed aggregate data and compute deterministic baseline decisions.
2. **Shadow mode:** run advisor without changing behavior; record predictions and latency.
3. **Offline replay:** evaluate against held-out traces and new devices, including phase changes.
4. **Guarded canary:** allow only one reversible setting within strict limits; use comparison cohort and automatic disable on regressions.
5. **Operational review:** measure reliability, policy drift, memory consumption, workload interference and rollback.

Predefine p95/p99 response time, available memory, advisor duty cycle, total energy and error budgets. A single allocator throughput win is not enough.

## Sources

- [Learning-based Memory Allocation for C++ Server Workloads](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [HALP learned cache eviction](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/)
- [Qwen3-0.6B model card](https://huggingface.co/Qwen/Qwen3-0.6B)
- [SmolLM2-135M-Instruct model card](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct)
