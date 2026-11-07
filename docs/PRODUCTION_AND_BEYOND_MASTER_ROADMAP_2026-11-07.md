# ᛗ MimirAlloc: Production & Beyond Master Roadmap
## From the 0.1.0 research allocator to a trustworthy edge-memory platform

**Initial baseline:** 7 November 2026  
**Baseline tree:** d9f1371f2a3824257e11cc206f709874c1de5e16 (README status amendment), with M-009 engineering evidence at 776ca8cc4424cb26375145622bf23b9ba48330ac.  
**Document status:** PROPOSED FOR EXECUTION; a technical plan, not evidence that future work is implemented.  
**Scope:** independent C allocator, Linux ARM64, x86-64, Android ARM64, Mojo and Project A.E.S.I.R., future embedded/AI and heterogeneous-memory research.  
**Relationship to history:** additive successor to [the original phases 0-13](MimirAlloc_Research_and_Development_Roadmap.md). Do not rewrite, erase or retroactively promote historical exit verdicts.

> **North star.** Make memory allocation correct when things go wrong, predictable when resources are scarce, competitive when speed matters, and comprehensible to the human (and agent) engineering team. Only then extend it into deliberate, safe experiments involving AI, accelerators, distributed systems and emerging hardware.
>
> **Non-negotiable:** this roadmap does not declare a product stable. Release 0.1.0 is a **research release**. The Phase 13 stable exit is **PARTIAL / NOT MET**. Every work item below must earn its status through a merged code change, independently reproducible run and signed-off evidence packet.

---

## Table of contents

