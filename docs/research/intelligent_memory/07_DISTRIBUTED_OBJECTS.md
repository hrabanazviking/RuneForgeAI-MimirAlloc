# Distributed memory objects and cross-device contracts

## Unifying the control plane

A distributed memory system can expose a single logical catalog and common object handles across devices while each backend retains its own storage and transport semantics. This provides useful coordination without claiming that a pointer is valid on another machine or that a network has hardware cache coherence. The coordinator and adapters are proposed components, not existing MimirAlloc features.

## Handle and access model

An illustrative reference is `(namespace, object_id, generation, rights, owner_epoch)`. The object service must authenticate the caller, check scope and rights, validate length/offset with overflow-safe arithmetic and enforce the current generation/epoch at the operation point. An opaque ID or checksum is not sufficient authorization. A returned view also specifies location, data format, consistency, expiry and how completion is signaled. Do not log private payloads in telemetry by default.

### Lifecycle

1. Caller requests an object with size, alignment, mutable/immutable semantics, durability and allowed backends.
2. A provider reserves a quota and creates a unique generation; lack of capacity is an ordinary explicit failure.
3. Data transfers through a supported interface; validate version and bounds, and record completion.
4. The consumer acquires a view or copy with explicit access and synchronization.
5. Release waits for outstanding users, devices and transfers, then invalidates the capability and reclaims capacity.
6. A retry uses an idempotency key; the system records its result so a lost reply does not allocate twice.

A lease expiring does not ensure a stopped client or completed DMA. A fencing token works only where the authority that accepts writes checks it. Direct remote keys or imported mappings may need revocation or isolation in addition to application-level epochs. If an operation times out, its final state can be unknown; query or reconcile before retrying.

## Failure model

| Event | Required observable behavior |
|---|---|
| Worker or guest crashes | reject stale generation; decide restart from verified checkpoint or report loss |
| Provider dies | invalidate or recover according to documented durability and replication |
| Network partition | no silent split-brain writers; enforce authority/epoch |
| Duplicate allocation | return original idempotent result or reject conflicting request |
| GPU reset | fail or invalidate device handles and pending completion; do not reuse memory early |
| Disk full / read error | surface error; preserve prior committed state as specified |
| Unauthorized request | reject before granting view or data |
| Corrupted transfer | detect via integrity mechanism and retry or fail; authentication is separate |

A checksum detects accidental corruption; authenticate data when adversarial tampering is in scope. Encryption in transit and at rest, access policy, log retention and key management must follow the deployment's privacy requirements. A serialized KV cache can contain sensitive derived data and requires an appropriate tenant scope.

## Placement and cost

For a movable object, a *proposal* for expected access cost can include transfer time `bytes / observed_bandwidth`, fixed latency, queue delay, serialization, migration energy, temporary duplication, storage price and expected reuse. These are workload measurements, not universal constants. A local object may cost less despite a smaller pool; remote capacity can be useful for cold, durable or recomputable data. Requirements such as confidentiality, allowed region, device accessibility and correctness are hard filters, not simply weights in a cost score.

## Consistency modes

- **Immutable:** versioned content; multiple readers and explicit replication.
- **Exclusive mutable:** one writer with a lease and enforced fencing; handoff only after quiescence.
- **Application-managed consistency:** caller controls synchronization and accepts documented weak guarantees.
- **Shared coherent:** only if the actual platform and connected devices support the promised memory model and failure handling.

Start with immutable objects. General arbitrary shared mutable pointers across hosts are a separate distributed shared-memory research project with high complexity and failure costs.

## Neural-computer workers

A model-embedded virtual CPU is a logical endpoint hosted by a physical runtime. Its guest offsets refer to guest state, not a network mapping. A host adapter can translate allowed requests, enforce limits and transfer data through a documented bridge. Guest memory and host resource budgets are distinct; test exact model and guest execution behavior before use. See [neural-computer research](../llm_computers/README.md).

## Acceptance experiments

Use disposable local processes or test nodes; inject duplicate messages, delay, partition, client crash, provider restart, stale handles, interrupted checkpoints and device reset. Confirm idempotency, quota release, access control, integrity and failure semantics before measuring throughput. Compare explicit remote copies with local DRAM and conventional object-storage baselines; report both latency and cost.

## Sources

- [Linux RDMA user verbs](https://docs.kernel.org/infiniband/user_verbs.html)
- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
- [Linux CXL](https://cxl.docs.kernel.org/)
- [CUDA stream ordered allocation](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
