# Distributed and networked memory systems

## Reality check

A normal `malloc` pointer is meaningful in a process address space and depends on local page tables, lifetime and ABI. Sending the integer to another computer does not grant a valid mapping. A networked memory system instead needs an explicit protocol for naming, allocation, access, authorization, consistency, failure, reclamation and data transfer. It belongs in a separate service or opt-in client API; it must not silently replace C pointer semantics.

## A reference architecture — proposed

- **Client:** requests a typed buffer with capacity, access mode, durability, locality and timeout requirements.
- **Control plane:** authenticates clients, assigns quota, selects a provider and returns an opaque capability containing a buffer ID and generation.
- **Data plane:** transfers data using a supported transport, potentially TCP or a properly configured RDMA implementation.
- **Provider:** accounts capacity, validates each request, enforces access, persists or replicates only when promised, and recovers safely.
- **Observability:** reports allocation, transfer, latency, failure and lease status without exposing user payloads.

A capability is not necessarily safe just because it is hard to guess. It must have scope, expiry, authenticated transport, revocation and protection against replay according to the system's threat model. Generation numbers help reject stale handles after reuse; wraparound must be handled.

## Locality, transport and semantic tiers

| Tier | Interface | Typical benefit | What must be made explicit |
|---|---|---|---|
| Local in-process | pointers | low latency | ownership and memory model |
| Local interprocess | shared mapping/handles | avoids some copies | process permissions, crash recovery, mapping lifetime |
| Remote object store | get/put and object IDs | explicit storage separation | serialization, consistency, cost and retry |
| RDMA-capable path | registered region and remote operations | can reduce CPU work for suitable transfers | registration, remote keys, permissions, completion, network failures |
| Shared coherent fabric | platform-specific mappings | direct access where actually supported | topology, coherence domain, isolation and failure behavior |

RDMA is not a synonym for automatic cache coherence or safe distributed malloc. It requires a compatible fabric, registered buffers, correct access permissions and completion handling. A write acknowledged by a transport is not automatically durable or visible to every CPU without the applicable platform's guarantees.

## Buffer lifecycle and crash scenarios

1. Client requests a quota-limited reservation with authenticated identity and a deadline.
2. Service atomically reserves capacity and creates `(object_id, generation, permissions, lease_expiry)`.
3. Client transfers data with checksums and completes an explicit commit if the service provides one.
4. Service validates access and renewal. Each state transition is idempotent or has a transaction identifier.
5. Release marks the handle invalid; reuse waits for outstanding operations and appropriate fencing or key revocation.
6. On timeout, partition or client crash, leases alone cannot prove the client has stopped issuing operations; use fencing tokens/epochs enforced by the provider, and define behavior of already-issued operations.
7. Provider restart reconstructs state from durable metadata if persistence is promised; otherwise report loss and invalidate handles.

A retry of allocation can duplicate reservations unless the request carries an idempotency key. A timeout means the outcome may be unknown. Do not free a region while a remote DMA operation may still touch it. Reconcile network partition, split brain, key revocation and in-flight operation behavior in a threat model and test harness.

## Consistency choices

Immutable data with versioned handles is simpler than concurrent mutable shared state. For mutable data choose one writer, leases plus fencing, explicit locks/epochs, or a documented weaker consistency model. Define visibility after writes, failures, concurrent reads, compare-and-swap and host restarts. Specify persistence separately from replication: two copies in volatile RAM are not durable storage.

## Multi-machine local AI experiment

Start with explicit block transfers for read-only weight shards or versioned KV blocks. Measure bytes transferred per token, network saturation, latency distribution, memory saved, retry counts and power versus local-only and existing distributed inference frameworks. KV cache coherence, sequence routing and recomputation must be designed at the application layer; a remote allocator alone cannot make a model execute across machines.

## Acceptance gates

Use disposable test nodes; inject network loss, delay, duplicate messages, process death and provider restart. Validate quotas, isolation, stale handles, partial transfers and correctness before measuring speed. Report clear security boundaries and whether data is encrypted in transit and at rest. Do not expose an unauthenticated allocation service to the internet.

## Primary reading

- [Linux RDMA user verbs documentation](https://docs.kernel.org/infiniband/user_verbs.html)
- [Linux DMA-BUF documentation](https://docs.kernel.org/driver-api/dma-buf.html)
- [CXL specification](https://computeexpresslink.org/wp-content/uploads/2024/02/CXL-3.0-Specification.pdf)
- [Linux CXL documentation](https://cxl.docs.kernel.org/)
