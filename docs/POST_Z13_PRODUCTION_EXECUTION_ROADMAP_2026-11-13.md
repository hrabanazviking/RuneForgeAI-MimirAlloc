# ᛗ MimirAlloc: Post-Z13 Production Execution Roadmap
## From the Strix Halo engineering campaign to a qualified Linux allocator, real A.E.S.I.R. integration, Android validation, and carefully gated future architectures

**Proposed:** 13 November 2026  
**Baseline:** `main` at `89206f2b3519582117f8cdd13fe9cb7825df1979`, containing the evidence through Z-097 (`3894db0844f71434b48834fdcfabc7b6bc6d2a09`).  
**Status:** **PROPOSED WORK**, not an implementation claim; no production designation is authorized by this document.  
**Primary responsibility:** the owner sets mission, risk and support decisions; Yrsa's Mythic Engineering project structure may autonomously plan and execute permitted technical slices; the keeper integrates only verified work.  
**Scope:** correctness, data structures, latency, concurrency, portability, Linux/Android and Mojo/A.E.S.I.R., release engineering, operational safety, and optional future AI/accelerator/distributed research.  
**Naming:** `PBR-001` onward are **new planning IDs** (Post-Baseline Roadmap), never preexisting merged issues or GitHub tickets. No item is complete solely by appearing here.

> **North star:** build a trustworthy allocator whose wins are demonstrated against a fair baseline, whose losses are visible, whose failure handling is predictable, and whose adoption is opt-in and reversible until each target qualifies. The ambition is substantial; the evidence gate is stricter.

### How this document relates to the other roadmaps

- [Z13 hardware roadmap](MIMIRALLOC_Z13_ROADMAP.md): the **117-slice Z13 campaign**, of which Z-001–Z-097 were banked when this roadmap was written. Finish **Z-098–Z-117 under their existing IDs**; do not silently renumber, reassign, or certify them here.
- [Production & Beyond master roadmap](PRODUCTION_AND_BEYOND_MASTER_ROADMAP_2026-11-07.md): the **earlier Phases 14–30** still provide the broad research and release strategy. This document provides an **updated, concrete implementation backlog** tied to post-Z13 code and evidence. `PBR` tickets may map to `N-14-xx` and later phases, but do **not** falsely claim those older tickets are closed.
- [Original research roadmap](MimirAlloc_Research_and_Development_Roadmap.md) and [Phase 13 exit evidence](PHASE_13_EXIT_EVIDENCE.md): original 117/117 historic campaign and 0.1.0 research release remain intact. Its **stable exit was NOT MET**; subsequent results do not retroactively rewrite that judgement.
- [13 November README status](../README.md): evidence-backed summary through Z-097. The source tree and fresh recorded runs override any historical prose that predates an implementation.
- This is an **additive architectural and delivery plan**, not a command to modify the Z13 keeper's special one-slice-per-commit process, not a code patch, and not permission for system-wide `LD_PRELOAD`.

### The standing facts that constrain all design

| Evidence area | Demonstrated at baseline | Never infer |
|---|---|---|
| Host | Flow Z13 Ryzen AI MAX+ 395, x86-64, 4 KiB pages, 128 GB UMA, Kubuntu/Ubuntu 26.10, kernel 7.3, glibc 2.44 | These results automatically transfer to Android, AArch64, other kernels, or all CPUs |
| Tests | Z13 standing local gate often **4,988/0/77 by-design skips**, plus many standalone tests | Every standalone probe, sanitizer, fuzz, and portability requirement passed |
| Old Pi | 16 KiB-page AArch64 research evidence, original 0.1.0, old Phase 13 partial exit | Old Pi performance factors describe the post-Z13 allocator |
| New API | Current default export map has **13 namespaced functions**; interposer has **20 defined exports** (ten namespaced plus ten standard) | A stable ABI, generic glibc substitute, or unchanged future symbol count |
| Large path | Z-054 large-batch workload produced a scoped **WIN** vs glibc; its ratio is indicative due to glibc drift concerns | General performance leadership |
| Mixed lifecycle | Z-054 measured **~71.9x LOSS** vs glibc in its same window | The large-path win solved mixed-lifecycle costs |
| Interposer | Python correctness matched but total runtime **5.01x slower**; Ollama load **~22–24x slower**; Z-097 fork/exec tax 1.17–2.30x | Safe/beneficial to preload services, the desktop, or every process |
| A.E.S.I.R. | Z-087 real source-built Mojo CPU engine emitted identical 32-token streams in four trials; performance observed under contention | Persistent A.E.S.I.R. allocation adoption, fixed stale A.E.S.I.R. binaries, Android on-device proof |
| Traces | Z-094 replayed 2,987,619 recorded operations across Python, Git and Ollama with matching checksums | The trace record is itself a timing benchmark or a complete survey of every allocation in libc |
| Batching | Z-093 internal prototype was not an answer to deep-burst metadata walks | An unproven generic batch API removes the dominant bottleneck |
| Flags | Z-096 selected **-O3** over -O2/-flto/-march=native variants for the measured host | -O3 beats glibc or should be used for every platform |

**Explicit historical inconsistencies to reconcile:** pre-Z13 four-symbol documentation; `SUPPORTED_TARGETS.md` and `LIMITATIONS_AND_KNOWN_DEFECTS.md` written before the Z13 host and expanded surface; old test-g008 214/1 stale expectation; the proposed, manually dispatched GitHub workflow has no observed run; D-013 zero-size semantics can break glibc callers; Z-095's canonical Z-094 replay revalidation is owed; the 25-case integrity probe and its 15-pass/10-fail state must not be conflated with the standing gate.

---

## 1. Desired end states and release boundaries

**R0 — evidence-synchronized experimental:** every advertised feature points to code + dated run + target environment; a reconciled debt ledger distinguishes `FIXED`, `PARTIAL`, `BLOCKED`, `OWED`, `DECIDED-NON-GOAL`, `VERIFIED-RUN` and `PROPOSED`. Green tests are recorded honestly, including expected skips and legacy failures.

**R1 — safe qualified Linux core:** namespaced C API, stable ownership/overflow/error contracts, no untriaged severe core defect, validated builds/debug/sanitizers, reproducible installation, and a deliberately small native Linux target matrix. This release **need not** include interposition, Android or AI integration.

**R2 — measured useful allocator:** competitive performance **on declared useful workload classes**, or a documented justified trade-off such as meaningfully improved bounded retention/diagnostics. A serious speed loss on a target workload must be fixed or explicitly excluded, not hidden behind averages.

**R3 — application-integrated allocator:** at least one real A.E.S.I.R./Mojo code path provably routes an identified allocation class to MimirAlloc, preserving identical output and clean fallback; scoped performance and memory are measured on the same model, seeds, tokenizer, runtime and host.

**R4 — Android/edge-qualified:** physical Android/bionic and Termux results separately, architectures/page sizes measured rather than inferred, responsible foreground/background memory-pressure behaviour, documented supported and unsupported device profiles.

**R5 — dependable stable release:** signed-off support scope, API/ABI policy, security reporting, deterministic artifact/provenance chain, CI/manual testing policy and release rollback. The exact version or public launch date is **an owner decision**, not implied by this plan.

**R6 — beyond allocator, isolated research:** optional domain-aware AI memory, heterogeneous or distributed pools, and agent-guided tuning are experiments with separate threat models; never silently change standard heap semantics or imply remote memory is local coherent RAM.

### 1.1 Proposed go/no-go gates

| Gate | Pass condition | Blocked by |
|---|---|---|
| **G0: truth** | reconciled current export map, test inventory, defects, platform matrix, trace provenance; no ungraded contradictions | unverifiable documentation or hidden failures |
| **G1: ownership/correctness** | all supported API contracts tested, high-severity defects closed or a release scope legitimately excludes their paths, race/fault evidence signed | overlap/corruption/unchecked allocator metadata or silent critical lock failures |
| **G2: native target** | reproducible native run of complete applicable suite on each claimed OS/ABI/page-size combination | cross-build-only and emulator-only results sold as native |
| **G3: usefulness** | meaningful, predeclared workload win/tie/acceptable trade-off supported by repeatable data, p99 and RSS/pressure bounds | regressions concealed in aggregates; incomparable benchmarks |
| **G4: release** | reproducible packages, manifest, provenance, SBOM, install/uninstall, security channel, ABI policy, owner review | unsigned/mismatched artifacts, missing rollback or unsupported promises |
| **G5: AI adoption** | allocation-route proof from a real engine with exact model/output identity + opt-out baseline | synthetic harness alone, hidden model differences, no real call-site evidence |
| **G6: optional research promotion** | isolated explicit opt-in, bounded resources, failure recovery, measured benefit and owner approval | a model/policy writing to live allocator state without validated guardrails |

### 1.2 Severity and scheduling

- **P0 stop-the-line:** live-block overlap, corrupted ownership, unbounded metadata walk leading to safety failures, unreported critical mutex failure, security exposure, system-wide preload hazard, false/corrupted published artifact.
- **P1 blocker:** known reproducible correctness failure in claimed scope, ABI surprise, unbounded RSS under supported load, failed reproducibility, stale source-of-truth documentation affecting safe adoption.
- **P2 performance/developer quality:** measurable slowdown or missed size class; cleanup that cannot change safety claims; test automation.
- **P3 optional research:** model-aware policies, energy experiments, accelerator tiers, distributed features; these do not block the core when disabled.

