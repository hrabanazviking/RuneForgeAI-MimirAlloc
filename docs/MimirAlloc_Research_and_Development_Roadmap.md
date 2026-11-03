# MimirAlloc: ARM Memory Allocator Research and Development Roadmap

**A clean-room, open-source memory allocator project for ARM, Mojo, Android, and Project A.E.S.I.R.**  
**Research date:** November 3, 2026  
**Status:** Research and proposed roadmap. MimirAlloc does not yet exist as a verified implementation.  
**Working name:** MimirAlloc. Check for name, trademark, package and domain conflicts before public release; Microsoft already publishes the prominently named **mimalloc**, so confusion is plausible. This is not a legal clearance.

---

## 1. The vision

MimirAlloc is a proposed independently engineered general-purpose memory allocator, initially focused on 64-bit ARM devices and the needs of Project A.E.S.I.R. and the Mojo toolchain. Its goals are correct memory ownership, dependable operation under pressure, useful diagnostics, low overhead, responsible mobile-device behavior, and competitive measured performance.

The practical motivation is an observed or suspected tcmalloc-related obstacle when building or running Mojo on an ARM device. The first task is to identify that exact obstacle and demonstrate that an allocator replacement can affect it. In parallel, MimirAlloc can be a useful standalone open-source project even if the eventual Mojo issue turns out to involve another subsystem.

A memory allocator manages dynamic memory used by software. For example, a program asks for space for objects, compiler data, strings, or temporary structures; an allocator obtains memory from the operating system, keeps track of which pieces are in use, and returns freed space for reuse. An allocator is not itself an inference engine or a GPU/NPU runtime. An efficient allocator can help Aesir and its build tools with startup, temporary allocations, fragmentation, and stability, but it cannot by itself make an unsupported compiler, driver, operating system, or model architecture work.

### Project goals

1. Determine what exact allocator symbols and behavior are needed for the specific Mojo build and runtime artifacts.
2. Make a standalone, original allocator for ARM64 Linux with a clear C API and strong correctness tests.
3. Establish an explicit, tested integration path for the Mojo compiler/build or generated application, whichever actually has the blocker.
4. Extend support to native Android ARM64 and the user's Samsung tablet where feasible.
5. Offer measured performance and memory behavior on realistic compiler and local-AI workloads.
6. Handle resource limits and failures predictably without claiming to eliminate all crashes or all corruption.
7. Preserve a clean provenance record and meaningful open-source governance.

### Non-goals for the first usable version

- Reproduce every internal tcmalloc algorithm, C++ class, API extension, profiling protocol or performance characteristic.
- Claim compatibility before confirming the actual linkage and exercising the real target.
- Claim Android support merely because an ARM64 Linux binary works under `proot`.
- Promise universal interposition, cross-allocator freeing, zero fragmentation, or guaranteed protection from out-of-memory termination.
- Make the allocator responsible for detecting every invalid pointer or repairing arbitrary application corruption.
- Assume that tcmalloc has no ARM support: its published Linux platforms include AArch64. The relevant question is whether the **specific revision, target ABI, build flags, address-space assumptions, and environment** work together [S1].

---

## 2. Evidence already available and unresolved questions

| Topic | Evidence or observation | What it does not establish |
|---|---|---|
| ARM64 support | TCMalloc lists Linux AArch64 as supported [S1], and its public implementation contains AArch64-specific code. | That any particular build works on Android, a particular kernel configuration, a specific VA width, Mojo revision, or device. |
| TCMalloc architecture | Public documentation describes small-object caches, central services, backend memory management, and different per-CPU/per-thread approaches [S2]. | A requirement for MimirAlloc to reproduce the same internal design. |
| Tablet | In this conversation, Codex running locally reported Samsung Galaxy Tab S10 Lite Wi-Fi SM-X400, Android 16, Exynos 1380, approximately 5.57 GiB usable RAM and 1.52 GiB available at one point. | A guaranteed persistent free-memory value; whether native Mojo or MimirAlloc can run there. |
| Android memory pages | Android documents 16 KiB page support alongside 4 KiB configurations [S3]. | The actual page size of this specific tablet, which must be measured. |
| Name | Microsoft has an existing allocator named mimalloc [S4]. | That MimirAlloc is legally unavailable or available. |
| Mojo blocker | The user reports a tcmalloc-related ARM build problem and difficulty getting upstream fixes accepted. | The exact error, affected component, revision, cause, or whether replacing the allocator solves it. These require a reproducible evidence packet. |

### Claims and evidence levels

Use the following labels throughout the project:

