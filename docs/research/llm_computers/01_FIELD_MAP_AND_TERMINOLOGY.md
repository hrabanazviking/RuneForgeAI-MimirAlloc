# Language models containing computers: research map

**Research date:** 2026-11-02. **Project status:** research and proposed extensions; neither an allocator nor a neural-computer backend is implemented in MimirAlloc. Reported third-party results belong to their authors and have not been independently reproduced for this collection.

## Why this matters

The unusual target considered here is a programmable computer represented within a neural network's computation, including processor logic encoded in added weights and guest memory represented by model state. Such a machine can itself execute a small memory allocator. Meanwhile the host runtime must allocate the much larger tensors and state that make the guest exist. A future distributed system could coordinate jobs across these machines using explicit messages and resource handles. Each layer calls for a different memory contract.

This distinction opens genuinely interesting research: can a portable allocator run in a tiny RISC-V machine implemented by a transformer, can model state be managed as a durable resource, and can a neural-computer node collaborate with ordinary CPU, GPU and NPU nodes? Yes, these are meaningful experiments. Whether a particular instance is usable, accurate, efficient or beneficial has to be demonstrated.

## Five things that are easily confused

| System | Where computation happens | Memory and interface | Relation to this research |
|---|---|---|---|
| **Computer compiled into model computation** | Model forward passes implement guest operations | State represented by activations, recurrent state or attention history; guest ISA/VM | Primary focus |
| **Transformer-based programmable machine** | Analytically constructed transformer implements a VM | A program and state are encoded for that model; may not be a general language model | Closely related; Percepta Transformer VM |
| **LLM controlling an external VM** | Conventional CPU, emulator or sandbox executes code | Guest RAM provided by external runtime | Useful baseline and tool environment, not an in-model CPU |
| **LLM pretending to execute a computer in text** | Autoregressive language generation predicts an execution trace | Approximate textual description unless externally checked | Research comparator; not reliable instruction execution |
| **LLM agent using memory tools** | LLM proposes actions; storage is external | Tool calls, database, file or retrieval system | Useful orchestration and state service; not a neural CPU |

A physical GPU/CPU still executes the model and its host runtime. A CPU represented inside model layers is not an extra physical chip, and its guest memory is not free physical memory. Model weights, writable state and guest addressable RAM are distinct resources.

## Evidence: two concrete lines of work

### Qwenputer

The author describes augmenting Qwen3.5-4B with 11 analytically compiled layers implementing an RV32IM + Zba processor, using selected Gated DeltaNet recurrent states as writable machine memory. A driver loads firmware and coordinates execution; the creator reports selected compliance tests, reference comparisons and GPU-based execution. It is a modified model plus inference software, not a capability of every ordinary Qwen checkpoint. The published P2 design documents **32 KiB of guest RAM**, separate read-only regions, and specific ISA/environment limits. Its guest RAM is not the host RAM required to load the checkpoint. The author reports roughly 3,200 trace tokens/s on an NVIDIA L4 in a stated executor configuration; trace tokens are not necessarily guest instructions or language-output tokens. See the [upstream repository](https://github.com/divagr18/qwenputer) and [RV32 design and results](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md).

### Transformer VM

Percepta's [Transformer VM](https://github.com/Percepta-Core/transformer-vm) describes an analytically constructed transformer implementing a subset of WebAssembly operations, with compilation/lowering for additional operations. Its published implementation includes a graph evaluator, weight compiler, reference generation and optimized inference. The authors describe an attention-history indexing optimization for their constructed model. This does not mean every general LLM supports that optimization or that arbitrary WASM modules and operating systems run unchanged.

## Immediate implications for MimirAlloc

1. **Guest allocator:** target the guest ISA and its actual memory map, with a freestanding bounded-memory backend. A host `mmap`-based Linux allocator cannot automatically run inside a 32 KiB guest without a port.
2. **Host allocator:** manage host CPU/device buffers for weights, state, workspaces, token trace and checkpointing. This is a separate performance question.
3. **Logical device interface:** describe the machine as an experimental execution endpoint with capabilities, not as a new physical architecture. It needs an adapter and externally provided networking.
4. **Application use:** bounded allocation policies, snapshot safety and distributed resource protocols may benefit both conventional and neural machines. All cross-device movement needs explicit ownership and serialization.
5. **Evaluation:** compare native CPU, conventional emulator, pure-circuit evaluator and free-running neural execution. Verify exact outputs and resource costs before claiming an advantage.

## Research vocabulary

- **Model weights:** parameters defining computation; normally fixed during an inference session. Writing guest RAM does not necessarily modify the checkpoint.
- **Recurrent state:** mutable state updated through model steps; restoring it requires the right model, driver and state schema.
- **Attention/KV history:** cached values representing prior sequence positions in a compatible model and runtime; not universally interchangeable with recurrent RAM.
- **Guest address:** an integer interpreted under the guest's memory map, not a host pointer or network address.
- **Trace token:** a model token used to represent an execution step or record in a specific design. One instruction can require multiple tokens.
- **State snapshot:** explicit serialized execution state; persistence, atomicity, compatibility and privacy depend on how it is captured and restored.
- **Free-running:** the candidate produces its own subsequent execution outputs. Teacher forcing or reference-guided replay is a separate, narrower test.

## A note on exactness

A circuit can be mathematically designed for exact discrete operations and still fail under a particular finite-precision inference path, kernel, quantization, state update, software bug or input. Qwenputer's own design notes describe a floating-point residue issue with a multi-row read and subsequent design changes. “Exact” should always mean a named instruction subset, precision, runtime, state initialization and tested range. A language model's natural-language outputs remain probabilistic or decoding-dependent even when an embedded subcomponent is designed to be deterministic.

## Reading order

Continue with [Qwenputer](02_QWENPUTER_DEEP_DIVE.md), [Transformer VM](03_TRANSFORMER_VM_DEEP_DIVE.md), [guest allocator design](04_GUEST_ALLOCATOR_PORT.md), [host state management](05_HOST_STATE_AND_AEISIR.md), [distributed design](06_DISTRIBUTED_NEURAL_COMPUTE.md), [reproducibility and experiments](07_VERIFICATION_AND_BENCHMARKS.md), [security](08_SAFETY_AND_PROVENANCE.md), [roadmap](09_ROADMAP_AND_OPEN_QUESTIONS.md) and [source catalog](10_PRIMARY_SOURCES.md).