Any P0 triggers halt of the affected release/experiment and a dedicated investigation, not silent acceptance. A P1 may be excluded only by a **written owner-approved narrowly scoped release contract** where technically defensible. The original M-009 historical verdict remains unchanged whichever release path is selected.

---

## 2. Execution protocol for Mythic Engineering

The six engineering functions below are **roles**, not claims that six actual agents were spawned on any run. They may be run by specialized subagents or transparently executed sequentially when infrastructure limits parallel work. Yrsa may direct the work and choose ordering within the approved policy.

| Role | Core duty | Required output |
|---|---|---|
| **I. Keeper / integration** | dependency graph, branch safety, merge arbitration, evidence bank, release scope | one source-of-truth status sheet, merge log, accepted/rejected decisions |
| **II. Contracts / architecture** | allocator invariants, ABI design, memory model, alternative comparison | exact observable contracts and explicit non-promises |
| **III. Core implementation** | smallest possible code patch against a demonstrated cause | restricted-file diff, rollback, tests |
| **IV. Adversarial verification** | fault injection, race review, fuzz, negative tests | independent failure search and severity judgement |
| **V. Performance/platform** | fair glibc comparator, hardware/Android probe, profile statistics | raw run files, environment, metric computations |
| **VI. Release/knowledge synthesis** | documents, changelogs, SBOM, packaging, target tiers | traceable decision packet and next-owner handoff |

**No repeated user "continue" prompts for routine progress:** Yrsa may select the next dependency-ready bounded task, coordinate team roles, run permitted tests, review and bank a slice if authorized. Escalation is reserved for **new permissions** (toolchain installation, root operations, resource-heavy tests beyond caps, account access, publication, global libc changes, security disclosure, API contract freeze, or user policy changes). Approval is not inferred from silence.

**Every implementation slice must publish these fields before it can bank:**

```yaml
id: PBR-000
state: PROPOSED # IN_PROGRESS | CODED | VERIFIED_RUN | PARTIAL | REJECTED | BLOCKED
source_base_sha: "<actual sha>"
owner_role: "<one accountable role>"
target_os_arch_libc_pages: "<exact target profile>"
files_allowed_to_change: ["<enumerated paths>"]
objective: "<one observable outcome>"
hypothesis: "<falsifiable performance/correctness statement>"
alternatives: ["<at least one viable alternate>"]
legacy_links: ["<Z / N / INC / D identifiers>"]
dependencies: ["<exact IDs and gates>"]
environment_permission: "<what can be run or installed>"
acceptance:
  positive: ["<exact checks>"]
  negative: ["<must-not-happen checks>"]
metrics: ["latency", "p99", "rss", "faults", "cpu", "bytes-retained"]
evidence: ["<log locations / raw data / hashes>"]
rollback: "<reversible change and recovery proof>"
verdict: "<PASS/FAIL/WIN/TIE/LOSS/BLOCKED with reason>"
```

**Evidence contract:** raw stdout/stderr, exit code, core SHA, compiled artifact hash, commands, environment, page size, compiler/linker flags, CPU topology, affinity/governor, host load/cotenants, page faults, memory pressure, sample count, medians, p95/p99 where meaningful, spread/confidence and repeated-run checksum must be filed. If a workload is too short for stable energy/latency measurement, say **resolution insufficient**, lengthen a valid workload or withhold the claim. Do not invent precision.

**Banking policy:** one integrator for `src/core/`, the public header, export maps, Makefile and release files. Worktree branches may explore alternatives in parallel; promotion is serialized and followed by a complete verified run on the merged tree. No production code is replaced just because a prototype wins one microbenchmark.

---

## 3. Critical dependencies and delivery trains

```text
Z13 Z-098..117 completion (its own keeper)
              |
              +--- Z13 closing table + true deficits ------+
              |                                             |
              v                                             v
     PBR-001..015 evidence truth                  PBR-016..039 profiling
              |                                             |
              +-----------------+---------------------------+
                                |
                contract + metadata invariants
                                |
                    safe algorithm prototypes
                                |
              correctness / sanitizers / pressure
                     /                 \
                    v                   v
          scoped Linux qualification   fair performance
                    \                   /
                     +-----> release gate G4
                                  |
                  +---------------+----------------+
                  |                                |
           real A.E.S.I.R. G5              Android G2/G4
                  |                                |
                  +---- optional heterogeneous/AI research
```

The dependency arrows define *promotion*, not serial prohibition on research. Parallel teams may do analysis and documentation earlier without claiming the later gate. Finish experimental tickets only when they provide accepted evidence, not because the original optimistic estimate expired.

### 3.1 Suggested release trains

- **Train E (research)**: current experimental `0.1.0` and clearly labelled preview iterations; no drop-in promise.
- **Train L (scoped Linux)**: qualified namespaced libraries for listed Linux/glibc/page-size variants only; Linux stable version requires an explicit owner-supported promise and gate record.
- **Train I (integration)**: A.E.S.I.R. opt-in adapter + separately scoped proof, released independently of allocator core maturity when its own experimental designation is clear.
- **Train A (Android/edge)**: bionic NDK, physical-device acceptance and packaging; do not make Linux stable depend on Android if owner chooses scoped Linux-only first.
- **Train X (research extensions)**: AI-adaptive, hardware-specialized and distributed experiments, in opt-in independent modules with rollback.

### 3.2 Existing Z13 slices to finish *as authored*

| Z13 item | Existing gate / required work | Carry-forward concern |
|---|---|---|
| **Z-098** | tuned-glibc tcache and mmap threshold fair-fight matrix | no cherry-picked glibc settings; also preserve default comparator |
| **Z-099** | final Wave 11 batching/trace verdict | reconcile Z-094 canonical vs Z-095 capture, no exaggerated batch win |
| **Z-100** | eight-hour mixed-lifecycle soak, sampled hourly | shared host caps, RSS drift, first/last percentiles |
| **Z-101** | thread churn, cross-thread free and fork storm battery | race/corruption checks and end-state retained bytes |
| **Z-102** | memory-pressure chaos at proposed 4/16 GiB ceilings | no collateral damage, clean refusal/recovery |
| **Z-103** | randomized corruption and false-positive study | 19/25 detection history; no fabricated silent-class closure |
| **Z-104** | ASan/UBSan plus warning matrix | positive and expected-negative failures labeled |
| **Z-105** | version, SBOM, two clean reproducible builds | signing/publication requires owner decision |
| **Z-106** | target and limitations/documentation truth pass | old M-008 limitations must reflect Z13 with dated addendum |
| **Z-107** | closing baseline vs original, default glibc and tuned glibc, including RSS/energy | valid same-window and confidence discipline |
| **Z-108** | final campaign verdict, all losses and blockers | no inferred stable release from 117/117 |
| **Z-109–116** | eight reserve tasks selected from Wave 12 evidence | one explicit hypothesis + success gate per reserve slice |
| **Z-117** | ledger sweep, keeper stand-down, owner-facing closing packet | 117/117 banking verified against git; evidence gaps remain visible |

**Special policy:** Z-109–Z-116 are reserve capacity, **not** pre-assigned to PBR work. Do not hijack the Z13 keeper's slice allocation. The post-Z13 plan may use independent `pbr/*` branches and begin its nonintrusive analysis before Z-117 closes.

---

## 4. Technical architecture: attack the causes, not the symptoms

### 4.1 Root-cause map (a hypothesis ledger, not facts invented by this plan)

| Symptom / loss | Current evidence | Candidate families to test | Must prove |
|---|---|---|---|
| Mixed-lifecycle `~71.9x` slower | [Z-054](Z13_WAVE06_VERDICT.md) | registry indexing, large/small handoff, span lifecycle locality, cross-class free | end-to-end same-workload improvement without growing RSS/failure risk |
| Burst512 batch `33.8–120.2x` slower | [Z-093](Z13_Z093_BATCH_VERDICT.md) | remove repeated linear live-set registry/span walk; per-span bulk operations | asymptotic change demonstrated by profiled work and depth sweep |
| Allocation-heavy Python `5.01x` slower under preload | [Z-090](Z13_WAVE10_VERDICT.md) | faster small path, object-local metadata, hot caches, less interposed startup | repeated real-process measurements plus exact output parity |
| Ollama cold model load `22–24x` slower under preload | [Z-090](Z13_WAVE10_VERDICT.md) | map allocation histogram, hot-path size bifurcation, pool reuse, loader interactions | separate model read, mmap, heap, page faults, prompt/decode metrics |
| Fork/exec process tax `1.17–2.30x` | [Z-097](tests/perf/recorded_run_z13_z097.txt) | relocation/initialization timing, cold-path feature selection, simpler init | no-allocation and first-touch legs both improve under repeatable storm |
| Lock contention / cross-thread ownership | [Z-045](Z13_WAVE05_VERDICT.md) | stable size-class shards, TLS magazines, remote-free queues, bounded transfer | 1,2,4,8,16,32-thread scaling curves; no race/fork regressions |
| Retention and fragmentation | [Z-063](Z13_WAVE07_VERDICT.md) | span reclamation, occupancy-aware placement, memory-pressure trimming | peak/steady-state RSS, PSS, mapped, RSS after trim under fair touch |
| D-013 glibc incompatibility | [Z-078](Z13_Z078_ZERO_SIZE_ERRNO.md) / [Z-090](Z13_WAVE10_VERDICT.md) | strict namespaced semantics + separate compatibility adapter as an explicit decision | no surprising ABI or silent cross-allocator ownership |

