# Host memory, model state and Project A.E.S.I.R.

## Two allocators, two budgets

The **guest allocator** divides bytes in the virtual computer's address space. The **host system** allocates actual model weights, activation buffers, recurrent state, attention cache, token logs, device memory and snapshots. They are separate address spaces and separate performance questions. A 32 KiB guest can require gigabytes of host model storage. Free guest bytes do not automatically release host model tensors; freeing host state while a guest is running destroys or invalidates the guest.

## Model-state inventory

| Resource | Typical owner | Lifetime / concern |
|---|---|---|
| Base and added-layer weights | inference runtime / backend | checkpoint format, precision and device placement |
| Recurrent memory and guest registers | model execution state | snapshot consistency, device residency and versioning |
| KV/attention history | inference runtime | grows with trace for relevant layers and implementations |
| Hidden activations / workspace | backend | transient peaks, kernel choice and batching |
| Input/output and trace buffers | host runner | log growth, serialization and data privacy |
| Guest heap/stack | virtual CPU's state | shared fixed guest budget and guest allocator rules |
| Checkpoints | host runner/storage | write amplification, integrity and restore compatibility |
| Transport buffers | host adapter | network budget, copies, in-flight and cancellation |

The physical bytes for recurrent state may include representation overhead, dtype, padding, device copies and framework internals. Measure device-reported and host-reported usage, being careful not to add duplicated views of the same unified physical pages.

## A.E.S.I.R. compatibility investigation

A.E.S.I.R. would need more than a model file to execute a particular neural computer. Inspect, without assuming implementation support:

1. Exact checkpoint architecture and tensor names/shapes, including added computer layers and altered vocabulary/tokenizer.
2. Gated DeltaNet or other recurrent update formula, state layout, reset behavior, and required numerical precision.
3. Correct ordering and selection of computation for language and machine trace tokens, including any specialized executor or driver behavior.
4. Guest firmware load, initialization protocol, machine trace format, halt/fault semantics and output bridge.
5. Weight loader and quantization. Preserve circuit-sensitive weights and state precision initially; quantization needs bit-exact validation rather than a general Q4 performance assumption.
6. Device backend availability (CPU, CUDA, ROCm/Vulkan or other), compatible kernels, workspace sizes and memory residency.
7. Full differential tests against the upstream reference and actual free-running model, not merely successful text generation.

A model named “Qwen” is not sufficient evidence that its architecture or execution driver matches an existing Qwen loader. A.E.S.I.R. compatibility is a proposal until implemented and tested.

## Host allocator opportunities

- Reuse bounded CPU-side trace chunks, serialization buffers and metadata for repeated guest calls.
- Represent execution sessions as lifetime scopes, with clear ownership and explicit release on cancellation.
- Offer high-water accounting by `weights`, `state`, `KV`, `temporary`, `trace`, `network`, `snapshot` and allocator metadata where APIs expose it.
- Optimize state snapshots with copy-on-write or page sharing only if the runtime, backend and concurrency rules actually support them. Read-only weights can potentially be shared across sessions; mutable state must be isolated unless sharing is explicitly designed.
- Use optional device-buffer pools with completion-aware reuse. A host allocation hook will not automatically intercept GPU, NPU or framework allocators.
- Drop or compress logs only when doing so does not invalidate the machine's required attention/state history, audit policy or replay requirements.

## Preserving exact behavior

A recommended first comparison keeps upstream precision and backend settings. Introduce one change at a time: weight format, quantization, kernel, fused operator, batching or device. For each, compare guest outputs *and internal state/trace at checkpoints* against the reference. A model that answers ordinary prompts correctly may still corrupt an embedded CPU state after thousands of steps. Fail closed for tasks requiring exactness when integrity checks fail, then record the state and configuration.

## Device-specific planning

- **ARM64 tablet:** distinguish native Android, Termux and proot; verify available memory, supported runtime, 4/16 KiB pages and accelerator APIs. No hardware model support is assumed.
- **Raspberry Pi:** inspect actual 64-bit OS, RAM and kernel; a CPU-only experiment may be slow, and an NPU may not support needed dynamic recurrent operations.
- **Discrete NVIDIA GPU:** upstream's published optimized path may require specific CUDA/Triton versions and precision; validate on the actual model and driver.
- **Integrated AMD GPU / shared memory:** shared physical RAM does not guarantee the upstream kernels or correct zero-copy behavior. Determine runtime and backend support experimentally and include host-device contention in measurements.
- **Cloud:** a larger GPU may enable faster reference runs but must be compared on cost, privacy and reproducibility, not just throughput.

## Acceptance gate

A.E.S.I.R. may claim a neural-computer target only after it loads the pinned model, initializes and resumes state, executes an agreed test suite free-running, matches reference outputs and selected state checkpoints, handles errors, and reports memory and performance on a named environment. A working GGUF loader or generic transformer inference alone is not sufficient.

## Sources

- [Qwenputer README](https://github.com/divagr18/qwenputer)
- [Qwenputer RV32 design](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Transformer VM](https://github.com/Percepta-Core/transformer-vm)
- [Vulkan memory specification](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [CUDA stream-ordered allocation](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
