# Modern allocator design and improvement

**Research status:** established approaches and proposed experiments; none is a claim of MimirAlloc functionality. The right design is workload- and platform-specific.

## Map the workload before optimizing

Capture sampled allocation sizes, lifetimes, call sites where permitted, thread ownership, allocation/free thread pair, phase changes and peak simultaneous live bytes. Keep tracing overhead measured, avoid recording payloads or personal information, and compare instrumented with uninstrumented behavior. Compiler workloads, a text-generation server and a mobile UI produce different histograms. Never infer the ideal size classes from one benchmark.

## Small-object layout

A segregated design rounds each request to a class and allocates slots from a slab. Dense classes lower rounding waste but increase metadata, partially used pages and tuning complexity. Estimate waste with a histogram: `sum(count[size] * (class(size)-size))`; separately measure metadata and empty slots, because this formula omits them. Test both geometric and workload-informed classes. Guarantee required alignment and overflow-safe arithmetic before computing rounded sizes. Group allocations with similar lifetimes only where the API or observations justify it: a single long-lived object can pin a slab full of dead objects.

## Thread and CPU locality

A local cache avoids central synchronization but may leave many idle threads each holding free memory. Compare per-thread, per-CPU and shared-cache variants. Enforce hard bounds in bytes, not only object count; drain on thread exit and on pressure. On heterogeneous CPUs, migration between cores can undermine naive per-CPU assumptions. CPU identity is not stable during a critical section unless the chosen platform mechanism guarantees it. Per-CPU designs must account for available OS primitives and fallback paths; do not assume `rseq` exists on all Android and Linux targets.

Cross-thread frees need an explicit owner and a safe transfer protocol. Candidate: enqueue returned blocks to the owning slab and batch their incorporation. Test producer death, owner thread exit, queue bounds, ABA/reuse, remote-free floods and when pages may be unmapped. A lock-free label does not establish progress or memory safety. A simple correct lock-based baseline is valuable.

## Backend and reclamation

For larger allocations compare mappings versus reusable extents with split/coalesce. Page size, virtual-address constraints, kernel mapping limits and address-space layout all affect outcomes. Experiment with decay and purge: choose a time or pressure threshold for returning entirely unused pages. Measure immediate reuse misses, page faults, retained RSS and latency. `madvise` choices have platform-specific semantics; check return codes and document whether address contents remain valid. Never reclaim pages containing live objects or outstanding asynchronous access.

Huge pages can reduce translation pressure for appropriate large, stable regions; partial release may split them, while retention can waste RAM. Compare system defaults, disabled and advised configurations only where supported, and publish page-fault, RSS, TLB and end-to-end effects. Huge pages are a candidate, not an automatic win on a memory-constrained device.

## Modernization opportunities

| Idea | Evidence or hypothesis | Benefit to test | Risk |
|---|---|---|---|
| Sharded free lists | Existing research | Parallel throughput and locality | More partially occupied slabs |
| Batched remote frees | Existing design family | Less contention | Deferred reclamation, queue growth |
| Pressure-aware retention | Engineering hypothesis | Lower stall and tail footprint | Churn and oscillation |
| Lifetime-aware arenas | Established application technique | Simple bulk release | Inaccurate lifetime grouping |
| Adaptive classes | Research direction | Match phases | Reconfiguration overhead and reproducibility |
| Background decay | Established design option | Smooth cleanup | CPU/energy wakeups and shutdown complexity |
| Tier-aware explicit buffers | Research direction | Place large movable objects appropriately | Topology volatility, migration and policy errors |

Avoid speculative SIMD in metadata hot paths unless profiling shows benefit; vector setup, alignment and code size may hurt tiny allocations. Hardware prefetch or software prefetch must be measured, not assumed. Reducing pointer chasing or cache misses often matters more than instruction count.

## A proposed pressure controller

Observe a bounded combination of live bytes, allocator-held bytes, cgroup events and PSI when accessible. On sustained pressure, reduce cache caps and request safe slab purging; use distinct enter/exit thresholds and a cooldown to avoid oscillation. On recovery, restore caps gradually. Use an opt-in policy with hard user-configured ceilings. No controller can guarantee avoiding OOM or access privileged metrics in every environment. Test invalid/missing signals, rapid workload phase changes, stale events and CPU cost. Compare with a fixed policy and publish both tail latency and memory.

## Security and correctness tradeoffs

Guard pages, delayed reuse, randomized placement, metadata separation, encoded links and tagging can reduce exploitability or detect corruption; each costs memory, speed or compatibility. Make hardening a measured profile with documented coverage, not a promise to prevent all exploitation. Use sanitizers in compatible separate builds and fault-injection. Production hardening must not rely exclusively on debug asserts.

## Research evaluation

For each proposed improvement record: baseline and exact revision; hypothesis; independent variable; metrics; representative trace; repeated run variance; regression thresholds; implementation complexity; rollback. Reject a win measured only on a synthetic hot loop if whole-program latency, responsiveness or memory worsens beyond agreed budgets.

## Sources

- [mimalloc research](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/)
- [snmalloc publication](https://spiral.imperial.ac.uk/entities/publication/a3c77e4b-1db1-4e65-ba35-73b075c351a6)
- [TCMalloc fleet characterization](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/)
- [Hugepage-aware fleet allocation](https://research.google/pubs/beyond-malloc-efficiency-to-fleet-efficiency-a-hugepage-aware-memory-allocator/)
- [Adaptive hugepage subrelease](https://research.google/pubs/adaptive-hugepage-subrelease-for-non-moving-memory-allocators-in-warehouse-scale-computers/)
