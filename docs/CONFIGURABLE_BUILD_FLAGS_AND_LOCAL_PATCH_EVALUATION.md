# MimirAlloc — Configurable Build Flags and Local Patch Evaluation

Slice: B-003 (Phase 2, routes 2 and 3). Companion slices: B-001
`docs/PHASE_2_EVALUATION_PLAN.md` (evaluation plan, routes P2-R02 and
P2-R03, Sections 6-7, exit table Section 9); B-002
`docs/SYSTEM_ALLOCATOR_EVALUATION.md` (P2-R01 system-allocator route,
per-route format followed here).
Status: research / evaluation. Phase 2 is NOT exited by this slice.
Base: 9296c9040908e0e7af322031ab17e1a8ee774258 on branch
`yrsa/mimiralloc-03` (B-001 banked at af893ee, B-002 banked at 9296c90).
Citations: Roadmap `docs/MimirAlloc_Research_and_Development_Roadmap.md`
(Phase 2 in Section 8; Phase 1 in Section 3), `docs/BASELINE_INVENTORY.md`,
`docs/SUPPORTED_TARGETS.md`, `docs/INTEGRATION_LAYER_MAP.md`,
`docs/CLEAN_ROOM_RULESET.md`, `docs/PROVENANCE_LEDGER_PROCESS.md`,
`docs/EVIDENCE_LEDGER.md`, `docs/failure-capture.md`, `docs/contracts.md`.
Reconciliation: B-001 HAS banked in this tree and its evaluation fields
WERE used for both routes. B-002's banner stating B-001 had not banked
was true on its own base and is now stale in-tree; see I-B003-1.
README.md was not touched by this slice.

## 1. Honest headline

### 1.1 What is proven

- P2-R02 (build flags), probe scope only, Device-tested on this Pi:
  a trivial C program linked with `-Wl,-z,max-page-size=4096` segfaults
  reproducibly on this 16 KiB-page Pi (exit 139, two fresh builds),
  while the default build and the `max-page-size=16384` build run
  correctly (exit 42). Full logged evidence: Section A8 and Appendix A.
  This is a probe result about page-size alignment, not a benchmark
  and not a result about any real target.
- Toolchain facts on this Pi, logged in Section A8: GCC 12.2.0,
  Clang 14.0.6, GNU ld 2.40, glibc 2.36, `getconf PAGESIZE` = 16384.
- P2-R03 (local patch): the negative facts in Section B1 are proven
  by the recorded repo searches: no patch file exists in the repo,
  no patch is identified anywhere checked, and no real Phase 1
  failure packet exists (the only packet is a synthetic smoke case).

### 1.2 What is not proven

- No real target has been rebuilt with candidate flags. No Mojo,
  Aesir, tcmalloc, jemalloc, or mimalloc build, link, or preload
  was run. No allocating workload, benchmark, RSS, latency, or
  throughput measurement was run for either route.
- No patch has been identified, written, applied, built, or
  measured. Every P2-R03 measured field is TBD.
- Phase 1 remains open. A-005 is synthetic only and the integration
  map records every relevant verdict as UNDETERMINED.

### 1.3 Verdict summary

| Route | Evidence label | Verdict |
|---|---|---|
| P2-R02 configurable build flags | Proposed (real target); Device-tested (trivial probe only) | TBD — Inconclusive for unblocking; no failure packet to score against |
| P2-R03 local patch | Blocked | Blocked — no Phase 1 failure packet, no patch identified |

Neither route cancels or replaces MimirAlloc. Both are diagnostic
comparators only (Roadmap Section 8).

## 2. What the routes mean

| Route ID | Name | Definition (per B-001 Sections 6-7) | Changes source? | Changes allocator? |
|---|---|---|---|---|
| P2-R02 | Configurable build flags | Same source, same allocator; rebuild the target only with different compiler/linker/build flags, compared against the P2-R01 baseline. If a flag set pulls in a different allocator, that experiment belongs to P2-R04, not here. | No | No |
| P2-R03 | Local patch | A local patch, in sub-routes C1/C2/C3 recorded separately and never conflated (Section B1.2). A success is a diagnostic conclusion only; it produces no MimirAlloc evidence. | Yes (the patched component) | Selection in C2; implementation in C3 |

Baseline comparator for both routes: P2-R01, the system allocator
(glibc 2.36) as evaluated in B-002.

Falsifiable hypotheses: for R02, if the failure signature changes
with flags alone, the cause is in build configuration, not in the
allocator; if it does not change, flags are excluded as the cause.
For R03, if the smallest patch testing the Phase 1 causal hypothesis
changes the verbatim failure, the hypothesis is supported; if not,
it is falsified. Roadmap Section 3.3 fallback applies if unchanged.

