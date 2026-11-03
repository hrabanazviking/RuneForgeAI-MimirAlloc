# Allocation strategies for neural-computer workloads

These examples are design studies. They do not report completed MimirAlloc code, supported upstream interfaces or measured performance.

## Case study A: a bounded arithmetic workspace

A model-contained RV32 guest receives a request containing a bounded list of integers and computes a deterministic aggregate. The host validates the input size and serializes it through the actually available bridge. The guest has a pre-reserved workspace rather than requesting OS pages.

**Choice:** a monotonic arena reset after the request. **Contract:** all temporary values become invalid together at completion; no guest pointer may escape the scope. **Why:** minimal metadata and predictable ownership; individual free is unnecessary if all objects share a lifetime. **Risk:** one unexpectedly long-lived object or unbounded input exhausts the region. **Test:** size boundaries, alignment, allocation failure, reset/reuse and correctness of a second request after reset. **Metrics:** guest instructions and trace tokens, peak arena bytes, host memory, complete job time.

## Case study B: persistent session with changing objects

The guest maintains a small map across requests while allocating and freeing variable-lived records. A bump allocator can leak capacity across requests until session reset. Consider fixed-size pools for common record sizes, then a bounded coalescing allocator only for genuinely variable records.

**Ownership:** session owns arena; map owns records; snapshot must capture both map and allocator metadata. **Fragmentation:** record exact requested/live bytes, rounded slots, free-but-unusable capacity and largest free extent. **Correctness:** remove and reinsert records with seeded operations; after each phase compare with an independent reference map. **Failure:** on exhaustion return an explicit error and preserve existing objects. **Decision:** adopt the variable allocator only if it supports the workload and improves the measured space/time frontier sufficiently to justify complexity.

## Case study C: transformer-WASM history costs

Suppose an allocator free-list walk reads one metadata entry per candidate. On a normal processor, a walk has conventional instruction/cache costs; on a token-history machine the compiled sequence and state lookups can generate substantially different costs. Measure the actual execution trace for a fixed operation sequence and count program operations, model tokens and history growth. Compare a bitmap scan and segregated free list at equivalent arena capacity and alignment. A smaller instruction count is a hypothesis for improved performance, not proof: model kernel overhead and history operations also matter.

For specialized weights, count time and bytes required to construct the model. Compare against the universal interpreter for multiple repetitions. The break-even point, if any, depends on both compile cost and per-run savings.

## Case study D: host state pool for many guests

A host runtime may share immutable model weights among multiple sessions while assigning independent recurrent state and guest arenas to each. This is possible only if the runtime actually provides these ownership and concurrency guarantees. The pool tracks `reserved`, `active`, `pending completion`, and `reusable` state buffers; a buffer cannot be reused until all device operations finish and the state is reset to the correct initial contents. Check that a new guest cannot observe data from a prior guest. Measure model memory shared once, per-session state, padding and peak workspace. A pool cap limits concurrency rather than silently exhausting the machine.

## Case study E: neural worker in a distributed pipeline

A conventional CPU coordinator creates an immutable input object. An admitted neural worker receives an authorized bounded task, loads the data via a documented host bridge and executes a fixed guest program. It returns a versioned output object and evidence fields. A native reference verifies the result. If the worker times out, the coordinator queries the request ID before retrying; it does not assume the first execution stopped. On cancellation, reclaim waits for acknowledged guest/device completion. Record copies, serialization, time and power; this may be much slower than a local CPU and still be useful as a research exercise.

## Case study F: low-precision optimization study

Keep an unmodified high-precision baseline and known-answer guest trace. Change one operator/weight format at a time. Compare guest output, memory snapshots and trace sequence, including long-run tests. Distinguish numeric processor state from natural-language wording. If a quantization setting fails, retain the failure case; selectively preserving precision is a future design hypothesis. Do not distribute a quantized artifact labelled correct until it passes the defined target suite.

## Practical decision matrix

| Question | If yes | If no |
|---|---|---|
| Do all allocations share a lifetime? | Consider bounded arena reset | Study pools or variable-size reuse |
| Is there a real guest byte-I/O interface? | Use it with checked ranges | Use supported bridge or mark integration blocked |
| Can state be restored exactly? | Test safe-point snapshots | Treat sessions as non-migratable |
| Does the backend expose device completion? | Defer reuse to validated completion | Avoid asynchronous pool reuse |
| Is the model target independently verified? | Record configuration-specific evidence | Require reference comparison and label experimental |
| Is a network worker useful end to end? | Select by measured workload objective | Keep as a research-only worker |

## Sources

- [Qwenputer RV32 notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Transformer VM source and examples](https://github.com/Percepta-Core/transformer-vm)
- [CUDA stream-ordered allocation](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