- **Proposed:** an idea not implemented.
- **Implemented:** code exists, but has not passed the full test gate.
- **Host-tested:** passed named tests in a specified host environment.
- **Device-tested:** ran on the named physical ARM or Android device.
- **Integration-proven:** the exact Mojo or Aesir workflow succeeded with MimirAlloc demonstrably handling the relevant allocations.
- **Performance-measured:** results have complete scripts, workload, baseline, versions and hardware.
- **Blocked:** a specific prerequisite or test is unmet, with an actionable resumption point.

Never promote an emulator result to physical-device proof, a synthetic test to full application compatibility, or a successful compile to proof of runtime interposition.

---

## 3. First priority: identify the actual Mojo blocker

This phase determines whether MimirAlloc is the appropriate solution and which integration boundary matters. It should be performed before committing to a tcmalloc-shaped ABI.

### 3.1 Capture the failure

Record without secrets:

- Repository, commit or release, branch, build instructions, clean build directory, and exact command.
- CPU architecture and ABI (`aarch64` versus Android `arm64-v8a`), OS, Android build if applicable, libc, toolchain, linker, Bazel and NDK versions.
- Native Android/Termux, Android app process, proot Linux, ordinary ARM64 Linux, or cross-build host. These are different environments even when all use ARM64.
- Compiler and linker output, full failure log, exit code, first relevant error, resource use and whether the failure is reproducible.
- Runtime page size (`getconf PAGESIZE` where available), measured user address-space characteristics if relevant, filesystem limits, and available memory.
- Whether the failure happens during dependency fetching, compilation, linking, tool startup, Mojo source compilation, generated binary startup, or generation/runtime.
- The exact tcmalloc revision, build options and local modifications; identify the upstream issue/patch and its status when available.

Save a minimal reproduction and environment manifest with the logs. Avoid posting access tokens, credentials, private model prompts or unnecessary device identifiers.

### 3.2 Classify the failure

| Category | Typical clues | Initial investigation |
|---|---|---|
| Source compile | C/C++ errors, unsupported intrinsics or headers | Reproduce the single target; identify platform macros and compiler/NDK version. |
| Link | Undefined symbols, duplicate definitions, incompatible ELF or missing library | Inspect the link line, ABI, linkage and symbol table. |
| Loader/startup | Missing interpreter or shared library, startup crash | Inspect ELF interpreter, dynamic dependencies and libc environment. |
| Address-space configuration | Failed mapping or tag/address assumptions | Verify actual virtual-address layout and source configuration; do not infer only from CPU name. |
| System call or sandbox | Unsupported syscall, seccomp, permissions | Capture errno and environment; compare native Linux, Android and proot. |
| Resource pressure | ENOMEM, OOM kill, huge virtual reservation | Record RSS/PSS, address-space limits, memory pressure and kernel logs where accessible. |
| Allocator correctness | Reproducible invalid free, race, corruption | Minimize; use sanitizers or trace tools on an appropriate host and inspect ownership. |

### 3.3 Map the integration layer

There may be several independent allocator consumers:

1. The **build tools themselves**: Bazel, compiler, linker, or dependencies.
2. The **Mojo compiler executable**: a prebuilt compiler or a compiler being built from source.
3. The **Mojo standard/runtime libraries** linked into generated applications.
4. The **generated Mojo executable** and its application-level allocator calls.
5. **Aesir-specific allocation** within a host or Android process.

For each, identify whether the allocator is statically linked, dynamically linked, explicitly called, interposed, or absent. Review exact exported and unresolved symbols using tools such as `readelf`, `nm`, `objdump`, compiler link maps, and the build system's query/aquery facilities. On Android, use NDK-compatible tools and inspect namespace and app sandbox restrictions. Dynamic preloading may not affect a statically linked executable, private/hidden symbols, secure execution, internal allocators, or Android app processes. A library with the same function names alone is not proof it is used.

**Deliverables:** `evidence/mojo-arm-repro.md`, `evidence/symbol-contract.md`, build logs, linkage diagram and a one-page conclusion naming the exact blocker and candidate substitution points.

**Exit gate:** someone else can reproduce the failure and can explain why the selected integration point can or cannot change it. If replacing tcmalloc is not viable at that point, continue MimirAlloc as an independent project but do not claim it fixes Mojo.

---

## 4. Clean-room engineering and provenance

The objective is an original, auditable implementation that satisfies public contracts and observed requirements. Reading public descriptions, standards, and documentation can inform design, but should not lead to copying protected expression or misrepresenting authorship. TCMalloc is open source: its license and attribution requirements matter if any source is actually reused; an upstream maintainer declining a patch is not itself a license restriction.

### 4.1 Practical process