Common protocol (B-001 Section 4, per Roadmap Section 11), honoured
throughout: selection is proven by `ldd`/`readelf`/`nm`, loader
binding, maps, or an allocation counter — never by name, package
presence, or compile success. Fresh processes; pinned versions;
per-run environment manifest; median/p95/p99, RSS/PSS where a real
run exists; benchmark classes 1-5 only (6-7 only if integration
proven, 8 tablet-only); one variable at a time, A-B-A, warm-up
discarded; raw and negative results preserved; on the Pi, `free -g`
before and after any heavy step and Samtal on port 8787 never starved.
Target honesty: this Pi is Linux AArch64, glibc, 16 KiB pages (16384).
That is not Android/bionic/musl and not a 4 KiB-page (4096) tablet;
no result here transfers to those targets.

# PART A — Route P2-R02: Configurable Build Flags

## A1. What this route means

Same source, same allocator (glibc 2.36 on this Pi); only compiler,
linker, and build flags change, one flag family per build, compared
against the P2-R01 baseline. A flag set that pulls in a different
allocator reclassifies the experiment to P2-R04; a source change
(mallopt-family calls or tunables set in code) belongs to P2-R01.
A flags result is valid only for its target, toolchain, and source
commit; it does not transfer.

| Target | Toolchain | Source commit | Evidence state |
|---|---|---|---|
| No real target is named in any source read | GCC 12.2.0 / ld 2.40 (Section A8) | TBD — no target pinned | Proposed |
| Trivial probe program (Appendix A) | GCC 12.2.0 / ld 2.40 | Probe source in Appendix A | Device-tested, probe scope only |

## A2. Exact questions (verbatim from B-001 Section 6)

1. What is the pristine build exactly (version output, build log, readelf .comment, hardening check)? A comparison with an unknown baseline is worthless.
2. Candidate flag families and each hypothesis: -O2 vs -O3, LTO, -fno-plt, stack protector cost, -DNDEBUG, section GC, relro/now, PIE, PGO.
3. -march: native is non-portable; record the deployment target and use an explicit baseline (e.g. armv8-a) unless native is justified.
4. Page size: do -Wl,-z,max-page-size=16384 / common-page-size matter here, and would a 16 KiB-aligned binary break on a 4 KiB machine?
5. Do --as-needed, link order, -Bsymbolic, or -z now silently change which malloc resolves — an accidental allocator change that invalidates the comparison?
6. Cost of -g and sanitizers (diagnostic only, never production) and hardening flags.
7. Reproducibility: same GCC 12.2.0 / ld 2.40 (Section 3), same source commit, flags verbatim in the build log.
8. If the target is interpreted (e.g. Python), there is no C build-flag route for it as deployed; this route then covers only a rebuilt interpreter/extension and must be stated as such.

Question 4 note: the probe in Section A8 tested the converse
direction (a 4 KiB-aligned binary on this 16 KiB machine) and it
failed. The direction asked here (16 KiB binary on a 4 KiB machine)
is still NOT tested; no 4 KiB machine was available to this slice.

## A3. Selection and configuration

Baseline first: the pristine/default-flags build is the baseline
row. One flag family per build, clean tree each time, never mix
objects across flag sets; candidate runs are separate rows, never
merged. Candidate families (hypotheses in A2 Q2): optimisation
level, LTO, -fno-plt, stack protector, -DNDEBUG, section GC,
relro/now, PIE, page size, -march. PGO is a two-build sequence
with a representative training input and is diagnostic-sensitive:
unrepresentative training data is a recorded limitation (A10).

## A4. Method / commands — DESIGN ONLY / UNRUN, except Section A8

```bash
<TARGET_BINARY> --version; readelf -p .comment <TARGET_BINARY>; readelf -h <TARGET_BINARY>
CFLAGS_CAND="-O3 -flto -fno-plt"   # UNRUN example candidate set from B-001 Section 6
LDFLAGS_CAND="-Wl,-O1 -Wl,-z,relro -Wl,-z,now -Wl,--gc-sections"  # UNRUN
ldd <TARGET_BINARY>; file <TARGET_BINARY>; sha256sum <TARGET_BINARY>  # UNRUN verify before measuring
```

Guard: builds are CPU-heavy on the Pi. Any future run requires a
`free -g` and load check first and must not starve Samtal on :8787,
checked before and after, as in B-002 Section 7.1.

## A5. Licenses and linking

