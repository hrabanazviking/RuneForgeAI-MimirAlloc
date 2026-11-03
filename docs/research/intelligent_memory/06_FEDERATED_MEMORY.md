# Federated heterogeneous memory: one system, explicit capabilities

## The vision

Let applications request a managed object by its needs—size, alignment, consumers, mobility, durability, access scope and latency—rather than manually managing every backing store. A resource manager discovers available devices and coordinates placement, sharing, copies and quotas. This can present a coherent *programming experience* across heterogeneous memory without pretending every resource supports the same operations.

**No universal capability is assumed.** An inaccessible NPU scratchpad, GPU-private allocation, protected page, third-party host or storage device cannot join a pool unless its owner and software expose a compatible interface. Local raw pointers remain valid only within their defined address spaces and lifetimes. Physical RAM, shared UMA allocations and multiple handles to the same pages must not be counted as independent capacity.

## Resource taxonomy

| Resource | Possible interface | Direct access and mobility constraints |
|---|---|---|
| CPU DRAM | process mapping, shared memory, OS | virtual addresses are process-specific; pinned/live memory limits movement |
| Integrated GPU sharing DRAM | Vulkan/CUDA/vendor API where supported | shared physical pages do not automatically mean shared pointers or cache coherence |
| Discrete GPU VRAM | device runtime / supported peer access | may require explicit transfer; properties vary with GPU and driver |
| NPU buffers/scratchpad | vendor runtime and driver | may be opaque, private, fixed-function or not exportable |
| SSD/NVMe | file/object/block interface | persistent-storage semantics, far higher latency and finite write endurance |
| CXL-attached capacity | OS-exposed system RAM or DAX where configured | actual topology, access, coherence and multi-host sharing vary |
| Remote host | authenticated message or supported remote-memory protocol | failures and network cost; no automatic pointer validity or coherence |
| Neural-computer guest memory | guest instructions/state via supported host bridge | guest offsets, tiny capacity and precision/state constraints |

Some CUDA platforms offer unified addressing or managed memory. Actual accessibility, coherence and migration depend on hardware and configuration; a shared virtual address does not by itself guarantee identical performance or supported atomics everywhere. Consult [NVIDIA's memory documentation](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html) and query real capabilities.

## Explicit object API — proposal

```text
request = {
  size, alignment, consumers, required_access, preferred_locality,
  mobility, mutability, consistency, durability, security_scope, budget
}
handle = allocate_managed(request)  // or explicit failure
view = acquire(handle, consumer, access_mode)  // backend-specific access
release_view(view, completion_token)
release_handle(handle)
```

This pseudocode is not implemented. Hard requirements must be satisfied or the request fails; preferences may be relaxed only if the caller permits it and actual placement is returned. A view can be a process pointer, device handle, mapped range or a transfer buffer. It is valid only for its defined consumer and lease. Raw C heap APIs and movable managed-object APIs should remain distinct.

## Catalog and placement

A resource catalog tracks owner, security domain, backend, reachable consumers, export/import support, total and currently allocatable bytes, estimated latency/bandwidth, NUMA topology, budget, health and failure domain. Treat estimates as staleable and probe carefully; memory availability can change concurrently. A placement policy filters by **hard constraints** before ranking candidates by cost. Reserve capacity atomically or fail; a model suggestion cannot create capacity or bypass permissions.

For shared physical pages, identify backing identities when the OS/backend exposes them. If identity is unknown, report an uncertainty range instead of summing all advertised capacities. Account for model weights, host runtime, GPU reservations, swap and system headroom. A remote device's capacity may be unsuitable for low-latency direct use even if its total is large.

## Coherence and movement contracts

- **Single-owner mutable:** simplest initial policy; transfer ownership at explicit synchronization points.
- **Immutable shared:** replicate content with versioned handles and an integrity check; revoke or supersede the version on update.
- **Managed copies:** require dirty-state tracking, producer completion, cache visibility, explicit invalidation and consumer-specific access.
- **Hardware-coherent mapping:** use only on verified compatible platforms and according to their exact APIs; do not extend its guarantees across unrelated networks.
- **Persistent object:** document flush/commit, recovery and replication; DRAM copies or a completed DMA operation do not inherently imply durability.

Migrate only movable objects while all conflicting views are quiesced. A C raw pointer to live heap memory generally cannot be invalidated without application cooperation. Resource handoff must wait for device fences or other backend completion, and cancellation/revocation must handle in-flight operations.

## From local to distributed

Start with one host and a few explicit managed-buffer backends. Add device sharing only through verified export/import interfaces. Add a local storage tier for objects whose application-defined contract allows serialization and latency. Later add an authenticated remote object service with leases, generations and explicit failures. CXL pooling can be explored on real compatible topology; it is not a generic substitute for network or cross-vendor support.

## Measurements and first prototype

Try a CPU memory buffer and a supported GPU buffer using one concrete runtime; define a movable immutable object and a bounded staging transfer. Compare application-level copies, latency and peak physical memory with manual management. Verify completion before reuse, errors on incompatible access and no double-counting. Only add a small-model advisor after the deterministic path is correct and a specific decision benefits from it.

## Sources

- [Linux CXL documentation](https://cxl.docs.kernel.org/)
- [Linux CXL device types](https://docs.kernel.org/driver-api/cxl/devices/device-types.html)
- [CUDA unified and system memory](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html)
- [Vulkan memory specification](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [Linux DMA-BUF and synchronization](https://docs.kernel.org/driver-api/dma-buf.html)
