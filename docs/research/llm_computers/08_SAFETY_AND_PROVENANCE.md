# Reliability, isolation, security and research provenance

## Why these concerns are architectural

A model-contained CPU can receive program bytes, maintain mutable state and output results. A surrounding LLM may also generate commands or decide which computation to invoke. The deterministic guest, natural-language model, host runner and network service are different trust boundaries. A correct CPU circuit does not prove a generated program is appropriate, an external result is authentic, or a host tool call is authorized.

## Threat and failure inventory

| Boundary | Failure or attack | Required design response |
|---|---|---|
| Prompt → model | adversarial instructions in external content | treat output as untrusted input; validate structured requests against policy |
| Model → guest | invalid guest program, unbounded loop or malformed offsets | validate program format and enforce step, time, memory and output limits |
| Guest → host | guest address mistaken for host pointer | checked offset/length translation; no direct pointer dereference |
| Guest → remote service | stale handle, replay or unauthorized allocation | authentication, scope, generations, authorization and idempotency |
| Model computation | numeric divergence or corrupted state | pinned execution config, differential checks and fail-safe behavior |
| Checkpoint | inconsistent or malicious state | safe-point capture, integrity/authentication and version validation |
| Device backend | asynchronous use after free | completion-aware release and explicit error handling |
| Deployment | excessive model/trace size and resource exhaustion | quotas, capacity accounting and bounded admission |

A language model's own output should not be the sole check for a critical memory safety invariant. Validate output and address bounds with deterministic host code; use a reference or independent verifier where appropriate.

## Numerical integrity

“Compiled” or “deterministic” does not mean every floating-point implementation is identical. Convolution, recurrence, attention, precision, batch shape and quantization can alter results. The Qwenputer design notes describe a floating point residue in an earlier multi-row read and a subsequent design revision. Record the exact execution configuration, test first divergence over long traces, and include both success and failure. An isolated deterministic circuit may be reliable within its tested limits while the surrounding language interface can still issue the wrong request.

## State and confidentiality

Weights, recurrent state, guest memory, prompt history and traces may contain sensitive or copyrighted material. Define retention and sharing before a deployment; minimize telemetry payloads. Use authenticated encrypted transport when carrying sensitive data, protect snapshots and keys, and grant narrow access. A hash is an integrity checksum only under an appropriate threat model; use a MAC or signature when authenticity is required. Confidentiality is not supplied by an opaque handle alone.

Checkpointing must consider torn writes, snapshots taken mid-operation, model identity, state schema and cancellation. A live guest may hold references to pending operations; either quiesce all relevant operations or capture a consistent protocol for completing them. For distributed allocation, generation counters reject stale handles only if all access paths enforce them; resource keys or mappings may require their own revocation.

## Source provenance and licenses

- Record research source, author, URL, retrieved date, exact commit or artifact revision, license and whether implementation was inspected.
- Model license, compiler code license, firmware/libc license, datasets and runtime dependencies are separate. The Qwenputer model card identifies Apache-2.0 for that artifact at the time examined; check every pinned component and notices independently.
- The Transformer VM repository identifies Apache-2.0; preserve notices and check dependencies if code is incorporated. MimirAlloc's existing MIT license does not automatically replace third-party obligations.
- An independent implementation and a strict two-team clean-room process are distinct claims. If a researcher who inspected source implements the design, keep that provenance accurate and do not call it strict separation.
- Do not place model weights, private logs, proprietary binaries or very large traces in the public repo unless redistribution and privacy are verified.

## Operational controls

Use a limited-privilege runner, resource limits, bounded output, known model and firmware hashes, network restrictions appropriate to the task, and separate test data. If an external VM or sandbox is used, its isolation properties depend on its actual implementation and configuration; a virtual CPU in model weights is not automatically an OS security sandbox. For untrusted workloads, review guest-to-host bridges, runtime native-code dependencies and device drivers as part of the attack surface.

## Suggested acceptance gate

Before connecting a worker to a shared distributed environment, demonstrate capability validation; quota enforcement; bounds checks; duplicate-request safety; timeout behavior; fencing and stale-handle rejection; safe checkpoint restore; model/runtime identity checks; logging without secrets; and failure isolation. These controls do not prove absence of vulnerabilities, so document scope and retest after changes.

## Sources

- [Qwenputer RV32 implementation notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Qwenputer model card](https://huggingface.co/divagr1925/Qwen3.5-4B-Qwenputer)
- [Transformer VM repository](https://github.com/Percepta-Core/transformer-vm)
- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