**Critical research bet:** removing O(number-of-live-spans/objects) behavior from the hot allocation/free paths may offer larger sustainable gains than flags, locks alone, or caching alone. This is a **testable hypothesis**, not permission for a monolithic rewrite. Preserve pointer provenance, double-free detection, bounded cache capacity and correct failure paths.

### 4.2 Proposed algorithm candidates (all behind comparative experiments)

1. **O(1)-expected lookup by address region:** page-aligned metadata map or guarded radix lookup, with checked key conversion and no unbounded hash probe. Test fragmentation, page-size independence and metadata self-allocation recursion.
2. **Per-span descriptor ownership:** size class, occupancy bitmap, generation, remote-free state and live count associated with a span; frees resolve their span without global O(n) scans.
3. **Class-local central queues:** full/partial/empty descriptors and bounded cache admission, avoiding a registry traversal on each operation.
4. **Tightly bounded TLS magazines:** hot allocation/free path without common global lock; upper retention bound and explicit thread-exit drain policy.
5. **Cross-thread free mailbox:** MPSC or batched lock-protected queue with proof of no lost frees, duplicate ownership, ABA and poison cases.
6. **Large-object mapping index:** page/extent keyed lookup and pooling policy with O(log n) or expected O(1) predecessor access and explicit huge-page interactions.
7. **Cold-path init split:** lazy metadata initialization whose early failure is clean; isolate expensive integrity tracking from the fast path without losing needed diagnostics.
8. **Tuned metadata layout:** cacheline placement and false-sharing proof by counters; align only measured hot fields, not all structures indiscriminately.

**No design is “adopted” until it beats the shipped core and tuned glibc or meets a different predeclared meaningful benefit while passing every applicable safety gate.** Select by Pareto frontier (latency, tail, resident memory, CPU/energy, complexity), not a single fastest number.


---

## 5. Workstream A: Evidence reconciliation and campaign handoff

**Priority:** P0/P1. **Quality gate:** G0. **Dependency posture:** Z-098–Z-117 where applicable; no code refactor before baseline is tagged. **Accountable role:** Keeper + release/knowledge.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-001 — Catalogue exact live API exports, headers, version nodes and interposer exports

- **Build or investigate:** produce an ABI inventory generated from nm/readelf plus source map and compare to 13/20 baseline.
- **Acceptance proof:** no undocumented export; fail on accidental standard malloc in default .so.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-002 — Import old M-009 14 carried-owed groups into a live defect ledger

- **Build or investigate:** each historic ID maps to current evidence, open owner and precise discharge test.
- **Acceptance proof:** historical verdict remains unedited; duplicate/stale entries linked.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-003 — Re-evaluate lock/unlock incident INC-M002-01/02 after Z-064/065

- **Build or investigate:** fault injection outcome table for every lock and unlock error site.
- **Acceptance proof:** cannot claim solved merely because counters exist; fail-safe behavior checked.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-004 — Reconcile legacy G-008 214/1 failure and standalone negative probes

- **Build or investigate:** explicitly reproduce original failure, test current intended semantics, repair expectation or file exception.
- **Acceptance proof:** old log preserved; current status verified, not silently skipped.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-005 — Regrade platform evidence for x86-64/4 KiB Pi/Android/musl

- **Build or investigate:** update target matrix with host, ABI, page-size and native/emulated distinction.
- **Acceptance proof:** no grade promotion without reproducible native evidence.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-006 — Reconcile old limitations register to expanded Z13 header

- **Build or investigate:** append dated status/provenance per legacy L/D/INC item.
- **Acceptance proof:** keep old citations intact; new current source-of-truth pointer.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-007 — Re-run exact standing and standalone suite inventory on clean checkout

- **Build or investigate:** emit commands and each exit status, including known expected fails and by-design skips.
- **Acceptance proof:** red negative tests visible; differences triaged.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-008 — Build Z13 final wave-to-feature change matrix

- **Build or investigate:** for every Z-slice map code/test/doc, adoption or rejection, and benchmark effect.
- **Acceptance proof:** no work counted completed solely from commit title.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-009 — Revalidate Z-095 candidate findings against Z-094 canonical traces

- **Build or investigate:** repeat identical checksum trace replays and synthetic oracle with current -O3 flags.
- **Acceptance proof:** no-adoption decision reaffirmed or revised with full cross-suite result.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-010 — Normalize MimirAlloc versus glibc use of zero-size and errno

- **Build or investigate:** publish separate namespaced/interposed observable behavior tables.
- **Acceptance proof:** anchored regex and realloc(p,0) hazards shown in public notes.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-011 — Write release-scope choice memo with alternatives A/B

- **Build or investigate:** legacy strict closure versus scoped Linux-only decision for owner.
- **Acceptance proof:** scope and exclusions explicit; no unsupported stable language.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-012 — Set machine-readable evidence registry schema

- **Build or investigate:** index run artifact hashes, toolchain, platform, commit, confidence and verdict.
- **Acceptance proof:** findable from README/roadmap; no private host paths represented as downloadable links.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream A exit:** reconcile its 12 tickets into a dated review packet; report all blocked/rejected work and why. Gate G0 can only change by its independently reviewed actual run evidence.


---

## 6. Workstream B: Hot-path indexing and metadata architecture

**Priority:** P0/P1. **Quality gate:** G1+G3. **Dependency posture:** A1–A12, Z-107 profile, contract stability. **Accountable role:** Core implementation + adversarial verification.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-013 — Profile small alloc/free and realloc cycles at instruction/basic-block granularity

- **Build or investigate:** attribute per-op CPU/cycles/cache misses to span lookup, metadata walk, locks and refill.
- **Acceptance proof:** sum of attributed components reconciles with wall baseline.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-014 — Specify pointer-to-span lookup invariant and reject foreign pointers policy

- **Build or investigate:** written exact mapping for valid/free/interior/stale addresses and generation handling.
- **Acceptance proof:** no unsafe dereference of untrusted address during lookup.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-015 — Prototype page-number radix metadata index in isolated branch

- **Build or investigate:** bounded address regions, metadata allocation discipline and lookup cost curve.
- **Acceptance proof:** no unexpected recursion, leak, growth or ASan warning.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-016 — Prototype guarded segmented/hash index alternative

- **Build or investigate:** bounded collision policy and failure on index OOM.
- **Acceptance proof:** prove correctness under adversarial address churn and huge allocations.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-017 — Perform data-structure bakeoff against shipped registry

- **Build or investigate:** micro+mixed lifecycle, 1..512 live-set depths, p95/p99, RSS and cache effects.
- **Acceptance proof:** choose by full Pareto metrics and bounded worst case.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-018 — Implement per-span occupancy and free bitmap candidate

- **Build or investigate:** verified transitions free->partial->full->empty and exact live counts.
- **Acceptance proof:** no double-admission, stale span reused incorrectly or cacheline race.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-019 — Replace repeated live-set traversal in selected allocation paths

- **Build or investigate:** measure algorithmic work vs live-set size before/after instrumentation.
- **Acceptance proof:** slope materially reduced; correctness/fault invariants unchanged.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-020 — Replace selected free-path traversal without unsafe pointer probing

- **Build or investigate:** detect pointer ownership on supported-valid path and fail-stop defined internal violations.
- **Acceptance proof:** no freed metadata deref, mapped-range mistakes or silent corrupt state.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-021 — Integrate bounded small-object free list and page retention policy

- **Build or investigate:** per-size-class caps, accounting on pressure and trim.
- **Acceptance proof:** no unbounded caches; retention controlled and visible.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-022 — Stress realloc/calloc/align semantics against new metadata

- **Build or investigate:** all overflow, preserve-old-on-fail, zeroing, alignment and overflow edges.
- **Acceptance proof:** no changed public behavior without owner-approved contract change.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-023 — Measure cross-class interleaved vs grouped traces with equal live bytes

- **Build or investigate:** Python/Git/Ollama canonical trace replay and K-002 oracle.
- **Acceptance proof:** no tuned-win declared without same work, hashes and external baseline.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-024 — Remove obsolete registry code only after guarded cutover

- **Build or investigate:** compile and run both implementations behind experimental selection, retire behind accepted decision.
- **Acceptance proof:** one source of truth after adoption, documented rollback branch.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-025 — Inspect C undefined behavior, arithmetic and aliasing in fast-path refactor

- **Build or investigate:** sanitizer, strict warnings, overflow and pointer provenance review.
- **Acceptance proof:** zero untriaged P0/P1 regressions.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-026 — Produce formal hot-path closure report

- **Build or investigate:** cycle accounting, depth scaling, CPU topology and reproducible before/after.
- **Acceptance proof:** claims bounded to exact build, host and workload.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream B exit:** reconcile its 14 tickets into a dated review packet; report all blocked/rejected work and why. Gate G1+G3 can only change by its independently reviewed actual run evidence.


