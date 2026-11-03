# Proposed neural-computer worker protocol

This is an exploratory protocol sketch for design review, not a published wire specification or implemented MimirAlloc API. A future implementer must choose a serialization format, threat model, authentication mechanism and precise integer bounds. A computer represented in model computation participates through a host adapter; it does not receive networking simply by possessing a guest CPU.

## Entities and trust boundaries

- **Client:** requests an operation with a project identity and a bounded resource budget.
- **Coordinator:** validates requirements, selects an admitted worker and holds authoritative task state.
- **Worker host:** authenticates requests, validates inputs, manages the model process and serializes guest access.
- **Guest:** runs the allocator test or other firmware within its defined virtual-machine memory map.
- **Object service:** optional independent component serving versioned immutable input/output blocks.

The coordinator cannot assume that a worker's claim of correct execution is independently verified. Results should include enough evidence for an appropriate independent check; a result hash is useful for integrity comparison but does not prove the computation occurred correctly.

## Capability advertisement: illustrative JSON

```json
{
  "protocol_version": 1,
  "worker_id": "example-only",
  "execution_kind": "neural-rv32",
  "target": {
    "isa": "rv32im_zba",
    "guest_endianness": "little",
    "runtime_revision": "record-immutable-revision",
    "model_revision": "record-immutable-revision",
    "firmware_digest": "sha256:record-actual-digest"
  },
  "limits": {
    "guest_arena_bytes": 0,
    "max_input_bytes": 0,
    "max_output_bytes": 0,
    "max_steps": 0,
    "max_concurrent_guests": 1
  },
  "features": {
    "checkpoint": false,
    "guest_byte_io": false,
    "network_in_guest": false
  },
  "evidence": "not-tested"
}
```

Zero is a placeholder, not a claim about Qwenputer. An actual registration rejects absent required digests, impossible values, unsupported versions and untrusted identities. Model-advertised features are confirmed by tests and administrator policy. Guest RAM capacity, currently free arena, host resident usage, device memory and time/step limits are separate values. Do not advertise unsupported guest I/O based on a hypothetical design.

## Illustrative request and response

```json
{
  "request_id": "unique-client-generated-id",
  "operation": "run_guest_test",
  "program_digest": "sha256:record-actual-digest",
  "input_object": {"id": "opaque-object-id", "generation": 1},
  "required_execution": "neural-rv32",
  "budget": {"steps": 10000, "wall_ms": 30000, "output_bytes": 2048},
  "verification": "reference-compare"
}
```

```json
{
  "request_id": "unique-client-generated-id",
  "status": "ok-or-explicit-error",
  "output_object": {"id": "opaque-object-id", "generation": 1},
  "guest_exit": 0,
  "model_revision": "record-immutable-revision",
  "trace_tokens": 0,
  "guest_instructions": null,
  "comparison": "not-run"
}
```

These examples illustrate fields only and are not ready for network deployment. IDs must be scoped, bounded and authorized; JSON numeric handling must specify exact integer limits and canonical digest calculation. `guest_instructions` may be unavailable; unknown is not zero. Timeouts need monotonic deadlines and clock-handling policy, and a client must not assume a timeout means the operation did not execute.

## State transition and retry design

`NEW → ADMITTED → RUNNING → COMPLETED | FAILED | CANCELLED` is a simplified view. Persist request ID and an input digest before mutating a durable object. A repeated identical request returns the existing record or a defined in-progress response; the same ID with different content is rejected. An operation that times out may be running or complete. Cancellation requires a named checkpoint and acknowledgement and may be best effort; return the actual state. Crash recovery must define whether jobs resume from a verified snapshot or fail and restart with a new epoch.

## Memory capability design

An optional object handle should include an opaque identifier, generation, owner/scope, permitted actions, maximum length, expiry and authority-issued fencing epoch. The owner validates every request and enforces the epoch *at the actual operation point*. If a token is signed, its encoding, key rotation, audience and replay prevention require design. If a transport grants a direct access key, revocation and in-flight operations require transport-specific mechanisms; an application-level generation counter alone cannot stop an already authorized direct write.

Reading or writing guest memory, if the real adapter supports it, follows `validate guest identity → validate offset/length without overflow → check allowed region → copy through supported interface → wait for completion → verify status`. Do not expose arbitrary register/state mutations to untrusted clients. Do not infer a safe API from a conceptual memory map.

## Consistency choices

Prefer immutable inputs and output objects for a first prototype. For mutable remote state choose a single writer and an explicitly versioned sequence, with atomic commit and recovery rules. Stronger shared-mutable semantics require an agreed consistency protocol and additional infrastructure. Network transport completion, service acceptance and persistence are distinct acknowledgements. A direct guest pointer or its numeric value is never a cross-host capability.

## Negative-path conformance

Test unauthorized identity, oversized lengths, integer overflow, unknown guest version, wrong model digest, unsupported instruction or operation, duplicate request, mismatched idempotency content, stale epoch, corrupt input, guest fault, timeout, partial output, host crash and late response. For each, define response, cleanup and whether retry is safe. Only then evaluate performance.

## Sources

- [Qwenputer design and interface](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Transformer VM](https://github.com/Percepta-Core/transformer-vm)
- [Linux RDMA userspace verbs](https://docs.kernel.org/infiniband/user_verbs.html)
