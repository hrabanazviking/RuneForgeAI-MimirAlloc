# MimirAlloc — Phase 2 Evaluation Plan: Lower-Cost Compatibility Routes

## Document control

| Field | Value |
|---|---|
| Deliverable | B-001 — Phase 2 evaluation design (overall evaluation plan) |
| Roadmap source | Section 8, Phase 2; Sections 11 and 15 as relevant; Wave 1 deliverables on main |
| Status | Proposed — plan only. No route in this document is a result. |
| Implementation status | None claimed. No allocator was built, selected, patched, or measured for effect in this slice. |
| Worktree / branch | mimiralloc-team-01 / yrsa/mimiralloc-01, base 43ae545ed797d300cbd09239a7d123ee2efb644d |

> **Honesty banner.** This is a design document. Every measured-result field is blank / TBD until a fresh-process run on a named device, with pinned versions and a preserved raw log, actually happens. The only device facts claimed are the light inventory probes in Section 3, quoted verbatim with their commands. Comparators are diagnostics only; they do not cancel the independent MimirAlloc project.

## 1. Purpose, scope, and reading rules

### 1.1 Purpose

Roadmap Section 8, Phase 2, verbatim task: "Evaluate system allocator, configurable build flags, a local patch, and established allocators such as mimalloc as controlled diagnostic comparators. Record licenses, linking constraints and outcomes. These experiments do not cancel the user's independent project; they establish whether a simpler route can unblock Aesir while MimirAlloc is built. Distinguish LD_PRELOAD, direct link, static replacement and namespaced API."

Exit, verbatim: "table of options with commands, measured results and integration limitations. No claims based on names alone."

This plan defines, per route in roadmap order: exact questions, method and commands (design), licenses and linking constraints to record, the four integration modes distinguished, measured-result fields, and integration-limitation fields, all feeding the Phase 2 exit table in Section 9.

### 1.2 Prerequisites and dependency on Phase 1

Phase 2 evaluates routes against the Phase 1 failure. Until a Phase 1 failure packet exists — repeatable failing command, first relevant error and exit code, exact host/target, tcmalloc and Mojo revisions, causal hypothesis — route runs can only be scoped, not scored. Wave 1 BASELINE_INVENTORY Section 7 verdict, quoted: "Not ready, and honestly so." Its readiness items 1-5 for a first comparator/target are: (1) pinned builds with consistent configuration, (2) selection proof, (3) harness/script/result schema, (4) workloads, (5) per-run environment manifest. Recommended first pair (recommendation, not result): B-01 platform glibc on Linux AArch64 + glibc.

### 1.3 Evidence labels

Roadmap Section 2 labels, used throughout: Proposed / Implemented / Host-tested / Device-tested / Integration-proven / Performance-measured / Blocked. No promotion: an emulator result is not device proof, a synthetic test is not application compatibility, a successful compile is not proof of runtime interposition. Routes P2-R01, P2-R02, and P2-R04 start at Proposed; P2-R03 starts at Blocked, because its Phase 1 failure packet does not exist yet (Section 7).

### 1.4 Open questions this plan must answer or preserve

From Roadmap Section 15: Q2 Which process must use the replacement? Q3 Is tcmalloc statically or dynamically linked? Q4 Which API and ABI symbols are required? Q10 What existing allocators can unblock the build now? Test objectively without abandoning the independent project.

From contracts.md (A-008): the core candidate symbol set is Proposed only — malloc, free, calloc, realloc, aligned_alloc, posix_memalign; conditional candidates reallocarray, malloc_usable_size, and the C++ operator new/delete family only if a target requires them. Ownership rule: free only by a compatible allocator; no silent fallback to a different allocator. Final symbol set is open (U-01 / Q4) and must not be silently filled by any route run.

### 1.5 Targets (from SUPPORTED_TARGETS.md, A-003)

No target is supported yet; every target row is Proposed, labelled Planned / Investigational only. First production candidate: Linux AArch64 + glibc (native CI, multithread tests, ABI and linker proof). Linux x86-64 + glibc is the dev/reference target. Linux AArch64 + musl, Android ARM64 + bionic, Termux, and proot are separate rows and never merged with a glibc result; proot is not native Android app proof. Page size is not one value: Samsung SM-X400 recorded as 4,096 B in SUPPORTED_TARGETS.md (prior Wave 1 summary, not measured in this slice), this Pi observed 16,384 B (Section 3), Android also ships 16 KiB configurations. A Pi / Linux-AArch64-glibc result is never an Android, bionic, musl, or tablet result.

## 2. Route inventory

| ID | Route | Roadmap order |
|---|---|---|
| P2-R01 | System allocator (platform libc, no replacement) | 1 |
| P2-R02 | Configurable build flags (same source, same allocator) | 2 |
| P2-R03 | Local patch (consumer/toolchain source, allocator selection, or allocator implementation) | 3 |
| P2-R04 | Established allocators as controlled diagnostic comparators: mimalloc, jemalloc, tcmalloc | 4 |

Pinned tcmalloc is also a Section 11 baseline comparator; in P2-R04 it is treated additionally as the implicated allocator (see Section 8.3). MimirAlloc itself is not a Phase 2 route: it does not exist as a verified implementation until Phase 3 (see inconsistency INC-3).

## 3. Evidence actually run on the Pi in this slice (light inventory only)

Guard check first (Samtal lives on this Pi, port 8787; loads kept light):

```
$ free -g
Mem: 15 total, 2 used, 13 available. Swap: 15 total, 1 used.
Samtal listening on 0.0.0.0:8787 (python3) — not touched, loaded, or restarted.
```