1. [Mandate, definitions and rules](#1-mandate-definitions-and-rules)
2. [Baseline and evidence debt](#2-baseline-and-evidence-debt)
3. [Release trains and gates](#3-release-trains-and-gates)
4. [Execution system, autonomy and resource constraints](#4-execution-system-autonomy-and-resource-constraints)
5. [Phase 14: repair the evidence and defect register](#5-phase-14-repair-the-evidence-and-defect-register)
6. [Phase 15: correctness, API contracts and allocator safety](#6-phase-15-correctness-api-contracts-and-allocator-safety)
7. [Phase 16: platform, ABI and build verification](#7-phase-16-platform-abi-and-build-verification)
8. [Phase 17: security hardening and adversarial verification](#8-phase-17-security-hardening-and-adversarial-verification)
9. [Phase 18: benchmark science and performance engineering](#9-phase-18-benchmark-science-and-performance-engineering)
10. [Phase 19: production operations, release and supply chain](#10-phase-19-production-operations-release-and-supply-chain)
11. [Phase 20: genuine Mojo and A.E.S.I.R. integration](#11-phase-20-genuine-mojo-and-aesir-integration)
12. [Phase 21: Android, tablets, edge and embedded deployments](#12-phase-21-android-tablets-edge-and-embedded-deployments)
13. [Phase 22: predictable memory budgets and adaptive pressure](#13-phase-22-predictable-memory-budgets-and-adaptive-pressure)
14. [Phase 23: concurrency, NUMA and next-generation backends](#14-phase-23-concurrency-numa-and-next-generation-backends)
15. [Phase 24: observability, profiling and application interfaces](#15-phase-24-observability-profiling-and-application-interfaces)
16. [Phase 25: AI-aware allocation and inference specialization](#16-phase-25-ai-aware-allocation-and-inference-specialization)
17. [Phase 26: heterogeneous and accelerator memory research](#17-phase-26-heterogeneous-and-accelerator-memory-research)
18. [Phase 27: federated and distributed memory research](#18-phase-27-federated-and-distributed-memory-research)
19. [Phase 28: self-optimization with bounded AI agents](#19-phase-28-self-optimization-with-bounded-ai-agents)
20. [Phase 29: ecosystem, bindings and packages](#20-phase-29-ecosystem-bindings-and-packages)
21. [Phase 30: sustaining engineering and long-lived releases](#21-phase-30-sustaining-engineering-and-long-lived-releases)
22. [Cross-cutting test and measurement matrices](#22-cross-cutting-test-and-measurement-matrices)
23. [Release checklists and mechanical evidence schemas](#23-release-checklists-and-mechanical-evidence-schemas)
24. [Critical path, wave allocations and priority queue](#24-critical-path-wave-allocations-and-priority-queue)
25. [Threat model, risk register and stop conditions](#25-threat-model-risk-register-and-stop-conditions)
26. [Engineering research bibliography](#26-engineering-research-bibliography)
27. [Next work orders](#27-next-work-orders)

---

## 1. Mandate, definitions and rules

### 1.1 Why a second roadmap exists

The original roadmap correctly created a research-to-implementation program. Phases 0–13 have now produced a substantial C allocator, public namespaced ABI, recorded tests, phased experiment packets, build machinery and versioned 0.1.0 research artifacts. However, finished waves are not the same as passed exits. The next program must convert a **single-device experimental implementation** into a **well-specified, secure, maintainable, reproducibly validated product**, while retaining the experimental freedoms that made MimirAlloc possible.

Three layers of aspiration are intentionally separate:

- **Level A: defensible production allocator.** Explicit adoption on supported Linux targets; correct ownership, corruption containment within realistic bounds, reproducible evidence, predictable behavior under memory pressure, transparent performance and maintained releases.
- **Level B: dependable deployment portfolio.** Additional architecture/libc/page-size targets, Android and Termux, opt-in C/C++ interoperability, verified Mojo/A.E.S.I.R. consumers, developer tooling and secure distribution.
- **Level C: research platform beyond ordinary allocation.** NUMA, accelerators, AI workload hints, policy-learning experiments, remote tiers and agent-directed experiments. Each remains opt-in, separately verified and quarantined from Level A until it passes equivalent correctness/security gates.

### 1.2 Vocabulary: do not inflate grades

- **PROPOSED:** described here only; no implementation or proof.
- **IN PROGRESS:** an assigned task or branch exists; not necessarily merged.
- **MERGED:** code/docs present on main; not proof of function.
- **BUILD-ONLY:** compiler/linker accepted a target; no executable proof.
- **EMULATED-RUN:** a measured emulation or simulator run; never described as physical-hardware verification.
- **VERIFIED-RUN:** named commit, exact environment and reproducible execution logs prove a bounded claim.
- **PARTIAL:** only a documented subset of the acceptance criteria passed.
- **OWED:** a required demonstration has not occurred.
- **BLOCKED:** the missing prerequisite and its owner are documented.
- **UNSUPPORTED:** explicit policy prohibits or does not implement a behavior.
- **PRODUCTION-QUALIFIED:** all predeclared *scope-specific* gates pass, signed off with evidence; not a universal compatibility or safety guarantee.
- **EXPERIMENTAL:** API stability, workload applicability or safety evidence is not yet sufficient for production use.

A zero-failure test run is not proof of race freedom, memory safety, universal portability or production maturity. A diagram is not a runnable integration. A compiler-accepted object is not a linked Android app. A benchmark run is not a performance victory.

### 1.3 Invariants every phase inherits

1. Public namespaced allocation ownership must be unambiguous; never silently pair a MimirAlloc allocation with another allocator's free.
2. No overlapping live user allocations, double ownership, unchecked length wrap, out-of-range metadata dereference or mutable metadata race in *supported* operations.
3. Failure paths must preserve original pointers and object contents where specified, and must never leak ownership silently.
4. Bootstrap and diagnostics must not recursively allocate through the allocator unless a bounded and separately proven bootstrap protocol exists.
5. The shared library must continue to export exactly the agreed functions for the 0.x ABI; any expanded API must use an explicit, reviewed ABI/version decision and updated symbol/export proof.
6. Global malloc/free interposition is **not** enabled by default. An opt-in interposition experiment cannot be labelled safe before full mixed-allocator, C++ and loader testing.
7. Debug/hardened/production builds remain distinct and independently tested; results from one flavor cannot substitute for another.
8. No unverifiable throughput, energy, memory, latency, device, AI model or remote-memory claims.
9. A research change that improves one workload but weakens correctness or stability is rolled back or confined to a research branch.
10. Security flaws and unsupported behavior must remain visible; security reporting must work before a mature release is offered.
11. Keep the MIT license and clean-room provenance process. Public allocator documentation is for **conceptual comparisons and independently designed experiments**, not source copying.
12. No automatic changes to a living system's system allocator or the Pi service environment. All adoption is explicit and reversible.

### 1.4 Distinguish four different deliverables

| Deliverable | What it means | What it explicitly does not mean |
|---|---|---|
| MimirAlloc core | Host-side memory allocator and namespaced C surface | A virtual machine, GPU driver or all-purpose distributed RAM |
| Compatibility layer | Explicit adapter to C/C++/Mojo consumers | Automatically replaces hidden/static/system allocations |
| AI workload layer | Schedules and accounts for memory classes controlled by that application | Automatically controls GPU/NPU driver memory or model internals |
| Research memory fabric | Coordinates leases/tiering across devices, using explicit transfers | Transparent coherent pointer access across incompatible devices |

### 1.5 Success metrics: decisions first, numbers second

Establish **gated, quantified targets** in Phase 18 after a clean benchmark baseline on each machine. Suggested targets below are planning objectives, not results:

- Correctness: zero unexplained failures in required suites, coverage fuzzers, differential property tests, fault injection, concurrency stress and documented soak durations.
- Reliability: no reproducible untriaged allocator crash, deadlock or corruption under supported operations; declared known limitations must be explicit, not hidden.
- Tail latency: for explicitly named workloads, p95/p99 bounded against a predeclared reference; never mask catastrophic tails with p50.
- Memory: accounted live, retained and mapped bytes; bound fragmentation, metadata and peak resident overhead by tested workload/target class.
- Throughput: eventually narrow the present 5×–65× glibc deficit; any release target must state which benchmark families are competitive and which are not. There is **no blanket requirement to beat glibc everywhere**.
- Energy: report power and energy-per-operation only when measurement attribution and ambient controls are credible; otherwise mark OWED.
- Distribution: every released artifact attributable to a reviewable commit, toolchain, SBOM, integrity manifest, and independent consumer test.
- Maintenance: regression turnaround, security-response process and compatibility guarantees are explicit obligations rather than branding.

---

## 2. Baseline and evidence debt

### 2.1 Banked factual starting point

The [final Wave 13 exit packet](PHASE_13_EXIT_EVIDENCE.md) is the governing baseline, not any older planning-era README language.

| Baseline fact | Evidence | Roadmap consequence |
|---|---|---|
| 0.1.0 research release; Phase 13 stable exit PARTIAL / NOT MET | M-009 / M-007 | Start with closure, not promotion |
| 20 stable criteria: **11 MET, 6 PARTIAL, 3 OWED, 0 UNMET** | M-009 Section 3 | Every outstanding criterion gets an owner and proof plan |
| ARM64 Linux/glibc Pi, 16 KiB pages VERIFIED-RUN | [SUPPORTED_TARGETS](SUPPORTED_TARGETS.md) | First potential Linux production scope only, with more evidence needed |
| Latest banked aggregate suite: **4,590 checks, 0 failed** | [M-009 run](../tests/abi/recorded_run_m009.txt) | Maintain this as a non-regression floor; do not confuse individual suite counts with totals |
| Exactly four dynamic exports | M-009 and [public header](../include/mimir_alloc.h) | Guard ABI compatibility until a versioned decision |
| Standalone G-008: **214/1**, known stale expectation | M-009 / INC-H001-01 | Repoint or formally retire check without hiding it |
| Fuzzer corpus run: **168,384/0** but assignment mismatch | M-004/M-009 | Align corpus with specification and expand coverage |
| Soak: ten cycles, **159,476 operations**, flat reported RSS, six dlopen cycles | M-005 | Extend duration, environment variety and instrumentation |
| Measured **~5× to ~65× slower than glibc** | [Phase 11](PHASE_11_EXIT_EVIDENCE.md) | Make profiling/performance a major track |
| x86-64 partial probe; no hosted suite | M-006 | Run native suite on separately provisioned host |
| Native Android/bionic, Termux and SM-X400 MimirAlloc runs absent | M-006 / Phase 4 | Build actual device evidence rather than analogy |
| A.E.S.I.R. end-to-end allocator adoption and Qwen3 comparison absent | Phase 12 | Proof of ownership routing is a prerequisite |
| Invariant 9 lock/unlock reporting leg gap | INC-M002-01/02 | Highest-priority correctness/diagnostic risk |
| SBOM/security/plattform documentation drift | M-008 vs M-006/007 | Reconcile and install a freshness gate |
| Proposed GitHub x86 workflow is **manual-only and unexecuted** | .github/workflows/cross-x86_64.yml | Do not claim CI coverage until real logs exist |

The existing 14 bundled carried-owed entries are enumerated in [M-009 Section 6](PHASE_13_EXIT_EVIDENCE.md). This roadmap does not collapse them into a fictitious completed phase.

### 2.2 Defect classes to resolve before optimizing

- **Priority P0 / correctness:** INC-M002-01 and INC-M002-02 (unreported synchronization failures); prove failure behavior and recoverability or decide an explicit fail-stop posture.
- **P0 / evidence integrity:** stale G-008 expectation, Phase 3 x86 hosted suite debt, Phase 4 Android device debt, historic Phase 1/6 original-failure gap, Phase 12 actual-model comparison gap.
- **P1 / release accuracy:** outdated supported-platform and baseline documents, security reporting activation, release-0.1.0 SBOM traceability, incomplete public register reconciliation.
- **P1 / specification closure:** unresolved behavior for zero sizes, errno, realloc(ptr,0), alignment, invalid pointers, fork and cancellation; make each promise or explicit non-promise reviewable.
- **P1 / hardened confidence:** six silent integrity-corruption probe classes, two wild-pointer crash classes, unmerged/debug validation debt, warnings outside standard gates, coverage mismatch in fuzzing.
- **P1 / speed and overhead:** global-lock contention, small-object span behavior, excessive mapping overhead, performance gap, missing memory attribution and energy confidence.
- **P2 / feature expansion:** soft budgets enforcement, R3 consumer route, richer telemetry, additional APIs, memory-class-aware AI adapters and future heterogeneous backends.

### 2.3 Carry all legacy and newly discovered debt

Create docs/PRODUCTION_DEBT_LEDGER.md **in a separate implementation slice** and give each item: stable ID, origin, severity, reproduction, status, owner, dependency, expected proof, affected release scope, next decision and closure commit. Import every still-open INC, U-* contract question and M-009 carried item. Preserve historical issue IDs. A later fix may change the live debt status, but **must not rewrite what prior evidence showed at its time**.

---

## 3. Release trains and gates

### 3.1 Four named tracks

| Track | Intended sequence | Can ship when | Explicit limitations |
|---|---|---|---|
| **R: research** | 0.1.x | Any honest, verified research increment | Experimental, limited ABI, not production |
| **L: scoped Linux** | 0.2.x candidate → 0.9.x release candidates → **1.0.0** | Supported Linux profiles pass their scoped release contract, security and quality gates | Does **not** imply Android, Mojo, GPU or universal ABI |
| **A: Android/edge** | Separate preview builds, then platform-qualified release | Physical devices, correct libc/NDK, 4/16 KiB runs and responsiveness gates pass | Platform-specific; production scope documented independently |
| **X: beyond allocator** | Feature-flagged experimental modules and optional APIs | Independent target and security proof for each module | No default-on AI/remote tiers, no misleading RAM-coherence claim |

Versions are proposals, not commitments. Preserve the current 0.1.0 SONAME/ABI policy until a formal release decision. The L/A/X labels denote **release scope**, not an assumption that all platforms share one maturity level.

### 3.2 Two legitimate paths to a stable gate

The historic [M-001 20-item rule](PHASE_13_FOUNDATION_PLAN.md) requires all items MET for the old Phase 13 stable exit. **Do not quietly waive or regrade it**.

- **Path A (strict legacy closure):** discharge all 6 PARTIAL + 3 OWED criteria, including the x86-64, Android and Qwen3 work, then publish a new evidence packet showing those criteria MET. Original M-009 remains historically PARTIAL.
- **Path B (new, scope-specific gate):** obtain an explicit owner decision establishing a **new Linux-only release contract**, with rationale, threat model, release exclusions, fresh tests and a versioned decision record. This does not change the M-009 verdict or create claims for Android/A.E.S.I.R. A future 1.0 Linux release may be qualified only by its new contract, while past unpaid work remains tracked rather than erased.
- **Decision needed:** document which path Volmarr selects in docs/RELEASE_SCOPE_DECISION.md. Until then, no new stable release is authorized.

### 3.3 Gates by milestone

**G0: evidence trustworthy (before large refactor).** All live claims reconcile against the current tree; CI/manual run provenance defined; defects severity-ranked; repeatable baseline on Pi; inherited historical failures still visible.

**G1: core safety (before broad beta).** Contract/ownership/race/overflow conditions verified; lock failure semantics fixed or fail-closed; automated repros for prior defects; debugger/hardening variants tested; no severe known exploit path without mitigation.

**G2: portability (before target qualification).** Native environment matrix completed for each advertised target, including page-size, ABI, libc, compiler and runtime tests; unsupported targets excluded explicitly.

**G3: bounded workload usefulness (before performance claims).** Correctness-preserving profiles; competitive or justified metrics on named workloads, measured retained memory and tail behavior; multi-client contention; repeatable methodology.

**G4: trustworthy releases (before 1.0).** Artifact integrity, provenance, SBOM, full dependency and toolchain inventory, working security contact, API/ABI version policy, installation/uninstallation, rollback, documented known limitations, reviewed support window.

**G5: application adoption.** A named consumer is instrumented, allocation call-site routing proved, end-to-end output matches a baseline, memory/performance effects measured, fallback tested.

**G6: experimental promotion.** Any AI/accelerator/remote policy remains opt-in until a threat model, controlled A/B proof, fault injection, stable fail-open/fail-closed semantics and resource bound justify a scoped production promotion.

### 3.4 Formal release blocker rule

No gate may be promoted by a generated summary alone. Required artifacts: source commit, clean build log, execution log, environment manifest, acceptance-test results, independent review, and release-scope decision. Any P0 correctness/security issue blocks a production claim, even if the aggregate suite is green. Any unapproved exception is treated as a failure.

---

## 4. Execution system, autonomy and resource constraints

### 4.1 Slice-based multi-agent Mythic Engineering workflow

A project owner defines the goal and desired behavior. Specialist agents can independently propose architecture, inspect sources, implement bounded tasks, analyze tests and critique changes. Every agent is a collaborator, **not** an unreviewed authority over safety-critical low-level code.

**Suggested specialist roles:**

1. **Keeper / integrator:** keeps main green; merges in dependency order; maintains evidence and provenance; reconciles conflicts and grades exits.
2. **Contract seer:** derives public behavior, ABI changes, compatibility matrices and invariants; flags ambiguity before code changes.
3. **Allocator core engineer:** writes minimal original implementation and fault-path fixes.
4. **Adversarial reviewer:** independently seeks invalid metadata, races, overflows, use-after-free, cross-heap ownership and failure-reporting holes.
5. **Platform engineer:** owns toolchains, Linux libc/page-size matrix, Android NDK/device proofs and reproducible execution.
6. **Performance engineer:** builds unbiased benchmarks, profiles overhead, measures energy/memory attribution and proposes one improvement at a time.
7. **Release guardian:** owns SBOM, artifact integrity, provenance, threat model, security disclosures and upgrade/rollback paths.
8. **AI integration engineer:** proves actual A.E.S.I.R. allocation adoption and valid model comparisons; never substitutes harnesses for application results.
9. **Research scout:** brings architectural ideas and links to original documentation without importing unlicensed code.

### 4.2 Mandatory task packet

Each work unit receives an MD packet with the following fields. A template appears in Section 23.

- Unique ID, owner, exact branch/base SHA and files permitted to change.
- Historical evidence and defect references; test that presently fails or evidence that is owed.
- Proposed design, alternative plans and rationale; no guessed architecture.
- Constraints including ABI, pointer ownership, no-interposition default, service safety and permitted installations.
- Dependencies and allowed host/device access; do not assume root, NDK, Docker, network or Android debugging privileges.
- Mechanical acceptance checks, command names, expected exit/absence behavior, and measurement uncertainty.
- Reversal/rollback procedure, test archive path, end-of-slice decision and next owner.
- Required independent reviewer sign-off on any allocator-core, ABI, security or system-level change.

### 4.3 Parallelism without false merges

- Partition by non-overlapping file ownership where practical. Race two competing algorithms on **separate** experimental branches, not in one code path.
- Assign one keeper to critical interfaces: src/core, include/mimir_alloc.h, src/api map and Makefile release rules.
- Use small PRs, no speculative mega-refactor. If two slices touch the same critical file, serialize them.
- Each banked slice: compile → targeted tests → full suite → ABI symbols → platform-specific probes → independent review → commit → recorded evidence.
- An agent's claim about a command is only accepted if its stdout/stderr, exit status, device and code SHA are preserved.
- Never allow an agent to auto-install a toolchain, alter system allocator symlinks, change security settings, publish a release or run unbounded stress on the Pi without the owner's explicit authorization.

### 4.4 Physical-host protection

The Pi has hosted other services in the past, including Samtal. Preserve the M-009 practice: assess free RAM, observe cohabiting service availability before and after tests, use bounded run times and refuse workload runs when they would jeopardize the host. Heavy soak, fuzz, sanitizer and failure-injection campaigns should preferentially move to a **dedicated** Linux machine or controlled CI runner. Report when a test was skipped for system safety.

### 4.5 Definition of done for a slice

- Change reviewed; functional result demonstrated with archived run.
- No unrelated files changed; ABI and test assertions still match policy.
- No new untracked security or portability regression.
- Provenance and license considerations documented.
- Evidence ledger, defect ledger and supported-target rows updated.
- Status graded MET, PARTIAL, UNMET, OWED or BLOCKED honestly.
- User-facing claim updated if the affected capability is public.

---

## 5. Phase 14: repair the evidence and defect register

**Purpose:** create one internally consistent source of truth.  
**Priority:** P0. **Dependency:** existing M-009 packet only. **Target gate:** G0.

### 14.1 Work packages

- **N-14-01 — Audit the tree.** Record current commit, filenames, versions, source count, public exports, executable Makefile targets, workflow states and artifact list. Classify documentation, code, proposed CI, built artifact, executed suite and physical run separately.
- **N-14-02 — Consolidate all carried debt.** Convert M-009's 14 grouped entries, public register's 44 entries and earlier issue families into one live ledger without double-counting identities or deleting history. Include severity, owner, branch, affected release and proof required.
- **N-14-03 — Lock reporting gap disposition.** Open highest-priority tickets INC-M002-01/02; assign explicit test and design owners. The safety claim and observability claim must be judged separately.
- **N-14-04 — Stale test-g008.** Reconstruct H-001 behavioral change, compare G-008's assertion to the intended unload contract, write a decision record: change the expectation, supersede the test, or keep it as a clearly quarantined negative probe. Never silently delete test output. Require a passing modern unload regression to replace any retired assertion.
- **N-14-05 — Reconcile M-006 and M-008 platforms.** Update SUPPORTED_PLATFORMS.md to follow SUPPORTED_TARGETS.md for x86-64 PARTIAL-RUN, musl BLOCKED and Android OWED/ANALOGY-ONLY. Attach exact evidence links.
- **N-14-06 — Correct obsolete files without rewriting prior records.** Mark BASELINE_INVENTORY.md and ENGINEERING_GUIDE.md historical or update their current-state preamble. Preserve original paragraphs and attach superseding guidance where needed.
- **N-14-07 — Release-0.1.0 reconciliation.** Update the public defect register's stale SONAME/install entries in a separately dated addendum referencing M-007. Confirm which SBOM rows match actual compiled objects.
- **N-14-08 — Document release-scope choice.** Decide strict legacy closure versus independently scoped Linux release gate; record excluded features and deliberate unsupported operations.
- **N-14-09 — Reproduce 0.1.0.** Clean Pi build, full suite, four exports, SONAME, checksum verification, explicit standalone g008 and fuzz targets; compare against M-009 evidence, noting any new head changes.
- **N-14-10 — Determine CI policy.** Document whether workflows remain manual-by-owner or become automatically triggered; if enabled, make the proposed x86 workflow actually correct before calling it CI evidence.
- **N-14-11 — Security contact readiness.** Validate the reporting address/channel and response ownership without posting private data or claiming that naming an unconfigured channel makes it functional.
- **N-14-12 — Structured capability ledger.** For every advertised feature, link contract → implementation → test → target-specific run → release claim.
- **N-14-13 — Existing benchmark archive.** Store measured glibc/Mimir ratios and exact workloads for future comparison; declare what was not measured.
- **N-14-14 — Publish a dated exit packet.** Grade all 14 work packages and carry exceptions forward.

### 14.2 Acceptance

- [ ] A single ledger covers every inherited open item with no unexplained omissions.
- [ ] Current docs no longer say there is no implementation/build; historic text retained as historic text.
- [ ] Platform matrix and public supported-platform page agree by exact grade and target.
- [ ] No known failing standalone test is concealed in the aggregate suite.
- [ ] Full-suite baseline, ABI and versioned release verification can be repeated at a named commit.
- [ ] A reviewable release-scope decision exists, or a plainly marked owner decision remains blocking.

**Suggested artifacts:** docs/PRODUCTION_DEBT_LEDGER.md; docs/RELEASE_SCOPE_DECISION.md; docs/PHASE_14_EXIT_EVIDENCE.md; tests/abi/recorded_run_n014.txt; docs/CAPABILITY_EVIDENCE_MATRIX.md.

---

## 6. Phase 15: correctness, API contracts and allocator safety

**Purpose:** settle the supported API contract and close observable safety gaps before changing hot paths.  
**Priority:** P0. **Dependencies:** 14.2; Phase 3/8/10 archives. **Target gate:** G1.

### 15.1 Surface semantics and ownership

- **N-15-01 — Contract registry.** Enumerate every exported call, argument domain, return/errno policy, pointer lifetime, alignment and special case. Separate API choices from C-library compatibility promises.
- **N-15-02 — Zero-size family.** Decide mimir_malloc(0), calloc with zero factors and realloc(NULL,0); make a table of outcomes. Existing behaviors remain documented until an explicit ABI-safe decision changes them.
- **N-15-03 — realloc(ptr,0).** Decide whether the existing NULL+retain-original posture is kept, changed in a major compatibility path or exposed as an unsupported case. Never silently free data or change semantics under a stable ABI.
- **N-15-04 — Allocator-owned pointers.** Prove pointer validation before metadata dereferences; define invalid pointer, interior pointer, already-freed pointer and cross-allocator pointer behavior without promising impossible universal detection.
- **N-15-05 — Alignment.** Specify and test minimum alignment from ABI requirements, not a magic constant; include user-requested alignment only if a separate API is intentionally introduced.
- **N-15-06 — Overflow and failure atomicity.** Checked multiplication, rounded sizes, span extents, mapping lengths and realloc copy bounds; test SIZE_MAX boundaries and allocator/OS fault injection.
- **N-15-07 — Initialization/reentrancy.** Test static init, first-use concurrency, diag emission without allocation, teardown order, dlopen/dlclose and late process exit.
- **N-15-08 — Lock acquisition failure.** Explicitly prove allocation/free/trim/drain behavior on pthread mutex errors; define fail-stop vs safe refuse and a diagnostic/reporting channel that does not deadlock. Close INC-M002-01/02 or block release.
- **N-15-09 — Unlock failures.** Do not discard failure status silently; implement a bounded observable policy, with test-only fault hooks and a recorded behavior decision.
- **N-15-10 — Global serialization.** Prove every shared metadata mutation is protected or safely atomic. Establish lock ordering and annotate invariants before introducing sharding.
- **N-15-11 — Cancellation and fork.** Preserve explicit support boundaries. Safe deferred cancellation is not proof of async cancellation. Fork-while-multithreaded remains UNSUPPORTED until designed and tested; no promises based on a quiescent case.
- **N-15-12 — Retention, reclamation and ownership transfer.** Verify live objects are never recycled during unload, trim or cross-thread free; reconcile thread exit and library unload.
- **N-15-13 — Internal diagnostic accuracy.** Decide default-on vs opt-in for safe error reports. Exercise dropped writes, closed fd, reentrant signal paths and concurrent reports without recursively allocating.
- **N-15-14 — Statistics invariants.** Address unprovable counter families by implementing required state or reducing the claim explicitly. Never equate estimated requested bytes with recorded requested extents.
- **N-15-15 — Production/debug union.** Integrate red-zones and poison/quarantine on one tree, rerun full test matrix for each configuration, and resolve class-stride and layout interaction.
- **N-15-16 — Contract differential tests.** Compare with glibc/mimalloc/jemalloc only when behavior is explicitly supposed to match; differences that are intentional are policy data, not failing tests.
- **N-15-17 — Small-path/large-path boundary.** Exercise every size class threshold, max small allocation, zero, page boundaries, huge allocation, realloc across class boundary and fragmented free lists.
- **N-15-18 — Compatibility ownership.** Validate mixed C/C++ module behavior with explicit bridges; forbid unloading an allocator before all its objects are released, unless the lifecycle is proven safe.
- **N-15-19 — State-machine model.** Document allowed descriptor/span states, legal transitions, publication timing, protected fields, reset and unmap conditions; add model-based property tests.
- **N-15-20 — No regressions.** Maintain 4-export ABI unless an approved change formally bumps policy and adds consumer migration tests.

### 15.2 Adversarial cases

| Test family | Minimum required observations | Stop condition |
|---|---|---|
| Arithmetic | Size, count, alignment and page-rounding overflow cannot map undersized objects | Any wrap/partial allocation |
| Ownership | Allocation, fill, realloc, free, double free and foreign pointer routes are documented | Overlap, unexpected corruption, silent wrong-heap free |
| Metadata | Corrupt free list, live-count, descriptor link, size and checksum classes | Undefended memory control or untriaged crash |
| Concurrency | Parallel alloc/free/realloc, trim, owner exit and cross-thread free | Deadlock, overlap, unbounded starvation |
| Fault injection | mmap, munmap, mutex, write, sysconf, guard allocation and failure rollback | Leaked ownership or invalid state |
| Loader | static/dynamic consumers, dlopen/dlclose, startup/exit order | Lost mapping, stale pointer, unloaded-code call |
| Hostile caller | zero/huge sizes and unsupported operations as contract defines | Behavior contradicts public contract |

### 15.3 Exit gate

- [ ] Contract decisions are encoded as executable tests and a semantic version decision.
- [ ] Invariant 9 reporting defect closed with a fault-injection demonstration.
- [ ] No unresolved critical metadata/synchronization corruption in supported use.
- [ ] Debug, hardened and ordinary variants run on a single integrated tree.
- [ ] Counter/diagnostics claims reconcile with actual exports and test capabilities.
- [ ] Any intentionally unsupported behavior remains clearly labelled, not implied to work.

**Artifacts:** docs/PRODUCTION_API_CONTRACT.md; docs/POINTER_OWNERSHIP_AND_LIFETIME.md; docs/LOCK_FAILURE_POSTURE.md; docs/PHASE_15_EXIT_EVIDENCE.md; tests/failure/* and tests/contract/* expansions.

---

## 7. Phase 16: platform, ABI and build verification

**Purpose:** replace portability assumptions with target-specific execution.  
**Priority:** P0/P1. **Dependencies:** contract stability, target machines/toolchains and explicit permission for installations. **Gate:** G2.

### 16.1 Precise support tiers

- **Tier S (supported):** native build, full suite, ABI/linker check, pressure/concurrency tests, versioned package consumer, maintenance commitment.
- **Tier P (preview):** cross build and selected runs; some known limitations remain; not advertised as stable.
- **Tier E (experimental):** one-off run or emulation, no compatibility guarantee.
- **Tier R (research only):** design analysis; no code support claimed.
- **Tier B (blocked/owed):** missing dependencies/run; no substitution with analogy.

Never place two distinct libc environments on one line and call both verified. ARM64 Linux glibc is not Android/bionic. Termux is not automatically equivalent to Linux proot. A 16 KiB host does not prove 4 KiB execution.

### 16.2 Target matrix of future qualification work

| Target | Present record | Production evidence required | Suggested lane |
|---|---|---|---|
| Pi ARM64 Debian/glibc 16 KiB | VERIFIED-RUN scoped to present tests | Phase 15 gates, full release/hardening, continuous reliability, release consumer and performance | Linux primary |
| ARM64 Linux/glibc 4 KiB | OWED | Native 4 KiB host, alignment/address-space suite and release consumer | Linux primary |
| x86-64 Linux/glibc | PARTIAL-RUN, no hosted suite | Native x86 host full suite, sanitizers, ABI, perf and independent CI logs | Linux primary |
| ARM64 Linux/musl | BLOCKED by toolchain | Provision musl, compile and run full contract suite, error/errno policy compatibility | Linux secondary |
| x86-64 Linux/musl | unqualified | Native/matched sysroot and runtime proof | Optional |
| Android ARM64/bionic 4 KiB | OWED | NDK build, ELF/API validation, physical-device program and stress | Android primary |
| Android ARM64/bionic 16 KiB | OWED | 16 KiB device/emulator tests plus physical-device confirmation for stable device claim | Android primary |
| Termux bionic | OWED | Exact Termux compilation/linking/runtime under its own restrictions | Android optional |
| Linux proot on Android | OWED | Separately labelled proot run, host/guest ABI and syscall evidence | Experimental |
| Samsung SM-X400 | field-noted; no MimirAlloc run | Native app/Termux test, responsiveness, memory pressure, power data | Device flagship |
| macOS arm64 | research only | Independent Mach VM, pthread and ABI port | Future |
| Windows ARM64/x64 | research only | Independent Windows VM, ABI and CRT ownership port | Future |
| RISC-V Linux | research only | ABI/page-size model, compiler and native device tests | Future |
| bare-metal/RTOS | research only | New backend and fundamentally different scheduler/ownership model | Separate edition |

### 16.3 Required work packages

- **N-16-01:** native x86-64 hosted suite to clear Phase 3 evidence debt; a proposed workflow file is not a CI result.
- **N-16-02:** fix CI script preflight, runner selection, explicit emulated vs native labeling, test artifacts and timeouts. Review before owner enables any scheduled runs.
- **N-16-03:** native ARM64/glibc 4 KiB page host run. Compare pointer alignment, mmap round-up, small-span layout and retained-mapping behavior.
- **N-16-04:** detect page size at runtime using target ABI facilities; test assumptions about sysconf and getpagesize alternatives per platform.
- **N-16-05:** musl toolchain provisioning decision; do not auto-install. Test unknown syscalls, errno expectations and static/dynamic linkage.
- **N-16-06:** compile with GCC and Clang under agreed warning flags; maintain builds with optimization O0/O1/O2/O3, explicit supported architectures and hardening flags.
- **N-16-07:** inspect PT_LOAD and SONAME, GNU symbol visibility, ELF relocations, static archive composition, executable-stack/RELRO/PIE applicability and incompatible loader assumptions.
- **N-16-08:** test C99/C11 consumers and C++ consumers, including aligned-new and sized-delete cases only if explicitly supported by adapters; preserve exact C symbols.
- **N-16-09:** test shared library upgrade while processes run: verify SONAME resolution and compatibility contract rather than invent hot-swap safety.
- **N-16-10:** Android NDK toolchain preparation and bionic source audit, minimum API-level policy, mmap/madvise, pthread attributes, errno and ELF page alignment.
- **N-16-11:** validate Android 4/16 KiB executable segment alignment and package installation on qualifying devices, not merely compiler flags. Android's official 16 KiB guidance is linked in Section 26.
- **N-16-12:** physical SM-X400 D-007 device packet: bounded allocate/fill/free, multi-thread churn, in-run page-size discovery, pressure, liveness and crash/log capture.
- **N-16-13:** separate Termux bionic and proot Linux runs with precise libc/toolchain details; never infer native support from a proot result.
- **N-16-14:** repeat native runs on at least two different kernels/CPU generations where available before claiming broad Linux stability.
- **N-16-15:** identify Android background-reclaim and thermal scheduling effects; distinguish allocation performance from platform frequency changes.
- **N-16-16:** export a machine-readable support matrix with device, kernel, libc, compiler, page size, backend, runtime mode and last passing commit.
- **N-16-17:** tie every supported-platform README row to latest recorded evidence, and withdraw claims automatically or by review when evidence expires.
- **N-16-18:** collect full platform exit reports, with copied logs and hashes, not only summary statements.

### 16.4 Exit gate

- [ ] At least one second native platform family has executed the full suite and ABI consumer tests, **if claimed supported**.
- [ ] Each published supported target has a named physical/native run and current acceptance record.
- [ ] Android/Termux stay explicitly non-production until separately qualified.
- [ ] Page-size and bionic compatibility tests include runtime evidence, not only cross-compilation.
- [ ] Installer and rollback tests are architecture/libc-scoped; no system-allocator replacement by default.

**Artifacts:** docs/PLATFORM_QUALIFICATION_2026.md; docs/ABI_COMPATIBILITY_MATRIX.md; tests/cross/* improvements; tests/android/device evidence; docs/PHASE_16_EXIT_EVIDENCE.md.

---

## 8. Phase 17: security hardening and adversarial verification

**Purpose:** make dangerous failures discoverable and containable, with honest limitations.  
**Priority:** P0/P1. **Dependencies:** Phase 15 contract state machine, sufficient isolated test machines. **Gates:** G1 and G4.

An allocator operates next to untrusted inputs and corrupted caller memory. It cannot promise to make arbitrary undefined behavior safe; it **can** reduce exploitability, detect some classes of corruption early, avoid self-inflicted metadata vulnerabilities, and define how it fails.

### 17.1 Threat model and attacker capabilities

Model at least these actors and fault sources:

- A mistaken but non-malicious C/C++ consumer: double-free, mismatched free, stale pointer, invalid size/alignment, erroneous concurrency.
- A memory-corrupting application bug: heap overflow, underflow, use-after-free, wild write into a free-list node, overwritten span/header metadata.
- A malicious plugin or dynamically loaded library in the same address space: mixed allocator ownership, destructor ordering and attempt to corrupt allocator internals.
- Adversarial workload: repeated tiny allocations, high-churn cross-thread frees, memory exhaustion, metadata exhaustion and forced fragmentation.
- A hostile build input or compromised workflow dependency: modified release artifacts, unauthorized tags, misleading SBOM entries and provenance substitution.
- Partial failures: mmap/munmap, sysconf, mutex error, invalid fd, process crash, signal reentrancy and loader teardown while other threads run.
- Optional later network-tier attacker: packet delay/reorder, replay, capability theft, remote host failure and stale lease reuse. This is **not** an allocator-core threat until the optional distributed module exists.

Trust boundaries: caller pointers are not trustworthy; allocator-owned metadata must be protected from caller writes to the degree realistically possible; OS page mappings and kernel interfaces are external dependencies; build runners and artifact distributors are supply-chain trust surfaces. Other processes on the host are **not** MimirAlloc-managed allocations.

### 17.2 Concrete hardening work packages

- **N-17-01 — Security requirements.** Document protected assets (heap integrity, secrecy of diagnostic data, process availability, artifact trust), attacker prerequisites, risk severity and supported threat models per build flavor.
- **N-17-02 — Metadata integrity follow-up.** Reproduce all 25 Phase 10 corruption probes on merged source. Address the six recorded silent classes or formally describe their detection boundary; preserve records for the two expected wild-pointer crash cases. Never infer complete corruption protection.
- **N-17-03 — Harden free lists.** Evaluate keyed/encoded links, double-free detection and bounded free-list validation. Treat metadata-access violations as failure cases. Compare performance impact and pointer secrecy properties, not just crash counts.
- **N-17-04 — Metadata separation.** Investigate out-of-band descriptors, guard pages or protected metadata mappings. Prove pointer-to-metadata lookup without trusting attacker-controlled object memory, then measure footprint and TLB cost.
- **N-17-05 — Integrity check state machine.** Ensure corrupted length/head/count cannot cause out-of-bounds writes during trim, drain, realloc or unload. Define stable panic vs error return for each corruption class.
- **N-17-06 — Allocation failure paths.** Under mapping faults or address exhaustion, test all intermediate state transitions, rollback and resource release. Include a deterministic injection matrix for every OS and sync operation the allocator invokes.
- **N-17-07 — Guard-page and quarantine modes.** Provide debug/security options with bounded and inspectable memory overhead, with test cases for OOB writes, delayed reuse and runtime toggles only where reconfiguration can be made safe.
- **N-17-08 — Initialization attack surface.** Audit first call from static constructors, recursive logging, signals, thread-local destructors, fork and dynamic loader callbacks. Never claim async-signal-safety without a formal supported contract.
- **N-17-09 — Concurrency fault paths.** Inject lock errors, cancellation requests at controlled locations, owner-thread death and reentrant diagnostics; verify fail-stop/safe-refusal behavior without deadlock or overlapping allocations.
- **N-17-10 — Fuzz harness rewrite.** Resolve M-004's rewritten-seed/model mismatch with new model-invariant coverage. Preserve original seed corpus and results; label new corpus separately.
- **N-17-11 — Coverage-guided fuzzing.** Add libFuzzer- or equivalent-compatible targets for valid call sequences, realloc state transitions, corrupted descriptors, configuration parser, optional telemetry and future protocol decoders. Maintain minimized corpora and regression reproductions.
- **N-17-12 — Sanitizer strategy.** Build dedicated ASan/UBSan/TSan/LSan runs as supported by each platform; record interceptor and allocator incompatibilities. An ASan-instrumented custom allocator may not be observed as a normal ASan-managed heap; add explicit red-zone/invariant tests. Never deploy sanitizer runtimes as the production security boundary.
- **N-17-13 — Valgrind and heap tools.** Where suitable and supported, cross-check leak/invalid access behavior and record tool limits; avoid claiming tool support on unqualified ARM/Android targets.
- **N-17-14 — Differential properties.** Execute the same legal-operation traces against MimirAlloc and reference allocators; compare observable data correctness and only contract-appropriate outcomes, not implementation-specific addresses.
- **N-17-15 — Model checking and schedules.** Use bounded deterministic schedulers or model-based concurrency testing on a simplified metadata state machine; publish counterexamples as minimized tests.
- **N-17-16 — Static code analysis.** GCC analyzer, clang static analyzer and selected available analyzers; triage findings by code SHA, not by arbitrary zero-warning vanity. Annotate any accepted false positive.
- **N-17-17 — Build-flavor defenses.** Define ordinary, hardened, debug and ultra-constrained builds. Record which protections are compiled in, default state and memory/performance cost. Hardened security must not silently modify default allocator semantics.
- **N-17-18 — Binary security checks.** Evaluate RELRO, NX stack, relocation rules, symbol versioning, compiler stack protectors, fortify compatibility and whether each applies to a library in a target architecture.
- **N-17-19 — Security response process.** Validate a private reporting path; define response owner, first acknowledgement objective, triage, embargo choices, advisory format, supported fixes and coordinated disclosure. Do not auto-publicize exploit details from a private report.
- **N-17-20 — External review.** Invite a separate low-level systems security review once interfaces stabilize; enumerate coverage and unreviewed areas, with remediation tracking.

### 17.3 Attack/failure acceptance record

| Class | Required evidence | Release treatment |
|---|---|---|
| Metadata length/header corruption | Deterministic injection, outcome before/after, no unauthorized write in supported mitigated cases | P0 until containment or bounded unsupported posture proved |
| Lock failure | Positive witness that the error is surfaced or fail-stop is deliberate | Blocks production if silent ambiguity remains |
| Free-list corruption | Detection or containment study with false-negative list | Hardened claim may be limited to documented classes |
| OOM / virtual address exhaustion | Clean NULL or contract-defined error, no loss of ownership, recovery where supported | Mandatory |
| Cross-thread race | Stress + deterministic schedule evidence + sanitizer interpretation | Mandatory |
| Unexpected pointer | Explicit supported/unsupported contract, limited crash claims | Must not promise safe arbitrary invalid-pointer free |
| Loader and fork | Reproduced supported routes and clearly excluded unsupported routes | No inferred dynamic interposition safety |
| Release supply chain | Checksums, signed provenance plan, permissions and verification procedure | Required for mature artifacts |

### 17.4 Exit gate

- [ ] P0 lock-error reporting gap and supported-memory corruption paths resolved or production release blocked.
- [ ] A published threat model matches tested hardening options.
- [ ] Unexplained sanitizer/fuzzer failure count is zero for the release candidate; all deviations triaged.
- [ ] Every known silent corruption class has an explicit mitigation/limitation entry.
- [ ] Repeatable fault-injection suite and bounded stress logs exist for the release scope.
- [ ] Security reporting is actually actionable.

**Artifacts:** docs/THREAT_MODEL.md; docs/HARDENED_FLAVORS.md; docs/SECURITY_TEST_MATRIX.md; fuzz corpus and harnesses under tests/fuzz; docs/PHASE_17_EXIT_EVIDENCE.md.

---

## 9. Phase 18: benchmark science and performance engineering

**Purpose:** make the implementation genuinely useful rather than merely green on a test runner.  
**Priority:** P1 immediately after safety baseline. **Dependencies:** 15/17 correctness, 16 target facts. **Gates:** G3/G5.

### 18.1 Measurement discipline

**Observed starting point:** Phase 11 found MimirAlloc roughly 5×–65× slower than the host glibc allocator across the tested workloads on one Raspberry Pi. This is not a universal performance ratio. The benchmark suite is valuable because it gives an honest reproducible starting reference, not because it proves how to optimize.

For every performance comparison:

1. Record exact git SHA, CPU/SoC, kernel, libc, compiler, flags, page size, memory limits, governor, clock, process affinity, background load and temperature.
2. Run identical workloads with the same ownership semantics and equivalent allocation operations; verify output correctness.
3. Warm up and measure under predeclared repetition counts. Capture raw distributions, not only a single median.
4. Record p50, p95, p99, maximum where stable, ops/s, alloc/free pair latency, tail outliers, CPU cycles, instructions, cache misses and syscalls where available.
5. Report mapped and retained memory, RSS/PSS or cgroup values with their measurement scope; kernel page cache and allocator-resident bytes are different concepts.
6. Separate cold startup, steady state, memory pressure, unload/trim and repeated process lifecycle.
7. Run both short and long workloads to expose different failure/fragmentation regimes.
8. Compare before/after on the same machine and, where possible, multiple runs alternating order to reduce drift.
9. Declare confidence and variance. A tiny median win that falls inside run noise is a NO-CHANGE result.
10. Never let a faster result override a correctness failure.

### 18.2 Benchmark workload corpus

| Workload ID | Shape | Critical metrics | Why it matters |
|---|---|---|---|
| PERF-01 | small allocations 8–256 B, same-thread | ns/op, p99, mapped pages | Small object overhead |
| PERF-02 | mixed sizes 16 B–64 KiB, interleaved | median/p99, fragmentation | Realistic lifecycle |
| PERF-03 | allocate-one/free-one hot loop | cycles/op, lock profile | Fast-path minimum |
| PERF-04 | FIFO and LIFO lifetimes | retained bytes, reuse | Fragmentation sensitivity |
| PERF-05 | burst allocate then idle/trim | footprint decline, latency spike | Mobile/background memory |
| PERF-06 | 1,2,4,8,... threads | throughput scaling, contention tails | Lock bottlenecks |
| PERF-07 | producer/consumer cross-thread free | queue cost, remote free latency | Thread ownership |
| PERF-08 | large allocations 4 KiB–256 MiB | map/unmap cost, zeroing overhead | GGUF-like buffers |
| PERF-09 | many sizes at class boundaries | internal/external fragmentation | Class table quality |
| PERF-10 | realistic trace replay | throughput, RSS, tail, correctness | Application relevance |
| PERF-11 | repeated dlopen/dlclose | bytes leaked/cycle, loader latency | Library lifecycle |
| PERF-12 | cgroup constrained run | OOM behavior, recovery, RSS/PSS | Deployment safety |
| PERF-13 | pinned/unaligned/address-space stress | failure semantics and latency | Robustness |
| PERF-14 | real tokenizer + GGUF metadata | class-separated overhead | AI workload |
| PERF-15 | A.E.S.I.R. full model run | tokens/s, TTFT, context resize | Actual integration only |
| PERF-16 | mobile foreground/background cycles | responsiveness, energy, PSS | Tablet target |
| PERF-17 | concurrent allocator + service load | latency, uptime, resident pressure | Coexistence |
| PERF-18 | standard external allocator benchmarks | benchmark-defined metrics | Neutral external comparators |

All benchmarks must have **reference correctness assertions**. Report benchmark provenance separately from production verification.

### 18.3 Performance engineering sequence

- **N-18-01 — Baseline lock cost:** instrument held/wait times without perturbing the benchmark too much; quantify current single-mutex cost and high-percentile tails.
- **N-18-02 — Profile allocations:** capture sizes, lifetimes, cross-thread rates and page faults for each workload family.
- **N-18-03 — Explain 5×–65× deficit:** attribute time to lock, lookup, span header checks, mapping syscalls, cache misses and metadata churn using profiles; list measured vs hypothesized causes.
- **N-18-04 — Cheap wins first:** remove avoidable syscalls and redundant calculations while keeping invariant checks; separately validate each change.
- **N-18-05 — Size-class optimization:** evaluate small class ladders, span packing and cache locality; do not blindly copy competitor structures.
- **N-18-06 — Retention policy:** measure retention_bound across cold, hot, churn, 16 KiB and future 4 KiB-page targets. Prior Phase 12 L-008 NO-CHANGE verdict stands unless new workload data support a change.
- **N-18-07 — Batch operations study:** K-007 P-1 is only proposed. Prototype isolated refill/return batching and compare locking, memory overhead and correctness before merging.
- **N-18-08 — Thread-local front end proposal:** attempt per-thread caches with bounded size, safe teardown and cross-thread handling only after concurrency proof.
- **N-18-09 — Per-CPU/rseq research:** study Linux rseq ABI and libc interaction, opt-in with capability detection/fallback; not for Android until separately verified.
- **N-18-10 — Large-object backend:** evaluate direct-mmap versus shared spans/runs, map/unmap frequency, page waste and fragmentation; preserve bounded failure handling.
- **N-18-11 — False sharing mitigation:** diagnose with counters and alignment-aware layout; separately account for extra memory.
- **N-18-12 — Arena sharding:** if lock dominates, prototype per-class/per-shard locks with rigorous ordering and fault injection; do not merge a deadlock risk to buy median speed.
- **N-18-13 — Backend and page-size tuning:** compare runtime page-size handling, potential hugepage opt-ins, split/merge and release policy; measure real RSS, not only internal retained counters.
- **N-18-14 — Pressure performance:** measure latency during trim and reclaim at high churn and under cgroup limits; optimize for *useful responsiveness*, not memory hoarding.
- **N-18-15 — Energy accounting:** use appropriately sampled package/device power meters and idle controls; if unsupported, say energy measurement is unavailable rather than estimating from CPU%.
- **N-18-16 — Real workload traces:** add open, privacy-scrubbed or synthetic traces representative of server, local LLM, game engine, tablet and general apps; verify ownership correctness on replay.
- **N-18-17 — Cross-target comparison:** a fast Pi configuration may not be the right configuration on x86 or tablet; publish separate tuning tables.
- **N-18-18 — Independent regression guard:** benchmark PRs against a pinned baseline with variance; flag broad regressions, not just average score.
- **N-18-19 — Optimize compilation:** test LTO/PGO where available with reproducibility and security controls; never treat LTO as a substitute for algorithmic work.
- **N-18-20 — Publish honest verdicts:** a failed optimization is a useful result, with its benchmark script and no-change decision kept in the repository.

### 18.4 Performance goals to negotiate, not claim

Example scoped release acceptance proposal:

- Supported Linux workflows should show **measured improvement over 0.1.0** without a correctness regression.
- Identify three core workloads where performance is within a negotiated factor of the local glibc baseline. An initial engineering target could be within **2×** on median and p99 where workload semantics allow; do not call this achieved until measured.
- Under configured memory pressure, the allocator must neither lose live objects nor cause unbounded service degradation.
- No sustained unexplained retained-memory growth across a declared long soak.
- Any workload still far behind a reference must be in the public performance limitations table, not excluded from the benchmark suite.

The numbers above are **proposed targets**, not factual current performance. Production usability may be justified by hardening, bounded memory or an edge-specific niche even when glibc wins speed tests. It must be an explicit engineering tradeoff, not marketing.

### 18.5 Exit gate

- [ ] Profiling identifies the critical hot paths with raw reproducible samples.
- [ ] Every accepted optimization has a before/after measurement and targeted correctness stress.
- [ ] Performance tables include slowdown cases and confidence/variance.
- [ ] At least one meaningful workload family is competitive under the accepted scope-specific target, or the niche reason for the allocator's use is compelling and clearly stated.
- [ ] No masked tail latency, footprint or energy regression.
- [ ] Benchmarks can be rerun by another developer on a declared supported host.

**Artifacts:** tests/perf/benchmark_manifest.yaml; docs/PERFORMANCE_BASELINE_V2.md; docs/TRADEOFF_DECISION_LEDGER.md; docs/PHASE_18_EXIT_EVIDENCE.md.

---

## 10. Phase 19: production operations, release and supply chain

**Purpose:** make published bytes maintainable, verifiable and safe to adopt.  
**Priority:** P1, parallel with performance once core safety settles. **Dependencies:** 14–17. **Gate:** G4.

### 19.1 Release integrity, provenance and packaging

- **N-19-01 — Release manifest.** Define artifact names per architecture/libc, package version, SONAME, supported APIs, build flags, symbol list, kernel requirements and compatibility restrictions.
- **N-19-02 — Deterministic builds.** Generalize the single-Pi 0.1.0 byte-identical build proof to at least two clean builds with controlled inputs, document nonreproducible portions and test reproducibility across runners only when possible.
- **N-19-03 — Source-to-binary traceability.** Preserve release commit, tag, immutable toolchain reference, build recipe, dependencies and hashes; add machine-verifiable build provenance.
- **N-19-04 — SBOM quality.** Refresh CycloneDX/SPDX-compatible machine-readable SBOM as selected; distinguish build dependencies, runtime dependencies and optional toolchain code; verify it matches actual linked components.
- **N-19-05 — Artifact signing.** Evaluate cosign/Sigstore or another reviewed signing workflow, with clear key custody, verification steps and keyless identity policy. Checksums detect accidents but do not authenticate publisher identity.
- **N-19-06 — Permissions and secrets.** Prefer read-only CI permissions for ordinary tests; narrowly scoped publishing identity; no tokens in logs or release artifacts; pin action dependencies to audited revisions.
- **N-19-07 — Security scanning.** Add source/dependency scans, OpenSSF Scorecard review and verified vulnerability intake. Scan results are signals, not evidence of exploit absence.
- **N-19-08 — Reproducible consumer check.** Test extracted archives with a fresh minimal C and C++ consumer, dynamic loader search path and versioned SONAME; never rely only on in-tree libraries.
- **N-19-09 — Installation.** Prefix-only, DESTDIR staging, uninstall manifest and package-manager-friendly file layout. Never replace libc or system-wide malloc by default.
- **N-19-10 — CMake/pkg-config integration.** Provide imported target and pkg-config metadata, with separate modes for static vs shared linking and correct transitive dependencies.
- **N-19-11 — License/provenance audit.** MIT license, attributions, source provenance and distribution requirements verified for all third-party contributions; a document saying clean-room is not itself legal proof.
- **N-19-12 — Version and compatibility.** Define SemVer rules for public C ABI, SONAME epoch, compatible patch releases, experimental options and security backports. A 1.0 promise must match what is tested.
- **N-19-13 — Release candidate soak.** A frozen RC gets long bounded soak, fuzz coverage, stress and all supported target runs; publish exact hours, operations, failure counts and environment.
- **N-19-14 — Regression management.** Maintain a supported baseline and bisectable benchmark/test records; each regression has an owner and release-blocking grade.
- **N-19-15 — User migration.** Document how to opt in by linking the four MimirAlloc entry points, how to fall back to system malloc, and how to verify actual allocation routing.
- **N-19-16 — Recovery/rollback.** A known-good release and tested uninstall/revert route. Do not make application startup depend on an unavailable optional control agent.
- **N-19-17 — Documentation completeness.** API reference, threading guarantees, examples, compatibility table, memory overhead, failure modes, performance data, security policy and upgrade guide.
- **N-19-18 — Release change-control.** Tag only an approved commit, obtain separate reviewer and owner sign-off, verify signatures/hashes and prevent automation from relabeling a failed gate as passed.
- **N-19-19 — Maintenance window.** Publish supported targets, severity-based update policy and conditions under which platform support is retired.
- **N-19-20 — Rehearsed incident response.** Simulate a reported memory-corruption defect and exercise triage, fix, review, release and advisory with no real secrets.
- **N-19-21 — Governance.** CONTRIBUTING, code of conduct decision, issue templates, external contribution review, reproducible evidence requirement and transparent design-change process.
- **N-19-22 — Zero surprise installs.** Every deployment tool should dry-run, describe files changed and avoid touching system allocator state without explicit informed permission.

### 19.2 Release artifact checklist

| Artifact | Current 0.1.0 record | Production-level expansion |
|---|---|---|
| libmimiralloc.so.0.1.0 / SONAME .so.0 | Research artifact staging documented | Architecture-qualified install package and consumer CI |
| libmimiralloc-0.1.0.a | Research archive documented | Static lifecycle tests and dependency disclosures |
| include/mimir_alloc.h | Four public functions | Complete contract and documented ABI stability |
| release/SHA256SUMS | Present | Signed publisher provenance and verification walkthrough |
| docs/sbom.json and SBOM.md | Present | Full package-bound SBOM generated and checked on every release |
| docs/REPRODUCIBLE_BUILD.md | Single-Pi scope | Clean-runner, cross-machine claims only when supported by data |
| SECURITY.md | Reporting instructions present | Working private channel, ownership and tested response |
| SUPPORTED_TARGETS.md | Authoritative graded matrix | Generated release-specific support evidence links |

### 19.3 Production release readiness packet

A release packet must include:

1. Requested release version, scope and **explicit exclusions**.
2. Evidence for every gate, including failing/owed items.
3. Fingerprints of tested source, toolchain and artifacts.
4. Description of public ABI/API contract and compatibility.
5. Supported target table with last successfully executed run.
6. Security review, fuzz/sanitizer results and severity of remaining defects.
7. Performance/footprint table compared against specified baselines.
8. SBOM, hashes, provenance, install and rollback verification.
9. A release owner's explicit approval recorded before tags are published.
10. A publicly readable limitations section that does not bury essential caveats.

**Exit:** a scoped RC and a separate production decision, not simply one passing test command.

---

## 11. Phase 20: genuine Mojo and A.E.S.I.R. integration

**Purpose:** fulfill the original reason for the project without claiming what harnesses cannot prove.  
**Priority:** P1 for user goals. **Dependencies:** 15, 16 and an actual runnable target application. **Gate:** G5.

### 20.1 Clear boundary

The [Phase 12 exit report](PHASE_12_EXIT_EVIDENCE.md) recorded genuine research into model file loading, tokenizers, KV shape, prefill/decode, and explicit adoption. It did **not** demonstrate that Project A.E.S.I.R. itself allocated through MimirAlloc, nor did it run the required Qwen3-1.7B Q4_K_M comparison. A tokenizer test or GGUF harness is valuable but is **not the same** as a working inference engine.

### 20.2 Work packages

- **N-20-01 — Capture the original failure.** Reconstruct exact old tablet environment, Mojo compiler/toolchain version, command line and first relevant diagnostic, without filling gaps with assumptions.
- **N-20-02 — Identify failure ownership.** Determine whether the failure is dependency, compiler runtime, missing headers, linker ABI, allocator implementation or Android process environment. Never presume tcmalloc itself is unsupported on all ARM systems.
- **N-20-03 — Baseline with no replacement.** Run original command unmodified on a comparable host, capture complete logs and classifying reproducible result.
- **N-20-04 — Low-cost compatibility routes.** Before redesigning internals, test supported toolchain configuration, target selection, upstream issue status and isolated adapter possibilities; preserve independent allocator design regardless.
- **N-20-05 — Build actual A.E.S.I.R. target.** Establish a reproducible full build on chosen supported Linux/Android target. Identify a model small enough to run within available limits.
- **N-20-06 — Call graph audit.** Trace allocation calls from inference engine, model loader, tokenizer, KV cache, scratch buffers and plugin runtime to their actual allocator implementations. Use symbol tracing/interposition only as a *probe* with limitations.
- **N-20-07 — Explicit adapter.** Provide a reviewed opt-in MimirAlloc allocator interface within A.E.S.I.R.'s real memory boundary, with pointer ownership and alignment rules. No magic LD_PRELOAD promise.
- **N-20-08 — Routed-memory proof.** Instrument each allocation class (weights metadata, tokenizer, KV, scratch, runtime); demonstrate exact count/byte matching through MimirAlloc. Separate mmap-backed file weight pages from heap-owned objects.
- **N-20-09 — Non-routed memory proof.** Identify and honestly report GPU/NPU allocations, drivers, other libraries, hidden allocations and file-backed pages not under MimirAlloc control.
- **N-20-10 — Model availability.** Verify true model bytes and hash, license and metadata; a manifest or pointer stub is not model availability.
- **N-20-11 — Qwen3 comparison.** Implement the existing [L-007 controlled protocol](PHASE_12_QWEN3_COMPARISON_PROTOCOL.md) on a device with the same model, prompt, context, backend, thread count and thermal controls.
- **N-20-12 — Correctness gate.** Compare token sequence/output validity against the system allocator leg; handle nondeterministic kernels through explicit numerical/tolerance policy rather than casual text similarity.
- **N-20-13 — Workload metrics.** Measure model loading, TTFT, tokens/sec, prefill and decode latency, KV-resize overhead, host heap/RSS/PSS, GPU/NPU allocation class, thermal and energy where credible.
- **N-20-14 — Cancellation and recovery.** Aborted inference, reload, context resize, multi-model switching and model unload must leave consistent host allocation ownership and bounded retained memory.
- **N-20-15 — Comparison roster.** System malloc and MimirAlloc mandatory; mimalloc/jemalloc/tcmalloc optional only where build/license/target make sense. Never use a substitute allocator's metric as evidence of MimirAlloc performance.
- **N-20-16 — Explicit disabled fallback.** A.E.S.I.R. must build and run with the baseline allocator when MimirAlloc is disabled/unavailable. Confirm no allocator crosses free boundaries.
- **N-20-17 — Mojo ABI boundary.** Review C ABI calls, dynamically loaded components, exception/ownership translation, startup order and all allocator types used by the Mojo runtime. A Mojo import compiling does not by itself prove allocator replacement.
- **N-20-18 — Device budget and workload planning.** For tablet models, separate weights size, KV capacity, tokenizer, runtime, activation/scratch and driver allocations, with safe headroom for Android/system apps.
- **N-20-19 — External confirmation.** Make a reproducible example project that a contributor can run on a supported machine and verify actual MimirAlloc counters or tracing.
- **N-20-20 — Transparent exit packet.** Grade individual integration stages: compilation, linkage, actual adoption, model correctness, comparison, stress and rollback.

### 20.3 Tiered demonstrations

**D1: C ABI link** demonstrates that C/C++ can explicitly call four functions.

**D2: Mojo binding** demonstrates a Mojo program or bridge can invoke those functions on the named toolchain.

**D3: routed engine allocations** demonstrates A.E.S.I.R. actually delegates an identified allocation class through the MimirAlloc interface.

**D4: complete workload** demonstrates model load, prefill, decode, teardown and recovery with routed allocations and correct output.

**D5: controlled performance comparison** demonstrates MimirAlloc versus a baseline on the exact same supported application and model, with verified controls and metrics.

D1 or D2 must never be rebranded as D5. D5 remains OWED until the controlled run exists.

### 20.4 Exit gate

- [ ] Original Mojo failure is either reproduced and fixed or explicitly classified as not reproducible with missing inputs listed.
- [ ] Actual A.E.S.I.R. engine, not a harness alone, uses MimirAlloc for a named memory class.
- [ ] Model file and engine runnable on a chosen target; strict comparison protocol executed.
- [ ] Correctness, memory and performance reports archived together.
- [ ] Default A.E.S.I.R. build remains operable without MimirAlloc, unless a separately reviewed design decision changes that scope.

**Artifacts:** examples/aesir_allocator_adapter; docs/AESIR_ALLOCATOR_BOUNDARY.md; docs/MOJO_REPRODUCTION_PACKET.md; docs/PHASE_20_EXIT_EVIDENCE.md.

---

## 12. Phase 21: Android, tablets, edge and embedded deployments

**Purpose:** make the constrained-device promise real on hardware, not just in architecture diagrams.  
**Priority:** P1. **Dependencies:** Phase 16 Android prerequisites, Phase 15 safety. **Gate:** G2/G5.

### 21.1 Android compatibility program

- **N-21-01:** provision a reproducible NDK/toolchain environment **with owner approval**; publish host and NDK revisions, API level, build steps and dependencies.
- **N-21-02:** verify bionic behavior, loader constraints, pthread and memory syscalls; document divergences from glibc and whether internal implementation changes are needed.
- **N-21-03:** build static and shared library variants with correct Android ABI naming and explicit symbol policy.
- **N-21-04:** validate 4 KiB and 16 KiB native page sizes, runtime detection and ELF load alignment; inspect real binary PT_LOAD data and app installation behavior.
- **N-21-05:** execute D-007 bounded native device tests on SM-X400 with exact memory, page size, CPU, kernel, permissions, SELinux context and thermal state reported in logs.
- **N-21-06:** execute selected multi-thread, cross-thread free, trim/reclaim and constrained-memory tests inside a safe application process.
- **N-21-07:** separately test Termux's bionic toolchain and proot Linux after recording their actual libc and loader boundaries.
- **N-21-08:** measure Android background/foreground transitions, memory trim callbacks, low-memory-killer interactions, suspend/resume, application service lifecycle and user-visible responsiveness.
- **N-21-09:** integrate low-memory lifecycle hints through a host-owned adapter rather than assuming an allocator can intercept Android memory policy globally.
- **N-21-10:** measure PSS/RSS/smaps under representative workloads; distinguish reclaimed anonymous pages from file-backed GGUF pages and GPU allocations.
- **N-21-11:** optimize page and span occupancy against the tablet's measured 4 KiB hardware, then remeasure 16 KiB Linux so one target's win does not silently harm another.
- **N-21-12:** test 32-bit ABI only if deliberately supported; otherwise document ARM64-only and reject unsupported packages.
- **N-21-13:** test application crash/restart and storage constraints; never write memory dumps containing user data unless explicitly authorized and scrubbed.
- **N-21-14:** collect realistic foreground frame-time, audio and UI responsiveness metrics in addition to allocation throughput.
- **N-21-15:** measure power and thermal throttling under controllable conditions; use battery drain only when sampling uncertainty is small enough.
- **N-21-16:** publish a low-overhead edge profile, with configurable caps, no automatic heavy diagnostics and bounded monitoring.
- **N-21-17:** validate the package from a clean install on a second physical device where available; first-device success is not general Android support.
- **N-21-18:** provide a small standalone sample APK/native test that verifies allocator adoption through an explicit C API, without bundling an unnecessary model.
- **N-21-19:** gate actual A.E.S.I.R. tablet inference separately from allocator correctness; the first does not imply the second and vice versa.
- **N-21-20:** publish a device-level exit matrix with real pass/fail/owed cells.

### 21.2 Resource-constrained builds

Provide optional compile-time profiles chosen by explicit consumers:

- **EDGE_MINIMAL:** minimal code size, minimal diagnostics, bounded retention, only verified platform facilities.
- **EDGE_BALANCED:** small-object efficiency and reasonable release policy for general apps.
- **EDGE_HARDENED:** added metadata/poison checks with documented footprint and thermal cost.
- **EDGE_REALTIME_RESEARCH:** preallocated regions and bounded latency experiments, never claimed real-time merely by naming; kernel scheduling and system calls still impose uncertainty.
- **EDGE_AI_HOST:** host allocations grouped/observed by model lifetime, with a separate adapter to inference systems.

Every profile receives a concrete build matrix, compatibility statement and resource overhead estimate. Options must not create an unbounded number of untested combinations.

### 21.3 Beyond Android

- Raspberry Pi and embedded ARM Linux: test different page sizes, libc, memory limits and thermal envelope.
- Jetson/accelerated Linux: keep CUDA/driver memory outside host heap ownership unless a real API adapter supplies explicit handles.
- RISC-V Linux: validate ABI, atomics, cache line, page size and toolchain; use a real board before supported status.
- WASM/VM guest memory: a guest allocator manages **guest address space**; host and guest mappings are not automatically one coherent heap.
- RTOS/bare-metal: require a separate backend for fixed arenas/interrupt context and explicit timing analysis; do not shoehorn Linux mmap calls into an embedded target.

### 21.4 Exit gate

- [ ] Android native binary successfully compiled, installed and executed on a physical device.
- [ ] Owner controls and logs show bounded device pressure and no unacceptable responsiveness regression.
- [ ] 4 KiB and 16 KiB compatibility claims validated per the advertised release scope.
- [ ] Termux and proot results remain separately graded.
- [ ] No unsupported implication that allocator deployment itself grants GPU/NPU inference acceleration.

**Artifacts:** docs/ANDROID_RELEASE_SCOPE.md; docs/SM_X400_DEVICE_VERIFICATION.md; examples/android; docs/PHASE_21_EXIT_EVIDENCE.md.

---

## 13. Phase 22: predictable memory budgets and adaptive pressure

**Purpose:** control memory residency and failure behavior without hiding policy from applications.  
**Priority:** P1/P2. **Dependencies:** accounting correctness, Phase 18 measurement. **Gate:** G3.

### 22.1 Budget and pressure architecture

A **soft budget** is not a physical RAM cap. Define the counted quantity: allocator-owned mapped bytes, retained bytes, requested-live bytes or a combination. Distinguish internal allocator accounting from RSS/PSS/cgroup memory and other process mappings. Hard budgets are a separate, more intrusive policy requiring honest guarantees about allocations that bypass MimirAlloc.

Potential mechanisms:

- A scoped allocator budget configured at application startup, with no hidden global side effects.
- A pre-allocation refusal policy when a request would cross a declared counted limit.
- Trim or purge on explicit request, with recursion-safe state changes and measured live-object safety.
- Background pressure hints from the host or OS, not compulsory background threads.
- Failure callbacks only when they cannot call allocation unsafely; preference for returning a clear error to an application-level policy manager.
- Tier-specific retention strategies selected from a bounded approved table, not an unconstrained self-changing default.

### 22.2 Work packages

- **N-22-01:** decide and document budget units, scope, reset and concurrent update semantics.
- **N-22-02:** add accounting counters that distinguish live request bytes, actual class allocation, mapped bytes and retained spans.
- **N-22-03:** close Phase 9 INC-I001-14 only with enforcement, not just a design document.
- **N-22-04:** design atomic reserve/commit/rollback for budget accounting, including integer overflow and simultaneous allocators.
- **N-22-05:** test calloc/realloc budget delta semantics and retain-original guarantees on failed growth.
- **N-22-06:** build a bounded forced-pressure harness around cgroups v2 memory.high/memory.max and RLIMIT_AS where suitable.
- **N-22-07:** account for page size, internal waste, metadata and impossible-to-release partial spans explicitly.
- **N-22-08:** compare trim aggressiveness with latency, fragmentation and re-allocation; prevent repeated trim/refill thrashing.
- **N-22-09:** optional host hint adapter for Android memory trimming or Linux PSI/cgroup pressure.
- **N-22-10:** define which policies are synchronous, which advisory and which must never run in an allocator fast path.
- **N-22-11:** allow application-owned priority groups (weights metadata, KV, UI, short-lived scratch) only when call sites pass trusted labels.
- **N-22-12:** test cancellation during pressure handling, low-memory recovery and multi-thread retries.
- **N-22-13:** ensure diagnostics and monitoring cannot exhaust the very budget they observe.
- **N-22-14:** test resident reclaim under different kernel page sizes and mapping strategies.
- **N-22-15:** publish per-profile limits, latency/footprint tradeoffs and fallback behavior.
- **N-22-16:** evaluate online tuning in *advisory* mode first; the allocator always retains a simple deterministic safe policy.

### 22.3 Exit gate

- [ ] Enforced budget metric is specified and mechanically verified.
- [ ] Under the budget, refusal/rollback never leaves corrupted metadata or lost live objects.
- [ ] Reclaim improves the declared metric without unacceptable application responsiveness cost.
- [ ] Application-visible errors allow safe fallback; no spin loops under persistent pressure.
- [ ] Distinct process/cgroup/OS metrics are never presented as identical.

**Artifacts:** docs/BUDGET_CONTRACT.md; tests/pressure; docs/PHASE_22_EXIT_EVIDENCE.md.

---

## 14. Phase 23: concurrency, NUMA and next-generation backends

**Purpose:** scale allocation while keeping safety proofs comprehensible.  
**Priority:** P2 after correctness; targeted P1 if single-lock cost blocks use. **Dependencies:** 15, 17, 18. **Gate:** G3.

### 23.1 Architecture options, in ascending risk

1. **S0: existing global-lock design.** Keep as correctness oracle and fallback; measure lock wait.
2. **S1: separated size-class locks.** May reduce contention; requires lock ordering and safe cross-class realloc.
3. **S2: per-thread caches with central refill.** Requires owner exit, remote free queues, memory cap and reclaim contracts.
4. **S3: sharded arenas.** Requires allocation-to-arena ownership metadata, scalable free and trim coordination.
5. **S4: NUMA-aware arenas.** Requires correct topology discovery, fallback under memory pressure, migration behavior and explicit opt-in.
6. **S5: per-CPU/rseq front end.** Depends on ABI/kernel/runtime support and needs special interrupt/preemption safety research.
7. **S6: hugepage-aware backend.** Depends on actual workload/TLB evidence and eviction policy, not assumptions.
8. **S7: alternative low-memory backend.** Prioritize predictable footprint over maximum throughput on tiny edge devices.

These are candidates, not all required to ship. A single suitable architecture is better than seven half-tested ones.

### 23.2 Work packages

- **N-23-01:** split current allocator fast/slow path with a published abstract ownership and mapping contract, without adding complexity purely for elegance.
- **N-23-02:** establish a reference slow path correctness implementation for oracle comparison.
- **N-23-03:** quantify synchronized metadata writes and lock hold-time across PERF-01–PERF-12.
- **N-23-04:** prototype a per-class lock version, preserving lock order and after-failure invariants.
- **N-23-05:** define remote free transfer states, lifetime and reclamation guarantees; test owner death and cancellation.
- **N-23-06:** prototype bounded thread caches and confirm release of cached memory at exit and under trim.
- **N-23-07:** harden cross-thread free against ABA and stale metadata; add deterministic schedule tests.
- **N-23-08:** design latency-friendly central refill/batch decisions; compare with K-007 proposal P-1 and preserve history.
- **N-23-09:** test sharded metadata consistency with registry walks, statistics snapshots and corruption guards.
- **N-23-10:** NUMA topology discovery via supported OS APIs, no silent node pinning or elevated permissions.
- **N-23-11:** NUMA benchmarking on true multi-node hardware; never infer multi-node behavior from a single-node Pi.
- **N-23-12:** optional large-page/hugepage strategy with fallback and conservative reclaim.
- **N-23-13:** evaluate restartable sequences only after the relevant kernel ABI and libc integration have been reviewed. Require a lock-based fallback.
- **N-23-14:** optimize per-CPU/per-thread cache memory overhead for devices with many logical cores.
- **N-23-15:** support dynamic CPU hotplug and affinity changes where target requires it; never assume CPU identity is immutable.
- **N-23-16:** test fork, async cancellation and process exit again; features already documented as unsupported remain unsupported until separately proven.
- **N-23-17:** measure p99 latency and resident memory under contention; a high-throughput design may still be worse for edge UI workloads.
- **N-23-18:** implement feature flags/bench harness so new backends can be tested side by side without destabilizing the default.
- **N-23-19:** audit all new atomic and memory-order assumptions with code review, sanitizers and model tests.
- **N-23-20:** reject any fast path whose correctness proof depends only on “it passed a million operations once.”

### 23.3 Exit gate

- [ ] Each accepted backend demonstrates performance improvement on a named workload without a correctness regression.
- [ ] Concurrency fault injection, teardown, remote frees and allocation ordering are reproducibly validated.
- [ ] Single-lock reference fallback remains available and tested.
- [ ] Target-specific NUMA/rseq/hugepage claims are backed by actual capable hardware.
- [ ] Memory overhead and tail-latency tradeoffs are public.

**Artifacts:** docs/CONCURRENCY_BACKEND_DECISIONS.md; docs/LOCK_ORDER_AND_ATOMICS.md; tests/concurrency extended; docs/PHASE_23_EXIT_EVIDENCE.md.

---

## 15. Phase 24: observability, profiling and application interfaces

**Purpose:** make allocation behavior visible without turning observability into another allocator failure.  
**Priority:** P2, some P1. **Dependencies:** API/ABI policy, stats invariants. **Gates:** G3/G4.

### 24.1 Principles

- The hot path should remain small; optional tracing may collect sampled information with bounded buffers.
- Any externally visible API requires an explicit ABI/version decision. The four-export invariant cannot be silently broken.
- Internal statistics are not automatically public or stable.
- Diagnostics must not leak memory addresses, model prompts, secrets or private filenames by default.
- In absence of a reader, tracing should consume near-zero CPU and bounded memory. Failure to export metrics cannot break allocation correctness.
- Provide independent measured attribution for allocator-resident bytes and actual process RSS/PSS where available.
- Distinguish event sampling from every-operation logging and establish overhead budgets before enabling either in production.

### 24.2 Work packages

- **N-24-01:** settle the statistics consistency model (atomic snapshot, quiescent-only, eventual or sampled) and document each counter.
- **N-24-02:** complete cross-thread free counters for both small and mapping paths, if they will be advertised.
- **N-24-03:** add race-safe lock contention statistics only if their overhead is justified by measured usefulness.
- **N-24-04:** define exported metrics in a separate optional library/API or explicit ABI revision.
- **N-24-05:** build a bounded per-thread sample buffer, with dropped-event counter and zero allocation recursion.
- **N-24-06:** make allocation/free trace records distinguish request size, class size, mapping size and lifetime, with anonymized identifiers.
- **N-24-07:** optionally expose per-size-class occupancy, retained pages and trim effectiveness.
- **N-24-08:** correlate profiling with application-owned phase tags (model load, prefill, decode, teardown) without guessing AI phase from raw allocation address.
- **N-24-09:** enable offline trace replay with deterministic input and explicit data-retention choices.
- **N-24-10:** provide JSON/CSV export outside the allocator hot path, from application-controlled capture logic.
- **N-24-11:** export to OpenTelemetry or system profilers only via an optional adapter, never a compulsory network dependency.
- **N-24-12:** distinguish process RSS, PSS, virtual mapped bytes, shared/file-backed mappings, GPU memory and allocator-owned bytes in dashboards.
- **N-24-13:** implement opt-in diagnostic severity, suppressed-message counters and documented stderr behavior under broken pipes/errors.
- **N-24-14:** add concise human-readable status tools that inspect public files or trace exports without running code with special privilege.
- **N-24-15:** build a low-noise allocation hot-spot report showing size class, lifetime distribution and call-site sample only when capture is authorized.
- **N-24-16:** benchmark tracing overhead separately for high-throughput server, low-memory tablet and local AI workload.
- **N-24-17:** define data minimization and redaction for debug dumps and multi-agent collaboration.
- **N-24-18:** publish example dashboards and scripts with realistic synthetic data explicitly labelled simulated.
- **N-24-19:** test telemetry drop/overflow and shutdown while active, including OOM, cancellation and dlclose.
- **N-24-20:** make all metrics reproducible and versioned so future tools can parse older archive formats.

### 24.3 Exit gate

- [ ] Every public metric specifies units, scope, sampling and uncertainty.
- [ ] Hot-path telemetry overhead bounded against predeclared targets.
- [ ] No recursion or resource-unbounded export behavior.
- [ ] Private/application data suppressed by default.
- [ ] Tools accurately distinguish host, file-backed, GPU/driver and allocator-owned memory.

**Artifacts:** docs/METRICS_CONTRACT.md; docs/TRACE_FORMAT.md; tools/mimiralloc-profile; examples/telemetry; docs/PHASE_24_EXIT_EVIDENCE.md.

---

