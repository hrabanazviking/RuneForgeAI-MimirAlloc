# Future architectures and MimirAlloc research roadmap

## Ground rules

This document proposes research directions, not a promise of implementation, hardware support or performance. An allocator cannot by itself create cache coherence, bypass an accelerator driver, overcome a hardware RAM limit, distribute a program or guarantee that Mojo runs on a particular device. Each milestone needs an actual implementation, named tests and evidence.

## Architectural horizon

**Near term — portable heap correctness.** A bounded, original, namespaced CPU allocator with a simple documented interface. Verify on x86-64 and physical ARM64 Linux; reproduce the exact Mojo blocker and its linkage. Baseline against the system allocator.

**Next — mobile-aware management.** Android/bionic API and page-size support as separate gates, bounded caches and measurable trim. Collect real tablet results. Add opt-in pressure policy only after a fixed-policy baseline and control-loop tests.

**Heterogeneous compute — explicit buffer domains.** Design optional GPU/NPU adapters for capabilities actually exposed by each platform. Use opaque buffer handles, completion-aware lifetimes, budgets and backend-specific tests. CPU allocation and device allocation stay separate abstractions.

**Tiered memory — optional placement.** Research NUMA and supported OS-exposed memory tiers. Make placements observable; only migrate movable buffers with application cooperation. Benchmark CXL if hardware exists rather than promising universal pooling.

**Distributed memory — independent service.** Investigate remote buffers with explicit handles, leases, authentication, consistency, quotas and failure recovery. Integrate only after a threat model, network-fault tests and end-to-end measurements show value.

**Longer horizon — emerging machines.** Explore chiplets, coherent accelerator fabrics, disaggregated memory, memory-side compute, heterogeneous cores, capability-oriented addressing and specialized AI accelerators. These terms describe diverse hardware with different maturity. No shared capability, standard, access model or date is assumed; run feasibility studies when target hardware and APIs exist.

## Proposed architecture boundaries

1. **Core:** portable data structures, checked arithmetic, contracts, diagnostics and original design.
2. **OS backend:** mapping, release, page-size query, synchronization and errors for each supported OS.
3. **Policy:** bounded local cache and configurable retention; pressure handling optional and separate.
4. **Observability:** minimal safe counters, optional sampling and exported diagnostics.
5. **Buffer domain adapters:** optional accelerator integrations, each with explicit lifecycle and completion semantics.
6. **Tier/remote research:** independent opt-in libraries or services using handle APIs; no implicit remote raw pointers.

These are conceptual module boundaries, not existing folders or APIs. A tier or device backend should never be selected unless it satisfies the caller's hard requirements. Track shared physical accounting to avoid double-counting host and device views of the same pages.

## Milestones and exit criteria

| Stage | Deliverable | Exit evidence |
|---|---|---|
| 0 | Reproduction packet | Exact Mojo command, artifact, error, version and suspected link path |
| 1 | Portable contract | target-specific semantics and ownership table, reviewed tests |
| 2 | Experimental CPU allocator | contract, overflow, alignment, concurrency and stress tests on named hosts |
| 3 | ARM64 device | physical test logs, page/ABI data and comparison to native allocator |
| 4 | Integration | original failure resolved or specific blocker documented; allocator use proven |
| 5 | Android | native build and physical runtime tests distinct from Termux/proot |
| 6 | Efficiency | reproducible end-to-end wins with memory, latency and energy tradeoffs |
| 7 | Accelerator research | one explicit supported backend, synchronization and teardown tests |
| 8 | Tier/network research | complete threat/failure model, realistic application experiments |

Stages may be explored in parallel as research, but no dependency is silently declared complete. ARM64, Android, GPU and network support are each separate claims.

## Research backlog

- **R1: Address-space assumptions.** Measure real devices and isolate the Mojo/TCMalloc failure. Success: reproducible cause and verified integration boundary, or a documented contradiction.
- **R2: Size classes.** Compare histogram-driven versus geometric classes using same traces. Success: memory/latency frontier and variance, not one universal winner.
- **R3: Remote free.** Compare bounded locked queues and batched transfer under asymmetric load, thread death and pressure. Success: correctness plus retained-memory and tail-latency results.
- **R4: Pressure controller.** Compare fixed and adaptive caps under repeatable limited workloads. Success: stable behavior with no required privileged metrics and no material regression beyond stated limits.
- **R5: Device lifetime.** Prototype event-based reuse with one real API. Success: no reuse before completion under multiple queues, reset and cancellation.
- **R6: Tier placement.** Compare local and available tier with movable handles. Success: measured system-level latency, cost and energy including migration.
- **R7: Remote block service.** Versioned immutable blocks with authenticated access and injected failure. Success: correctness, quota enforcement, recoverability and a useful real workload.
- **R8: Sustainability.** Compare joules per completed task, device thermal behavior and wakeups; power measurement methodology and noise must be reported.

## Decision log template

Record date, question, hypothesis, primary references, reproducible baseline, target, design alternatives, safety/compatibility concerns, experiment, results, decision, unresolved questions, owner and next review. Link implementation and logs; state whether each conclusion is researched, prototyped, device-tested or integration-proven.

## Sources

- [Linux CXL](https://cxl.docs.kernel.org/)
- [RISC-V memory model](https://docs.riscv.org/reference/isa/unpriv/rvwmo.html)
- [Linux pressure stall information](https://docs.kernel.org/accounting/psi.html)
- [CUDA stream ordered allocator](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
- [CXL specification](https://computeexpresslink.org/wp-content/uploads/2024/02/CXL-3.0-Specification.pdf)