Toolchain and environment probes (verbatim outputs):

```
$ ldd --version | head -n 2
ldd (Debian GLIBC 2.36-9+rpt2+deb12u14) 2.36
$ getconf PAGESIZE -> 16384
$ getconf _NPROCESSORS_ONLN -> 4
$ uname -m -> aarch64 ; uname -r -> 6.12.112-v8-16k-hailo+
$ gcc --version -> gcc (Debian 12.2.0-14+deb12u1) 12.2.0
$ ld --version -> GNU ld (GNU Binutils for Debian) 2.40
$ cat /proc/sys/vm/overcommit_memory -> 0
$ cat /sys/kernel/mm/transparent_hugepage/enabled -> always [madvise] never
$ env | grep -i -E 'MALLOC|LD_PRELOAD|GLIBC_TUNABLES' -> no malloc/LD_PRELOAD/GLIBC_TUNABLES env set
```

Allocator presence probes (inventory only — presence is not selection, configuration, or measurement):

```
$ ldconfig -p | grep -i mimalloc  -> [no output] exit 1
$ ldconfig -p | grep -i jemalloc  -> libjemalloc.so.2 (libc6,AArch64) => /lib/aarch64-linux-gnu/libjemalloc.so.2
$ ldconfig -p | grep -i tcmalloc  -> [no output] exit 1
$ dpkg -l | grep -i jemalloc -> ii libjemalloc2:arm64 5.3.0-1 arm64 general-purpose scalable concurrent malloc(3) implementation
$ dpkg -l | grep -i -E 'mimalloc|tcmalloc' -> [no output] exit 1
```

This matches BASELINE_INVENTORY Section 5. No allocator was built, preloaded, linked, or measured in this slice. Everything in Sections 4-8 beyond this section is DESIGN ONLY and marked UNRUN where commands are given.

## 4. Common evaluation protocol (every route, per Roadmap Section 11)

1. Verify each allocator was actually selected: record dynamic vs static linkage and exported/unresolved symbol evidence; runtime binding evidence (loader trace, /proc/<pid>/maps, or allocation counter/audit hook) for any interposition claim. Name match, package presence, or compile success is never selection proof.
2. Fresh process runs; pin versions; record thermal state, core selection, clock policy, workload, and repetitions in a per-run environment manifest.
3. Report median, p95/p99 and outliers for allocation latency and throughput; peak RSS/PSS; virtual size; retained memory after free; mapping count; startup time; CPU time; responsiveness and energy where reliably instrumented.
4. Benchmark classes (Section 11): 1 single-thread small alloc/free, 2 producer/consumer and cross-thread free, 3 thread churn / big.LITTLE clusters, 4 large allocations and mixed lifetimes, 5 fragmentation / return-to-OS / startup-shutdown. Class 6 (real Mojo build) and class 7 (Aesir load/prefill/decode/unload) only where integration is proven for that mode. Class 8 is tablet-only.
5. One variable at a time, baseline interleaved (A-B-A), warm-up discarded, identical harness/input/settings across routes. Tuned vs untuned and system-package vs pinned-build are separate rows.
6. Store raw data, harness revision, scripts, and negative results; flag noise rather than invent certainty. Never call swap physical RAM. A lower allocation-time number that increases memory pressure or UI stutter does not satisfy the mobile objective.
7. On this Pi: check free -g before and after any run; stop if available memory falls materially; never run a stress harness concurrently with Samtal serving.

## 5. Route P2-R01 — System allocator

- **Route ID / Status / Evidence label:** P2-R01 / Proposed / Proposed. Definition: keep glibc 2.36 ptmalloc in libc.so.6, no replacement library. Change only runtime tunables, per-process environment, and — if the target owns its source — mallopt/malloc_trim calls. This is the control/baseline route and the lowest-risk intervention route.
- **Questions:** (1) What is the unmodified baseline: which binary/process, dynamic or static (ldd), any MALLOC_*/GLIBC_TUNABLES already set? (2) Is the workload allocation-bound at all (count, bytes, size mix, lifetimes, contention), or is tuning a no-op? (3) Fragmentation/retention: RSS vs live heap, arena count on this 4-core box, mmap threshold, trim behaviour. (4) Which tunables exist in glibc 2.36 specifically (glibc.malloc.arena_max, mxfast, tcache_count, trim_threshold, top_pad, mmap_threshold, hugetlb, check) and which are restricted under AT_SECURE/setuid? (5) Does the 16 KiB page size change sensible defaults vs 4 KiB assumptions? (6) Does tcache/arena multiplication inflate RSS on a Pi shared with Samtal? (7) Is every change reversible by unsetting one env var and restarting one process? (8) Which debug settings (MALLOC_CHECK_, libc_malloc_debug.so) must never ship in production?
- **Method / commands (DESIGN ONLY, UNRUN unless Section 3):**

```bash
ldd <TARGET_BINARY>; file <TARGET_BINARY>                       # UNRUN baseline identity
getconf GNU_LIBC_VERSION; ld.so --list-tunables | grep -i malloc # UNRUN verify vs glibc 2.36
GLIBC_TUNABLES=glibc.malloc.arena_max=1 ./<harness>              # UNRUN one tunable per run
GLIBC_TUNABLES=glibc.malloc.tcache_count=0 ./<harness>           # UNRUN
# UNRUN in-process reporting the target must emit itself: malloc_info, malloc_stats,
# mallinfo2, /proc/<pid>/status VmRSS/VmHWM, /proc/<pid>/smaps_rollup
LD_PRELOAD=/usr/lib/aarch64-linux-gnu/libc_malloc_debug.so.0 ./<harness> # UNRUN debug only
```