---

## 7. Workstream C: Thread scaling, TLS ownership and lifecycle

**Priority:** P1. **Quality gate:** G1+G3. **Dependency posture:** B3–B9; public contracts. **Accountable role:** Concurrency specialist + adversarial reviewer.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-027 — Define ownership handoff state machine across threads

- **Build or investigate:** formal states for local/remote/draining/retired ownership.
- **Acceptance proof:** every transition has one writer and linearization point.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-028 — Redesign remote-free queue with ABA and lifetime hazards addressed

- **Build or investigate:** compare sharded mutex, MPSC queue and batch drain with proofs.
- **Acceptance proof:** no lost/double frees under randomized schedules.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-029 — Test bounded TLS magazines versus current cache

- **Build or investigate:** 1..32 thread workloads and cross-thread producer/consumer.
- **Acceptance proof:** tail and memory improvements without crashes or aggregate regressions.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-030 — Tune refill and drain batches by class

- **Build or investigate:** independent per-class caps and locality curves.
- **Acceptance proof:** accept only candidate meeting predeclared regressions bar.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-031 — Audit thread-exit cleanup in presence of remote frees

- **Build or investigate:** thread churn/fork stress and destructor-order tests.
- **Acceptance proof:** no leaked live ownership or use-after-thread-exit.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-032 — Harden fork/atfork posture and child reinitialization

- **Build or investigate:** isolate invariants for fork from multiple running threads.
- **Acceptance proof:** no inherited locked mutex deadlock or leaked metadata state.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-033 — Prove cancellation behavior at every lock and mapping edge

- **Build or investigate:** error injection, cancellation safe points and rollback.
- **Acceptance proof:** no unrecoverable poisoned lock in supported scope.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-034 — Benchmark CCD/SMT alternatives on Strix Halo

- **Build or investigate:** pinning, locality, max throughput, p99, energy.
- **Acceptance proof:** accept only stable within-host win, not isolated run.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-035 — Check deterministic accounting across concurrent snapshot calls

- **Build or investigate:** stats invariants under racing alloc/free/trim.
- **Acceptance proof:** no impossible negative/overflow counters and no data races.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-036 — Document platform portability costs of TLS/atomics

- **Build or investigate:** compile x86-64, AArch64, musl, Android where available.
- **Acceptance proof:** no hidden x86 intrinsics in portable build.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream C exit:** reconcile its 10 tickets into a dated review packet; report all blocked/rejected work and why. Gate G1+G3 can only change by its independently reviewed actual run evidence.


---

## 8. Workstream D: Large allocations, fragmentation and reclaim

**Priority:** P1. **Quality gate:** G1+G3. **Dependency posture:** A evidence, B ownership structures. **Accountable role:** Core memory specialist + performance.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-037 — Refresh size-class and mapping-threshold data by current workloads

- **Build or investigate:** minimum/median/maximum and tail cost by size 1B..multi-GiB.
- **Acceptance proof:** report thresholds on each native page-size target.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-038 — Measure mapping-cache hit/miss under canonical traces

- **Build or investigate:** pool occupancy, retained resident and page-fault costs.
- **Acceptance proof:** no cache win claimed without post-free RSS.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-039 — Model large extent fragmentation and reuse

- **Build or investigate:** interval tree vs size-segregated address-index prototypes.
- **Acceptance proof:** verify best/worst-case lookup and stale-map behavior.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-040 — Audit mremap grow/move and object validity under contention

- **Build or investigate:** preservation and failure no-loss in hard cases.
- **Acceptance proof:** no invalid original ownership, no stale descriptor.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-041 — Compare mmap/madvise/munmap/THP strategies separately

- **Build or investigate:** multiple page sizes and THP policies including never.
- **Acceptance proof:** no THP benefit claimed without footprint and latency.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-042 — Implement memory pressure signal and bounded reclaim candidate

- **Build or investigate:** explicit call/cgroup event/periodic optional driver.
- **Acceptance proof:** bounded work, no hidden background allocation loop.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-043 — Run worst-case one-live-object-per-page fragmentation oracle

- **Build or investigate:** requested vs committed vs RSS/PSS/mapped bytes.
- **Acceptance proof:** prove upper bounds or publish limitation.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-044 — Test trim/reclaim idempotency after burst/free and budget edge

- **Build or investigate:** exercise pad=0 and >0 with live blocks.
- **Acceptance proof:** live memory intact; second trim semantics hold.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-045 — Conduct huge-allocation tests with resource envelopes

- **Build or investigate:** guarded 256 MiB..8 GiB cases only when host permits.
- **Acceptance proof:** no shared-host service degradation or OOM kill outside cgroup.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-046 — Select bounded policy with two-host corroboration

- **Build or investigate:** repeat on Z13 4 KiB and AArch64 16 KiB or separate qualified host.
- **Acceptance proof:** publish target-specific defaults instead of universal tuning.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream D exit:** reconcile its 10 tickets into a dated review packet; report all blocked/rejected work and why. Gate G1+G3 can only change by its independently reviewed actual run evidence.


---

## 9. Workstream E: Correctness, hardening and adversarial validation

**Priority:** P0. **Quality gate:** G1. **Dependency posture:** A ledger; B/C/D after source changes. **Accountable role:** Adversarial verification + core.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-047 — Define each public pointer-lifetime invariant

- **Build or investigate:** valid/null/invalid/stale/foreign pointers and unsupported UB clearly distinguished.
- **Acceptance proof:** negative tests do not rely on accidental undefined outcomes.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-048 — Complete D-01/D-02 failure observability classification

- **Build or investigate:** every lock/unlock path fault injected and observed.
- **Acceptance proof:** dangerous errors cannot silently continue an unsafe mutation.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-049 — Resolve integrity probe 19/25 table by risk

- **Build or investigate:** classification, probability and exposure of three by-design silent cases.
- **Acceptance proof:** no promotion of 15/25 old test probe to 25 green.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-050 — Design optional metadata integrity protections

- **Build or investigate:** canaries/encoded links/generation checks chosen by measured threat model.
- **Acceptance proof:** measure false positives, actual detections and overhead.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-051 — Add model-based allocator sequence fuzzing

- **Build or investigate:** malloc/calloc/realloc/align/free/trim/stats/budget combinations.
- **Acceptance proof:** save minimized repro seed and core SHA for every crash.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-052 — Refresh fuzz-spec gaps and corpus coverage

- **Build or investigate:** differential deterministic corpus and 4 KiB/16 KiB invariants.
- **Acceptance proof:** cover changed API surfaces, not just original four.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-053 — Add ASan/UBSan clean builds and separate target warning gates

- **Build or investigate:** distinct debug, hardened, release compile matrices.
- **Acceptance proof:** all actual sanitizer findings triaged; expected negatives isolated.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-054 — Use concurrency schedule stress and race tools where feasible

- **Build or investigate:** bounded randomized interleavings and thread cache teardown.
- **Acceptance proof:** races and deadlocks reproducible or explicitly unsupported.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-055 — Check hostile size arithmetic and offset overflow

- **Build or investigate:** SIZE_MAX, alignment, count multiplications, page rounds.
- **Acceptance proof:** no wrapped allocation or aliasing of live region.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-056 — Measure hardening cost with opt-in/off modes

- **Build or investigate:** same-object/output checksums across four oracle workloads.
- **Acceptance proof:** no hidden default-on performance penalty.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-057 — Write incident response for memory corruption

- **Build or investigate:** crash dump redaction, reproducer preservation, owner notification.
- **Acceptance proof:** reporting does not publish user secrets or dump raw heaps.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-058 — Execute an independent pre-release security review

- **Build or investigate:** reviewed source map, danger list, mitigations and residual risk.
- **Acceptance proof:** unresolved P0 blocks stable scoped release.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream E exit:** reconcile its 12 tickets into a dated review packet; report all blocked/rejected work and why. Gate G1 can only change by its independently reviewed actual run evidence.


---

## 10. Workstream F: Public API, ABI and controlled interposition

**Priority:** P0/P1. **Quality gate:** G1+G4. **Dependency posture:** A scope + E safety. **Accountable role:** Contract/ABI + release.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-059 — Create conformance table for all 13 namespaced functions

- **Build or investigate:** C/C++ calling conventions and success/failure ownership.
- **Acceptance proof:** no API promise exceeds testable behavior.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-060 — Freeze or intentionally evolve experimental ABI by owner decision

- **Build or investigate:** semver/SOVERSION plan and compatibility horizon.
- **Acceptance proof:** no accidental 1.0/SONAME promise.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-061 — Test ABI compatibility against versioned consumers

- **Build or investigate:** shared/static/cross-C++ tests and symbol maps.
- **Acceptance proof:** old consumer behavior explicit and link errors classified.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-062 — Map all GNU/Posix allocator entry points and gaps

- **Build or investigate:** malloc family, memalign/valloc/pvalloc, cfree, introspection, usable size.
- **Acceptance proof:** supported subset stated, no implied universal libc interposition.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-063 — Evaluate D-013 safe interposer compatibility option

- **Build or investigate:** consider separate adapter semantics for zero-size/realloc(p,0) with ownership proof.
- **Acceptance proof:** never change namespaced behavior silently or mask cross-allocator frees.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-064 — Reproduce anchored-regex failure and triage alternatives

