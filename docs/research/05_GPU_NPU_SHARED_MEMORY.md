# GPU, NPU and shared-memory allocation

## Different allocators, different contracts

A CPU heap allocates host objects. Accelerator APIs allocate or import device buffers, bind resources, schedule asynchronous operations and synchronize producers/consumers. A CPU `malloc` replacement does not automatically manage VRAM, NPU scratchpads, device command queues or driver allocations. A useful MimirAlloc ecosystem could have a separate, opt-in buffer manager behind backend-specific adapters; this is a research proposal.

| Memory kind | Typical access | Main concern |
|---|---|---|
| Ordinary host allocation | CPU virtual address | Not automatically DMA-ready or device-accessible |
| Host-pinned/registered | Runtime permits device I/O | Scarce locked pages, registration overhead and teardown |
| Device-local allocation | GPU/device API | Capacity, lifetime and synchronization |
| Unified/managed memory | API-defined address/migration behavior | Coherence, placement, migration and page-fault costs vary |
| Imported/shared buffer | Valid handles accepted by participating drivers | Capability, compatibility, ownership, fences and cache maintenance |
| On-chip scratchpad / local memory | Accelerator-specific scope | Explicit size, bank layout and kernel lifetime |

Unified physical memory on an integrated device does not guarantee a single pointer usable by all devices, zero-copy execution, cache coherence or no memory reservation. Probe capabilities and follow each driver and API's requirements.

## Proposed buffer descriptor

Represent `backend`, `device_id`, logical size, allocation size, alignment, handle type, host visibility, access flags, cache/coherence contract, ownership, generation, lifetime and release callback. Separate CPU pointer from device address or opaque handle. A request should express required properties separately from preferences; report unsupported combinations instead of silently substituting a different safety contract.

## Timeline and safe reuse

1. Create or import a buffer according to backend API.
2. Populate data with required mapping and cache operations.
3. Submit work, recording the completion token/fence/stream relationship.
4. Allow the next consumer access only after the required dependency and visibility operation.
5. Defer pool reuse or release until every relevant operation has completed.
6. On reset or error, transition buffers to an explicit invalid state; do not simply recycle them because a host handle was dropped.

Host-reference count reaching zero does not prove GPU or NPU work is finished. Cross-queue dependencies must be explicit. CUDA's stream-ordered allocator establishes particular stream ordering semantics; they cannot be generalized to all accelerators. Vulkan memory types and heaps require device queries; host-visible memory may still require flushing or invalidating non-coherent mapped ranges. Linux DMA-BUF uses exporter/importer handles and fences under appropriate driver contracts; sharing an FD alone does not prove all cache and lifetime obligations are satisfied.

## Pooling and suballocation

Where an API and device support it, acquire larger device blocks and suballocate aligned ranges to reduce per-allocation overhead. Respect API granularity, alignment, dedicated-allocation rules and maximum-allocation limits; keep a per-block map to detect overlaps. Record live, pending-free and reusable bytes separately. Defer reuse until completion. Pool caps must cover device and host accounting without double-counting shared UMA pages. Large pinned allocations can degrade the rest of the system; budget and release them.

## NPU-specific issues

There is no universal NPU allocation ABI. Model execution providers may use opaque memory and allocate workspaces internally. Query the actual platform's runtime: supported import/export handles, tensor strides/layout, quantization, physical alignment, cache maintenance, synchronization, device reset and security. If the runtime does not expose memory control, measure its observable footprint instead of pretending a general allocator can control it. Separate model weights, activations, scratch and temporary conversion buffers.

## Edge experiment

Compare direct backend allocation to a bounded suballocation pool for repeated tensor shapes. Measure allocation calls, end-to-end inference, peak physical memory, device queue wait, host CPU, bytes copied, thermal behavior and correctness checksum. Test changing tensor shapes, device errors, multiple queues and teardown with work in flight. Demonstrate reduced copying with tracing rather than labeling a path zero-copy from API names.

## Sources

- [Vulkan memory allocation specification](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [Vulkan embedded memory considerations](https://docs.vulkan.org/guide/latest/embedded_programming.html)
- [CUDA stream-ordered allocation](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
- [Linux DMA-BUF sharing and synchronization](https://docs.kernel.org/driver-api/dma-buf.html)
- [ONNX Runtime execution providers](https://onnxruntime.ai/docs/execution-providers/)
