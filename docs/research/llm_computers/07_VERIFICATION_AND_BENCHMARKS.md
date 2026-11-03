# Verification, experiments and benchmarking protocol

## Separate the execution paths

A report must identify the actual route used. These paths answer different questions:

| Route | What it helps check | What it does not prove |
|---|---|---|
| Native test program | allocator contract on a host CPU | guest instruction correctness |
| Conventional RISC-V/WASM emulator | guest binary and reference semantics | transformer execution |
| Upstream symbolic/IR graph evaluator | circuit design against a model | finite-precision model inference |
| Teacher-forced model replay | values under supplied expected history | free-running correctness |
| Standalone compiled neural core | actual numerical circuit path | integrated language-model bridge |
| Full free-running augmented LLM | end-to-end specific model and driver | portability to other kernels, hardware or weights |

Do not mix these results under a single “passes” label. Pin revisions and identify any reference assumptions, and preserve scripts and generated artifacts where licensing permits.

## Baselines

For the same bounded allocation trace compare: a simple fixed arena on native CPU, a conventional emulator or WASM runtime, a symbolic reference, a free-running transformer VM and, when possible, Qwenputer's actual merged-model path. A generic prompted LLM can be an exploratory comparison, but it is not a trusted correctness oracle. Use identical requested operations and output format, adjusting only for platform-specific permitted semantics.

## Experiment 0: inventory and reproducibility

Record upstream git SHA, model checkpoint ID and immutable revision/hash, tokenizer revision, guest firmware/build hash, compiler/ABI/linker flags, inference driver, dtype, hardware, OS, versions, page size and physical RAM. Record memory maps, all dependencies and model licenses. Hardware listed on a model card does not prove support on your machine. Set resource budgets and run a non-destructive known-answer test before a long workload. No executions are claimed in this document.

## Experiment 1: guest fixed-arena allocator

Program operations in a guest-compatible test binary: initialize a fixed arena; allocate variable supported sizes; write different patterns; free selected blocks; allocate again; verify live contents and non-overlap; exhaust the arena; and release. Save deterministic seeds and expected output. Explicitly test pointer/offset zero, alignment, checked arithmetic, fragmentation, coalescing if implemented, and exact API-specific zero-size behavior.

**Success:** same expected operations and exit status in each selected reference and free-running neural execution; no overlap and correct output after all operations; measured stack and arena remain within the actual memory map. Report unsupported tests as skipped, with reason.

## Experiment 2: long-run and numerical stability

Run a fixed seeded trace across increasing lengths, with periodic internal checksums and guest state comparisons where the actual runtime exposes them. Separately run input forcing and free-running. Compare CPU/reference, upstream precise settings, different batches, modified precision, quantization and backends **one at a time**. Include reset, restart and snapshot/restore. First divergence point, not only final output, is useful. A hash can detect divergence but cannot prove identical behavior for all executions or provide cryptographic attestation by itself.

## Experiment 3: resource accounting

Measure model/checkpoint bytes, host peak RSS/PSS as applicable, GPU/NPU allocated and peak reserved memory from the actual runtime, recurrent-state bytes, attention/KV cache, workspace, output buffers, guest arena occupancy and state snapshots. On a unified-memory machine identify overlapping allocations rather than summing incompatible counters. Record guest program instructions, neural trace tokens, wall time, time to first result, joules if accurately measurable and host/device utilization.

Useful derived quantities:

- `trace_tokens / guest_instruction` for a defined program and machine revision.
- `guest_alloc_ops / second` and `end_to_end_jobs / second`, with setup included or excluded explicitly.
- `physical memory peak / guest usable bytes` as a descriptive ratio; it includes the model and is not a universal efficiency metric.
- `checkpoint_bytes / live_guest_bytes` and restore latency, specifying all captured state.

Do not equate trace tokens per second with LLM response tokens per second. A fast isolated executor may not represent end-to-end text bridge performance. Avoid comparing theoretical native instruction rate with wall-clock neural throughput without matching workloads.

## Experiment 4: state and failure

Capture a safe-point checkpoint during a half-full heap, continue through a known trace, restore and rerun. Check allocation outcomes and memory content. Inject truncated checkpoint, wrong model hash, wrong dtype, interrupted write, stale generation, duplicate command, deadline expiration, runner crash and partial network message. A worker must fail explicitly rather than continuing with a corrupted state. If restore to different hardware is being studied, label it experimental until bit-exact validation.

## Experiment 5: network chain

Run a local native coordinator, a reference guest worker and a neural-machine worker. Send bounded operation batches with input hash and unique request ID. Intentionally duplicate a request and disconnect before acknowledgment; verify idempotency and authoritative fencing. Measure serialization, queueing, execution and transfer separately. Compare a local conventional worker to the networked neural one; include setup cost and failures.

## Report template

**Hypothesis; source/revision; test environment; target path; workload and seeds; commands; expected output; observed output; state comparison; resource accounting; failures/skips; threat and privacy scope; limitations; conclusion; raw logs and reviewer.** Provide repeated run distribution with sample count and identify noise sources. A successful experiment is evidence for exactly its named configuration and test scope.

## Sources

- [Qwenputer RV32 test notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Percepta Transformer VM tools](https://github.com/Percepta-Core/transformer-vm)
- [RISC-V ISA tests](https://github.com/riscv-software-src/riscv-tests)
- [Linux pressure stall information](https://docs.kernel.org/accounting/psi.html)
