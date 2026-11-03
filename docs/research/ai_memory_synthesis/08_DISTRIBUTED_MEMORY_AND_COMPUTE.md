# Distributed memory, execution and failure

## One catalog does not imply one physical memory

A distributed system can name objects across machines and provide an explicit interface for copying, acquiring or releasing them. It cannot make a process pointer meaningful on another host without a supported mapping, and it must account for network partitions, latency, authorization and different consistency requirements. A model-contained virtual CPU is a logical worker hosted by a real runtime; guest offsets are not host pointers or network addresses.

## Proposed architecture

A coordinator receives bounded requests; a catalog describes authorized backends; a data plane transfers immutable blocks or documented mutable views; an object owner enforces versions, quotas and fencing; workers return typed outputs and evidence. The advisor, if used, may rank already-feasible placements but does not authorize operations or bypass the object owner's checks.

### Object lifecycle

1. Authenticate client and validate a request with size, scope, access, deadline and idempotency key.
2. Reserve capacity and assign an opaque ID, generation and rights.
3. Transfer or map through an actually supported backend, and verify completion and integrity.
4. Read or mutate under an explicit consistency mode.
5. Quiesce users, wait for device/network operations and revoke access before reclamation.
6. Persist or replicate only when the contract promises it; otherwise a provider failure may lose the data.

A timeout can mean an operation is still running or has completed. Retrying non-idempotent actions without reconciliation can duplicate allocations. A lease is not a substitute for fencing: the enforcing authority must reject stale epochs, and direct remote keys or in-flight device operations may require separate revocation. Checksums detect corruption; authentication and encryption address different threats.

## Candidate workloads

- **Immutable RAG sources or indexes:** distribute versioned blocks only when ACL, updates and data residency allow. Index portability depends on exact engine/version.
- **KV blocks:** use only engine-supported serialization with a complete compatible prefix identity and tenant scope; offload or recompute based on measured costs.
- **Large model weights:** share file-backed or remote objects when runtime and licensing allow, then account for replicas and local copies.
- **Experiment evidence:** share sanitized workload profiles and reproducible results; distinguish observations from advisor-generated claims.
- **Neural-computer worker:** host translates explicit input/output through a documented bridge; guest memory is not globally coherent.

## Experiments and failure injection

First run a local reference service and then two authorized isolated machines. Test duplicate requests, lost responses, partition, provider crash, restart, corrupted transfer, quota exhaustion, stale generation and cancellation with work in flight. Report data loss, recovery time, safety violations, throughput and p95/p99 latency. Compare the same work on a local CPU and ordinary object storage. A distributed design is valuable only for workloads whose benefits exceed transfer, complexity and failure costs.

## Sources

- [Linux RDMA user verbs](https://docs.kernel.org/infiniband/user_verbs.html)
- [Linux CXL documentation](https://cxl.docs.kernel.org/)
- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
- [Qwenputer](https://github.com/divagr18/qwenputer)
- [Transformer VM](https://github.com/Percepta-Core/transformer-vm)
