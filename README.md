<div align="center">

<img src="assets/mimir-well.png" alt="Mímir’s well beneath Yggdrasil, lit by circuitry and Norse knotwork" width="100%">

# ᛗ MimirAlloc

### Memory forged for the machines at the edge

**An independent, open-source memory allocator project for ARM, Android, Mojo, and resource-conscious computing.**

[Explore the roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) · [Engineering guide](docs/ENGINEERING_GUIDE.md) · [Memory systems research library](docs/research/README.md) · [Android ARM64 field notes](docs/field-notes/android-arm64-2026-11/README.md) · [Project A.E.S.I.R.](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir) · [MIT license](LICENSE)

`Research & planning`　 •　 `ARM64 first`　 •　 `Open source`　 •　 `Evidence before claims`

</div>

---

## 🧭 Current engineering status | 7 November 2026

> **Research release 0.1.0 exists. Stable/production release: NOT APPROVED.**
> The project has progressed substantially beyond the documentation-only launch description preserved later in this README. The source code, public header, build targets, local test evidence, benchmarking, and versioned release procedure now exist. **Phase 13 is complete as a wave of work, but its stable-release exit is PARTIAL / NOT MET.** This is a dated evidence snapshot based on the [M-009 Phase 13 exit packet](docs/PHASE_13_EXIT_EVIDENCE.md) and repository state at [commit 776ca8c](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/commit/776ca8cc4424cb26375145622bf23b9ba48330ac).

| Area | Status on 7 November 2026 | Evidence / important boundary |
|---|---|---|
| Allocator implementation | **Implemented; experimental** | Native C allocator in [src/core](src/core), namespaced API in [src/api](src/api), platform layer in [src/platform](src/platform), and [public header](include/mimir_alloc.h). |
| Public interface | **Four exported namespaced functions** | mimir_malloc, mimir_free, mimir_calloc, mimir_realloc. No general-purpose malloc/free interposition or ABI-stability guarantee. |
| ARM64 Linux correctness | **VERIFIED-RUN, narrow scope** | Raspberry Pi running Debian 12 / glibc 2.36 / AArch64 / 16 KiB pages. Latest recorded full suite: **4,590 checks; 0 failed**. This does not mean production-ready. |
| 0.1.0 research release | **Versioned build/release procedure implemented** | VERSION, versioned shared library, SONAME libmimiralloc.so.0, static archive, install target, checksum file, SBOM and [reproducible-build instructions](docs/REPRODUCIBLE_BUILD.md). Artifacts were staged and verified for the documented experiment; prebuilt binaries and tarballs are **not committed** to this repository. |
| Fuzzing / repeated load | **Recorded evidence with caveats** | Seeded fuzz harness logged **168,384 checks / 0 failed**, and M-009 reproduced PASS. Ten-cycle soak recorded **159,476 operations**, flat reported RSS (1,392 kB) and six passing dlopen/dlclose cycles. Fuzz specification mismatch remains PARTIAL. |
| Performance | **Measured; not competitive yet** | [Phase 11](docs/PHASE_11_EXIT_EVIDENCE.md) reports MimirAlloc **roughly 5×–65× slower than glibc on the measured workloads**. No speedup claim is made. |
| Android / Termux / SM-X400 | **Not verified** | Device field notes and plans exist, but no MimirAlloc NDK/device run or Termux execution has been recorded. |
| Mojo and Project A.E.S.I.R. | **Investigation and boundary demonstrations; end-to-end goal unmet** | The original tablet Mojo failure has not been literally reproduced/fixed, and there is no evidence that the A.E.S.I.R. engine is actually allocating through MimirAlloc. The required Qwen3 comparison is owed. |
| Phase 13 exit criteria | **11 MET / 6 PARTIAL / 3 OWED / 0 UNMET (20 total)** | The [final exit review](docs/PHASE_13_EXIT_EVIDENCE.md) explicitly rejects a stable-release claim and carries **14 outstanding item groups** forward. |

### ⚒️ What has actually landed

