# Engineering work packages and milestones

These work packages transform the synthesis into reviewable, independent tasks. They are proposed work; this repository does not acquire the features merely by listing them.

## WP1 — portable resource accounting

**Input:** documented process and backend allocation APIs. **Deliverable:** a schema for requested/live/retained/metadata/physical-accounting estimates with provider and timestamp. **Tests:** integer overflow, uninitialized counters, snapshots under concurrency, unavailable metrics and duplicate backing identity. **Gate:** a deterministic baseline that works without retrieval or a model. **Not included:** claims that raw host/GPU counters can always be summed.

## WP2 — allocator contracts

Specify namespaced allocation/free, alignment, zero-size, ownership and failure for one Linux ABI. Build a minimal bounded arena, then test sanitizer-compatible configurations, randomized operations, cross-thread free if supported, failure injection and real ARM. **Gate:** published reproducible logs and a documented unsupported-feature list. A separate freestanding guest target needs its own ABI and memory-map tests.

## WP3 — evidence catalog

Create versioned records for experiments and primary references. Include source, timestamp, target, claim kind, method, limitations and visibility. Begin with exact metadata lookup; add lexical or dense retrieval only when evaluation supports it. **Gate:** update, contradiction, deletion and permission tests, plus measured index cost. **Failure policy:** no evidence means an untested hypothesis, not a fabricated result.

## WP4 — shadow predictor

Collect sampled and consented lifetime or pressure features; train/compare a simple rule, classifier and optional tiny LLM. Split by run/version/device, account for censored allocations, and log hypothetical decisions only. **Gate:** measured net benefit opportunity with no mutations and documented resource overhead. **Fallback:** completely model-free operation.

## WP5 — guarded adaptive policy

Choose one reversible cache cap with hard limits and an explicit allowed action. Implement epoch validation, clamping, hysteresis, cooldown, rollback and independent authorization. Simulate bad model output, drift, OOM and rapid load changes. **Gate:** same safety invariants under arbitrary suggestions plus repeatable A/B benefit.

## WP6 — inference cache adapter

Select one inference runtime with a real supported interface; document its KV/state compatibility, asynchronous completion and cache ownership. Export managed accounting and an opt-in reclaim request only where the runtime permits. **Gate:** exact-prefix key mismatch tests, cross-tenant isolation, cancellation and output quality plus TTFT/decode/memory metrics. **Failure policy:** no compatible state means miss/recompute or explicit error.

## WP7 — retrieval adapter

Register index/source/candidate-buffer budgets for one retrieval engine; preserve document ACLs, provenance and deletion propagation. Compare structured, lexical and ANN retrieval. **Gate:** ground-truth recall, time-aware updates, privacy and index footprint. **Failure policy:** unavailable retrieval cannot override allocator correctness or invent evidence.

## WP8 — one heterogeneous backend

Define an opaque handle for one immutable CPU/device object. Implement supported allocation, import/copy, view release and completion. Test device reset and invalid handle. **Gate:** independently verified synchronization and physical backing accounting. **Failure policy:** incompatible device means explicit error or authorized copy.

## WP9 — distributed prototype

Implement a separate opt-in object service with scoped identity, quota, idempotency, generation, expiry, fencing and actual-operation checks. Add timeouts and crash recovery. **Gate:** failure-injection suite and data integrity before performance. **Failure policy:** network loss can prevent progress and never justifies inventing coherent memory.

## WP10 — long-horizon synthesis

Run a real application that combines a supported retrieval index, model runtime, limited cache and optional advisory policy. Compare fixed independent quotas, deterministic coordination and guarded learned coordination. **Gate:** report recall/quality, end-to-end latency, device/host peak, controller overhead, error and energy. Publish negative results and retain only genuinely useful complexity.

## Exit vocabulary

**Designed** = specified; **implemented** = code exists; **host-tested** = tests on a named host; **device-tested** = physical target logs; **integrated** = actual application uses the component; **measured** = reproducible baseline comparisons; **production-ready** requires additional documented operational and security evidence. A milestone cannot be promoted solely because a markdown document describes it.