- **Build or investigate:** minimal regcomp/grep case under plain/preload.
- **Acceptance proof:** no claim of fixed until actual parity test passes.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-065 — Create interposition compatibility suite by application class

- **Build or investigate:** CLI, Python, Ollama, editors, shell, glibc internal paths.
- **Acceptance proof:** all supported per-process opt-in use cases documented.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-066 — Protect global system processes by never-preload rules

- **Build or investigate:** regression test and documentation preventing environment leakage.
- **Acceptance proof:** no systemd/live desktop/session/database/service alteration.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-067 — Make preload rollback portable and user-visible

- **Build or investigate:** one-variable clear + new process path, with failure drill.
- **Acceptance proof:** rollback tested without relying on a working interposer.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-068 — Prove two-path allocations do not mix ownership

- **Build or investigate:** namespaced and standard symbol route instrumentation.
- **Acceptance proof:** correct deallocation only by compatible family; undefined calls excluded.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-069 — Separate optional diagnostic/hardening ABIs

- **Build or investigate:** namespaced normal/debug/hardened mapping audits.
- **Acceptance proof:** no accidental extra dynamic exports.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-070 — Publish compatibility/ABI decision record

- **Build or investigate:** exact supported subset + regression counters.
- **Acceptance proof:** owner signs explicit consumer support level.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream F exit:** reconcile its 12 tickets into a dated review packet; report all blocked/rejected work and why. Gate G1+G4 can only change by its independently reviewed actual run evidence.


---

## 11. Workstream G: Native Linux and cross-target portability

**Priority:** P1. **Quality gate:** G2. **Dependency posture:** A truth, E/F contracts. **Accountable role:** Platform engineering.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-071 — Define tiered target taxonomy

- **Build or investigate:** HOSTED-VERIFIED, BUILD-ONLY, EMULATED-PROBE, BLOCKED, UNTESTED.
- **Acceptance proof:** no 'supported' based solely on compiler success.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-072 — Requalify Z13 x86-64 4 KiB native suite on clean checkout

- **Build or investigate:** full suite + standalone + ABI + lib build with hashes.
- **Acceptance proof:** run provenance, no hidden skips.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-073 — Requalify AArch64 Linux 16 KiB Raspberry Pi baseline

- **Build or investigate:** updated API on a newer clean core and versioned ABI.
- **Acceptance proof:** fresh run after Z13 refactor, not old M-009 numbers.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-074 — Obtain native AArch64 Linux 4 KiB host proof

- **Build or investigate:** real 4 KiB device and full test suite, with owner-provisioned target.
- **Acceptance proof:** not satisfied by Z13 x86-64 4 KiB.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-075 — Evaluate x86-64 and AArch64 musl build/runtime

- **Build or investigate:** static/shared and thread/errno semantics.
- **Acceptance proof:** do not claim musl support without actual toolchain/runtime.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-076 — Run GCC/Clang matrix with strict warning profiles

- **Build or investigate:** GNU/Linux C11 headers, visibility, LTO/no-LTO, PIC and reproducibility.
- **Acceptance proof:** warnings and codegen bugs accounted for.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-077 — Check architecture-specific word and pointer sizes

- **Build or investigate:** 32-bit assumptions quarantined or explicit exclusion.
- **Acceptance proof:** no unsafe implicit sizeof(pointer)==8 in claimed profile.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-078 — Reproduce 4/16/64 KiB page-size arithmetic in harnesses

- **Build or investigate:** page rounding, alignments, budget and trim bounds.
- **Acceptance proof:** unsupported page size fails cleanly or is explicitly excluded.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-079 — Audit fork, TLS, signal and loader contracts per libc

- **Build or investigate:** record exact unsupported paths; prioritize correctness over emulated completeness.
- **Acceptance proof:** no universal fork-safe claim inferred from one host.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-080 — Publish signed-off per-target qualification packet

- **Build or investigate:** OS/kernel/libc/toolchain/page size + commands + acceptance records.
- **Acceptance proof:** one row per exact target, not one vague 'Linux' label.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream G exit:** reconcile its 10 tickets into a dated review packet; report all blocked/rejected work and why. Gate G2 can only change by its independently reviewed actual run evidence.


---

## 12. Workstream H: Android, Termux and tablet deployment

**Priority:** P1/P2. **Quality gate:** G2+G4. **Dependency posture:** G native contract; permission for toolchains and hardware. **Accountable role:** Android platform + core.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-081 — Obtain owner-approved NDK and device access plan

- **Build or investigate:** specific Android API targets, ABI, physical device and storage budget.
- **Acceptance proof:** no surprise downloads, rooting or system changes.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-082 — Build Android/bionic arm64 research library

- **Build or investigate:** NDK CMake or explicit compatible build with ELF inspection.
- **Acceptance proof:** source compiles and links without assuming glibc internals.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-083 — Test 4 KiB and 16 KiB Android platform packaging

- **Build or investigate:** ELF segment alignment and app loading requirements.
- **Acceptance proof:** native device or approved test farm evidence; no emulated substitution.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-084 — Run on-device allocator correctness suite

- **Build or investigate:** native test executable with writable/zeroing/threads/align/trim.
- **Acceptance proof:** same host/model/log hash and bionic versions filed.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-085 — Run Termux native and proot Linux as distinct profiles

- **Build or investigate:** separate toolchains, libc paths and execution logs.
- **Acceptance proof:** proot cannot certify bionic.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-086 — Reproduce original tablet Mojo/tcmalloc failure exactly

- **Build or investigate:** record previous command, dependency/toolchain versions, first error.
- **Acceptance proof:** determine actual allocator relevance rather than presumed cause.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-087 — Measure foreground app responsiveness and thermal behavior

- **Build or investigate:** battery/sustained CPU, frame responsiveness and memory pressure under controlled ambient conditions.
- **Acceptance proof:** no improvement claim based on single short run.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-088 — Stress Android background suspension, process death and low-memory killer

- **Build or investigate:** ownership and recovery across app lifecycle.
- **Acceptance proof:** clean restart and no corrupted persisted state.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-089 — Audit Android permissions and exported symbols

- **Build or investigate:** no need for system allocator replacement, minimal app integration.
- **Acceptance proof:** no root/LD_PRELOAD broad claims.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-090 — Issue Android support tier decision

- **Build or investigate:** device-specific pass/fail with known constraints.
- **Acceptance proof:** native target support only after device evidence.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G2+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream H exit:** reconcile its 10 tickets into a dated review packet; report all blocked/rejected work and why. Gate G2+G4 can only change by its independently reviewed actual run evidence.


---

## 13. Workstream I: Real Mojo and A.E.S.I.R. integration

**Priority:** P1. **Quality gate:** G5. **Dependency posture:** F ABI safety, G2 target; A.E.S.I.R. maintainer consent. **Accountable role:** AI integration + contracts.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-091 — Re-run Z-087 controlled baseline on current A.E.S.I.R. build

- **Build or investigate:** same compiled CPU engine/Meta-Llama fixture/tokenizer/seed and input.
- **Acceptance proof:** four-way token identity + recorded load state.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-092 — Fix A.E.S.I.R.-side stale/prebuilt binary separately in its repo if authorized

- **Build or investigate:** triage B-087-1 crash, B-087-2 canned stub, B-087-3 empty fixture.
- **Acceptance proof:** no accidental edits to A.E.S.I.R. from MimirAlloc scripts.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-093 — Inventory memory ownership categories in the engine

- **Build or investigate:** tokenizer, GGUF load, tensors, KV, scratch, decoder and FFI.
- **Acceptance proof:** distinguish host heap from mmap, GPU UMA and driver-managed bytes.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-094 — Choose an explicit allocator adapter entry point

- **Build or investigate:** C ABI bridge that routes a named engine buffer to mimir_*.
- **Acceptance proof:** test owns its allocation; no cross-allocator free.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-095 — Instrument actual allocator call path end-to-end

- **Build or investigate:** counter/evidence from engine invocation not only standalone tests.
- **Acceptance proof:** Mimir-owned bytes >0, matching frees, no remaining live leakage.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-096 — Measure same workload with and without adapter

- **Build or investigate:** exact GGUF/model hash, token stream, context, batch, seeds, prefill/decode.
- **Acceptance proof:** no unverified quality or performance claim.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-097 — Contrast adapter vs preload on same code and input

- **Build or investigate:** named route, preload route and glibc-only control; compare wall/p99/RSS.
- **Acceptance proof:** choose explicit adoption if preload remains costly.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-098 — Prove error and cancellation teardown paths

- **Build or investigate:** failed model load, OOM, user cancellation, repeated inference.
- **Acceptance proof:** no lost ownership or truncated output silently accepted.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-099 — Profile model loading separately from inference

- **Build or investigate:** reader throughput, memory map, allocator ops, KV churn, GPU accounting.
- **Acceptance proof:** do not attribute all model-load cost to heap.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-100 — Evaluate Qwen3-1.7B Q4_K_M target experiment if model/engine available

- **Build or investigate:** checksum and engine parity, repeatable short controlled inference.
- **Acceptance proof:** mark OWED/BLOCKED when prerequisites absent.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-101 — Implement owner-selectable fallback to system allocator

