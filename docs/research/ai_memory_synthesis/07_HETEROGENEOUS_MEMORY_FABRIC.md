# A heterogeneous memory fabric: common interface, explicit semantics

## Research goal

A future MimirAlloc fabric could coordinate host memory, accelerator buffers, local storage and authorized remote objects under a shared vocabulary. It can improve visibility and simplify application management across supported backends. It cannot force hardware to export inaccessible memory or make all resources uniformly coherent and equally fast.

## What a resource descriptor must say

- Backend, owner/security scope, generation and health.
- Actual import/export capability, consumers, address/view types and alignment.
- Available and reserved capacity, measurement time and physical-backing identity if known.
- Locality, estimated transfer cost, durability, consistency and failure domain.
- Whether contents may be moved, reconstructed, discarded, shared or persisted.
- Required device fences, CPU cache operations, concurrency and teardown rules.

Unknown is a real value. A GPU-visible and CPU-visible range may share the same physical DRAM; adding the two reported capacities may double-count. Query the real API and reserve resources before promising them.

## Object views, not universal pointers

A managed request names required size, consumers, access, security and durability, plus preferences such as locality. The fabric returns an opaque handle and actual accepted placement or an explicit error. Acquiring a view yields a backend-specific pointer, GPU handle, mapped range or transfer object valid for a declared scope. Live C heap pointers remain governed by their existing allocation contract and are not migrated behind the application's back.

### Sharing classes

1. Immutable versioned objects, with authenticated scope and independently verified integrity.
2. Exclusive mutable objects, with a single owner and explicit handoff.
3. Copies with dirty tracking and synchronization specified by the application/backend.
4. Hardware-coherent sharing only when the actual devices, platform and API provide that guarantee.

For GPUs, [Vulkan](https://docs.vulkan.org/spec/latest/chapters/memory.html), [CUDA](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html) and [DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html) offer different mechanisms and constraints. A shared FD, pointer or physical DRAM alone is not proof of correct visibility or completion.

## Tiers and CXL

An SSD is not equivalent to RAM: it has different latency, durability and write behavior. Remote memory has network latency and partial failures. [Linux CXL documentation](https://cxl.docs.kernel.org/) covers supported CXL device and exposure models; multi-host sharing depends on specific devices, topology, firmware, OS and isolation. A type of capacity is useful only when a real application can access it through its contract.

## A first testable fabric

Use one host, a CPU buffer and one device buffer with documented import/copy support. Manage one immutable object, explicit acquire/release and completion. Compare against application-managed copies. Test failed imports, insufficient memory, device reset, cancellation and early release. Measure latency, data movement, peak physical memory, and synchronization. Do not build a distributed coordinator or learned policy before this path is correct.

## Integration with AI memory

KV and recurrent-state adapters must ask the model runtime whether a block is compatible, movable, in use or reconstructible. A RAG adapter manages document/index buffers and ACLs, not the truth of retrieved text. A semantic-agent adapter operates on versioned records and provenance. These may share accounting and object transport but must retain distinct correctness policies.

## Sources

- [Linux CXL documentation](https://cxl.docs.kernel.org/)
- [Vulkan memory specification](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [CUDA unified/system memory](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html)
- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
