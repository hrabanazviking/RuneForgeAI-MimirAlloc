# Allocator landscape: what to study and compare

The projects below are research subjects and possible baselines, not dependencies or claims that MimirAlloc copies or replaces them. Architecture and license can vary by revision and build configuration. Check upstream repository, release, platform and license before using code. Independently document any implementation inspection in the provenance ledger.

| Project/family | What to investigate | MimirAlloc question | Primary source |
|---|---|---|---|
| Platform libc allocator | default ABI, small/large behavior and OS integration | What does the user already have, and does a replacement help? | [glibc malloc manual](https://www.gnu.org/software/libc/manual/html_node/Memory-Allocation.html) |
| Google TCMalloc | scalable front/middle/back ends and deployment data | Which component and linkage are implicated in the actual Mojo failure? | [TCMalloc overview](https://google.github.io/tcmalloc/overview.html), [fleet study](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/) |
| jemalloc | arenas, bins, decay and statistics | How do concurrency, tuning and retention compare? | [official project](https://jemalloc.net/) |
| mimalloc | free-list sharding and local/remote behavior | What locality and retention tradeoffs emerge on edge traces? | [research paper](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/) |
| snmalloc | ownership and cross-thread message passing | When do batching and remote frees help? | [publication](https://spiral.imperial.ac.uk/entities/publication/a3c77e4b-1db1-4e65-ba35-73b075c351a6) |
| scalloc | scalable allocation with virtual-memory-oriented design | How do constrained virtual address spaces change its tradeoffs? | [research](https://research.google/pubs/fast-multicore-scalable-low-fragmentation-memory-allocation-through-large-virtual-memory-and-global-data-structures/) |
| TLSF | two-level segregated fits designed for bounded operation | What is actually bounded, including locking, faults and OS access? | [original paper DOI](https://doi.org/10.1109/EMRTS.2004.1311009) |
| Scudo | hardened allocator | How much security value versus footprint and speed? | [LLVM documentation](https://llvm.org/docs/ScudoHardenedAllocator.html) |
| StarMalloc | formal verification of hardened allocator | Which invariants could be mechanically checked? | [research paper](https://doi.org/10.1145/3689773) |

## How to make a fair comparison

1. Choose exact target platform, architecture, libc, compiler and workload. Confirm each candidate actually supports it.
2. Record upstream release/commit and build settings. Note which symbols are intercepted and which modules bypass the allocator.
3. Compare an unchanged system baseline, each mature allocator, then the experimental design. Run correctness tests before speed comparisons.
4. Test tiny, mixed, large, same-thread and cross-thread allocations plus idle and long-lived object patterns. Include at least one compiler or AI application.
5. Record start-to-finish metrics and repeat under low memory. Report workload failure and correctness failures as results, not missing data.
6. Keep independent observations; a published study's results describe its own hardware, version and workloads, not a promise for a different device.

## Research contrasts

**Throughput vs retention:** local caches cut coordination but can strand memory. **Security vs space:** quarantine slows reuse but increases retained bytes. **Throughput vs predictability:** background purge or page faults can introduce latency spikes. **Virtual space vs portability:** a large reserved region may be fine on one system and impossible on another. **Reuse vs isolation:** sharing pools can increase efficiency while creating contention and security risks. Optimize a measured Pareto frontier rather than proclaiming one allocator best.

## Licenses and naming

A license permits particular uses under its terms; it does not by itself establish a clean-room workflow. Keep copyright notices where needed and record version-specific terms. MimirAlloc and Microsoft's mimalloc are distinct projects with potentially confusable names; check naming and trademark availability before release.