- **License / linking to record:** glibc license GNU LGPL-2.1-or-later; using the system shared libc.so.6 unmodified creates no new distribution obligation for the target, but record glibc + Debian package version (2.36-9+rpt2+deb12u14, Section 3). No source copied, no allocator vendored. mallopt/malloc_trim/malloc_info are glibc-specific, non-POSIX: source using them needs a portability guard, recorded as a constraint. Env tunables are not a linking act.
- **Integration modes:** LD_PRELOAD — No by definition (nothing preloaded; sole exception is the debug library above, diagnostics only). Direct link — No change (target already links libc.so.6 dynamically, to be confirmed per target, UNRUN). Static replacement — No; a statically linked target would defeat env tunables and must be flagged if found. Namespaced API — Partial, the only code surface: the glibc mallopt-family API, isolated behind a project wrapper so it can be compiled out; env-only use has zero API surface.
- **Measured-result fields (ALL TBD):** Target binary/commit TBD | Workload/harness/input TBD | Baseline wall/usr/sys, VmHWM, VmRSS end, allocs, bytes, arena count TBD | Variant (one named tunable + value) TBD | Variant metrics TBD | Delta % (computed, not estimated) TBD | Repetitions/median/spread TBD | Correctness result TBD | Samtal :8787 impact TBD. Any filled value must cite its raw log file + timestamp.
- **Integration-limitation fields:** Tunable ignored (static binary / AT_SECURE) TBD per target | Tunable absent in glibc 2.36 TBD | RSS not returned to OS TBD | Process-local effect only — CONFIRMED BY DESIGN | 16 KiB-page interaction untested TBD | Multi-thread arena scaling untested TBD | Rollback: unset env / remove mallopt call, restart process only. **Does not establish:** anything about a replacement allocator, Android/bionic, or musl.

## 6. Route P2-R02 — Configurable build flags

- **Route ID / Status / Evidence label:** P2-R02 / Proposed / Proposed. Definition: keep source and allocator the same; rebuild the target only, with different compiler/linker/build flags, compared against the P2-R01 baseline. If a flag set pulls in a different allocator, that experiment belongs to P2-R04, not here.
- **Questions:** (1) What is the pristine build exactly (version output, build log, readelf .comment, hardening check)? A comparison with an unknown baseline is worthless. (2) Candidate flag families and each hypothesis: -O2 vs -O3, LTO, -fno-plt, stack protector cost, -DNDEBUG, section GC, relro/now, PIE, PGO. (3) -march: native is non-portable; record the deployment target and use an explicit baseline (e.g. armv8-a) unless native is justified. (4) Page size: do -Wl,-z,max-page-size=16384 / common-page-size matter here, and would a 16 KiB-aligned binary break on a 4 KiB machine? (5) Do --as-needed, link order, -Bsymbolic, or -z now silently change which malloc resolves — an accidental allocator change that invalidates the comparison? (6) Cost of -g and sanitizers (diagnostic only, never production) and hardening flags. (7) Reproducibility: same GCC 12.2.0 / ld 2.40 (Section 3), same source commit, flags verbatim in the build log. (8) If the target is interpreted (e.g. Python), there is no C build-flag route for it as deployed; this route then covers only a rebuilt interpreter/extension and must be stated as such.
- **Method / commands (DESIGN ONLY, ALL UNRUN):**

```bash
<TARGET_BINARY> --version; readelf -p .comment <TARGET_BINARY>; readelf -h <TARGET_BINARY>
# one flag family per build, clean tree each time; never mix objects across flag sets
CFLAGS_CAND="-O3 -flto -fno-plt"   # UNRUN example candidate set
LDFLAGS_CAND="-Wl,-O1 -Wl,-z,relro -Wl,-z,now -Wl,--gc-sections"  # UNRUN
# PGO is a two-build sequence with representative training input
ldd <TARGET_BINARY>; file <TARGET_BINARY>; sha256sum <TARGET_BINARY>  # UNRUN verify before measuring
```

Builds are CPU-heavy on 4 cores: future runs need a free -g + load check first and must not starve Samtal.
- **License / linking to record:** GCC 12.2.0 (GPL-3.0 with GCC Runtime Library Exception) and GNU ld 2.40 (GPL-3.0); record reliance on the runtime exception, not as legal advice. LTO/PGO add no third-party code, but -static / -static-libgcc / -static-libstdc++ change the license and distribution analysis (glibc LGPL relinkability, bundled-runtime notices) and must be re-checked per variant. Stripping symbols does not strip license/NOTICE files. Record the exact flag set + toolchain versions in the build manifest for every shipped binary.
- **Integration modes:** LD_PRELOAD — No (nothing injected; if a comparison needs it, it is not a flags experiment). Direct link — Yes, primary mode: flags act at compile/link time. Static replacement — Conditional/flagged only if a candidate statically links an allocator/runtime; default candidate set is dynamic; any static variant needs its own license + limitation sign-off and defeats P2-R01 tunables. Namespaced API — No (source unchanged by definition; a required source change belongs to P2-R01).
- **Measured-result fields (ALL TBD except toolchain, quoted in Section 3):** Source commit/tree TBD | Baseline flags verbatim TBD | Candidate flags (one family) TBD | Binary size/sha256/ldd diff TBD | Workload (must match P2-R01 harness) TBD | Baseline and candidate wall/usr/sys/VmHWM/VmRSS TBD | Delta % TBD | Repetitions/median/spread TBD | Correctness TBD | Build time / build RSS on Pi TBD | Samtal impact during build TBD.
- **Integration-limitation fields:** Interpreted target — no applicable flags TBD/check first | -march=native non-portable TBD | 16 KiB alignment breaks 4 KiB machines TBD | PGO training data unrepresentative TBD | Flag silently changed malloc resolution TBD (ldd/symbol check per build) | Hardening cost unmeasured TBD | Static variant defeats tunables + LGPL relink duty TBD if proposed | Rollback: rebuild with recorded baseline flags / redeploy baseline binary by sha256. **Does not establish:** allocator superiority of any kind.