1. Maintain a dated source ledger listing public documents, standards, tests and tools consulted.
2. Write a behavior specification in original language: inputs, outputs, edge cases, error behavior, public ABI and measurable properties.
3. Create an original design from the specification and general allocator concepts, documenting design alternatives and decisions.
4. Keep third-party source snippets, copied implementation code, proprietary material and unlicensed disassembly out of the implementation and prompts intended for implementation.
5. When team separation is available, assign one investigator to public-interface evidence and a separate implementer to the independent specification. Have an independent reviewer check the result.
6. When strict personnel separation is unavailable, record that limitation. Do not label a solo or AI-assisted process as a legally verified two-team clean room.
7. Keep commit history, provenance notes, contributor sign-off and dependency notices.
8. Compare observable behavior using independent test cases; do not copy tests verbatim unless their license and reuse terms are followed.
9. Before releases, review source similarity concerns, license obligations, trademarks and any patent risks with qualified counsel if warranted. This document is an engineering process, not a legal opinion.

Open-source license selection is the owner's decision. MIT, Apache-2.0 and BSD-style licenses are common possibilities, but the choice should consider patent terms, compatibility, attribution and long-term project goals. Do not copy the project name or use branding that implies Google or Microsoft affiliation.

---

## 5. Supported-target matrix

| Target | Initial role | Required evidence |
|---|---|---|
| Linux AArch64 + glibc | First production candidate | Native CI, multithread tests, ABI and linker proof. |
| Linux x86-64 + glibc | Development/reference platform | Same correctness tests; useful for sanitizers and differential testing. |
| Linux AArch64 + musl | Compatibility target | Correct compile and runtime, interposition assessed separately. |
| Android ARM64 + bionic | Separate native port | NDK build, Android application/native test, device ABI and page-size validation. |
| Termux on Android | Developer workflow | Native bionic compilation and execution; permission limits documented. |
| Linux userland in Android proot | Convenience test environment | Independently labeled proot results; not treated as native Android app proof. |
| Samsung SM-X400 physical tablet | Flagship constrained device | Measured page size, memory conditions, stress and device responsiveness. |
| Raspberry Pi ARM64 Linux | Secondary physical ARM target | Real-device test, OS/compiler/kernel manifest and performance data. |

Later targets may include macOS ARM64, Windows ARM64 and other architectures, each with its own allocation ABI and OS interface. None should be claimed until tested.

---

## 6. Contract-first API and ABI work

### 6.1 Core API candidates

- `malloc(size_t)` and `free(void*)`.
- `calloc(count, size)` with overflow checking before multiplication and guaranteed zeroing of the requested bytes.
- `realloc(ptr, size)` with preservation of the old block on allocation failure; copy at most the smaller valid size.
- `aligned_alloc` and `posix_memalign` with target-specific validity, alignment and error requirements.
- `reallocarray`, `malloc_usable_size`, and related interfaces **only when required and defined per target**.
- C++ `operator new`/`delete`, array, nothrow, aligned and sized variants only when the integration requires them, with the C++ ABI, exception and runtime rules checked on each platform.

The precise zero-size behavior, errno semantics, aligned allocation requirements, `realloc(p, 0)` behavior and `malloc_usable_size` availability vary by standard and libc. Do not present a Linux/glibc behavior as universal Android or POSIX behavior. Build a target-specific contract table from authoritative documentation [S5, S6].

### 6.2 Ownership rules

- A pointer must be freed by an allocator compatible with the one that created it. Never pass an allocation to a different allocator unless its ownership and API explicitly allow it.
- Define behavior for `free(NULL)` and allocation failures per the chosen ABI.
- For invalid pointers, use-after-free and double-free, offer debug detection where possible; ordinary C does not require recovery from undefined behavior.
- No silent fallback to a different allocator if the returned pointer could later be freed by the wrong implementation.
- Explicitly distinguish internal logical pages, system page size, spans and huge pages.
- Runtime changes that affect cache, statistics or diagnostics must be thread-safe and versioned.

### 6.3 Allocation-free startup and diagnostics

Global allocators may execute before `main`, from constructors, loader code or during logging. A design that allocates with the same allocator in its own initialization can recurse indefinitely. Design a minimal bootstrap path, avoid allocator-dependent C++ containers during initialization, explicitly handle reentrancy, and avoid unbounded dynamic allocation in error reporting.

Allocator use from signal handlers and the child of a multithreaded `fork()` needs a narrowly specified policy. Do not claim async-signal safety for functions that cannot meet it. Investigate `pthread_atfork` interactions, lock state, and child behavior; test with watchdogs on every supported platform.

---

## 7. Proposed architecture

The following is an original candidate architecture for evaluation, not a description of tcmalloc internals to copy and not an implementation claim.

### 7.1 Layers

