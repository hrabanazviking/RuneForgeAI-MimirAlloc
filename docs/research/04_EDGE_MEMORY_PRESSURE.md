# Edge devices, low memory and resource pressure

## What makes an edge allocator different?

A small device may share memory between the operating system, graphics, neural processing, camera, browser and background services. Some allocations use system-managed pools or driver reservations outside a process heap. A large memory figure does not mean all RAM is available to a model; conversely a large reserved virtual range is not necessarily resident. Capture actual memory budgets, reservations, swap and application limits on the target.

The priority is not always maximum throughput: system responsiveness, stable frame times, thermal behavior, startup and avoiding memory-pressure stalls can matter more. Devices with little RAM also expose a tradeoff: aggressively reclaiming pages decreases idle footprint but can increase faults, CPU energy and latency on a burst.

## A practical memory budget

Estimate peak as **weights + activations + KV cache + temporary workspace + runtime and allocator overhead + driver/device allocations + application baseline + safety headroom**, adjusting for shared physical pages to avoid double-counting. Weights on disk, mapped size and resident bytes differ. Accelerator memory visible through a CPU mapping is not automatically an additional independent pool. Peak budget must include simultaneous model loading and old-model teardown, compilation buffers, context growth, prefill and multiple sessions.

For a transformer with standard full multi-head or grouped-query attention, an illustrative uncompressed KV estimate is `2 * layers * tokens * kv_heads * head_dimension * bytes_per_element * concurrent_sequences`. The factor 2 is keys and values. Actual architecture, sliding windows, paging, padding, block metadata, batching, cache quantization, shared prefixes and implementation change the result. This is a planning estimate, not an allocation guarantee. Quantized weights do not imply equally quantized activations or KV.

## Design options

1. Establish a compact fixed-policy baseline with strict upper bounds on local caches and deferred remote frees.
2. Prefer reuse within known lifetimes; release whole empty slabs and explicit arenas when no live users remain.
3. Consider separate profiles for idle, interactive and throughput use; switch only at safe boundaries and record policy.
4. Make memory budgets visible by category. Reserve enough headroom for non-allocator memory and the OS.
5. Expose an explicit application-controlled trim operation; it may request release but cannot guarantee a precise RSS reduction.
6. Add optional pressure integration outside allocation fast paths, with no requirement that Android permits reading each Linux metric.
7. If pressure persists, coordinate with the application to reduce batch, shorten context, unload optional models or reject work. The allocator cannot shrink live objects without cooperation.

## Signals and limits

Linux PSI measures time stalled by resource pressure, not a direct number of free bytes. cgroup v2 `memory.high` can trigger reclaim/throttling, whereas `memory.max` can lead to cgroup OOM if reclaim cannot satisfy the limit. Interfaces and permission vary. A memory controller should combine several signals with hysteresis; rising pressure can result from other processes. Android's process lifecycle and memory management differ from a generic Linux container, so require platform-specific validation.

## Device experiment: model lifecycle

- **Inputs:** exact model file hash, runtime revision, quantization, context, batch, thread count, device driver, power state and page size.
- **Phases:** baseline idle; load; prefill; decode; idle; unload; repeated reload; competing application.
- **Measurements:** peak and time-series RSS/PSS where available, allocator live/retained/metadata, GPU/NPU allocation where observable, pressure/stall, major/minor faults, time to first token, tokens/s, p95/p99 latency, temperature and system responsiveness.
- **Interventions:** system allocator baseline, namespaced prototype, fixed cache cap, then optional trim/controller; change one variable at a time.
- **Decision:** require correctness, no sustained retention growth over repeated cycles, and measured improvements with uncertainty. Record unsuccessful runs and causes.

Do not run artificial OOM tests on a device containing unsaved work. Use an isolated test environment and explicitly limited disposable workload; a process can still be killed even if limits are set.

## Sources

- [Linux PSI](https://docs.kernel.org/accounting/psi.html)
- [Linux cgroup v2 memory controller](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Android page-size guidance](https://developer.android.com/guide/practices/page-sizes)
- [ONNX Runtime execution providers](https://onnxruntime.ai/docs/execution-providers/)