## 7. Route P2-R03 — Local patch

- **Route ID / Status / Evidence label:** P2-R03 / Blocked / Blocked — resumption point: a Phase 1 failure packet naming the failure, consumer, pinned component revision, and candidate patch locus. No patch has been identified, written, applied, built, or measured in this slice. Per INTEGRATION_LAYER_MAP.md (A-007) that packet does not yet exist and the Section 3.3 gate is Blocked; a patch aimed at an unnamed failure is speculation, not a route.
- **Definition and sub-routes (record separately, never conflated):** C1 consumer/toolchain source patch (source file, platform macro, build target, linker script). C2 allocator-selection patch (which allocator is selected: link line/order, build flag naming a different library, baked-in preload config). C3 allocator-implementation patch (pinned upstream allocator revision plus a local diff). A success here is a diagnostic conclusion only; it produces no MimirAlloc evidence and does not cancel the independent project.
- **Questions:** Identity — (1) What exactly is patched, named by path and pinned revision? If it cannot be named, the route stops. (2) Which consumer does it reach (build tool, compiler, runtime/stdlib, generated executable, Aesir — Roadmap Section 3.3), with what symbol/linkage evidence? (3) Which sub-route, and what is the smallest diff testing the Phase 1 causal hypothesis? (4) What verbatim failure is it predicted to change, and what observable change counts as success, decided before trying? Upstream vs local — (5) Does an upstream fix exist? Record project, issue/PR identifier, status (open/merged/declined/abandoned/superseded), revision, date checked; for tcmalloc also its exact revision, build options, local modifications (Roadmap Section 3.1). (6) If upstream merged an equivalent fix, why patch locally instead of taking that revision? A maintainer declining a patch is not itself a license restriction (Ruleset Section 3.2). (7) Backport, original local change, or third-party-derived? This determines license/provenance treatment. (8) Local-only or distributable? Maintainability — (9) Divergence measured from the actual diff (git diff --stat), not estimated. (10) Rebase owner and cadence, re-test duty per rebase. (11) Sunset criterion (upstream release/fix or revision); a patch with none is an open limitation. (12) Does it touch a private/internal interface upstream may change without notice? Record as fragility. (13) Carried as one separable patch file against a pristine pinned tree (preferred), or intermingled (limitation: cannot be reverted/audited independently)?
- **Method / commands (DESIGN ONLY, none executed for this route):**

```text
git rev-parse HEAD ; git status --short            # pin pristine baseline
git worktree add ../patch-route-C-<id> <pinned>     # isolate; never patch the only clean tree
<verbatim failing command from Phase 1 packet> ; echo exit:$?   # baseline FIRST
git apply --check <patch-file> ; git apply --stat <patch-file>  # dry-run + divergence record
git apply <patch-file> ; <recorded clean-build command> ; echo exit:$?
readelf -d <binary> ; nm -D <binary> ; ldd <binary>  # prove reach, not just success
<verbatim failing command> ; echo exit:$?           # identical command, patched
git diff > <patch-artifact> ; sha256sum <patch-artifact>
```