Records license facts, not legal advice and not a legal opinion.
Project license is settled MIT (LICENSE, (c) 2026 Volmarr Wyrd);
MIT governs project code and does not license third-party material.
GCC 12.2.0: GPL-3.0 with the GCC Runtime Library Exception
(on-Pi: /usr/share/common-licenses/GPL-3,
/usr/share/doc/gcc-12/copyright) — this route records reliance on
the exception. GNU ld / binutils 2.40: GPL-3.0. glibc 2.36:
LGPL-2.1-or-later. This route configures and links; it never
modifies glibc: the result is a work that uses the Library
(LGPL-2.1 Section 5) in the Section 6 shared-library (6b) case
for default dynamic linking, per B-002 Section 4. LTO and PGO
add no third-party code. `-static`, `-static-libgcc`, and
`-static-libstdc++` change the analysis (LGPL Section 6a/6c
relink/source duty, bundled-runtime notices, and the NSS/dlopen
exact-version trap reported in B-002) and need a per-variant
sign-off; a static variant also defeats the P2-R01 tunables.
Stripping symbols does not strip license or NOTICE files. The
exact flag set and toolchain versions go in the build manifest
for every shipped binary. Clang 14.0.6 is present on the Pi; its
license file was NOT inspected in this slice — TO-VERIFY if a
clang build is ever used as evidence.

## A6. Four integration modes — for this route specifically

Modes are the four named in Roadmap Section 8 / B-001, cross-
referenced to INTEGRATION_LAYER_MAP modes M1-M5 (see I-B003-2).

### A6.1 LD_PRELOAD (M1) — No
Nothing is injected in a flags experiment. If a comparison needs
a preload, it is not a flags experiment. Preload may be used only
as a null control or for its removal. It cannot reach static
variants of this route.

### A6.2 Direct link (M2+M3) — Yes, primary
Flags act at compile/link time on a project-controlled rebuild.
Proof required per build: verbatim build log, `readelf -h`,
`readelf -d`, `readelf -p .comment`, `ldd`, DT_NEEDED and
malloc-symbol diff against the baseline build, sha256 and
BuildID. Presence of a flag on the command line is not proof
of its effect.

### A6.3 Static replacement (M3, static form) — Conditional / flagged only
Only if a candidate variant statically links the allocator or
runtime (-static / -static-libgcc / -static-libstdc++). The
default candidate set is dynamic. Any static variant needs its
own license analysis (A5) and limitation sign-off, defeats the
P2-R01 tunables, and every statically linked consumer needs its
own rebuild and its own result row.

### A6.4 Namespaced API (M4) — No
The source is unchanged by definition in this route. Adopting a
named API (mallopt-family calls in code, or a future mimir_* API)
is a source change and belongs to P2-R01 / API work, not here.
No such API may be invented by this route (INC-8).

### A6.5 Summary

| Mode | Mojo compiler/wheel | Generated L4 executable | Aesir L5 |
|---|---|---|---|
| LD_PRELOAD | No | No | No |
| Direct link | Primary, if the target is rebuilt under project control | Primary, same condition | Primary, same condition |
| Static replacement | Conditional, flagged only | Conditional, flagged only | Conditional, flagged only |
| Namespaced API | No | No | No |

No Mojo, generated, or Aesir consumer was exercised in this slice;
the table states applicability, not results.

## A7. Phase 6 gate, restated for this route

The gate closes only if the original failing command succeeds
with selection proof (A6.2 proof set), or a blocker is recorded.
A hello-world or trivial-program pass does not close it. A Pi
pass is not tablet evidence. Negative-result next step: if flags
alone do not change the signature, flags are excluded and the
next route in roadmap order is taken.

## A8. Pi-observed facts (Team-03, 2026-11-06) — the only measured section in Part A

### A8.1 Gate and manifest

All commands below were actually run on the Pi via device.invoke
and logged by Builder-1; full draft log preserved in the slice
working record (see Appendix A). `free -g` before and after:
Mem 15 total / 2 used / 13 available; Swap 15 / 1 used — loads
kept light, probe work in /tmp only. Samtal on :8787 was observed
listening (python3) and was not touched. Manifest: Linux AArch64,
4x Cortex-A76, glibc 2.36 (2.36-9+rpt2+deb12u14), GCC 12.2.0
`gcc (Debian 12.2.0-14+deb12u1)`, Clang 14.0.6
(aarch64-unknown-linux-gnu), GNU ld 2.40, `getconf PAGESIZE` =
16384, loader /lib/ld-linux-aarch64.so.1 (/lib64/ld-linux-aarch64.so.1
does not exist on this Pi). `ld.so --list-tunables` lists 13
glibc.malloc.* tunables. No MALLOC_*, LD_PRELOAD, GLIBC_TUNABLES,
or CFLAGS variables were set in the environment. /etc/ld.so.preload: absent (per B-002 §7.1).

### A8.2 Probe (small, light, logged)

Source: trivial C program `int main(void){return 42;}` (27 bytes;
Appendix A). Nine flag sets were run fresh (route2 §6): -O0, -O2,
`-O3 -flto -fno-plt` (70,360 B), `-O2
-fstack-protector-strong -Wl,-z,relro,-z,now` (70,248 B),
-march=native, -march=armv8-a, clang -O2 and the page-size
variants — all exited 42 except the max-page-size=4096 build,
which exited 139 on two fresh builds. A tenth build, -static
(702,960 B, EXEC not DYN), was built but NOT run.

