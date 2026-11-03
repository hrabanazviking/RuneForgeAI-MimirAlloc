# Neural-computer roadmap for MimirAlloc

## Project vision

Give MimirAlloc a portable path to manage memory inside resource-limited virtual computers implemented through model computation, and research host-side management and distributed participation for these execution environments. This is an extension of the allocator research roadmap, not a replacement for proving the original Mojo/ARM blocker. All stages below are proposed and no capability is implemented merely by describing it.

## Stage 0 — Source and feasibility study

- Pin Qwenputer code/model artifacts and Percepta Transformer VM revision. Separate documented claims by version and execution path.
- Inventory guest memory map, compiler, ABI, execution bridge, state representation, host dependencies and license.
- Choose a small safe program and independent native/emulator reference. Estimate storage and actual host hardware requirements.
- **Exit:** reproducible plan with explicit unsupported operations, available RAM and numeric assumptions. If hardware or model is unavailable, record a blocker; do not claim a run.

## Stage 1 — Freestanding allocator interface

- Specify namespaced allocation from a caller-provided bounded region. Document alignment, zero-size, overflow, exhaustion and ownership.
- Implement and test first on a conventional host and an ordinary constrained guest emulator when implementation begins.
- Start with monotonic and fixed-pool designs, then compare an optional bounded variable-size allocator.
- **Exit:** property tests, exact linker/layout accounting and documented resource bounds; independent review.

## Stage 2 — Guest CPU and WASM targets

- Compile a minimal allocator test for a pinned RV32IM+Zba firmware environment where toolchain permits; fit alongside stack/static data in the guest's published RAM.
- Compile a separate compatible WASM test for the transformer VM's supported operation set; verify imports, lowering and memory model.
- Compare native/emulated, upstream reference/graph and free-running model execution separately.
- **Exit:** machine-readable operation traces, output and memory checks under named versions. If the full integrated model is not run, state that explicitly.

## Stage 3 — Host model-state management

- Inventory all host and accelerator allocations. Make an optional adapter for checkpoint/session/state ownership without changing third-party runtimes until interfaces are known.
- Test preallocation and bounded reuse for trace, state snapshots and host metadata, respecting device completion.
- **Exit:** correct reset, cancellation and restore with recorded host/GPU peaks and comparison to an unmodified baseline.

## Stage 4 — Project A.E.S.I.R. feasibility

- Identify model and tokenizer format, recurrent state operations, additional layers, precision, memory and required trace driver.
- Bring up a single reference step, then complete bounded programs. Compare each change to upstream behavior; keep baseline precision.
- Evaluate on named physical hardware. ARM, CUDA and integrated GPU support are separate claims.
- **Exit:** end-to-end free-running test suite with reference outputs and state checkpoints, or a clear unresolved blocker.

## Stage 5 — Logical-worker interface

- Design an opt-in host adapter and versioned capability descriptor. Implement bounded request/response through the real supported bridge.
- Add input validation, quotas, task IDs, timeout handling, deterministic serialization and ownership-safe guest-offset translation.
- **Exit:** local coordinator-to-worker test with fault injection and no assumption of native networking in the guest.

## Stage 6 — Distributed research prototype

- Introduce authenticated transport, capability-scoped resource IDs, durable/idempotent operation semantics where needed, generations and enforceable fencing.
- Compare native, conventional VM and model-contained worker on identical tasks. Use network fault injection and independently verify results.
- **Exit:** reproducible correctness, isolation, recovery, cost and latency results. Explicitly state what is shared, copied and persisted.

## Stage 7 — Optimizations driven by evidence

Possible topics: guest-aware allocation algorithms minimizing trace reads/writes, bounded recurrent-state checkpoints, differentiated precision for circuit-sensitive layers, sparse execution for engineered layers, and cost-aware workload placement. Each is a research hypothesis. Each must preserve relevant accuracy and resource bounds and beat a baseline on the chosen metric without concealing unacceptable regressions.

## Research questions and experiments

| Question | Discriminating experiment | Possible outcome |
|---|---|---|
| Can allocator code fit the actual guest? | link and run fixed arena; report full RAM breakdown | pass, fail or toolchain blocker |
| What metadata layout is cheapest for a model computer? | fixed-slot vs variable-list with same trace | latency/space frontier, not universal winner |
| Can A.E.S.I.R. execute an existing augmented checkpoint? | exact recurrent-state and trace comparison | support, partial support or missing kernel/format |
| Can state survive checkpoint/restore? | compare state and result before/after safe-point restore | supported on exact configuration or incompatibility |
| Is cross-node execution useful? | same task on neural worker, native CPU and emulator | quantitative costs or a specific research use |
| Does lower precision preserve CPU semantics? | individual precision changes and long free-running tests | qualified safe configuration or first failing case |
| Does pooled host state reduce peak memory? | fixed session count, identical workloads, device counters | measured savings, no gain or correctness failure |

## Prioritization

Prove the guest environment and correctness first. Host allocator optimization should proceed only where instrumentation shows it controls relevant allocations. Distributed compute is most useful after a single worker has clear boundaries and can be tested. The original MimirAlloc project remains a proposed independent allocator; external model code and licenses must remain clearly attributed.

## Sources

- [Qwenputer design](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Transformer VM](https://github.com/Percepta-Core/transformer-vm)
- [Android page-size guidance](https://developer.android.com/guide/practices/page-sizes)
- [RISC-V ISA tests](https://github.com/riscv-software-src/riscv-tests)