Design rules: identical command/environment/input for baseline and patched runs — only the patch differs; compile success is not runtime effect; if the dry-run fails, record Blocked with that output, do not silently hand-edit and retry.
- **License / linking to record (ALL TBD until a target is named):** Patched component + pinned revision + source origin TBD | License identified from the component itself (name + license-file path + hash) TBD | Patch license if third-party-derived TBD | Notices/attribution TBD | Local-only vs distributed TBD | Compatibility with the project license — settled MIT (LICENSE, (c) 2026 Volmarr Wyrd); MIT governs project code and does not license third-party material to the project — record, do not assume TBD | Linking constraint (static/dynamic/header-only; conditions triggered by static linking, modification, or distribution) TBD | Symbols added/removed/renamed/versioned TBD | Patent/trademark flags TBD (Ruleset Section 9.1; no branding implying Google/Microsoft affiliation or confusable with mimalloc). This records license facts; it is not a legal opinion.
- **Clean-room / provenance (per CLEAN_ROOM_RULESET.md):** A licensed patch is licensed reuse, not clean-room implementation and not contamination — if recorded first: identified license permitting the use, conditions known before use, notices carried, provenance-ledger entry before first reliance (Sections 2.7, 5.4, 9.2); attribution added afterwards does not cure unlicensed copying. Material with unknown/unrecorded license is forbidden until identified (Section 3.1(e)); "found online" is not a license. Third-party implementation source must never be copied or paraphrased into MimirAlloc spec, code, comments, tests, or agent prompts (Section 3.1(a)-(b)); a patch may inform a diagnostic conclusion and dated behaviour observations only. If tcmalloc is patched: its source stays Reference-Team-only (Section 3.2). Contamination trigger: STOP, mark QUARANTINED, remove and seal, assess, remediate from the clean spec (Section 7.2); never quietly edit a quarantined artifact clean (Section 7.3). Never describe this route as making MimirAlloc clean-room or the project as a legally verified two-team clean room (Section 1.4); log role switches and any separation-limited downgrade (Section 4.4).
- **Integration modes:** LD_PRELOAD — patch the preload library or the build/launch config that preloads it; proof = loader binding evidence for the target process; limitation: ineffective for static executables, hidden symbols, secure-execution contexts, internal allocators, Android app-process restrictions; one process proves nothing for another consumer. Direct link — patch the link line/target; proof = link map/line + readelf/nm evidence; limitation: record link order and duplicate-definition risk; presence on the link line is not proof of use. Static replacement — patch the archive/object set compiled into the binary; proof = rebuilt binary hash + symbol-table evidence; limitation: LD_PRELOAD cannot reach it, every statically linked consumer needs its own rebuild and result row. Namespaced API — patch call sites to a named allocator API; proof = call-site diff + counter/audit hook showing the calls entered the patched path; limitation: no interposition claim, unpatched sites and runtime allocations keep their original allocator, mixed-allocator ownership recorded, cross-allocator free is a correctness limitation.
- **Measured-result fields (ALL TBD):** one row per consumer x mode x patched-vs-baseline: Run ID/date/device TBD | Environment (arch/ABI, OS, libc, toolchain, page size) TBD | Component + pinned revision; patch artifact + SHA-256 TBD | Mode + consumer TBD | Verbatim command; clean build dir Y/N TBD | Build outcome/exit/first error TBD | Failing-command outcome/exit/first error TBD | Proof-of-reach pointer TBD | Runtime/resource figures only if measured with a named method TBD | Correctness/regression command + result only if run TBD | Raw log pointer TBD | Evidence label: Not assigned (never inferred) | Verdict: Unblocks / Partially unblocks / Does not unblock / Inconclusive / Blocked + resumption point TBD.
- **Integration-limitation fields (TBD per row):** Consumers NOT reached + why | Modes NOT tested | Static/hidden-symbol/sandbox/Android limitation | Mixed-allocator ownership remaining | Private-interface fragility | Page-size/ABI/VA-width assumption introduced | Distribution/license limitation beyond this device | Maintainability burden (owner, cadence, sunset) | What this result does not establish (mandatory free text).
- **Revert / rollback criteria (design):** Revert immediately and record failed/blocked if: the patched run does not change the original failure (or changes a different consumer/mode); any new correctness failure appears; proof-of-reach cannot be produced; component or patch license cannot be identified/recorded or provenance entry was not made before use (contamination triggers follow quarantine, not ordinary rollback); the diff exceeds the recorded acceptable divergence, touches a private interface with no sunset, or owner/cadence is blank; upstream ships an equivalent fix (retire the patch, re-score against the pinned upstream revision); or a measured, logged regression in build time, runtime, or memory appears (no threshold claimed in advance). Procedure: stop work, preserve patch artifact + SHA-256 + both log sets (deleting failure evidence is forbidden), discard the patched worktree / restore the pristine pinned revision, rebuild clean, re-run the baseline command to confirm restoration, append (never edit) outcome and pointers to the evidence/provenance records.

## 8. Route P2-R04 — Established allocators as controlled diagnostic comparators

- **Route ID / Status / Evidence label:** P2-R04 / Proposed / Proposed. Comparators answer: would a simpler established route unblock Aesir, and what does a mature allocator do on this workload? Diagnostics only.
- **License grounding rule for this whole route:** local roadmap/research sources state only that license terms can vary by revision and build configuration and must be checked upstream before use (research/11_ALLOCATOR_LANDSCAPE.md), and for tcmalloc that license/attribution matter if source is actually reused (Roadmap Section 4.1). No license name for mimalloc, jemalloc, or tcmalloc is asserted anywhere in this plan: every license field is TO-VERIFY from the pinned artifact actually used, at pin time. Diagnostic binary use, source inspection under the clean-room ruleset, and source reuse are three different license questions — never collapsed.
- **Common questions (identical per comparator):** (1) Is a pinned AArch64-compatible build available/buildable for this exact target (Linux AArch64 + glibc on this Pi first; Android/bionic, musl, tablet are separate)? (2) By which mode does it actually service allocations in (a) a small standalone test program, (b) the implicated Mojo process, (c) an Aesir workload — with what symbol/linkage evidence (Section 11)? (3) Does selecting it change the Phase 1 failure signature? If unchanged: not selected, or allocator not the cause? (4) Retention / return-to-OS and behaviour under benchmark classes 1-5 vs the B-01 libc baseline, equivalent settings, correct compatible build modes. (5) License, attribution, notice, redistribution obligations of the pinned revision — binary use, and separately source reuse (none planned). (6) What does it not reach: static consumers, hidden/-Bsymbolic symbols, secure execution, internal allocators / direct mmap, Android app processes?
- **Common method (DESIGN ONLY):** pin (upstream repo URL, release/commit, checksum, build flags, host/target, page size, compiler/linker) -> presence/linkage inventory (ldconfig, dpkg, file, readelf -d/-sW, nm -D/-u, objdump -T/-R, ldd) -> per-mode selection run -> harness classes 1-5 on the standalone program first; classes 6-7 only if integration is proven for that mode -> log per research/12 lab notebook (date, operator, hypothesis, machine, CPU/memory, OS/kernel, ABI/libc, page size, toolchain, allocator revision/flags, workload revision, non-default settings, thermal state, competing processes, raw log path, privacy review). Symbols sought (contracts.md / integration map watch-list): malloc, calloc, free, realloc, posix_memalign, aligned_alloc, C++ operator new/delete variants where present, plus allocator-specific extensions only when observed in an actual binary — never invented.
- **Proof-of-selection (Section 11, per mode):** readelf -d DT_NEEDED entry (direct link); LD_DEBUG=bindings output binding malloc/free to the comparator library plus /proc/<pid>/maps showing it loaded (LD_PRELOAD); exported T/D symbols in the library vs U in the consumer; a visible allocation counter/audit hook proving the comparator serviced the test process. Name match alone is not proof (Roadmap Section 3.3).
- **Common measured-result fields (ALL TBD):** Allocator + pin TBD | Mode TBD | Linkage dynamic/static TBD | Selection-proof artifact TBD | Workload/class TBD | Repetitions (fresh processes) TBD | Median/p95/p99 latency TBD | Throughput TBD | Peak RSS/PSS TBD | Virtual size TBD | Retained after free TBD | Mapping count TBD | Startup time TBD | CPU time TBD | Responsiveness/energy where reliable TBD | Correctness failures TBD | Raw log path TBD.
- **Common limitation fields (ALL TBD):** Modes not viable for this consumer + why | Symbols not intercepted / modules bypassing | Android/bionic applicability (separate, unproven) | Page-size limitation | Failure signature changed? (Y/N + evidence) | What this result does not prove.