- **Build or investigate:** compile/runtime explicit switch with provenance and two mode tests.
- **Acceptance proof:** one toggle restores baseline without lost state.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-102 — Publish A.E.S.I.R. adoption packet

- **Build or investigate:** source SHA, ABI route graph, live call proof, correctness, timing and rollback.
- **Acceptance proof:** only then claim *scoped* integration success.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream I exit:** reconcile its 12 tickets into a dated review packet; report all blocked/rejected work and why. Gate G5 can only change by its independently reviewed actual run evidence.


---

## 14. Workstream J: Performance science, energy and fair competition

**Priority:** P1. **Quality gate:** G3. **Dependency posture:** A evidence, E contracts and performance branches. **Accountable role:** Performance engineering + independent reviewer.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-103 — Define immutable K-002/Z13 workload suite specification

- **Build or investigate:** operations, touch semantics, checksum, warm-up and duration.
- **Acceptance proof:** both allocator legs execute exactly equivalent work.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-104 — Re-run default and tuned glibc comparator on same machine

- **Build or investigate:** tcache, threshold parameter grid with provenance.
- **Acceptance proof:** do not compare tuned Mimir to untuned glibc only.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-105 — Capture runtime call-stack hotspots for Python and Ollama

- **Build or investigate:** allocation sizes, stack attribution and hot return sites.
- **Acceptance proof:** testable hypothesis for each expensive path.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-106 — Add per-op and per-process cold-start measurements

- **Build or investigate:** startup, dlopen, first alloc, 8-class ladder, churn.
- **Acceptance proof:** reproduce Z-097 taxes with confidence intervals.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-107 — Design streaming percentile benchmarks and tail-latency study

- **Build or investigate:** p50/p95/p99/p99.9 and inter-thread jitter under load.
- **Acceptance proof:** show distributions not merely mean throughput.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-108 — Track memory footprint truthfully

- **Build or investigate:** RSS/PSS/USS/mapped/resident/committed/live-requested/retained by class.
- **Acceptance proof:** avoid equating allocator counters with whole-process memory.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-109 — Build long-run fragmentation/perf cross-matrix

- **Build or investigate:** random, FIFO/LIFO, mixed class and adversarial retention traces.
- **Acceptance proof:** report pressure recovery not just peak speed.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-110 — Measure cycles, cache misses, branch misses and syscalls

- **Build or investigate:** perf counters or approved profiler on each host.
- **Acceptance proof:** attribute bottleneck changes vs control.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-111 — Measure energy and bandwidth with instrumentation limits disclosed

- **Build or investigate:** joules/op and peak watts only when meter resolution supports.
- **Acceptance proof:** no invented subresolution efficiency wins.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-112 — Build regression thresholds per supported profile

- **Build or investigate:** predeclare max accepted p99/RSS/throughput change.
- **Acceptance proof:** gate fails if severe regression even when average improves.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-113 — Re-run benchmark after every compiler and link-flag change

- **Build or investigate:** GCC/Clang, LTO, -O2/-O3 flags with compatible object identity.
- **Acceptance proof:** no silent return to earlier Z-035 assumptions.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-114 — Publish final Pareto and non-win appendix

- **Build or investigate:** named workload wins/ties/losses, confidence and full raw runs.
- **Acceptance proof:** a losing workload is visible and given owner/decision.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream J exit:** reconcile its 12 tickets into a dated review packet; report all blocked/rejected work and why. Gate G3 can only change by its independently reviewed actual run evidence.


---

## 15. Workstream K: Memory budgets, telemetry and operations

**Priority:** P1/P2. **Quality gate:** G1+G3. **Dependency posture:** E correctness; D reclaim. **Accountable role:** Core + observability.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-115 — Specify allocated/usable/committed/retained accounting identities

- **Build or investigate:** invariants linking public stats to internal class counters.
- **Acceptance proof:** no double counting cached free spans.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-116 — Test hard/soft budget enforcement under concurrency

- **Build or investigate:** racing allocations, failure-atomic counters and limit changes.
- **Acceptance proof:** no quota bypass and no live block revoked.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-117 — Separate diagnostics from application logging

- **Build or investigate:** opt-in FD/handler with bounded formatting.
- **Acceptance proof:** no private contents leaked, no recursive allocation on failure.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-118 — Add backpressure-safe metrics and snapshots

- **Build or investigate:** versioned stats structure and atomic sampling policies.
- **Acceptance proof:** no inconsistent counter overflow or data races.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-119 — Build cgroup v2 pressure integration candidate

- **Build or investigate:** read-only initial probe and optional pressure-triggered trim.
- **Acceptance proof:** explicit owner policy, no global daemon required.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-120 — Exercise budget + model-load failure on A.E.S.I.R.

- **Build or investigate:** failed load, cancellation, retry, fallback and process isolation.
- **Acceptance proof:** clean error and no silent partial inference.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-121 — Define low-memory profile for Raspberry Pi and Android

- **Build or investigate:** size-class cap/reclaim differences; default only if tested.
- **Acceptance proof:** no desktop tuning imposed blindly.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-122 — Publish metrics cookbook

- **Build or investigate:** minimal reproducible Linux commands and privacy caution.
- **Acceptance proof:** run-level numbers traceable to actual executable.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G1+G3**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream K exit:** reconcile its 8 tickets into a dated review packet; report all blocked/rejected work and why. Gate G1+G3 can only change by its independently reviewed actual run evidence.


---

## 16. Workstream L: CI, build reproducibility and supply chain

**Priority:** P1. **Quality gate:** G0+G4. **Dependency posture:** A source of truth, F version decisions. **Accountable role:** Platform + release.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-123 — Fix proposed cross-x86_64 workflow's true execution path

- **Build or investigate:** QEMU invocation explicitly wired if enabled; no hidden direct execution of cross binaries.
- **Acceptance proof:** manual-only until owner approves triggers.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-124 — Run native x86-64 and cross-AArch64 jobs in an authorized test environment

- **Build or investigate:** capture runner image, compiler and artifact hashes.
- **Acceptance proof:** passing jobs visible by run ID; no inferred execution.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-125 — Design cache-safe deterministic builds

- **Build or investigate:** clean workspace, predictable timestamps, path/debug sections policy.
- **Acceptance proof:** same toolchain 2-run byte-identical artifacts or explicit reproducibility limits.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-126 — Build self-hosted/multi-platform runner isolation plan

- **Build or investigate:** resource caps, no secrets to untrusted PRs, read-only defaults.
- **Acceptance proof:** no public untrusted job can access owner private agents/devices.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-127 — Add ABI/export/version-map regression job

- **Build or investigate:** snapshot 13 namespaced and interposer export allowlist.
- **Acceptance proof:** unexpected standard symbol fails check.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-128 — Add sanitizer/clang/gcc/nightly bounded stress lanes

- **Build or investigate:** clear required vs informational failures and time budgets.
- **Acceptance proof:** failures cannot be silently waived.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-129 — Generate source provenance, SBOM and dependency audit

- **Build or investigate:** record compiler/toolchain version and source SHA per artifact.
- **Acceptance proof:** manifest refers to exact bytes, not unbuilt expectations.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-130 — Pin and audit third-party build actions and dependencies

- **Build or investigate:** immutable action revisions, least privilege, update process.
- **Acceptance proof:** document release trust chain and rollback.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G0+G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream L exit:** reconcile its 8 tickets into a dated review packet; report all blocked/rejected work and why. Gate G0+G4 can only change by its independently reviewed actual run evidence.


---

## 17. Workstream M: Packaging, documentation and production release

**Priority:** P1. **Quality gate:** G4. **Dependency posture:** G1 G2 G3; owner scope. **Accountable role:** Release and knowledge.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-131 — Define library product names and ABI migration policy

- **Build or investigate:** development vs stable SONAME/versioning and exported symbol contract.
- **Acceptance proof:** no accidental binary break without documented major policy.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-132 — Prepare Linux packages in staging prefix

- **Build or investigate:** static/shared libraries, headers, pkg-config or CMake metadata.
- **Acceptance proof:** install/link/run/uninstall complete in clean sandbox.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-133 — Write 'do not globally preload' installation warning

- **Build or investigate:** systemd/desktop/service safety banner in README and release docs.
- **Acceptance proof:** no guidance implies system-wide malloc replacement.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-134 — Provide deterministic release artifact checksum and SBOM

- **Build or investigate:** SHA256SUMS and exact source-to-binary trace.
- **Acceptance proof:** checksums match downloadable artifacts.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-135 — Set security reporting and vulnerability triage policy

- **Build or investigate:** working approved private channel and contact workflow.
- **Acceptance proof:** never invent response SLAs or addresses.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-136 — Build compatibility release notes and migration examples

- **Build or investigate:** C/C++ starter, known D-013 semantics, failure channels.
- **Acceptance proof:** examples tested as shipped with release artifact.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-137 — Audit licensing and original implementation provenance

- **Build or investigate:** file notices, dependency licenses and contributions.
- **Acceptance proof:** no copied allocator algorithm code without license compliance.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-138 — Run independent release-candidate audit

- **Build or investigate:** clean host install + full supported target matrix + adversarial review.
- **Acceptance proof:** no P0, P1 outside documented narrowed scope.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-139 — Publish owner-approved release-scoping decision

