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