### 8.1 mimalloc (Baseline B-05 — candidate only, no build or run recorded; not present on this Pi, Section 3)

- **Specific questions:** Does a pinned mimalloc build support this Pi's 16 KiB page configuration without fixed-page assumptions? Which modes are true global replacement here vs namespaced API requiring call-site adoption? Naming control: is "mimalloc" written in full and never conflated with "MimirAlloc" in every log/artifact (Phase 0 confusable-names concern)? Research 11 question: what locality and retention tradeoffs emerge on edge traces (free-list sharding, local/remote behaviour)?
- **License to record:** TO-VERIFY from the pinned upstream release license file; record version, copyright holder, notice-retention and redistribution terms in the provenance ledger.
- **Linking constraints to record (TBD at pin):** shared vs static availability for AArch64; symbol visibility/versioning; whether all consumers must agree on one build configuration; ABI compatibility with the consumer's glibc and C++ runtime.
- **Modes (DESIGN ONLY):** LD_PRELOAD: LD_PRELOAD=<pinned-mimalloc.so> LD_DEBUG=bindings <test-binary> — dynamic, non-secure-execution consumers only. Direct link: cc <objs> -L<pin> -l<lib> with -Wl,-Map recorded; record DT_NEEDED. Static replacement: pinned static archive at relink time; only for binaries this project rebuilds. Namespaced API: allocator's prefixed API at call sites in a project-owned test program only — not global replacement, never reported as such; record adopted call sites by diff.

### 8.2 jemalloc (Baseline B-04 — candidate only; system library present on this Pi, Section 3: libjemalloc.so.2, package libjemalloc2:arm64 5.3.0-1 — presence is inventory only, not a run, configuration, or measurement)

- **Specific questions:** Provenance J-1: public Mojo source selects jemalloc for the Linux Mojo driver while shipped wheels abort in TCMalloc — can jemalloc selection in this project's own test program be proven independently of that unresolved question? Research 11: how do concurrency, tuning, and retention compare (arenas, bins, decay, statistics)? Does the system libjemalloc.so.2 suffice for a diagnostic run, or is a pinned upstream build required for a fair comparator (system package version/config are uncontrolled)? Set-membership decision (inconsistency INC-3): record explicitly that jemalloc is in the Phase 2 comparator set, or remove it.
- **License to record:** TO-VERIFY from both the Debian package copyright file for libjemalloc2 5.3.0-1 and the pinned upstream release if one is built; record which source governed the run actually made.
- **Linking constraints to record (TBD):** inventoried system library is shared-only; static availability, version-symbol compatibility, and configuration/tuning environment for a pinned build are unverified; record whether the consumer must be relinked or can be preloaded. A tuned run and an untuned run are different comparators.
- **Modes (DESIGN ONLY):** LD_PRELOAD: LD_PRELOAD=/lib/aarch64-linux-gnu/libjemalloc.so.2 LD_DEBUG=bindings <test-binary> — the only mode usable with the present artifact without relinking (direct link below uses the same shared artifact at link time); still design only, not run. Direct link: cc <objs> -ljemalloc with link map; record DT_NEEDED=libjemalloc.so.2. Static replacement: N/A until a pinned static build exists. Namespaced API: jemalloc's prefixed/non-standard API and statistics/configuration interfaces in a project-owned test program only; statistics readout is corroboration, never sole selection proof. Extra measured fields: source (system 5.3.0-1 / pinned upstream) TBD | tuning/config TBD | arena/decay settings TBD.

### 8.3 tcmalloc (Baselines B-02 upstream-documented, no build/log/result in repo; B-03 field experiment — summaries only, artifacts NOT in repo, guarded build recorded in BASELINE_INVENTORY.md B-03 as reaching 264 of 300 Bazel actions (prior summary only, artifacts not in repo, not measured in this slice), no archive, no allocator execution, Field 05 gates 1-6 open; not present on this Pi, Section 3)