Page-size finding (the one measured result of this slice):

| Build | File size | LOAD align | Run result |
|---|---|---|---|
| Default | 70,376 B | as logged | exit 42 |
| -Wl,-z,max-page-size=16384 | 21,224 B | 0x4000 | exit 42 |
| -Wl,-z,max-page-size=4096 | 8,936 B | 0x1000 | exit 139 (segfault), reproduced on two fresh builds |

`size` shows identical text/data/bss (1538/616/8) for the default
and explicit-16384 builds: the file-size difference is alignment
padding, not code. Interpretation, stated narrowly: a binary
whose maximum page size is set below the running kernel page
size can fail to load/run on this 16 KiB-page Pi. This is NOT a
benchmark, NOT a result about any real target, and NOT the
direction asked in A2 Q4. Hashes for the first four flag sets
are recorded in the Builder-1 draft log (Appendix A pointer).

### A8.3 NOT run (explicit)

No Mojo, Aesir, tcmalloc, jemalloc, or mimalloc build, link, or
preload. No real target rebuilt. No allocating workload,
benchmark, RSS/latency/throughput measurement, PGO sequence,
sanitzer build, hardening-cost measurement, or build-time
measurement. No tunable value was set for a measured run. No
16 KiB-on-4 KiB, musl, bionic/Android, x86-64, or tablet test.
All commands in A4 remain DESIGN ONLY / UNRUN.

## A9. Measured-result fields (B-001 Section 6; all TBD except toolchain and the A8 probe)

| Field | Value |
|---|---|
| Source commit / tree | TBD — no real target named or pinned |
| Baseline flags verbatim | TBD for a real target; probe baseline = default flags (A8) |
| Candidate flags (one family) | TBD; probe families in A8.2 only |
| Binary size / sha256 / ldd diff | TBD; probe sizes in A8.2, probe hashes in Appendix A log |
| Workload (must match P2-R01 harness) | TBD — not run; no tracked harness exists yet (I-B002-7) |
| Baseline and candidate wall / usr / sys / VmHWM / VmRSS | TBD — not measured |
| Delta % | TBD |
| Repetitions / median / spread | TBD |
| Correctness | Probe only: exit codes in A8.2; real-target correctness TBD |
| Build time / build RSS on Pi | TBD — not measured |
| Samtal impact during build | Probe: none observed; Samtal not touched. Real builds TBD |
| Raw log pointer | Appendix A (probe); otherwise none exists |
| Evidence label | Proposed (real target); Device-tested (probe only) |

## A10. Integration-limitation fields

| Field | Value |
|---|---|
| Interpreted target — no applicable flags | TBD / check first for any named target (A2 Q8) |
| -march=native non-portable | Standing limitation: native results do not transfer; explicit baseline required |
| 16 KiB alignment breaks 4 KiB machines | NOT tested in that direction; converse direction measured and failing (A8.2) |
| PGO training data unrepresentative | TBD — PGO not run |
| Flag silently changed malloc resolution | TBD — ldd/symbol check required per build (A2 Q5); probe made no such check claim for a real target |
| Hardening cost unmeasured | Standing: unmeasured in this slice |
| Static variant defeats tunables + LGPL relink duty | Applies if a static variant is proposed (A5, A6.3); probe -static build was not run |
| Rollback | Rebuild with the recorded baseline flags / redeploy the baseline binary by sha256 |
| Does not establish | Allocator superiority of any kind. Mandatory: this route, even when fully run, establishes nothing about allocator superiority. |

# PART B — Route P2-R03: Local Patch

## B1. What this route means, and patch identification status

### B1.1 Status banner (mandatory, read first)

Patch identification status: NONE IDENTIFIED. No patch has been
identified, written, applied, built, or measured. No component,
pinned revision, patch locus, or upstream issue/PR is named
anywhere checked. Route verdict: Blocked. Resumption point: a
Phase 1 failure packet naming the failure, the consumer, the
pinned component revision, and a candidate patch locus (per
INTEGRATION_LAYER_MAP A-007). That packet does not exist: the
only packet in the repo is synthetic —
evidence/runs/20261106T114752Z-smoke-fail-0f4a3c/ (case
smoke-fail, deliberate exit 3, failing_process: other), whose
manifest claim_scope states it is harness-evidence-only and not
evidence of the Mojo/tcmalloc blocker. docs/failure-capture.md
confirms no Mojo failure, revision, or log exists in the repo
yet; EVIDENCE_LEDGER E-006 records the Mojo blocker as a user
report establishing neither error, component, revision, nor
cause. A patch aimed at an unnamed failure is speculation,
not a route. Searches actually run and logged (Builder-2):
grep -rn for failure packet across the repo; find for *.patch,
*.diff, and *patch* files — zero patch files; find evidence
-type f; grep for candidate patch in docs.