1. **Public boundary:** standard C ABI plus an optional namespaced API (`mimir_malloc`, `mimir_free`, stats, trim). Each exported interface has explicit ownership and error behavior.
2. **Admission and checked arithmetic:** validate sizes, alignment, overflow and optional explicit process budget. OS pressure callbacks are separate from hard reservations.
3. **Small-object fast path:** initially simple thread-local lists or bounded caches, subject to measured need.
4. **Central ownership:** size-class management, free-span accounting, remote-free handling and synchronization.
5. **Page/span manager:** acquire, divide, combine and return mappings; maintain metadata and occupancy.
6. **OS abstraction:** page-size discovery, mappings, unmapping, reclamation/advice, threading primitives and platform-specific behavior.
7. **Optional observability:** low-overhead counters and opt-in diagnostic snapshots; default to no private-data collection and no network telemetry.

### 7.2 Implementation language decision

The core must work even if Mojo is unavailable on the target. Start with a small portable C11 or C++17 core, depending on the actual Mojo integration and dependency needs; expose a C ABI. C11 minimizes runtime and bootstrap coupling, while carefully constrained C++ can help express invariants but makes startup, exceptions and C++ allocation recursion more complex. Record the decision with a working proof. A Mojo interface or Mojo-specific integration can be developed above the core where supported, without making a working Mojo compiler a bootstrap dependency.

### 7.3 Threading and slow paths

Begin with a simple correct design, potentially coarse locks, and make correctness measurable. Add per-thread caching or sharded structures only when a benchmark shows a benefit. Account for:

- The thread that allocates may differ from the thread that frees.
- Thread termination must drain, transfer or safely reclaim local state.
- CPU migrations and preemption make naive per-CPU data structures unsafe.
- Bounded caches should reduce retained memory on low-RAM tablets.
- Shared metadata and cross-thread lists need explicit ownership and memory-order reasoning on ARM's weaker memory model.
- Do not implement lock-free algorithms merely because they sound faster; verify ABA, memory reclamation and false sharing first.

### 7.4 RSEQ and per-CPU designs

TCMalloc's documentation describes Linux restartable sequences for its per-CPU mode [S7]. MimirAlloc may research this public mechanism as an optional **later** optimization. Probe kernel and libc ownership, ABI, registration, preemption and fallback; do not assume it works in Android/proot. Avoid double-registration and assumptions about who owns a thread's rseq area. The reliable path must function with rseq unavailable, blocked, or disabled.

### 7.5 Large allocations and address-space safety

Use checked arithmetic for allocation sizes and alignments. Discover actual page size at runtime where appropriate. Avoid hardcoded 48-bit VA assumptions or pointer tagging unless the target explicitly supports and validates them. Plan for AArch64 pointer tagging, top-byte-ignore, MTE, PAC and sanitizers as separate compatibility concerns—not universal guarantees. Do not reserve huge virtual ranges without measurement. Selectively return unused large extents using documented OS mechanisms and measure the CPU cost of reclaiming them.

---

## 8. Step-by-step implementation roadmap

Each phase produces usable evidence. Later phases may begin early only when dependencies are met and their work cannot misrepresent an unproven feature.

### Phase 0 — Charter, naming and baseline

**Tasks:** create project charter; determine owner and license; reserve/check repository and package names; record the mimalloc naming concern; set code of conduct and issue templates; create evidence/decision ledgers. Capture current Aesir and Mojo build revisions and tablet inventory.

**Exit:** project scope, target matrix, release terminology and exact baseline documented. Naming availability remains unverified until separately checked.

### Phase 1 — Reproduce the actual tcmalloc/Mojo problem

Perform Section 3's failure and linkage investigation. Test a small standalone build against the implicated tcmalloc revision on the exact host/target if practical. Separate Mojo build failure from allocation under generated programs. Save minimal reproduction and symbol evidence.

**Exit:** documented causal hypothesis and a repeatable validation command; include conflicting evidence and exact unknowns.

### Phase 2 — Evaluate lower-cost compatibility routes

Evaluate system allocator, configurable build flags, a local patch, and established allocators such as mimalloc as **controlled diagnostic comparators**. Record licenses, linking constraints and outcomes. These experiments do not cancel the user's independent project; they establish whether a simpler route can unblock Aesir while MimirAlloc is built. Distinguish `LD_PRELOAD`, direct link, static replacement and namespaced API.

**Exit:** table of options with commands, measured results and integration limitations. No claims based on names alone.

### Phase 3 — Minimal correct allocator

Build namespaced allocation/free using OS mappings and metadata under a simple lock. Deliver checked allocation arithmetic, alignment, clean initialization, errors, and tests. A slow mapping-per-allocation prototype is acceptable only as an explicitly experimental correctness baseline; do not ship it as a performance allocator. Add calloc and realloc only with precise semantics and ownership.

**Exit:** portable unit/contract suite passes for explicitly supported calls on x86-64 and ARM64 Linux. Document unsupported behavior.

### Phase 4 — ARM64 and Android foundation

