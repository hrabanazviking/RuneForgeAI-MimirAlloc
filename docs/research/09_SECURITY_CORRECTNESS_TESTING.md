# Correctness, security, testing and evidence

## Correctness invariants

- Every live returned block is correctly aligned, within owned mapped storage and non-overlapping with every other live block.
- Metadata transitions are synchronized; each free block belongs to exactly one permitted state at a time.
- Integer additions, multiplications and alignment rounding are checked before use.
- A failed `realloc` preserves the original allocation as required by the applicable interface; zero-size behavior follows the documented target contract.
- A released page contains no live blocks, active readers, pending remote frees or in-flight accelerator operations.
- Allocation and deallocation use compatible ownership domains; mixed-allocator frees are undefined or rejected according to the explicit API.
- Failure handling and initialization do not recursively require the allocator in an unbounded way.

Test conditions separately for C, C++ operators, shared libraries, process exit, `fork`, signals, dynamic loading and static linking if those paths are claimed. Async-signal safety and after-fork guarantees must be explicitly scoped; avoid promising them without implementation and tests.

## Test pyramid

| Layer | Examples | Evidence |
|---|---|---|
| Contract unit tests | size boundaries, null free, alignment, overflow, realloc | named platform, compiler and exact API |
| Model/property tests | randomized sequences compared with a reference ownership model | seed, trace and minimized failure |
| Concurrency stress | allocate on one thread, free on another, thread teardown | repeat, race detection where compatible |
| Fault injection | mapping failure, quota denial, queue saturation | deterministic failure point and expected cleanup |
| Integration | loader/symbol inspection plus allocation audit counter | proof target calls MimirAlloc |
| Physical device | 4 KiB/16 KiB as available, ARM/RISC-V target | machine and OS, exact build and logs |
| Application | actual Mojo reproducer and A.E.S.I.R. workload | baseline versus candidate; output verification |

Do not create a test that double-frees a raw pointer in ordinary release mode and then treat arbitrary behavior as proof of safe detection; invalid free is outside many APIs' contracts. If debug mode promises detection, test that documented debug behavior explicitly. Use isolated subprocesses for intentionally destructive tests.

## Sanitizers and tools

Use ASan, UBSan and TSan where toolchain, allocator interception and environment are compatible; allocator internals can conflict with sanitizer interceptors, so document the configuration and do not assume every build is instrumented. Run separate debug and production-like builds. Use fuzzing with saved seeds and bounded memory; add deterministic tests when an actual bug is found. Measure metadata corruption and memory-mapping error paths under explicit failure injection.

## Threat model

Threats include buffer overflow, use-after-free, double-free, metadata manipulation, stale handles, untrusted plugins, shared-buffer information leaks and resource exhaustion. Mitigations such as bounds checks, randomized reuse, quarantines, encoded freelists, guard pages, metadata isolation, zero-on-reuse and hardware tagging have cost and incomplete coverage. Quarantine competes with low-memory targets; support bounded options and measure. MTE can catch classes of errors on supported platforms but has finite tags and incomplete spatial/temporal detection. Never claim it makes code memory-safe.

## Provenance and clean-room

Keep a ledger: source URL, retrieval date, license, what was learned, whether implementation code was inspected, contributors and allowed use. Behavioral requirements should be independently stated. Avoid implying strict legal clean-room separation unless roles, access and records support it. Follow third-party licenses when reusing permitted material. Consult qualified counsel on legal questions when needed.

## Benchmark design

Run allocator microbenchmarks and realistic applications; compare system allocator, chosen mature alternatives, and MimirAlloc on equal compiler flags and same workload. Pin versions; report warmup, repetitions, medians and tail distributions, confidence or spread, peak and time-series memory, background activity and CPU governors. Performance should include app throughput, p50/p95/p99, faults, RSS/PSS, retained bytes, energy when reliable and failure rates. Do not compare incompatible workloads, mismatched allocations or sanitized/debug builds against optimized production without disclosure.

## Integration proof

A successful build or `LD_PRELOAD` setting is not proof that all relevant calls use the new allocator. Inspect dependencies and symbols; use a safe observable counter or audit hook in a test target, exercise the exact failure, and check allocation/free pairing across modules. Static, hidden, inlined and explicitly linked symbols may bypass interposition. Record an integration blocker if the relevant process cannot be modified.

## Sources

- [Linux MTE](https://docs.kernel.org/arch/arm64/memory-tagging-extension.html)
- [Android MTE guidance](https://developer.android.com/ndk/guides/arm-mte)
- [StarMalloc verification research](https://doi.org/10.1145/3689773)
- [TCMalloc allocator characterization](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/)