What a packet must contain (B-001 Sections 1.2/7, Roadmap
Section 3.1): repeatable failing command, first relevant error
and exit code, exact host/target and environment class, tcmalloc
revision, build options, and local modifications, Mojo revision,
causal hypothesis, named consumer/integration layer, and a
candidate patch locus by path and pinned revision.

### B1.2 Definition and sub-routes (record separately, never conflated)

| Sub-route | Meaning |
|---|---|
| C1 consumer/toolchain source patch | A patch to consumer or toolchain source: a source file, platform macro, build target, or linker script. |
| C2 allocator-selection patch | A patch that changes which allocator is selected: link line or order, a build flag naming a different library, or baked-in preload configuration. |
| C3 allocator-implementation patch | A patch to a pinned upstream allocator revision, carried as a local diff against that revision. |

## B2. Exact questions (verbatim from B-001 Section 7)

Identity:
1. What exactly is patched, named by path and pinned revision? If it cannot be named, the route stops.
2. Which consumer does it reach (build tool, compiler, runtime/stdlib, generated executable, Aesir — Roadmap Section 3.3), with what symbol/linkage evidence?
3. Which sub-route, and what is the smallest diff testing the Phase 1 causal hypothesis?
4. What verbatim failure is it predicted to change, and what observable change counts as success, decided before trying?

Upstream vs local:
5. Does an upstream fix exist? Record project, issue/PR identifier, status (open/merged/declined/abandoned/superseded), revision, date checked; for tcmalloc also its exact revision, build options, local modifications (Roadmap Section 3.1).
6. If upstream merged an equivalent fix, why patch locally instead of taking that revision? A maintainer declining a patch is not itself a license restriction (Ruleset Section 3.2).
7. Backport, original local change, or third-party-derived? This determines license/provenance treatment.
8. Local-only or distributable?

Maintainability:
9. Divergence measured from the actual diff (git diff --stat), not estimated.
10. Rebase owner and cadence, re-test duty per rebase.
11. Sunset criterion (upstream release/fix or revision); a patch with none is an open limitation.
12. Does it touch a private/internal interface upstream may change without notice? Record as fragility.
13. Carried as one separable patch file against a pristine pinned tree (preferred), or intermingled (limitation: cannot be reverted/audited independently)?