- **Real allocator and build structure:** namespaced memory operations, core/platform components, a public C header, Makefile targets, linkage checks, ABI inspection, multithread and failure-path tests. Implemented behavior is bounded by the [contracts](docs/contracts.md) and header rather than assumptions about drop-in libc compatibility.
- **A release identity, not a maturity claim:** version **0.1.0** and SONAME **libmimiralloc.so.0** are implemented. [M-007](docs/PHASE_13_VERSIONING_AND_RELEASE_M007.md) describes the experimental versioned shared/static artifacts, install decisions, checksum verification, and two clean byte-identical builds **on the same Raspberry Pi toolchain**. Cross-machine reproducibility, release signing, and stable ABI are not claimed.
- **A substantial verification trail:** [recorded M-009 test output](tests/abi/recorded_run_m009.txt), [fuzzing evidence](docs/PHASE_13_FUZZING_M004.md), [soak evidence](docs/PHASE_13_SOAK_M005.md), [target matrix](docs/SUPPORTED_TARGETS.md), [SBOM](docs/SBOM.md), [security instructions](SECURITY.md), and [known limitations](docs/LIMITATIONS_AND_KNOWN_DEFECTS.md). The numbers above report committed *test logs*, not a fresh independent execution by this README updater.
- **A guarded boundary to A.E.S.I.R.:** Phase 12 documents tokenizer, GGUF, KV-cache, prefill/decode and teardown research or harness measurements and explicit-adoption wiring. These are **not** evidence of a fully integrated A.E.S.I.R. inference run or a Qwen3-1.7B Q4_K_M comparison. See [Phase 12 exit](docs/PHASE_12_EXIT_EVIDENCE.md).
- **Security and portability work:** code review, warning gates, target audit, fuzzing, repeated-use probes, disclosure paperwork and a public defect register were added during Phase 13. Completing a work wave is not equivalent to passing every acceptance gate.

### 🗓️ Most recent project events (7 November 2026)

1. **M-004 / M-005:** Added seeded model-based fuzzing and repeated-cycle soak tests. Recorded PASS results are retained, alongside deviations in test depth or the later-updated assignment.
2. **M-006:** Refreshed [SUPPORTED_TARGETS.md](docs/SUPPORTED_TARGETS.md) against actual runs. Only the 16 KiB-page AArch64/glibc Raspberry Pi target is **VERIFIED-RUN**. x86-64 has a **PARTIAL-RUN** freestanding/emulated probe, not a hosted suite; musl is **BLOCKED** by missing tooling; Android and Termux runs are **OWED**.
3. **[M-007](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/commit/6e066cec073979904fbe2acddf0045ad1844a5b5):** Landed VERSION 0.1.0, SONAME/versioned library and install rules, checksummed artifact manifest and reproducible-build instructions for the experimental release.
4. **[M-008](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/commit/83125d680004b449370e5619bf5287141375395b):** Added SBOM, supported-platform disclosure, security-reporting instructions and a limitations/defects register. Some M-008 document judgments predate M-006/M-007 being integrated and need reconciliation.
5. **[M-009](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/commit/776ca8cc4424cb26375145622bf23b9ba48330ac):** Closed Wave 13 with a fresh **4,590/0** suite run, a four-symbol export check and fuzz PASS. The separate legacy **test-g008** check remains **214 checks / 1 failure** from a known stale expectation after the H-001 unload fix. M-009 graded stable-release exit **NOT MET**, recorded **6 PARTIAL and 3 OWED** criteria, and listed the remaining work without hiding the failure.

### 🧪 Present verification boundary

| Target | Evidence grade | What is still missing |
|---|---|---|
| AArch64 Linux + glibc, **16 KiB** pages, Raspberry Pi | **VERIFIED-RUN** | Broader target coverage and production hardening |
| AArch64 Linux + glibc, **4 KiB** pages | **OWED; design analogy only** | Full native run on a 4 KiB-page host |
| x86-64 Linux + glibc | **PARTIAL-RUN** | Hosted allocator suite on x86-64; existing probe is not equivalent |
| AArch64 Linux + musl | **BLOCKED** | Appropriate musl toolchain/sysroot and subsequent tests |
| Android/bionic, Termux, proot, SM-X400 tablet | **OWED / analogy only** | Distinct physical-device builds, runs, logs, pressure and responsiveness results |
| A.E.S.I.R. + Qwen3-1.7B Q4_K_M | **OWED** | Available model, runnable aarch64 A.E.S.I.R. engine, genuine allocator-use proof and controlled comparison |

For exact per-platform distinctions, **[SUPPORTED_TARGETS.md](docs/SUPPORTED_TARGETS.md) is the most recently reconciled authority**. The older [SUPPORTED_PLATFORMS.md](docs/SUPPORTED_PLATFORMS.md) still has grades that predate M-006 and is due for refresh.

### 🔧 Building and reproducing the *experimental* state

These are repository targets documented for the tested Raspberry Pi environment, **not** universal installation promises. Use the [engineering evidence](docs/PHASE_13_EXIT_EVIDENCE.md) and [reproducible-build guide](docs/REPRODUCIBLE_BUILD.md) to verify prerequisites and inspect failures.