- **Build or investigate:** explicit exact support matrix and exclusions with decision date.
- **Acceptance proof:** a release version does not imply Android or AI integration.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-140 — Execute reversible release rehearsal before publishing

- **Build or investigate:** staged artifact push/rollback process documented.
- **Acceptance proof:** no destructive user filesystem changes, version rollback verified.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream M exit:** reconcile its 10 tickets into a dated review packet; report all blocked/rejected work and why. Gate G4 can only change by its independently reviewed actual run evidence.


---

## 18. Workstream N: Ecosystem adoption and developer ergonomics

**Priority:** P2. **Quality gate:** G4+G5. **Dependency posture:** F/M stable scope. **Accountable role:** Bindings + documentation.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-141 — Build standalone C and C++ consumer examples

- **Build or investigate:** link test from outside repo against staged .so and .a.
- **Acceptance proof:** no implicit library-internal headers.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-142 — Add safe allocator ownership guide for FFI

- **Build or investigate:** explicit create/free pairs and foreign-pointer exclusion.
- **Acceptance proof:** no mismatched ownership in examples.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-143 — Test consumption from Mojo with a trivial custom resource

- **Build or investigate:** make minimal deterministic alloc/free bridge before inference.
- **Acceptance proof:** source and artifact reproducible.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-144 — Measure behavior in selected low-memory local AI tools

- **Build or investigate:** named opt-in integration with controlled workload and rollback.
- **Acceptance proof:** owner-approved integration, zero secret or user-data exfiltration.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-145 — Publish target comparison guide against glibc/jemalloc/mimalloc

- **Build or investigate:** cite fair measured workloads and precise target scope.
- **Acceptance proof:** no untested benchmark claims or brand superiority.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-146 — Prepare public contributor issue/PR templates

- **Build or investigate:** one ticket/one proof reproducible with immutable baseline.
- **Acceptance proof:** no mass unreviewed speculative feature patches.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-147 — Publish user-facing limitations and FAQ

- **Build or investigate:** wrong-size operations, pressure, interposition dangers, Android status.
- **Acceptance proof:** consistent with reviewed source and live ledger.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-148 — Create public performance dashboard from stored evidence

- **Build or investigate:** static Markdown or generated plots with checksums/host filters.
- **Acceptance proof:** never merge unlike environments into fake trend.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4+G5**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream N exit:** reconcile its 8 tickets into a dated review packet; report all blocked/rejected work and why. Gate G4+G5 can only change by its independently reviewed actual run evidence.


---

## 19. Workstream O: Optional beyond-production architecture research

**Priority:** P3 optional. **Quality gate:** G6. **Dependency posture:** R1 core safety, owner opt-in, separate module. **Accountable role:** Research + adversarial.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-149 — Define AI-aware allocation taxonomy

- **Build or investigate:** tokenizer, KV, scratch, model mapping, activation and streaming buffers.
- **Acceptance proof:** no assumption all allocation types share same lifetime.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-150 — Prototype KV-cache class tags only via explicit adapter

- **Build or investigate:** tagged handles and policy hints, not global malloc introspection.
- **Acceptance proof:** fallback transparent; equal output and bounded RAM.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-151 — Investigate Strix Halo unified memory effects

- **Build or investigate:** CPU allocator vs Radeon GPU/driver allocations and bandwidth contention.
- **Acceptance proof:** separate physical accounting; no fabricated VRAM ownership.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-152 — Prototype optional pressure-aware class policy

- **Build or investigate:** deterministic bounded policy plus logged rationale.
- **Acceptance proof:** no agent-generated unsafe direct heap mutation.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-153 — Study huge pages/THP under actual AI workloads

- **Build or investigate:** isolate kernel accounting and fault costs.
- **Acceptance proof:** win measured with energy/RSS/p99 and rollback.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-154 — Explore persistent-memory/storage-backed allocations only as explicit handles

- **Build or investigate:** checksum, crash-recovery, versioned backing and access control.
- **Acceptance proof:** never pass remote/persistent memory as ordinary freed C pointers.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-155 — Explore federated/distributed memory coordination

- **Build or investigate:** authorization, failure timeouts, no raw-pointer transparency, encryption.
- **Acceptance proof:** network loss cannot silently corrupt local allocator.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-156 — Build closed-loop autonomous tuning sandbox

- **Build or investigate:** AI proposes flag/config candidates, isolated benchmarks vet them.
- **Acceptance proof:** promotion by deterministic tests + owner policy, not model confidence.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G6**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream O exit:** reconcile its 8 tickets into a dated review packet; report all blocked/rejected work and why. Gate G6 can only change by its independently reviewed actual run evidence.


---

## 20. Workstream P: Maintenance, incident response and sustainability

**Priority:** P1/P2. **Quality gate:** G4 onward. **Dependency posture:** first scoped release. **Accountable role:** Keeper + release/security.

The items below are ordered by likely dependency, but research and review can overlap; `PBR` numbers are planning identifiers, not banked achievements.

### PBR-157 — Define support policy by OS/libc/page-size/version

- **Build or investigate:** explicit supported and excluded profiles.
- **Acceptance proof:** no universal compatibility promise.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-158 — Automate issue-to-reproducer record

- **Build or investigate:** seed/stack/host/toolchain/artifact hash with privacy filter.
- **Acceptance proof:** one-command local replay of every retained severe defect.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-159 — Create backport and release branch rules

- **Build or investigate:** security fixes vs performance features, ABI policy.
- **Acceptance proof:** stable branch no accidental experimental features.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-160 — Establish performance drift monitoring

- **Build or investigate:** periodic manual/scheduled benchmark on reserved host only if approved.
- **Acceptance proof:** new hardware/kernel results isolated from historic trends.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-161 — Conduct periodic threat-model review

- **Build or investigate:** new APIs, agents, dependencies, telemetry and memory corruption vectors.
- **Acceptance proof:** unmitigated critical risk escalated.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-162 — Update user documentation with evidence deltas

- **Build or investigate:** add dated changelog and backward-looking status.
- **Acceptance proof:** historical exit packets preserved and live status clear.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-163 — Harden review of external contributions

- **Build or investigate:** repro tests, provenance, restricted CI, minimal permission.
- **Acceptance proof:** no risky code merge solely from green summary.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

### PBR-164 — Maintain fail-safe retirement plan

- **Build or investigate:** unsupported artifact warning, last known-good and migration.
- **Acceptance proof:** users not left with silently broken allocator packages.
- **Artifacts:** a bounded source/test/docs change or a clearly labelled negative result; command transcript, toolchain/host/commit identity, explicit PASS/FAIL or WIN/TIE/LOSS, rejected alternatives and rollback if code changes. The keeper must map the result to **G4 onward**, update the live ledger, and refuse to call a proposed or merely compiled item verified.

**Workstream P exit:** reconcile its 8 tickets into a dated review packet; report all blocked/rejected work and why. Gate G4 onward can only change by its independently reviewed actual run evidence.

---

## 21. Cross-cutting acceptance matrices

### 21.1 Core API behavior (repeat on every claimed native target)

| API family | Positive evidence | Negative/failure evidence | Scope caveat |
|---|---|---|---|
| malloc/free | nonoverlap, correct alignment, roundtrip, reuse | OOM/oversize NULL, free(NULL), exact release | invalid/free-foreign pointer may remain unsupported UB |
| calloc/realloc/reallocarray | zeroed user range, preserved prefix, unchanged old block on failure | multiplication overflow, zero-size D-013, forced mmap failures | do not use glibc realloc(p,0) assumption |
| aligned_alloc/posix_memalign/memalign/valloc/pvalloc | modulus, invalid align refusal, page rounding, normal free | overflow, impossible alignment, null memptr | POSIX-ish entry returns error code; errno is not universal |
| usable_size | correct minimum, never below requested, consistent on a live block | NULL/unknown ptr contract as specified | do not inspect arbitrary stale pointer |
| stats_snapshot | structure version, counts and monotonic counters | races, unknown version, NULL out | no stale runtime promises |
| trim | idempotent, retained policy and live preservation | lock failure, positive pad no unpromised mapping pool drain | correctness over reclaim quantity |
| budget | quota enforcement and atomic accounting | over-budget, pressure and recovery, races | quota not OS process memory limit |
| interposer | true binding into tested processes; exact checksum, rollback | D-013 anchored regex divergence and unsafe global-use prohibition | entirely separate **experimental** product |

### 21.2 Test dimensions

| Dimension | Mandatory examples | Evidence note |
|---|---|---|
| Size | 0, 1, 7, 15, 16, 17, 31, 32, 63, 64, 65, 255, 256, 257, 1023, 1024, 1025, 4 KiB, 64 KiB, MiB, huge | adjust to actual class boundaries, not illustrative assumptions |
| Lifetime | LIFO, FIFO, random, alternation, long-lived-pin and high-churn | enforce distinct pointer ownership |
| Threads | 1, 2, 4, 8, 16, 32; cross-thread free, thread-exit, fork | target-specific CPU affinity and topology |
| Faults | malloc mapping failure, metadata allocation failure, lock/unlock failure, invalid sizes | no unbounded partial success; report exact failure outcome |
| Memory pressure | no pressure, capped cgroup, trim storms, allocator under other application load | never kill shared services |
| Platform | x86-64 glibc 4 KiB, AArch64 glibc 16 KiB, AArch64 glibc 4 KiB, musl, bionic/Termux as permitted | native vs emulated vs compile-only grade must be unambiguous |
| Performance | small_churn, mixed_batch, large_batch, mixed_lifecycle, burst depth, real trace replay, Python, Ollama, A.E.S.I.R. | same-work hash, tuned glibc, raw medians and drift |