Every question above is currently unanswerable: no component is
named (Q1 stops the route, by B-001's own rule).

## B3. Method / commands — DESIGN ONLY, none executed for this route

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

Design rules: identical command, environment, and input for
baseline and patched runs — only the patch differs. Compile
success is not runtime effect. A dry-run failure is recorded
as Blocked with its output; it is never silently hand-edited
and retried.

## B4. Licenses and linking — ALL TBD / TO-VERIFY until a target is named

Records license facts, not a legal opinion.

| Field | Value |
|---|---|
| Patched component + pinned revision + source origin | TBD — no component named |
| License from the component itself (name + license-file path + hash) | TO-VERIFY |
| Patch license if third-party-derived | TBD |
| Notices / attribution | TBD |
| Local-only vs distributed | TBD |
| Compatibility with project license | Project license settled MIT (LICENSE, (c) 2026 Volmarr Wyrd); MIT governs project code, does not license third-party material |
| Linking constraint (static/dynamic/header-only; conditions triggered) | TBD per sub-route; see note below |
| Symbols added/removed/renamed/versioned | TBD |
| Patent/trademark flags | TBD (Ruleset Section 9.1; no branding implying Google/Microsoft affiliation or confusable with mimalloc) |

Sub-route note (from Builder-3, grounded in the LGPL-2.1 text
on the Pi): C1 on the project's own MIT code is MIT only. C2
modifies nothing; the newly selected library's license governs
(all comparator licenses TO-VERIFY, none named). C3 on glibc
would be a true LGPL Section 2 modification (library form,
change-and-date notices Section 2b, whole work under LGPL
Section 2c, Sections 1/4 on distribution). A private,
never-distributed patch's duties are NOT asserted as nil:
on-Pi sources condition Sections 1/2/4 on copying/distribution
and leave running unrestricted; the private-use question is
left OPEN (uncertainty U-L3 in the Builder-3 working record).

## B5. Clean-room / provenance duties (patch route only, per CLEAN_ROOM_RULESET.md)

### B5.1 Licensed reuse is not clean-room
A licensed patch is licensed reuse, not a clean-room
implementation and not contamination — only if recorded first:
the license permitting use is identified, its conditions are
known before use, notices are carried, and a provenance-ledger
entry is made before first reliance (Ruleset Sections 2.7, 5.4,
9.2). Attribution added afterwards does not cure unlicensed
copying. Clean-room rule D-001 cross-reference (per B-002
Section 4): use and link; do not ingest upstream allocator
source into MimirAlloc.

### B5.2 Provenance-ledger entry before first reliance
Gap recorded in this slice: there is no live provenance ledger
file in this worktree — only docs/PROVENANCE_LEDGER_PROCESS.md
(the process describing one). Resumption therefore requires
that an append-only entry (origin-before-reliance, P-### fields,
derivation statement) can actually be made before any patch is
relied on. Results belong append-only in docs/EVIDENCE_LEDGER.md
with P2-R03 as the join key.

### B5.3 Forbidden material
Material of unknown or unrecorded license is forbidden until
identified (Ruleset Section 3.1(e)); found online is not a
license. Third-party implementation source is never copied or
paraphrased into MimirAlloc spec, code, comments, tests, or
agent prompts (Section 3.1(a)-(b)); a patch may inform a
diagnostic conclusion and dated behaviour observations only.
If tcmalloc is ever the patched component, its source stays
Reference-Team-only (Section 3.2).

### B5.4 Contamination procedure
On a contamination trigger: STOP, mark the artifact
QUARANTINED, remove and seal it, assess, and remediate from
the clean spec (Section 7.2). A quarantined artifact is never
quietly edited clean (Section 7.3).

### B5.5 Role / separation logging
This route is never described as making MimirAlloc clean-room,
or as a legally verified two-team clean room (Section 1.4).
Role switches and any separation-limited downgrade are logged
(Section 4.4).

## B6. Four integration modes — for this route specifically

### B6.1 LD_PRELOAD (M1)
Patch the preload library itself, or the build/launch
configuration that preloads it. Proof: loader binding evidence
for the target process. Limitation: ineffective for static
executables, hidden symbols, secure-execution mode, internal
allocators, and Android app-process restrictions. One process
proves nothing for another consumer.

### B6.2 Direct link (M2+M3)
Patch the link line or link target. Proof: link map/line plus
readelf/nm evidence. Limitation: link order and duplicate-
definition risk must be recorded; presence on the link line
is not proof of use.

### B6.3 Static replacement (M3, static form)
Patch the archive/object set compiled into the binary. Proof:
rebuilt binary hash plus symbol-table evidence. Limitation:
LD_PRELOAD cannot reach it; every statically linked consumer
needs its own rebuild and its own result row.

### B6.4 Namespaced API (M4)
Patch call sites to a named allocator API. Proof: call-site
diff plus a counter/audit hook showing calls entered the
patched path. Limitation: no interposition claim; unpatched
sites and runtime allocations keep the original allocator;
mixed-allocator ownership must be recorded; cross-allocator
free (Integration condition C6) is a correctness limitation
and a failed gate.

### B6.5 Summary

| Mode | Mojo compiler/wheel | Generated L4 | Aesir L5 |
|---|---|---|---|
| LD_PRELOAD | Applicable per sub-route, untested | Applicable per sub-route, untested | Applicable per sub-route, untested |
| Direct link | Applicable per sub-route, untested | Applicable per sub-route, untested | Applicable per sub-route, untested |
| Static replacement | Applicable per sub-route, untested | Applicable per sub-route, untested | Applicable per sub-route, untested |
| Namespaced API | Applicable per sub-route, untested | Applicable per sub-route, untested | Applicable per sub-route, untested |

## B7. Phase 6 gate, restated for this route

As A7, plus: a proof-of-reach pointer is mandatory per consumer
x mode, and the signature change must be shown on the exact
pinned artifact in the exact failing environment (the tablet,
not the Pi, for the recorded blocker).

## B8. Pi-observed facts for this route

Gate and manifest as A8.1. Probe: none for this route — no
patch exists, and none was invented. The repo searches in B1.1
are the only runs behind this Part, and they are searches, not
measurements. NOT run: no patch dry-run or apply, no patched
build, no failure reproduction, no comparator, no Mojo or
Aesir workload, no upstream or license verification (no
component named), no measurement of any kind.

## B9. Measured-result fields (ALL TBD, one row per consumer x mode x patched-vs-baseline)

| Field | Value |
|---|---|
| Run ID / date / device | TBD |
| Environment (arch/ABI, OS, libc, toolchain, page size) | TBD |
| Component + pinned revision; patch artifact + SHA-256 | TBD — none exists |
| Mode + consumer | TBD |
| Verbatim command; clean build dir Y/N | TBD |
| Build outcome / exit / first error | TBD |
| Failing-command outcome / exit / first error | TBD |
| Proof-of-reach pointer | TBD |
| Runtime/resource figures | Only if measured with a named method; none measured |
| Correctness/regression command + result | Only if run; none run |
| Raw log pointer | None exists |
| Evidence label | Not assigned (never inferred) in the per-row field; route label Blocked per B-001 Sections 1.3/9 — see I-B003-4 |
| Verdict | Blocked + resumption point (B1.1) |

## B10. Integration-limitation fields (TBD per row)

| Field | Value |
|---|---|
| Consumers NOT reached + why | TBD; currently all — no packet names any consumer |
| Modes NOT tested | All four (B6 states applicability only) |
| Static/hidden-symbol/sandbox/Android limitation | TBD per row; standing forms in B6 |
| Mixed-allocator ownership remaining | TBD |
| Private-interface fragility | TBD (B2 Q12) |
| Page-size/ABI/VA-width assumption introduced | TBD; VA-width relevance per INC-10 |
| Distribution/license limitation beyond this device | TBD (B4) |
| Maintainability burden (owner, cadence, sunset) | TBD (B2 Q9-Q11) |
| What this result does not establish | Mandatory free text per row. For this slice: this route establishes nothing about MimirAlloc, and nothing about the original failure, because neither a packet nor a patch exists. |

## B11. Revert / rollback criteria (patch route only)

Triggers — revert immediately and record Failed/Blocked if:
the patched run does not change the original verbatim failure
(or changes a different consumer/mode); any new correctness
failure appears; proof-of-reach cannot be produced; the
component or patch license cannot be identified or recorded,
or the provenance entry was not made before use (this is
quarantine under B5.4, not ordinary rollback); the diff
exceeds the recorded acceptable divergence, touches a private
interface with no sunset, or owner/cadence is blank; upstream
ships an equivalent fix (retire the patch and re-score against
the pinned upstream revision); or a measured, logged regression
in build time, runtime, or memory occurs (no threshold is
claimed in advance).
Procedure: stop work; preserve the patch artifact, its
SHA-256, and both log sets (deleting failure evidence is
forbidden); discard the patched worktree / restore the
pristine pinned revision; rebuild clean; re-run the baseline
command to confirm restoration; append (never edit) the
outcome and pointers to the evidence and provenance records.

## 8. Phase 2 exit table

Roadmap Section 8 exit, verbatim in substance: a table of
options with commands, measured results, and integration
limitations; no claims based on names alone. Exit rule (B-001
Section 9): Phase 2 passes only when every row has an exact
command, a measured result, and integration limitations filled
from evidence. An empty, name-only, or compile-only row is
Blocked, not a pass. Columns below are B-001 Section 9 verbatim;
B-002 Section 8 used a vertical Field|Value variant instead —
the divergence is recorded as I-B003-1 and this slice follows
the plan-defined B-001 format.

| Route ID | Route | Mode(s) to test | Exact command(s) | Environment / pinned revisions | License | Linking constraint | Measured result (summary + raw-data ref) | Evidence label | Integration limitations / Does not establish | Verdict |
|---|---|---|---|---|---|---|---|---|---|---|
| P2-R02 | Configurable build flags | Direct link (primary); LD_PRELOAD No; Static replacement conditional; Namespaced API No | Design: Section A4 (UNRUN); probe commands: Appendix A | Pi AArch64, glibc 2.36, GCC 12.2.0, ld 2.40; real target + source commit TBD | GPL-3.0 + GCC Runtime Library Exception (toolchain); static variants change analysis | Dynamic by default; per-build ldd/readelf/nm proof that flags did not silently change malloc resolution | TBD — not run for any real target. Probe only: max-page-size=4096 build segfaults (exit 139) on this 16 KiB Pi; Appendix A | Proposed (Device-tested, probe scope only) | Interpreted targets out of scope; -march=native non-portable; does not establish allocator superiority of any kind | TBD — Inconclusive for unblocking |
| P2-R03 | Local patch (C1/C2/C3) | All four, per sub-route | Design: Section B3 (none executed) | Component + pinned revision TBD — none named | TO-VERIFY (no component named) | TBD per sub-route (B4) | TBD — not run; no patch exists | Blocked | Blocked on Phase 1 failure packet; does not establish anything about MimirAlloc | Blocked |

Consumer/layer note: B-001's table has no consumer/layer
column, which B-002 and Integration Section 10.6 require
(I-B003-5). For both rows here the consumer is TBD — for R02
no target is named; for R03 no packet names a consumer.

## 9. Verdict and resumption

P2-R02: Proposed. Resumption requires a pinned pristine build
of a named target, one-flag-family-at-a-time clean rebuilds,
and a per-build symbol/ldd check (A6.2), scored against the
P2-R01 harness once a tracked harness exists.
P2-R03: Blocked. Resumption requires a real Phase 1 failure
packet passing Integration Section 12 Step 0 (path, sha256,
and BuildID pinned), a named patch locus, a live provenance
ledger to receive the pre-reliance entry (B5.2), and the
license of the patched component identified before use.
Phase 2 is not exited by this slice: R02 has no measured
real-target result and R03 is Blocked, so by the Section 8
exit rule above Phase 2 remains open (see I-B003-8).

## 10. Inconsistencies recorded

Inherited, cited not re-claimed: INC-1..INC-11 (B-001 Section
10), of which INC-3 (Section 11 comparison set naming the
not-yet-existing MimirAlloc and omitting jemalloc), INC-4
(Section 15 Q12 license question vs the settled MIT LICENSE),
INC-8, and INC-10 bear directly on this slice; I-B002-3, -4,
-7, and -9 (B-002 Section 10) likewise. New in this slice:

- I-B003-1: B-002's banner (B-001 has NOT banked; its fields
  were NOT used) is stale in-tree — git log shows B-001 af893ee
  banked immediately before B-002 9296c90 — and the two slices
  bank the Phase 2 exit table in incompatible shapes (B-001
  Section 9 wide columns vs B-002 Section 8 Field|Value).