- **Specific questions:** It is the implicated allocator, not a neutral comparator: per run, record whether the goal is failure reproduction under a pinned tcmalloc or comparison against it. Address-width: Field 05 header-level results as summarised in BASELINE_INVENTORY.md (default 48 accepted, explicit 39 accepted, 38 and 49 rejected — prior summary, not re-measured in this slice) — reproducible from a pinned in-repo build, given B-03 source/log/cache live outside the repo (J-2)? Distinguish Google TCMalloc from gperftools (Roadmap S11: not identical APIs/implementation) — which project exactly is pinned? Roadmap Q3: is tcmalloc statically or dynamically linked in the implicated consumer, and what prevents a configurable allocator from being selected? Upstream support listing (S1: Linux AArch64 supported) is not evidence for this build/OS/ABI/device.
- **License to record:** TO-VERIFY from the pinned upstream repository (Roadmap S8) at pin time; diagnostic use plans no source reuse.
- **Linking constraints to record (TBD):** Bazel-based pinned build; verbatim Field warning from BASELINE_INVENTORY Section 4.1 — every TCMalloc translation unit and consumer must agree on the address-width setting; static vs dynamic availability; rseq/platform requirements (S7); C++ ABI; TCMalloc-specific symbols observed in real binaries (integration map: TCMallocGlobals::tc_new/tc_delete, TCMallocInternal*, weak tcmalloc_internal::* in libAsyncRTRuntimeGlobals.so) — defined internal symbols prove code is linked in, not which path services an allocation.
- **Modes (DESIGN ONLY):** LD_PRELOAD: only if a pinned shared build exists and the consumer is dynamic; a statically linked/bundled-TCMalloc consumer (as recorded for parts of the installed Mojo runtime) cannot be reached this way — record as mode-not-viable for that consumer, not as a tcmalloc failure. Direct link: pinned library into the standalone test program; record DT_NEEDED + link map. Static replacement: Bazel malloc attribute / linkopts / archive selection at rebuild of a project-controlled target (integration map M3); not applicable to prebuilt shipped binaries. Namespaced API: TCMalloc-specific extensions/profiling interfaces from a project-owned test program only. Extra proof bar: separate (a) TCMalloc-derived symbols defined inside a consumer library, (b) standard symbols undefined there and bound elsewhere, (c) runtime binding audit showing which implementation serviced allocations. Extra measured fields: project (google/tcmalloc vs gperftools) TBD | pinned commit TBD | address-width setting, all units agree TBD | Bazel actions completed TBD | failure signature reproduced (Y/N + packet link) TBD.

### 8.4 Comparator guardrails

1. Diagnostics only — comparators never cancel, replace, or declare redundant the independent MimirAlloc project.
2. No selection without Section 11 proof; presence, name match, or compile success is never proof.
3. No invented measurements and no invented licenses (TO-VERIFY until verified from the pinned artifact).
4. One variable at a time, same workload and environment manifest as the B-01 baseline.
5. Mode honesty: a namespaced-API success is not a drop-in replacement success; an LD_PRELOAD failure on a static consumer is a mode limitation, not an allocator verdict.
6. Target honesty: Pi/glibc results are not Android/bionic/musl/proot/tablet results; Android has no LD_PRELOAD route for installed apps (zygote namespaces, SELinux).
7. Mobile objective governs: latency wins that raise memory pressure, retention, or stutter do not satisfy the objective; report negative results.
8. Clean-room separation: comparator behaviour may be observed and documented; comparator implementation must not be copied into MimirAlloc; any source inspection is provenance-logged.
9. Naming discipline: "mimalloc" and "MimirAlloc" in full, everywhere.

## 9. Phase 2 exit table (the Section 8 exit gate — all rows Proposed/Blocked, nothing measured yet)

Exit rule: Phase 2 passes only when every row has an exact command, a measured result, and integration limitations filled from evidence. An empty, name-only, or compile-only row is Blocked, not a pass. Later phases may begin early only when dependencies are met and their work cannot misrepresent an unproven feature (Roadmap Section 8 gate).

| Route ID | Route | Mode(s) to test | Exact command(s) | Environment / pinned revisions | License | Linking constraint | Measured result (summary + raw-data ref) | Evidence label | Integration limitations / Does not establish | Verdict |
|---|---|---|---|---|---|---|---|---|---|---|
| P2-R01 | System allocator | Namespaced API (mallopt-family) + env tunables; LD_PRELOAD debug-only | Design: Section 5 | Pi, aarch64, glibc 2.36, PAGESIZE 16384 (Section 3); target TBD | LGPL-2.1-or-later (glibc, system use) | No new link; non-POSIX API guard required | TBD — not run | Proposed | Process-local only; not Android/musl; does not establish replacement-allocator behaviour | TBD |
| P2-R02 | Configurable build flags | Direct link (primary) | Design: Section 6 | GCC 12.2.0 / ld 2.40 (Section 3); source commit TBD | GPL-3.0 + Runtime Exception (toolchain) | Static variants change license analysis | TBD — not run | Proposed | Interpreted targets out of scope; does not establish allocator superiority | TBD |
| P2-R03 | Local patch (C1/C2/C3) | All four, per sub-route | Design: Section 7 | Component + pinned revision TBD | TO-VERIFY (no component named) | TBD | TBD — not run; no patch exists | Blocked | Blocked on Phase 1 failure packet; does not establish anything about MimirAlloc | Blocked |
| P2-R04a | mimalloc (comparator) | All four, distinguished | Design: Section 8.1 | Pin TBD; not present on Pi | TO-VERIFY | TBD at pin | TBD — not run | Proposed | Diagnostics only; does not establish MimirAlloc properties | TBD |
| P2-R04b | jemalloc (comparator) | LD_PRELOAD / direct link / namespaced API (static N/A) | Design: Section 8.2 | System libjemalloc.so.2 5.3.0-1 present (inventory only); pin decision TBD | TO-VERIFY | Shared-only as inventoried | TBD — not run | Proposed | Diagnostics only; Mojo provenance J-1 unresolved | TBD |
| P2-R04c | tcmalloc (comparator / implicated) | All four, distinguished | Design: Section 8.3 | Pin TBD; not present on Pi; B-03 artifacts outside repo | TO-VERIFY | Address-width agreement across all units | TBD — not run | Proposed | Implicated allocator; bundled consumers unreachable by LD_PRELOAD | TBD |

