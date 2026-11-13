<div align="center">

<img src="assets/mimir-well.png" alt="Mímir’s well beneath Yggdrasil, lit by circuitry and Norse knotwork" width="100%">

# ᛗ MimirAlloc

### Memory forged for the machines at the edge

**An independent, open-source memory allocator project for ARM, Android, Mojo, and resource-conscious computing.**

[Explore the roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) · [Engineering guide](docs/ENGINEERING_GUIDE.md) · [Memory systems research library](docs/research/README.md) · [Android ARM64 field notes](docs/field-notes/android-arm64-2026-11/README.md) · [Project A.E.S.I.R.](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir) · [MIT license](LICENSE)

`Research & planning`　 •　 `ARM64 first`　 •　 `Open source`　 •　 `Evidence before claims`

</div>

---

## 🚀 Production & Beyond master roadmap

**[Read the comprehensive Production & Beyond Master Roadmap (Phases 14–30)](docs/PRODUCTION_AND_BEYOND_MASTER_ROADMAP_2026-11-07.md)**

A substantial, evidence-grounded plan to take MimirAlloc from experimental **0.1.0** through correctness closure, production security, native Linux/Android qualification, performance optimization, release engineering, real Mojo/A.E.S.I.R. integration, and optional AI-aware, accelerator and distributed-memory research. Includes hundreds of uniquely identified work items, dependency gates, acceptance checklists and agent-ready execution orders. **This roadmap describes planned work; it does not claim those phases are implemented or that the current release is production-ready.**

---

## 🧭 Current engineering status | 13 November 2026

> **Z13 campaign: Z-001–Z-097 recorded on `main` (97/117 planned slices). This is substantial engineering progress, NOT a stable or production release.**
> The original campaign's **117/117 slices** are a separate, older milestone; they must not be confused with the Z13 campaign's 97/117. The published project version remains **experimental `0.1.0`**, and neither broad ABI stability nor drop-in `malloc` compatibility has been approved.
> **Review basis:** inspected the `main` repository tree (883 entries), current allocator API/core/platform and build/export contracts, the Z13 roadmap and verdict packets, recorded test/benchmark output, and the November 12–13 commit history through [Z-097 (`3894db0`)](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/commit/3894db0844f71434b48834fdcfabc7b6bc6d2a09). Figures below are **committed, machine-recorded results**, not new tests executed by this README editor. Later changes on `main` must be re-evaluated separately.

**What changed since the November 7 snapshot:** on November 12–13, a second, explicitly hardware-specific [Z13 engineering roadmap](docs/MIMIRALLOC_Z13_ROADMAP.md) advanced through ten complete nine-slice waves (Z-001–Z-090) and seven slices of Wave 11 (Z-091–Z-097). The work covers a native x86-64/4 KiB-page baseline, small/large-path optimization, concurrency, memory pressure, hardening, expanded APIs, an optional `LD_PRELOAD` experiment, real AI-program trials, trace replay and repeated performance verdicts. The old November 7 snapshot below is retained as history, not silently rewritten.

### 🧪 Host of record and verification boundary

| Area | Current evidence | Qualification |
|---|---|---|
| Z13 measurement host | ASUS ROG Flow Z13 GZ302EA, Ryzen AI MAX+ 395 (16C/32T), 128 GB shared LPDDR5X, x86-64 Linux, **4 KiB** base pages, Kubuntu/Ubuntu 26.10, kernel 7.3.0-10, glibc 2.44 | This is a **specific measured host**, not a generic x86-64 or Android certification. See [host/measurement protocol](docs/Z13_MEASUREMENT_PROTOCOL.md). |
| Native correctness | [Z13 Wave 1 baseline](docs/Z13_WAVE01_BASELINE.md) records the first hosted x86-64 portable-suite run and the first 4 KiB-page hosted run, closing former evidence debts O-01 and O-06 | The [Z13 standing gate](scripts/z13/run_gate.sh) is repeatedly recorded at **4,988 checks / 0 failed / 77 by-design skips** (for example [Z-094](docs/Z13_Z094_REAL_WORKLOAD_TRACES.md), [Z-096](tests/perf/recorded_run_z13_z096.txt)). A green standing gate does **not** certify every standalone test or all portability targets. |
| Evidence and automation | Extensive in-repo `tests/` fixtures, runners, `recorded_run_z13_*` reports, Z13 verdicts and [manual cross-build workflow](.github/workflows/cross-x86_64.yml) | Recorded runs were executed by the project's own agents on their stated machines. No GitHub Actions workflow runs were found at review time; the workflow remains **manual-only/proposed**, not CI proof. |
| Release identity | `VERSION` and versioned/static/shared artifacts remain documented for the **0.1.0 research release** | This Z13 work is not a signed/stable production rollout, Android deployment or compatibility guarantee. |