- I-B003-2: The four named modes do not map one-to-one to
  Integration modes M1-M5: direct link spans M2+M3, and static
  replacement has no number of its own (B-002 Section 5.3 files
  it as M3, static form).
- I-B003-3: Three unreconciled verdict vocabularies coexist:
  B-001 (Unblocks / Partially unblocks / Does not unblock /
  Inconclusive / Blocked), Integration (VIABLE / VIABLE WITH
  OWNERSHIP DEFECT / and kin), and B-002's mixed usage.
- I-B003-4: B-001 contradicts itself on R03's evidence label:
  Sections 1.3/9 say Blocked; Section 7's per-row field says
  Not assigned (never inferred). This slice follows Sections
  1.3/9 for the route label and keeps Not assigned in the
  per-row field (B9), and flags the contradiction rather than
  silently resolving it.
- I-B003-5: B-001's exit table lacks the consumer/layer column
  that B-002 and Integration Section 10.6 require (Section 8
  note above).
- I-B003-6: One act, three routes: a flag naming an allocator
  is R04 per B-001 Section 6, R03-C2 per Section 7, and R04
  per Section 8.
- I-B003-7: R02's max-page-size=16384 question (A2 Q4) collides,
  uncross-referenced, with Phase 4's no-fixed-page-assumption
  exit — and the evaluation Pi is itself the 16 KiB device, so
  the harmful variant works locally. The A8 probe shows the
  converse failure is real on this device.