Create native AArch64 and Android NDK builds, CI cross-compilation and physical tests. Inspect ELF properties, Android API level, libc usage and alignment. Measure 4 KiB/16 KiB configurations, and avoid fixed page assumptions. Run a small native Android program in the actual target environment. Verify Termux and Android app behavior separately.

**Exit:** physical-device allocate/fill/free and multithread tests pass with logs, system page size and ABI recorded. A cross-compile alone does not pass the gate.

### Phase 5 — ABI completeness and interposition

Implement only the documented symbol contract for the integration route. Test C/C++ variants if needed, exported symbol visibility, startup ordering, shared libraries, static/dynamic linking, aligned allocations and cross-module ownership. Use a visible allocation counter or explicit audit hook to prove the intended call path invokes MimirAlloc. Reject ambiguous mixed-allocator ownership.

**Exit:** independent test executable and target tool show the intended symbols binding to MimirAlloc and pass lifecycle tests. Exact link route recorded.

### Phase 6 — Actual Mojo integration

Wire the demonstrated build or runtime boundary using documented build configuration. Rebuild from clean sources or use the appropriate published package only where legally and technically possible. Run a minimal Mojo program, compile a representative program repeatedly, and execute relevant upstream tests. Compare against the same revision without the replacement. Capture proof MimirAlloc actually serviced allocations.

**Exit:** original failing command succeeds on the affected ARM environment with explicit proof of allocator use and no new correctness failures, or record a blocker with exact results. A successful unrelated Mojo hello-world does not close this milestone.

### Phase 7 — Mobile-friendly small objects

Add measured size classes, bounded local reuse, central fallback, reclaim policies and thread exit handling. Avoid memory bloat when many threads are idle. Test device multitasking and repeated model-load/unload. Make tuning parameters configurable, validated and visible.

**Exit:** no regression in correctness; documented allocation latency, retained memory and responsiveness versus Phase 3 and the platform allocator.

### Phase 8 — Cross-thread and contention resilience

Implement and verify cross-thread frees and contention controls; then consider sharding or bounded remote-free queues. Include owner death, thread cancellation and teardown. Test at small thread counts and under oversubscription. Verify correctness with stress runs and independent data-pattern checks.

**Exit:** no corrupted live allocations or lost ownership across repeated stress scenarios; latency and memory reports archived.

### Phase 9 — Memory-pressure behavior

Design explicit soft budgets, process telemetry, trimming and cache release. Handle genuine allocation failure cleanly where the caller contract permits it. Measure reclaim overhead and fragmentation. Separate allocator-managed bytes from process-wide memory, model weights, mmap-backed files and device-driver memory. Android may kill a process under pressure before an allocation can be gracefully recovered.

**Exit:** fault injection and constrained-memory runs show documented, bounded behavior; no claim to prevent Android low-memory kills.

### Phase 10 — Diagnostics and hardened builds

Expose counters: requested, allocated, cached, mapped, released and peak bytes; allocation failures; span occupancy; cross-thread activity; optional lock contention. Define exactly what each counter means. Add debug-only red zones, canaries, poisoning, quarantine or metadata checks where appropriate, with overhead reported. Protect against integer overflow and corrupted metadata; fail clearly rather than pretending to repair arbitrary corruption.

**Exit:** statistics balance under tests, error reports are useful and do not recursively allocate, and production/debug behavior is clearly distinguished.

### Phase 11 — Performance research

Investigate direct fast paths, ARM SIMD where applicable, better sizing, batching, TLB/cache locality, metadata placement, false-sharing control and safe concurrency. Profile before adding complexity. Optional rseq or per-CPU mode comes only after platform proof, a tested fallback and benchmarked gain. Optimize both compiler-build and Aesir traces; a microbenchmark win is not a universal win.

**Exit:** publish reproducible results, loss cases, tail latency, RSS/PSS, energy where measurable, and throughput with confidence intervals or repeat distributions.

### Phase 12 — Aesir model-workload integration

Track where Aesir actually allocates: model loading, GGUF metadata, tokenizer, KV cache, prompt prefill, decode and teardown. Separate mmap weight residency, CPU buffers and GPU allocations. Verify allocator integration through Aesir's actual boundary and compare Qwen3-1.7B Q4_K_M on supported devices. Tune allocations only where profiles show useful benefit.

**Exit:** same model hash, prompt, context, thread count, backend and thermal state used across baseline and comparison; output correctness and tablet responsiveness maintained.

### Phase 13 — Security and production release

Run code review, API/ABI audit, compiler-warning gates, fuzzing, long soak and target-matrix verification. Publish signed or checksum-verified artifacts, versioning, SBOM, reproducible-build instructions, documented supported platforms and a security-reporting channel. No default global interposition until well validated.