Recommendation rule: a route verdict is one of Unblocks / Partially unblocks / Does not unblock / Inconclusive / Blocked (the same verdict vocabulary as Section 7), decided only from filled rows; null and negative results are recorded, never hidden. Cross-route comparison uses the identical harness, settings, and environment manifest as the B-01 platform-libc baseline.

## 10. Roadmap inconsistencies recorded in this slice

Recorded in-doc per the slice instruction; pre-catalogued in Wave 1 ledgers (N- = decision/evidence ledgers, I- = SUPPORTED_TARGETS, J- = BASELINE_INVENTORY, U- = contracts register) and confirmed against roadmap text by the scout.

- **INC-1 (Section 15 mislabelled):** Heading "15. Decision register and open research questions" contains thirteen open questions and zero recorded decisions; items 1-13 are all questions (DECISION_LEDGER finding).
- **INC-2 (Section 2 unassigned levels, N-1):** Section 2 says "Use the following labels throughout the project" but assigns none of them to its own six claims; EVIDENCE_LEDGER E-001–E-006 are all "Not assigned in roadmap Section 2".
- **INC-3 (Comparator-set conflict, J-3/I-3/J-1):** Section 11 names the comparison set as platform libc, pinned tcmalloc, mimalloc, and MimirAlloc — MimirAlloc does not exist as a verified implementation until Phase 3, after Phase 2 — while omitting jemalloc, the allocator the public Mojo source actually selects for its Linux driver, even though the distributed Mojo 1.2.0 wheel aborted with a TCMalloc error. This plan resolves it operationally, without editing the roadmap: MimirAlloc is excluded from Phase 2 routes (Section 2) and jemalloc is included as P2-R04b with the set-membership decision recorded in Section 8.2.
- **INC-4 (License question stale, N-4):** Section 15 Q12 asks "Which public license supports the owner's goals? Decide before accepting outside contributions" while the repo already carries an MIT LICENSE and ENGINEERING_GUIDE preserves MIT. P2-R03 records the project license as settled MIT.
- **INC-5 (Section 13 layout unsettled, N-5/N-6):** Section 13 shows both docs/decisions/ and a root evidence/, and ROADMAP.md at root with include/, src/, tests/, benchmarks/ — none of which exist; the actual roadmap is docs/MimirAlloc_Research_and_Development_Roadmap.md and ledgers live under docs/.
- **INC-6 (Section 5 category mix, U-08):** One list mixes ABIs, environments (Termux, proot), and devices (Samsung SM-X400, Raspberry Pi). SUPPORTED_TARGETS.md already separates these into distinct row kinds; this plan follows that separation.
- **INC-7 (Section 6 under-sourced, U-04):** Section 6 demands target-specific behaviour tables for musl/bionic while citing only [S5, S6] Linux/POSIX man pages.
- **INC-8 (Section 7.1 API absent from Section 6, U-03):** The Section 6.1 core list has no mimir_*/statistics/trim API that Section 7.1 describes; the Section 6.2 runtime-config clause is the only hook. No route in this plan may invent such an API.
- **INC-9 (C++ tension, U-06):** Section 6.1 makes C++ operators conditional ("only when the integration requires them") while Section 6.3 requires allocator work before main, from constructors, loader code, or during logging.
- **INC-10 (Support vs failure, I-4/I-9):** Section 2 reports "TCMalloc lists Linux AArch64 as supported [S1]" while Field 03 records an SM-X400 failure on a 48-bit VA assumption (both as recorded in Wave 1 summaries, not re-measured in this slice); no per-target VA width is recorded in the roadmap.
- **INC-11 (Section 7.2 stale in the roadmap alone):** Heading "7.2 Implementation language decision" leaves C11 vs C++17 open in the roadmap text; it is resolved only outside the roadmap by DECISION_LEDGER D-006 (C11 core adopted, superseding D-005).

## 11. Evidence, ledger linkage, and what was honestly left unmeasured

- Ledger linkage: results from any future run of this plan are appended to EVIDENCE_LEDGER.md (append-only, verbatim, supersession by new entry, never by editing an old one) with DECISION_LEDGER references where a route verdict changes a decision. Route IDs P2-R01..P2-R04 are the join keys between this plan, the exit table, and future evidence entries.
- Actually run in this slice: the Section 3 light inventory probes only (free, ldd --version, getconf, uname, gcc/ld versions, overcommit/THP reads, env scan, ldconfig/dpkg presence checks). Samtal on port 8787 was observed listening and not touched.
- Honestly left unmeasured (everything else): no baseline workload was run; no tunable was changed; no binary was rebuilt; no patch was identified or applied; no comparator was built, preloaded, linked, or measured; no Mojo or Aesir workload was run; no license for mimalloc, jemalloc, or tcmalloc was verified (all TO-VERIFY); the Phase 1 failure packet does not exist yet, so no route could be scored against the original failure.

## Appendix A. Command / artifact index

All commands appear in Sections 3-8 in place: Section 3 = run and quoted; Sections 5-8 = design only, marked UNRUN/DESIGN ONLY at each site. Artifacts produced by this slice: this document only.

## Appendix B. Change log

| Date | Change |
|---|---|
| 2026-11-06 | B-001 initial plan: routes P2-R01..P2-R04, common protocol, exit table, inconsistencies INC-1..INC-11, Section 3 Pi inventory evidence. |