~~~bash
make clean && make test          # M-009 recorded: 4590 checks, 0 failed
make lib                         # namespaced shared library
make test-fuzz-m004              # recorded seeded fuzz harness
make lib-versioned static-lib    # research-release library artifacts
~~~

The standalone **make test-g008** probe is *not green*: it intentionally remains a recorded **214/1** evidence-policy regression until its stale check is reconciled. Do not treat a passing aggregate suite as proof that every standalone or target-specific gate passes. No repository-hosted binary package or blanket drop-in replacement is promised.

### 🛡️ Blocking defects and next gates

1. **Fix or formally rule on the lock/unlock reporting gap** (INC-M002-01/02, Invariant 9). The recorded safety path does not return overlapping allocations, but certain synchronization failures are not observably reported. This is a named obstacle to a production claim.
2. **Reconcile old tests and documentation:** decide how to repoint the stale g008 check; reconcile the Phase 13 public defects/platform documents with the later M-006/M-007 changes; refresh the obsolete baseline inventory and historical README/engineering-guide claims. The original historical writing is retained below in this additive update.
3. **Close platform evidence gaps:** perform the full hosted x86-64 suite; test native Android/bionic and Termux separately on real hardware; capture a native **4 KiB-page** run. Any package installation explicitly reserved for the project owner requires that separate decision.
4. **Complete the original mission's proof:** recover the exact Mojo-on-tablet failure and retry it; prove A.E.S.I.R. *actually* uses MimirAlloc; make the Qwen3 model and runnable engine available, then execute Phase 12's gated apples-to-apples comparison.
5. **Advance performance and release quality:** profile and improve the measured glibc deficit without weakening correctness, address extended warning/fuzz-spec gaps, enable the private security-reporting channel if approved, and regrade all acceptance criteria before any stability claim.

**Source of truth:** [Phase 13 exit packet](docs/PHASE_13_EXIT_EVIDENCE.md) · [Phase 13 issue/limitation register](docs/LIMITATIONS_AND_KNOWN_DEFECTS.md) · [supported-target matrix](docs/SUPPORTED_TARGETS.md) · [reproducible release](docs/REPRODUCIBLE_BUILD.md) · [full roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md).

> **Historical-text notice:** Everything after this dated section remains preserved from the project's earlier planning-era README. Older claims that the repository has no allocator, build commands, tests or release are **superseded** by the evidence-backed update above, but were deliberately not removed or rewritten to honor the additive-edit request.

---


> **Project status — the forge is just being lit.** This repository currently contains documentation, artwork, and an MIT license. The allocator, installation package, benchmarks, Android support, and Mojo integration are **planned**, not implemented or verified yet. There is no build or install command for MimirAlloc at this stage.

---

![file_000000003640822fa8b8a9b041610fd7.png](file_000000003640822fa8b8a9b041610fd7.png)

---

## 🌌 The idea

MimirAlloc is a proposed clean-room memory allocator: a piece of foundational software that manages the memory a program requests and releases. The goal is to build a small, dependable foundation for devices where memory, power, and responsiveness matter—from ARM Linux machines to Android tablets and local AI systems.

The project began with an obstacle encountered while exploring how to build Mojo for an ARM-based tablet. A tcmalloc-related issue inspired the idea of an independently engineered allocator that could help open another path for Mojo and [Project A.E.S.I.R.](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir)—and potentially be useful to other projects, too.

The exact build failure and replacement point still need to be reproduced and verified. Google’s public TCMalloc documentation lists Linux AArch64 support; the question here is how the **specific build, OS, ABI, and device** interact, not whether ARM exists upstream. See the [research roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) for sources and the investigation plan.

<img src="assets/runic-forge.png" alt="A Nordic forge with a glowing processor on an anvil and luminous circuit-like Norse knotwork" width="100%">

## 🧠 What does a memory allocator do?

When software needs temporary space for objects, it requests memory. An allocator finds available space, tracks it, and makes freed space available for reuse.

A good allocator must do more than make allocations quickly: it needs correct ownership, reliable handling of errors, safe behavior across threads, and reasonable memory use. On a tablet, it should also consider the other apps sharing the device.

**MimirAlloc’s mission:** explore those challenges with an original design, ARM-focused testing, and transparent results.

## ⚒️ What we want to build

These are **goals**, not current features:

| Focus | Planned work |
|---|---|
| **ARM64 foundations** | Build and test a portable allocator on real ARM64 Linux hardware. |
| **Android and Termux** | Investigate native Android/bionic support, device constraints, and different system page sizes. |
| **Mojo compatibility** | Identify the actual build blocker, required interfaces, and a demonstrable integration path. |
| **A.E.S.I.R. and local AI** | Measure allocator behavior in real model-loading and inference workloads. |
| **Correctness and resilience** | Test allocation contracts, alignment, concurrency, failures, and memory pressure. |
| **Measured performance** | Benchmark latency, throughput, retained memory, and device responsiveness against documented baselines. |

A memory allocator is one part of a larger system. It does not by itself provide GPU or NPU inference, and replacing an allocator does not automatically make a compiler or model run on Android.

## 🗺️ The journey ahead

1. **Find the actual blocker.** Capture the exact Mojo/ARM failure, toolchain versions, linker details, and reproducible commands.
2. **Define the contract.** Document the required interfaces and ownership rules before implementation.
3. **Build a small, correct foundation.** Start with a namespaced allocator, checked arithmetic, and meaningful tests.
4. **Prove the ARM and Android paths.** Test native environments separately from Linux running under Android’s `proot`.
5. **Demonstrate integration.** Verify that the intended Mojo or A.E.S.I.R. process actually uses MimirAlloc.
6. **Refine and measure.** Improve performance and memory use with benchmarks, stress tests, and physical-device evidence.

The full [research and development roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) expands these into detailed engineering phases, acceptance gates, test matrices, and source references.

## 🚀 Getting started

### Explore the project

You can inspect the documentation now:

```bash
git clone https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc.git
cd RuneForgeAI-MimirAlloc
```

- Read the [research and development roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md).
- Read the [engineering guide](docs/ENGINEERING_GUIDE.md) for the first investigations and contribution workflow.
- Explore the [memory systems research library](docs/research/README.md) for allocator design, ARM and RISC-V, edge AI, accelerators, CXL, distributed memory, experiments, and primary sources.
- Discover the [neural computer research collection](docs/research/llm_computers/README.md), including guest allocators and distributed computing proposals.
- Explore [intelligent and federated memory research](docs/research/intelligent_memory/README.md), covering optional small-model policies, RAG, KV caches and multi-device resources.
- Read the [integrated AI and memory research synthesis](docs/research/ai_memory_synthesis/README.md) for cross-domain designs, primary sources, experiments and implementation work packages.
- Check the repository’s [issues](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/issues) for current work.

**There is no allocator to install or run yet.** Build, integration, and benchmark instructions will be added when the corresponding implementation exists and has been tested.

### Help investigate

If you have a reproducible Mojo/ARM issue that may involve an allocator:

1. Record the exact command and the first relevant error.
2. Note the Mojo revision, operating system, architecture, compiler, linker, and whether the environment is native Android, Termux, `proot`, or Linux.
3. Remove passwords, tokens, personal data, and unnecessary device identifiers.
4. Open an issue with the minimal reproduction and relevant logs.

A reproducible failure is more useful than assuming every ARM or Android environment has the same cause.

## 🛡️ Engineering principles

- **Independent design:** define behavior and document source provenance; do not present copied implementation as clean-room work.
- **Correctness first:** check overflow, alignment, ownership, concurrency, and allocation failure.
- **Physical evidence:** identify what ran on a real device and what was only cross-compiled or simulated.
- **Mobile awareness:** watch memory pressure, thermal behavior, and responsiveness to other apps.
- **Honest results:** publish limitations and unsuccessful experiments alongside successes.
- **Open collaboration:** make the work understandable and useful to developers and curious newcomers.

## 📜 The story so far

MimirAlloc grew out of Volmarr’s work on **Project A.E.S.I.R.**, an independent local AI inference engine, and his effort to explore bringing that work to an Android tablet. Investigating Mojo on ARM led to a larger idea: create a separate open-source allocator project that could research the build obstacle while pursuing reliable, efficient memory management for ARM and edge devices.

The project is beginning with research and a staged roadmap. Its future implementation and integration claims will be earned through real tests, reproducible builds, and documented results.

## 🤝 Join the forge

Thoughtful contributions are welcome: reproductions, documentation improvements, ABI research, test design, ARM/Android expertise, and eventually code. Start with the [engineering guide](docs/ENGINEERING_GUIDE.md), then open an issue or pull request with the context and evidence behind your proposed change.

Please keep examples reproducible, avoid committing credentials or private device data, and distinguish tested results from ideas.

## 📜 License

MimirAlloc is released under the [MIT License](LICENSE).

<div align="center">

**Forged in curiosity. Grounded in evidence. Built in the open.**

ᚠ ᚢ ᚦ ᚨ ᚱ ᚲ

</div>