**Exit:** explicit stable release criteria met; all limitations and unresolved defects public.

---

## 9. Reliability engineering and failure handling

### 9.1 Invariants

1. Every live allocation has one valid owner and non-overlapping usable storage.
2. Metadata describes actual allocated memory and is not accessed after its own lifetime ends.
3. Size and alignment calculations cannot overflow.
4. Freeing a null pointer behaves according to the target contract.
5. A failed `realloc` preserves the original valid allocation where the selected target contract requires it.
6. Cross-thread handoff preserves ownership; no list node appears simultaneously in incompatible free lists.
7. A full cache or failed backing allocation has a bounded and testable slow path.
8. Statistics do not drive unsafe decisions if they are approximate or stale.
9. A synchronization failure is reported or terminates safely in accordance with the contract; it is not hidden by returning overlapping memory.
10. Every fallback either preserves allocation ownership or explicitly fails.

### 9.2 Error detection versus correction

Debugging features can detect some overruns, stale metadata, repeated frees, or corrupted canaries. They cannot safely recover every corrupted heap: continuing with unknown ownership can make damage worse. The safe policy is to preserve diagnostic evidence and stop or report according to the host's contract. Hardware ECC is a separate hardware capability; allocator checks must not be called ECC. Protect user data through application-level atomic persistence and recovery rather than claiming allocator can repair it.

### 9.3 Controlled failure tests

- Artificially fail the Nth mapping or metadata allocation.
- Enforce low process limits where supported; simulate fragmented address space.
- Kill/restart a process during separate application-level state writes.
- Allocate near integer and alignment boundaries; verify overflow is rejected.
- Repeatedly create/destroy threads while transferring objects between them.
- Test process initialization, shared-library loading and teardown order.
- Confirm failures neither hang indefinitely nor return overlapping valid blocks.
- Track results separately for normal Linux, Android and proot.

---

## 10. Test and verification matrix

| Area | Minimum tests | What success means |
|---|---|---|
| C contracts | zero-size by target policy, NULL, overflow, errno, alignment, realloc copy/failure | Meets the documented selected ABI. |
| Data integrity | fill unique patterns, random sizes, check live allocations repeatedly | No unexpected changes to live bytes. |
| Ownership | same-thread, cross-thread, thread exit, shared object boundaries | Every valid allocation can be released exactly once by the correct owner. |
| Concurrency | thread churn, randomized operations, low/high contention, repeated seeds | No reproducible races, overlaps, hangs or incorrect ownership. |
| Memory | repeated cycles, fragmentation, cache drain and trim, large mapping pressure | Counters and OS metrics align with their stated meanings. |
| Robustness | failing OS calls, OOM, interrupted workloads, malformed debug inputs | Defined failure or diagnostic behavior; no false success. |
| ABI/linking | C and required C++ symbols, shared/static, dynamic loader | Actual consumer resolves intended implementation. |
| Platform | AArch64 Linux, native Android, Termux, optionally proot | Results attributed to the exact environment and target. |
| Tooling | ASan, UBSan, TSan and Valgrind where compatible | Explain custom allocator visibility and any unsupported combinations; avoid claiming coverage merely because the tool ran. |
| Regression | every confirmed defect gets a minimal reproducer | The exact defect is caught without relying only on synthetic tests. |

Fuzz sequences such as allocate, write, reallocate, transfer, free, trim, and thread exit against a simple ownership model. Save seeds and version every trace. Use differential comparison of **defined observable behavior**, not identical pointer addresses or timing.

---

## 11. Benchmarking and success metrics

### Principles

- Compare platform libc allocator, a pinned tcmalloc build where supported, mimalloc, and MimirAlloc using correct compatible build modes and equivalent settings.
- Verify each allocator was actually selected. Record dynamic versus static linkage and exported symbol evidence.
- Use fresh process runs; pin versions and record thermal state, core selection, clock policy, workload and repetitions.
- Report median, p95/p99 and outliers for allocation latency, throughput, peak RSS/PSS, virtual size, retained memory after freeing, mapping count and startup time.
- Measure CPU time, device responsiveness and energy/battery where reliable instrumentation exists.
- Include real compiler builds and Aesir model-load/inference workloads. Note that model quality and output should remain equivalent for allocator comparisons.
- For tablet trials, measure while other foreground tasks run; repeat under both cool and sustained conditions. Never call swap physical RAM.
- A lower allocation-time number that increases memory pressure or UI stutter may not satisfy the mobile objective.
- Store raw data, harness revision, scripts and negative results; flag noise rather than invent certainty.

### Benchmark classes