### ⚙️ Implementation now present in the tree

- **Core allocator and portable boundary:** [C implementation](src/core/mimir_core.c), [namespaced API](src/api/mimir_alloc.c), [platform/mapping and synchronization](src/platform/mimir_platform.c), and [public header](include/mimir_alloc.h) are real code, not just plans. The normal library's [export map](src/api/mimir_alloc.map) now lists **13 namespaced `mimir_*` entries**: malloc, free, calloc, realloc, aligned_alloc, posix_memalign, usable_size, stats_snapshot, trim, reallocarray, memalign, valloc and pvalloc. This is an **experimental evolving API**, and older four-symbol text elsewhere is historical.
- **Memory-path and concurrency experiments:** class analysis, TLS caching and cross-thread free handling; alternative arena/shard designs; mapping-cache retention, `mremap` growth, huge-allocation experiments, trim/pressure policies, and fragmentation/soak investigations. These features and variants have **different adoption grades**. Source presence or a successful narrow harness does not make every variant a shipped default. See [Wave 4](docs/Z13_WAVE04_VERDICT.md), [Wave 5](docs/Z13_WAVE05_VERDICT.md), [Wave 6](docs/Z13_WAVE06_VERDICT.md) and [Wave 7](docs/Z13_WAVE07_VERDICT.md).
- **Observability and hardening:** lock/unlock failure accounting, public statistics, budget enforcement, silent-corruption analysis, double-free/poison checks, and opt-in diagnostics. [Wave 8](docs/Z13_WAVE08_VERDICT.md) records a 25-case integrity classification of **19 detected, 3 silent by design, 2 expected crash and 1 valid**; it also explicitly records a separate **15-pass/10-fail** legacy probe result. These are **not** 25 clean safety passes, and remaining silent classes must not disappear behind the green standing gate.
- **Explicit contracts and interposition:** [Z-073–Z-081](docs/MIMIRALLOC_Z13_ROADMAP.md) broadened and specified the namespaced API; [Z-082](docs/Z13_Z082_INTERPOSING_VARIANT.md) added a **separate opt-in** shared library (`build/libmimiralloc_interpose.so`) exposing standard allocation names via `LD_PRELOAD`. Its [variant export map](src/api/mimir_interpose.map) lists **20 exports (10 namespaced + 10 standard)**; the default [namespaced export map](src/api/mimir_alloc.map) is distinct and now has 13 namespaced entries after Z-083. Neither library is a blanket glibc replacement.
- **Build policy changed again:** [Z-096's final flag trial](tests/perf/recorded_run_z13_z096.txt) adopted **`-O3` without `-flto` or `-march=native`** on the measured Z13 core. The [Makefile](Makefile) reflects that decision. Its clean-run improvement vs an otherwise identical `-O2` build was about **12.4% on `small_churn`** and **6.9–8.6% on `mixed_batch`**, with no accepted regression on the other two synthetic workloads. These are *intra-MimirAlloc build-flag* wins, **not claims of beating glibc**.

### 📈 Dated wave-by-wave progress and measured verdicts

| Slice / evidence | Accomplishment or finding | Honest standing |
|---|---|---|
| **Z-001–Z-009** · [Wave 1](docs/Z13_WAVE01_BASELINE.md) | Hardware/protocol baseline, native x86-64 and 4 KiB-page correctness evidence, latency/counter/RSS/energy comparisons | Initial same-host glibc comparison **lost all four** synthetic workloads (small 10.4×, mixed batch 40.2×, large 2.4×, mixed lifecycle 67.2× slower). These are **starting measurements**, not current final numbers. |
| **Z-010–Z-027** · [autopsy](docs/Z13_WAVE03_AUTOPSY.md) | CPU/cache, syscall, alignment, lock contention and small/large allocation path anatomy established the expensive operations to attack | Diagnostics and decisions grounded; not a universal speed win. |
| **Z-028–Z-045** · [Wave 4](docs/Z13_WAVE04_VERDICT.md) / [Wave 5](docs/Z13_WAVE05_VERDICT.md) | TLS-cache prototype and remote-free handling; arena, size-class, metadata, sharding, scaling and fork/thread experiments | Selected paths improved, other variants rejected or flagged for remeasurement; multithread scaling remains workload-sensitive. |
| **Z-046–Z-063** · [Wave 6](docs/Z13_WAVE06_VERDICT.md) / [Wave 7](docs/Z13_WAVE07_VERDICT.md) | Mapping cache, huge/mapped allocations, `mremap`, retention, fragmentation and repeated pressure/soak studies | Wave 6 measured a **large_batch WIN** (MimirAlloc 63.48 ns/op vs glibc 658.29 ns/op in that window; glibc's drift-rejected median means the **factor is indicative**). **Mixed lifecycle remained ~71.9× slower** in that same-window verdict. Do not extrapolate one workload's win. |
| **Z-064–Z-081** · [Wave 8](docs/Z13_WAVE08_VERDICT.md) / [contracts](docs/Z13_Z078_ZERO_SIZE_ERRNO.md) | Instrumented failure/silent-corruption paths, diagnostics choices, aligned/usable-size/stats/trim/reallocarray APIs, budget enforcement, version/install/ABI audit | Work is implemented and tested in named scopes; **some undetected error classes and portability debts remain**. |
| **Z-082–Z-090** · [Wave 10 final verdict](docs/Z13_WAVE10_VERDICT.md) | Optional interposer, command-line/desktop smoke tests, Python/Ollama real-program comparison, read-only A.E.S.I.R. trial, failure and rollback drills | **Correctness passed the reported tested cases**, but compatibility is **not universal** and interposer throughput often **loses**. Wave 10 is final as a work wave, not a general deployment approval. |
| **Z-091–Z-093** · [batching verdict](docs/Z13_Z093_BATCH_VERDICT.md) | Re-examined caller-side batching; implemented and contract-tested an **internal-only** batch prototype; measured prototype and loop | At burst512, ordinary MimirAlloc batching was **33.8× (64 B) and 120.2× (1024 B) slower than glibc** on the measured shapes. The internal prototype **tied** the existing deep-burst loop, so batching is **not adopted as a performance answer**. The expensive live-set metadata walk remains. |
| **Z-094–Z-095** · [trace packet](docs/Z13_Z094_REAL_WORKLOAD_TRACES.md) / [tuning run](tests/perf/recorded_run_z13_z095.txt) | Captured Python-import, Git-churn and Ollama traces; replayed **2,987,619 operations** against both allocator legs with matching checksums, **0 replay failures and 0 dirty callocs**. Tested class/cache tuning variants | Replay is **correctness evidence, not a throughput benchmark**. Z-095 **adopted no tuning candidate**: improvements in some traces were rejected for regressions in the synthetic oracle. Z-095 also explicitly owes revalidation against Z-094's subsequently banked canonical traces. |
| **Z-096–Z-097** · [flag matrix](tests/perf/recorded_run_z13_z096.txt) / [short-process cost](tests/perf/recorded_run_z13_z097.txt) | Final eight-variant compiler-flag matrix chose `-O3`; fork+exec storms measured the interposer's load/init and allocation tax | `LD_PRELOAD` **lost every measured short-process shape** despite **19,600/19,600 children exiting successfully**: no-allocation spawn **+25.154 µs** (+17%) and 1,000 small alloc/free cycles **+202.966 µs** (+130%, **2.30×** process time) versus glibc, per process in the run of record. |

### 🤖 Real programs and the A.E.S.I.R. boundary

- [Z-084/Z-090](docs/Z13_WAVE10_VERDICT.md): the 13-subject CLI smoke battery passed; a Python import/churn workload returned the expected result but took **1.3876 s under preload vs 0.2772 s plain (5.01× slower)**. A short Ollama run produced matching output, while its model-load time was **~12.1–12.9 s vs 0.542 s plain (~22–24× slower)**. These are the interposer experiment's measured costs, not permission to preload performance-sensitive applications.
- [Z-087](docs/Z13_Z087_AESIR_BOUNDARY_TRIAL.md): a real source-built Mojo A.E.S.I.R. CPU engine ran a **Meta-Llama-3.1-8B-Instruct Q4_K_M** fixture with **identical 32-token output in four plain/preloaded legs**. Observed preload wall time was **~5.1% slower**, but the shared host was contended (load average up to 4.43), so **this is not a quiet-machine performance baseline**. A.E.S.I.R. repository files were left unchanged. Three pre-existing A.E.S.I.R.-side binary/stub/fixture blockers were documented, not repaired by MimirAlloc. This is a genuine **boundary trial**, not proof that A.E.S.I.R.'s main production allocation path has adopted MimirAlloc.
- [Z-088/Z-089](docs/Z13_Z089_ROLLBACK_NEVER_PRELOAD.md): isolated failure tests and a one-variable rollback drill passed, but some failures can still be **silent** with the diagnostics gate disabled. **Never globally export `LD_PRELOAD` or inject it into systemd, the live display/session, Git banking/repair tooling, long-lived shared services, or other programs on the documented never-preload list.** Use contained per-command trials with a glibc control and an easy rollback only.

### ⚠️ Blocking limitations and release reality

1. **Interposition semantics differ from glibc.** Under [decision D-013](docs/Z13_Z078_ZERO_SIZE_ERRNO.md), zero-size allocation returns NULL and `realloc(p, 0)` leaves `p` owned and live. [Z-090](docs/Z13_WAVE10_VERDICT.md) documents a concrete **anchored `regcomp`/`grep` compatibility failure** (T-084-1). This is a deliberate contract divergence, not a generic safe substitute for libc. [Z-089](docs/Z13_Z089_ROLLBACK_NEVER_PRELOAD.md) carries the safety boundary.
2. **Performance is uneven.** The mapping-heavy large-batch win coexists with very costly mixed lifecycles, deep-burst registry walks, and large Python/Ollama preload penalties. Compiler optimization wins are **not overall glibc wins**. A final apples-to-apples comparison against tuned glibc and consistent quiet-host runs is still owed.
3. **Correctness evidence has a defined envelope.** The 4,988/0/77 standing gate and many standalone runs are valuable, but intentional skips, silent classes, environment-specific ABI/locking behaviours, and the need for longer stress, sanitizers and security grading remain. **Do not equate a green local test with production readiness.**
4. **Original cross-platform mission remains incomplete.** Native Android/bionic and Termux runs on target devices, the original exact Mojo-on-tablet failure reproduction, supported-device performance, and an approved end-to-end A.E.S.I.R. integration/release claim remain **unverified/owed** on the available evidence. The Z13 Z-087 source-built CPU trial is progress, not a replacement for those proof obligations.
5. **Docs are mixed-age.** The older [Phase 13 exit packet](docs/PHASE_13_EXIT_EVIDENCE.md), initial four-symbol descriptions, historical README text below, and some internal code comments predate Z13's API changes. Prefer this dated update **together with** newer Z13 tests/verdicts and the actual export maps when assessing today's tree; do not quietly erase historical evidence.

### 🧭 Immediate next gates (planned, not completed at this snapshot)

- **Z-098:** measure a tuned-glibc comparator under identical conditions; **Z-099:** file the complete Wave 11 batching/traces verdict, including any Z-094/Z-095 trace reconciliation.
- **Z-100–Z-108:** sustained soak, memory retention/budget chaos, hardening and sanitizer coverage, release engineering, documentation truth pass, final performance tables and a formally reviewed campaign verdict. The exact acceptance conditions are in the [Z13 roadmap](docs/MIMIRALLOC_Z13_ROADMAP.md).
- **Z-109–Z-117:** verdict-driven reserve and final closure; not presumed done simply because earlier waves banked.

**Primary evidence:** [Z13 campaign plan](docs/MIMIRALLOC_Z13_ROADMAP.md) · [baseline](docs/Z13_WAVE01_BASELINE.md) · [Wave 6](docs/Z13_WAVE06_VERDICT.md) · [Wave 8](docs/Z13_WAVE08_VERDICT.md) · [Wave 10](docs/Z13_WAVE10_VERDICT.md) · [batching](docs/Z13_Z093_BATCH_VERDICT.md) · [traces](docs/Z13_Z094_REAL_WORKLOAD_TRACES.md) · [flag trial](tests/perf/recorded_run_z13_z096.txt) · [fork/exec trial](tests/perf/recorded_run_z13_z097.txt) · [API](include/mimir_alloc.h) · [Makefile](Makefile).

> **Historical-text notice:** the November 7 report and all earlier prose remain below, unchanged, for an audit trail. The Z13 project roadmap's instruction to its agents not to edit this README is distinct from this **explicit owner-requested, additive documentation review**; this edit does not bank a Z13 slice or certify an unverified result.

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