- I-B003-8: Phase 2 exit as literally specified has no defined
  evaluated, honestly Blocked resting state — the state B-002
  already occupies and B-003 now also occupies — while Phase 1
  remains open.
- I-B003-9: Three unreconciled Phase 1 exit gates (Roadmap
  Section 3.3 vs Section 8 vs Section 17), as recorded in
  docs/failure-capture.md; carried forward here because R03's
  resumption depends on which gate issues the failure packet.
- I-B003-10: No live provenance ledger file exists — only
  docs/PROVENANCE_LEDGER_PROCESS.md describing one (B5.2).
  R03 cannot satisfy its own pre-reliance entry duty until
  the ledger exists.

## Appendix A. Probe / inventory source

Probe source (Builder-1, run on the Pi, /tmp only): a trivial
C program, `int main(void){return 42;}`, 27 bytes, built and
run as recorded in A8.2 with GCC 12.2.0 / ld 2.40. Full
per-build commands, file sizes, readelf/file/ldd/size output,
and sha256 hashes for the first four flag sets are in the
Builder-1 working draft /tmp/b003-route2.md on the Pi
(217 lines, sha256
3058576034cae2b735fd40d3388bc049b68a83f7d3b6bc263d26b458c4c29e42
at drafting). Route-3 search record: /tmp/b003-route3.md
(222 lines, sha256
98873c765feade85c09f62d8ba0b7507fc86aa9d49259a4207b9f73337fe801a).
Shared license/exit-table working record: /tmp/b003-shared.md
(301 lines, sha256
a5c74ac4d2cfce2f029ce08f5e07395c52752d53dc842e23566ebe026e161579).
Honesty note: the /tmp drafts are working records on the Pi,
not banked artifacts; the banked evidence in this repository
is this document's Section A8 summary itself. No probe exists
for Part B.

## Appendix B. Acceptance mapping

Roadmap Section 8 (Phase 2): this slice contributes evaluated
routes R02 and R03; it does not exit Phase 2 (R01 was evaluated
in B-002; R04 is a separate slice). Section 11 principles:
baseline-first (A3), selection proven by ldd/readelf/nm and
never by name (Sections 2, A6.2), no benchmark claim without
the Section 11 protocol (no benchmark is claimed). Section 15
open questions touched, none closed: Q2/Q3/Q4/Q10 as mapped in
B-001/B-002; Q12 (license) is answered for the project (MIT)
but remains TO-VERIFY for every patch-route component (INC-4).
Phase 6 gate: restated per route in A7 and B7; not closed by
this slice for either route.
