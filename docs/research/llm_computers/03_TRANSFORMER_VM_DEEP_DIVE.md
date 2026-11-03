# Transformer VM: compiled WebAssembly execution

## The concept

[Percepta's Transformer VM](https://github.com/Percepta-Core/transformer-vm) describes a transformer with analytically constructed weights that implements a WebAssembly-like machine. The repository describes a computation graph, compiler/scheduler, execution engine, reference traces and C examples. Its machine state is represented through attention lookups and cumulative operations across the token history. This is a transformer-based programmable computer; the presence of transformer layers alone does not establish that it is a conversational LLM or a drop-in Qwen modification.

The distinction matters for MimirAlloc: one could compile allocator source to the supported WASM subset, run that program on this research machine, and check whether the state transitions match a reference. A successful WASM experiment would be useful evidence for that specific guest target, not proof that the allocator works on Qwenputer or standard WebAssembly runtimes.

## Universal versus specialized programs

The authors describe two modes:

- **Universal interpreter:** bytecode is supplied as part of the model input, and a constructed transformer executes the encoded operations.
- **Specialized model:** a particular program is compiled into weights, removing the need to fetch its full bytecode from the input in the same way.

A specialized allocator test might reduce some guest fetch costs, but it may require regenerating weights whenever the test program changes. Measure build time, artifact size, host memory, steps, startup and correctness. Do not infer that specializing one program creates a general-purpose guest.

## Supported behavior

The upstream README enumerates the operations implemented directly, including loads/stores, control flow, local/global variables, integer comparisons, addition/subtraction and output. It describes lowering some other operations such as multiply, divide, bitwise and shifts to sequences of supported operations. This is not full WASM specification compliance and does not imply arbitrary modules, WASI, threads, networking, operating-system services or unconstrained linear memory work. Some C operations or compiler output may be unsupported or expensive after lowering. Inspect the pinned version's tests, lowering implementation and maximum address/trace limits before preparing a guest.

## State and attention

In this implementation the model uses token history for machine-state lookup. The README describes an optimized convex-hull index for a specific low-dimensional hardmax-like attention construction. It reports logarithmic insert/query for that specialized lookup structure. It does not follow that arbitrary softmax attention on Qwen or another LLM is logarithmic; total transformer inference may have other costs and memory still grows with stored history or index. Benchmark attention cache, auxiliary index, trace storage and model buffers separately.

## Proposed allocator study

1. Start with a small static byte arena and a tiny fixed-slot allocator; avoid OS calls and unsupported C features.
2. Write operations as an explicit test vector: allocate sizes, write distinct bytes, free, reuse, validate alignment and detect exhaustion. Print structured results through the supported output mechanism.
3. Compile with the upstream documented WASM toolchain. Save compiler version, flags, exact program, artifact hash and lowering diagnostics.
4. Run a native/reference WASM interpreter where compatible, the upstream reference trace, the graph evaluator, and the actual transformer model. Keep them separate in the results.
5. Compare logical allocation outcomes, guest memory snapshots where available, execution errors, token trace and instruction or operation counts. Treat an interpreter match as necessary but not proof of all reachable programs.
6. Try a bounded coalescing allocator and then a MimirAlloc guest port when available. Compare codesize, trace length and state growth against the fixed-slot baseline.

## Toolchain and verification caveats

LLVM compilers can emit library calls, WASI imports, arithmetic helpers or opcodes outside a restricted subset. A successful compilation of C to WASM is not proof this transformer can execute it. Check module imports, memory limits, integer widths and linked support libraries. Internal floating-point execution is not automatically bit-exact for every optimization or kernel; define the author's expected arithmetic and validate all relevant paths. Reference evaluations can differ from the actual numeric transformer, so test both.

## MimirAlloc-specific research

- Map one guest byte to its actual representation in the program trace and model state. Compute host bytes per guest byte empirically rather than assuming a 1:1 ratio.
- Explore a compact bitmap or offset-based freelist; in a token/history machine, metadata access count and trace length may matter more than CPU-style nanoseconds per call.
- Treat finite trace and context growth as a bounded resource. Determine how state history is truncated, compacted or checkpointed without changing semantics; do not assume arbitrary KV eviction is safe.
- Evaluate program specialization of a fixed workload versus a general interface. Report specialized-model generation cost and lost flexibility.

## Primary sources

- [Transformer VM repository](https://github.com/Percepta-Core/transformer-vm)
- [Constructing an LLM-Computer](https://www.percepta.ai/blog/constructing-llm-computer)
- [Can LLMs Be Computers?](https://www.percepta.ai/blog/can-llms-be-computers)
- [WebAssembly core specification](https://www.w3.org/TR/wasm-core-2/)