1. Single-threaded small allocation/free cycles with varied size distributions.
2. Producer/consumer and cross-thread free patterns.
3. Thread churn and multiple CPU clusters on ARM big.LITTLE devices.
4. Large allocations and mixed short/long lifetimes.
5. Fragmentation, return-to-OS and repeated startup/shutdown.
6. A real clean Mojo build and compiler invocation, if integration is proven.
7. Aesir load, prefill, decode and unload on the same model/configuration.
8. A time-limited multitasking and thermal soak on the physical tablet.

Targets are established **after** a device baseline; avoid fixed claims of superiority. Useful acceptance dimensions include zero test failures, bounded memory growth under a defined workload, measured responsiveness, and no material regression against a documented baseline.

---

## 12. Android and Samsung tablet checklist

- Confirm model, Android version, ABI and current security patch without collecting serial numbers.
- Record `getconf PAGESIZE`, `/proc/meminfo` where readable, and CPU architecture. Values can change or be restricted.
- Confirm whether testing is native Termux, proot Linux, an Android NDK command-line binary, or a packaged Android app.
- Avoid building enormous compiler dependencies on the tablet before measuring storage, RAM and time requirements; use ARM64 cross-builds or a separate machine where practical, but still test on device.
- Differentiate physical RAM, swap, page cache and current available memory.
- Account for Android background limitations, process lifecycle, low-memory kills, and access restrictions. A user-space allocator cannot force the OS to keep an app alive.
- Start with low concurrency and bounded allocations; test impact on foreground UI and thermal state.
- Treat CPU, GPU and NPU as separate execution systems. MimirAlloc manages host-side allocations; GPU/NPU allocations generally need their own documented APIs and synchronization.
- Samsung ENN/NNAPI libraries being present is not proof that a supported third-party app can run Qwen3 on the NPU.
- Support 4 KiB and 16 KiB system-page environments; do not assume this specific tablet uses one without measuring it [S3].

---

## 13. Suggested repository layout

```text
MimirAlloc/
  README.md
  LICENSE
  CONTRIBUTING.md
  SECURITY.md
  ROADMAP.md
  docs/
    architecture.md
    contracts.md
    clean-room-provenance.md
    mojo-integration.md
    android.md
    benchmarking.md
    decisions/
  include/
    mimir_alloc.h
  src/
    api/
    core/
    platform/
  tests/
    contract/
    concurrency/
    failure/
    integration/
    fuzz/
  benchmarks/
  examples/
  evidence/
  scripts/
```

This is a proposed layout. Set concrete implementation details after the toolchain and integration experiments. Avoid placing private prompts, credentials, model weights or device identifiers in the repository.

### Suggested initial issues

| ID | Title | Deliverable / acceptance |
|---|---|---|
| MIM-001 | Capture the failing ARM Mojo build | Exact command, revisions, environment and first error. |
| MIM-002 | Map allocator and linker ownership | Symbols, link mode, call path and candidate integration boundary. |
| MIM-003 | Test configurable allocator alternatives | Reproducible comparator matrix, no permanent project dependency implied. |
| MIM-004 | Write clean-room behavior specification | Documented standards, target-specific edge cases and provenance. |
| MIM-005 | Build namespaced mapping-backed allocator | A tested minimal prototype, clearly marked experimental. |
| MIM-006 | Contract and arithmetic test harness | Independent tests, deterministic seeds and fault injection. |
| MIM-007 | Establish ARM64 Linux build | Reproducible cross-build and physical or native target tests. |
| MIM-008 | Establish Android NDK build | Actual bionic-target test and 4/16 KiB policy. |
| MIM-009 | Prove symbol binding | Audit evidence that MimirAlloc services the intended consumer. |
| MIM-010 | Reproduce/fix original Mojo workflow | Original command succeeds or exact unresolved failure reported. |
| MIM-011 | Capture baseline memory/latency | Raw data and reproducible methods. |
| MIM-012 | Review name and licensing | Record conclusions and unresolved issues. |

---

## 14. Autonomous development instructions for Yrsa or another agent

**Mission:** Create the proposed allocator and its integration in small, verified slices. Work from the owner's current project repository and instructions. Do not assume any feature in this roadmap has been implemented.

1. Inspect the workspace, project rules, current branch and uncommitted work before editing. Preserve existing work.
2. Begin by reproducing the exact Mojo/ARM blocker and identifying actual allocator consumers.
3. Consult primary sources and pin versions. Keep a clean provenance and evidence ledger.
4. Write an original behavioral contract and a test plan before implementing the core.
5. Build the simplest correct slice, run its tests, fix failures, then continue to the next independent slice.
6. Work without repeated routine permission questions when the owner has authorized the scope and the environment permits the action. Respect required OS prompts, credentials, repository protection and other genuine access boundaries.
7. Do not install untrusted software, alter device security, grant broad access or replace system libraries merely to make a test pass. Use project-local builds and explicit test processes.
8. Never claim that preloading worked unless symbol/counter evidence proves it, or that Android was tested when only Linux/proot ran.
9. When blocked, save precise commands, logs, environment, failed hypotheses and next steps, and continue useful independent work.
10. Keep a running changelog and update the capability ledger with exact evidence. Tag plans, partial implementation and physical proof separately.
11. Commit in coherent, reviewable slices using the repository's actual contribution rules. Never claim tests that were not run.
12. Complete a clean rebuild, meaningful regression tests and a detailed handoff before presenting a milestone as done.

