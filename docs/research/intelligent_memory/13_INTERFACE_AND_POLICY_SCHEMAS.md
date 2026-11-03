# Proposed interfaces, policy schemas and state transitions

These are design sketches, not implemented APIs, wire standards or production-ready schemas. Before implementation, write a formal behavior contract, choose ABI and serialization, and include unit, concurrency, failure and security tests.

## Resource descriptor

```json
{
  "resource_id": "example-cpu-memory-0",
  "backend": "cpu-local",
  "owner_domain": "example-tenant",
  "total_bytes": 0,
  "available_bytes": 0,
  "backing_identity": null,
  "access": ["host-read", "host-write"],
  "coherence": "host-local",
  "durability": "volatile",
  "health": "unknown",
  "measurement_epoch": 0
}
```

Zero and null are placeholders, not real device values. An actual catalog must define integer ranges, units, freshness, source, permissions and error states. A device with unknown available capacity is not automatically equivalent to zero or unlimited. `backing_identity` is optional because many APIs do not reveal physical sharing; if unknown, mark accounting uncertain. A remote host's availability is subject to reservation races, quotas and network partition.

## Managed-object request

```json
{
  "schema_version": 1,
  "request_id": "example-unique-id",
  "size_bytes": 4096,
  "alignment_bytes": 64,
  "consumers": ["host-cpu"],
  "required": {
    "mutability": "exclusive",
    "durability": "volatile",
    "security_domain": "example-tenant"
  },
  "preferences": {"locality": "cpu-local"},
  "mobility": "explicit-handle-only"
}
```

Return either a scoped handle and actual accepted properties or a specific failure. Requirements cannot be silently weakened. Alignment must be validated against backend capability and checked arithmetic; an object descriptor does not automatically yield a CPU pointer or exportable GPU handle. If a preferred resource is unavailable, fallback is allowed only if the caller accepts it.

## Object state machine

`NEW → RESERVED → READY → IN_USE → QUIESCING → RELEASED`, with explicit `FAILED`/`INVALIDATED` transitions. Multiple read-only views may coexist if supported, while an exclusive mutable view requires a handoff protocol. `release_view` must carry relevant device completion information when the backend uses asynchronous work. A reference count alone is insufficient when an in-flight kernel or transfer can still touch the object. Crash, timeout, reset and cancellation need defined transitions; state mutation must be atomic relative to the authority for that object.

## Advisor request

```json
{
  "policy_epoch": 42,
  "observation_window_ms": 10000,
  "observations": {
    "live_bytes": 0,
    "retained_bytes": 0,
    "pressure_some_avg10": null,
    "active_sessions": 0
  },
  "allowed_actions": ["no_change", "set_cache_cap"],
  "bounds": {"min_bytes": 0, "max_bytes": 1048576}
}
```

A deterministic guard independently obtains authoritative bounds; it does not trust values echoed by a model. Validate input/output length, schema, numerical ranges, finite values, target identity, authorization, epoch, expiry and rate. A model response may be informative without being applied. If the model or IPC fails, continue deterministic allocation and record an optional diagnostic.

## A KV cache identity record

Use runtime-owned compatibility keys rather than a guessed universal hash. The record can include engine version, model and adapter revision, exact prefix token identities, positional/masking configuration, representation dtype/layout, security scope and multimodal preprocessing identifiers. Each engine may require more attributes. Verify digest collisions and authorize scope appropriately; preserve privacy. A cross-runtime converter needs a separately verified layout/semantics contract and round-trip tests. A textual semantic match does not authorize KV reuse.

## Retrieval records

A source record needs immutable ID and version, source URI, content digest, timestamps, trust classification, access-control scope, embedding model/version and preprocessing/index version. Before returning a result, recheck caller permissions and deletion/revocation state. A retrieved document is evidence for the model to examine; it cannot amend hard policy. For small corpora use deterministic structured search as a baseline.

## Distributed messages

A request includes authenticated principal, idempotency key, authorized object reference, operation, bounded offset/length and deadline. Responses distinguish accepted, completed, committed and failed where those stages differ. The owner enforces generation and fencing for actual reads/writes; revocation must cover direct device or RDMA keys if such paths exist. An accepted request after a network timeout may have completed; never assume it failed or blindly duplicate non-idempotent actions.

## Tests before enabling a feature

- Every malformed field, arithmetic boundary, unsupported capability and stale schema version fails safely.
- Local/remote object scope, generation, permissions and quotas are checked under concurrency.
- Device completion, reset and cancellation cannot cause premature buffer reuse.
- Advisor timeout, invalid output, RAG absence and model crash preserve normal allocator behavior.
- KV cache keys reject mismatched model, prefix, tenant and runtime configurations.
- RAG updates, deletes and permissions invalidations propagate to every derivative cache and index according to the documented model.

## Sources

- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
- [CUDA stream-ordered allocation](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
- [vLLM prefix caching](https://docs.vllm.ai/en/latest/design/prefix_caching/)
- [Linux RDMA user verbs](https://docs.kernel.org/infiniband/user_verbs.html)