### 21.3 Standard benchmark comparison table template

| Workload + model/size | Host + ABI + page | glibc default | glibc tuned | old Mimir | candidate Mimir | p95/p99 | peak PSS/RSS | faults/energy | checksum | verdict |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| *Fill from a specific run; never pre-populate a proposed metric* | *exact configuration* | | | | | | | | | |

All metrics must report units and iteration counts. A “win” requires the predeclared threshold, acceptable measurement drift, and no unacceptable memory/reliability regression. If the comparator has a drift-rejected median, either repeat or mark factor *indicative* and narrow the grade.

---

## 22. The first thirty actionable decisions (critical path)

The following order is a *dependency-first suggestion*, not a claim that work is undone if a fresh agent later proves it already landed.

| Order | Ticket or existing campaign task | What the keeper can dispatch without owner interaction | Stop or escalation |
|---|---|---|---|
| 1 | Z-098 | fair glibc tuning experiments within stated host limits | tools/unbounded CPU require approval |
| 2 | Z-099 | wave-11 verdict from actual committed evidence | missing Z-094/095 parity must be named |
| 3 | PBR-001 | dynamic ABI inventory and contract diff | any ABI freeze requires owner choice |
| 4 | PBR-002 | historical debt ledger import | do not erase past failures |
| 5 | PBR-003 | lock-error status regrading | critical silent failure -> P0 |
| 6 | PBR-004 | g008 current repro and negative history | tests changed only after independent review |
| 7 | PBR-005 | per-target support reconciliation | no Android native claim |
| 8 | PBR-006 | update live limitations appendix | preserve old source packet |
| 9 | PBR-007 | exhaustive test inventory / expected-negative classification | no summary-only GREEN |
| 10 | PBR-008 | Z13 slice-to-code evidence map | keep Z13 keeper's commits untouched |
| 11 | PBR-009 | revalidate trace tuning against canonical captures | code/flag changes if warranted need gate |
| 12 | PBR-010 | D-013 vs glibc contract matrix | no unapproved interposer semantic change |
| 13 | PBR-012 | run evidence registry prototype | no secret paths or private logs published |
| 14 | PBR-013 | profile hot path and validate hypotheses | heavy run is serialized |
| 15 | PBR-014 | pointer-to-span ownership model | no source patch before invariant review |
| 16 | PBR-015 | radix index isolated prototype | P0 on unsafe metadata pointer |
| 17 | PBR-016 | alternate guarded index | do not merge mutually exclusive implementations |
| 18 | PBR-017 | index bakeoff vs shipped | reject one-metric wins |
| 19 | PBR-018 | occupancy bitmap design | correctness under all interleavings |
| 20 | PBR-019 | guarded allocation path adoption candidate | complete standing/standalone suite |
| 21 | PBR-020 | guarded free path adoption candidate | ensure no silent ownership error |
| 22 | PBR-026 | final hot-path closure report | measured gain or documented rejection |
| 23 | PBR-027–PBR-036 | thread/remote free work | race and lifetime invariants |
| 24 | PBR-047–PBR-058 | adversarial safety/fuzz | severe bug blocks release |
| 25 | Z-100–Z-104 | long soak, chaos, sanitizer matrix | shared-host guard / owner resource cap |
| 26 | Z-105–Z-108 | release, docs, final head-to-head/verdict | no production rebranding |
| 27 | Z-109–Z-117 | reserve/closure as evidence selects | no guessing remaining slice themes |
| 28 | PBR-059–PBR-070 | ABI + interposition containment | contract freeze needs owner |
| 29 | PBR-071–PBR-090 | Linux and Android target work | physical device / NDK approval |
| 30 | PBR-091 onward | app proof, performance, releases and longer-term extensions | G1–G4 and later G5/G6 |

**Note:** the numbered PBR ranges above are illustrative ordering helpers; the *individual headings* in workstreams A–P are the authoritative titles. Any revision to the count/order should update the critical path automatically and must never mark an ID banked when it has merely been planned.

### 22.1 Long-form slice review checklist

- [ ] Baseline SHA and exact target recorded.
- [ ] Relevant old defect/decision IDs linked.
- [ ] Contract or test for an intentional change reviewed *before* code.
- [ ] Minimal implementation chosen after alternate assessment.
- [ ] Build and static warnings pass on every advertised target.
- [ ] New positive/negative tests pass, with expected skips kept explicit.
- [ ] Full standing gate and relevant standalone suites pass *on the merged tree*.
- [ ] Sanitizer/fault and concurrency checks run or recorded as OWED/BLOCKED.
- [ ] Bench comparison uses identical checksums and fair comparator.
- [ ] No unacceptable RSS/retention/p99/energy regression.
- [ ] Public ABI/export map and version intent reviewed.
- [ ] No cross-allocator pointer lifetime violation.
- [ ] Rollback and incident observation documented.
- [ ] Independent adversarial review recorded.
- [ ] Logs+artifact SHA+host identity and limitations filed.
- [ ] Decision ledger, live debt and supported-target grades updated.
- [ ] Documentation claims are no broader than the result.
- [ ] Owner-only changes, publication and system modifications held for approval.

---

## 23. Failure modes and stop conditions

**Immediately suspend the affected candidate and investigate** when: an allocated pointer overlaps a distinct live allocation; zeroed calloc bytes are dirty; reallocation loses the preserved prefix; a lock failure permits mutation with unknown ownership; a release of an empty span can unmap another live span; a cross-thread free duplicates or drops ownership; statistics overflow to misleading values; corrupt metadata is silently trusted on a path advertised as checked; any systemd/display/login service inherits a preloaded experimental allocator; benchmark checksums differ; a nightly stress job runs beyond its approved memory/time cap; an artifact hash disagrees with its manifest; an experiment accesses or exposes private model/user data outside explicitly authorized boundaries.

**A test failure is useful information**, but it is **not a passing gate**. Preserve minimized repro cases, clear scope, severity, corrected documentation and an explicit next assignment. Benchmarks and deployment tests are never license to risk the shared development host or its other agents/services.

---

## 24. Owner-decision register (do not assume)

The keeper should gather these into one review packet rather than repeatedly interrupting normal bounded engineering:

| Decision | Safe default until approval |
|---|---|
| Strict historic exit or scoped Linux-only initial release | no stable release designation |
| Public API/ABI freeze, semver/SOVERSION | experimental contract, no ABI guarantee |
| Toolchain/NDK/musl provisioning and Android device access | research and source-only work |
| CI policy/manual vs push/PR automation | manual-only; no deployment hooks |
| Local machine resource caps for soaks and repeated stress | conservative bounded tests or skipped with reason |
| Any public releases/signatures/security contact | staged offline artifacts only |
| Any new A.E.S.I.R. repository changes | read-only diagnostic trials |
| Interposer compatibility contract change | preserve D-013 namespaced contract and explicit unsafe-use warning |
| Telemetry/data transfers or agent-run remote access | local/private by default |
| Optional accelerator/distributed memory module shipping | off by default and clearly experimental |

---

## 25. Evidence-index and roadmap maintenance policy

Every milestone review must contain:

1. **What changed:** PR/commit SHA, complete changed file list, tag/branch, code path and linked `PBR-` or `Z-` task.
2. **What was proved:** test names, counts, raw logs, platform, flags, model/hash, deterministic checksums, repeated runs and confidence.
3. **What is still lost:** named workload, slowdown/overhead, residual danger, unverified target, and the next experiment that could change the grade.
4. **What was rejected:** candidate architectures and why, including measured regressions, so future agents do not rediscover and reintroduce them.
5. **What permissions remain needed:** owner decisions, external downloads, hosted service impacts, publishing/signing and privacy changes.
6. **What will happen next:** dependency-ready slice IDs, assigned role(s), resource cap, stopping condition and an owner-facing succinct update.

**Documents to maintain with the code, not only at the end:** `docs/DECISION_LEDGER.md`, `docs/LIMITATIONS_AND_KNOWN_DEFECTS.md` (dated appendices), `docs/SUPPORTED_TARGETS.md`, `docs/EVIDENCE_LEDGER.md`, Z13 final packets, and the top-level README. Historical packets may be superseded by a new dated report but never rewritten to conceal their original verdict.

### Definition of success

The first victory is **not** “MimirAlloc beats glibc everywhere.” It is a small allocator with verified invariants, repeatable wins or justified trade-offs in specifically useful workloads, bounded memory and failure behavior, a documented reversible adoption route, and a credible maintained release. The larger vision then adds real Mojo/A.E.S.I.R. adoption, physical-device Android proof, and independently gated research into intelligent, heterogeneous and distributed memory.

**Forge law:** the project advances when evidence earns a new claim. A clear LOSS with a named root cause is a useful completed investigation; claiming a win without the run is not.