---

## 15. Decision register and open research questions

1. **What is the actual tcmalloc failure?** Exact source/build log and target environment required.
2. **Which process must use the replacement?** Compiler, build tool, generated executable, Aesir, or multiple independent consumers?
3. **Is tcmalloc statically or dynamically linked?** What prevents a configurable allocator from being selected?
4. **Which API and ABI symbols are required?** Standard C, C++, Google-specific extensions, profiling or other interfaces?
5. **Can the Mojo toolchain build or run against Android bionic?** Determine separately from ARM64 Linux support.
6. **What are the tablet's system page size and supported VA range?** Measure; do not infer from the CPU label.
7. **What is the safe mobile budget?** Measure available memory and foreground responsiveness across real workloads.
8. **Is per-thread caching beneficial?** Compare locality, memory retention and thread churn.
9. **Is rseq viable and worthwhile?** Prove availability and compatibility before designing around it.
10. **What existing allocators can unblock the build now?** Test objectively without abandoning the independent project.
11. **Does the working name cause confusion with mimalloc?** Search repositories, packages, domains and trademarks before release.
12. **Which public license supports the owner's goals?** Decide before accepting outside contributions.
13. **Can testing demonstrate meaningful improvement for Aesir?** Benchmark actual allocation-bound work and report null results honestly.

---

## 16. Sources and further reading

These links are research starting points; pin source versions and review their applicable license terms when using them.

- **[S1] TCMalloc platform support:** <https://google.github.io/tcmalloc/platforms.html> — includes Linux AArch64 and required compiler/library context.
- **[S2] TCMalloc architecture:** <https://google.github.io/tcmalloc/design.html> — public high-level design, caches and metadata tradeoffs.
- **[S3] Android page-size guidance:** <https://developer.android.com/guide/practices/page-sizes> — Android native code and 16 KiB page-size compatibility.
- **[S4] Microsoft mimalloc project:** <https://github.com/microsoft/mimalloc> — existing allocator with a visually similar name and useful comparison baseline.
- **[S5] Linux malloc manual:** <https://man7.org/linux/man-pages/man3/malloc.3.html> — glibc/Linux allocation behavior and portability caveats.
- **[S6] POSIX aligned allocation:** <https://man7.org/linux/man-pages/man3/posix_memalign.3p.html> — interface constraints; verify current target implementation.
- **[S7] Restartable sequences and TCMalloc:** <https://google.github.io/tcmalloc/rseq> — public explanation of Linux rseq in one allocator.
- **[S8] Google TCMalloc repository:** <https://github.com/google/tcmalloc> — official code, licensing, build and issue history. Review only within the project's documented clean-room process.
- **[S9] Modular repository:** <https://github.com/modular/modular> — obtain and pin the actual Mojo source/build documentation for the version under test.
- **[S10] Android ABI documentation:** <https://developer.android.com/ndk/guides/abis> — platform ABI details.
- **[S11] TCMalloc and gperftools distinction:** <https://google.github.io/tcmalloc/gperftools.html> — these related projects should not be assumed to have identical APIs or implementation.
- **[S12] A reported 39-bit ARM64 address-space issue in a different project:** <https://github.com/google-antigravity/antigravity-cli/issues/64> — an example of a specific report, not proof of the user's Mojo failure.

---

## 17. Initial definition of done

The first project milestone is complete when:

1. The real ARM Mojo issue has a reproducible, documented cause and the exact allocator integration boundary is mapped.
2. MimirAlloc has an original implementation with documented provenance and no unlicensed copied code.
3. The required C/C++ API subset passes its target-specific correctness and concurrency tests.
4. At least one ARM64 Linux device runs those tests, and Android support is claimed only after a separate native-device result.
5. The original Mojo failure is either demonstrably fixed with verified MimirAlloc bindings or accurately marked blocked with evidence.
6. Performance and memory results include reproducible baselines, failures and limitations.
7. Aesir integration, if claimed, is demonstrated on an actual workload with identical model/configuration and validated output.
8. No feature, speed claim, hardware capability or reliability guarantee exceeds its supporting evidence.

**Long-term ambition:** a dependable, maintainable allocator that helps bring the user's AI and development projects to more ARM devices, while standing as a useful open-source project in its own right.