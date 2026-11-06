# MimirAlloc Established Allocator Evaluation -- Diagnostic Comparators

Slice: B-004 (Phase 2, route 4 -- P2-R04). Companions: B-001 (Phase 2 evaluation
plan, docs/PHASE_2_EVALUATION_PLAN.md, Section 8 defines this route and its
fields) and B-002 (system-allocator evaluation, docs/SYSTEM_ALLOCATOR_EVALUATION.md,
whose per-route format this document follows). Status: research / evaluation.
Phase 2 is not exited by this slice. Citations: Roadmap =
MimirAlloc_Research_and_Development_Roadmap.md; Plan = B-001; Baseline =
BASELINE_INVENTORY.md; Targets = SUPPORTED_TARGETS.md; Integration =
INTEGRATION_LAYER_MAP.md; Research 11 = docs/research/11_ALLOCATOR_LANDSCAPE.md.
Pi-observed facts only in Section 10.

## 1. Honest headline

The comparators in this slice are **diagnostics only**. Evaluating mimalloc,
jemalloc, or tcmalloc as a comparator implies **no project dependency** on any
of them, in any form: no dependency is adopted, proposed, or implied by a
comparator run, a comparator result, or the existence of this document. The
comparators do not cancel MimirAlloc, do not replace it, and do not pre-empt
it. MimirAlloc remains the independent project; this route exists only to
gather evidence while it is built.

Phase 1 remains open: the Phase 1 failure packet still does not exist. There
is no repeatable failing command with its first relevant error and exit code,
exact host/target, tcmalloc and Mojo revisions, and causal hypothesis on
record against which a comparator could be scored. Therefore no comparator in
this slice is scored against the original failure, and no claim of that kind
is made. All three comparators stand at **Proposed** as routes.

One narrow exception to "nothing was run", recorded in full in Section 10:
the system jemalloc library already present on this Pi was exercised in one
tiny, bounded LD_PRELOAD probe (three runs, exit 0, selection proven by loader
binding evidence, /proc/self/maps, a behavioural size-class delta, and
jemalloc's own statistics header). That is Device-tested evidence **for that
probe process only**. It proves nothing about Mojo, nothing about Aesir, and
nothing about performance: no benchmark protocol was run and no timing is
claimed. mimalloc and tcmalloc are absent on this Pi (re-verified live,
Section 10) and were not run in any mode.

## 2. What the route means

Route 4 (P2-R04) means: run the **same workload** under a mature established
allocator, with that allocator **proven selected**, as a controlled diagnostic
comparator. It answers two questions, and only those two:

- (a) **Unblocking (Roadmap Q10):** would a simpler established route unblock
  Aesir while MimirAlloc is built? Test objectively, without abandoning the
  independent project.
- (b) **Comparison discipline (Roadmap Section 11):** what does a mature
  allocator actually do on this workload, measured under the common protocol
  in Section 3, against the B-01 platform-libc baseline?

The hypothesis is falsifiable both ways:

- If the failure signature **changes** when the allocator changes, that is
  allocator-specific evidence for that consumer.
- If the signature is **unchanged**, the allocator is not causal for that
  consumer -- but only after the alternative explanation, that the comparator
  was never actually selected, has been excluded by selection proof first
  (Section 3, item 1). An unchanged signature without selection proof proves
  nothing in either direction.

What this route cannot do:

- It cannot reach static or bundled consumers by preload; there is nothing to
  interpose on without a rebuild.
- It cannot fix internal allocators (Integration C4).
- It cannot fix address-width, loader, or driver faults.
- It cannot interpose in Android apps (Integration C5).

A result on one target or libc is evidence about that target and libc only. A
Pi / Linux AArch64 + glibc result is not an Android, bionic, musl, proot, or
tablet result.

Route membership: P2-R04a mimalloc (Baseline B-05), P2-R04b jemalloc
(Baseline B-04), P2-R04c tcmalloc (Baselines B-02 + B-03). jemalloc's
membership is recorded explicitly: it IS in the comparator set (see
I-B004-1). The other Research 11 landscape allocators (snmalloc, scalloc,
TLSF, Scudo, StarMalloc) are study-only and are not comparators in this
route; their disposition is recorded in Section 8.

## 3. Common protocol and fields

Condensed from B-001 Section 4 (the common evaluation protocol, per Roadmap
Section 11). This protocol governs every comparator row in this document; a
run that does not follow it is not a row in the exit table.

1. **Selection proof, per Section 11.** Verify each allocator was actually
   selected: record dynamic vs static linkage and exported/unresolved symbol
   evidence -- a DT_NEEDED entry for direct link; LD_DEBUG=bindings output
   binding malloc/free to the comparator library **plus** /proc/<pid>/maps
   showing it loaded, for LD_PRELOAD; exported T/D symbols in the library vs
   U symbols in the consumer; and a visible allocation counter / audit hook
   proving the comparator serviced the test process, for any interposition
   claim. Name match, package presence, or compile success is never
   selection proof.
2. **Fresh processes; pinned versions.** Every run is a fresh process. Pin
   versions (upstream repo URL, release/commit, checksum, build flags,
   host/target, page size, compiler/linker). Record thermal state, core
   selection, clock policy, workload, and repetitions in a **per-run
   environment manifest**.
3. **Metrics reported.** Median, p95/p99 and outliers for allocation latency;
   throughput; peak RSS/PSS; virtual size; retained memory after free;
   mapping count; startup time; CPU time; responsiveness and energy where
   reliably instrumented.
4. **Benchmark classes.** Classes 1-5 (1 single-thread small alloc/free,
   2 producer/consumer and cross-thread free, 3 thread churn / big.LITTLE
   clusters, 4 large allocations and mixed lifetimes, 5 fragmentation /
   return-to-OS / startup-shutdown) run on the standalone test program
   **first**. Class 6 (real Mojo build) and class 7 (Aesir load / prefill /
   decode / unload) run only where integration is proven for that mode.
   Class 8 is tablet-only.
5. **One variable at a time.** Baseline interleaved (A-B-A), warm-up
   discarded, identical harness/input/settings across routes. Tuned vs
   untuned and system-package vs pinned-build are separate rows.
6. **Raw data and negative results are stored.** Store raw data, harness
   revision, scripts, and negative results; flag noise rather than invent
   certainty. Never call swap physical RAM. A lower allocation-time number
   that increases memory pressure or UI stutter does not satisfy the mobile
   objective.
7. **Pi guard.** On the Pi: check free -g before and after any run; stop if
   available memory falls materially; never run a stress harness
   concurrently with Samtal serving, and never starve Samtal on :8787.

### Common measured-result fields

Verbatim-compatible with B-001 Section 8's common fields. Every field is TBD
until a run under the protocol above fills it from a preserved raw log:

Allocator + pin TBD | Mode TBD | Linkage dynamic/static TBD | Selection-proof
artifact TBD | Workload/class TBD | Repetitions (fresh processes) TBD |
Median/p95/p99 latency TBD | Throughput TBD | Peak RSS/PSS TBD | Virtual size
TBD | Retained after free TBD | Mapping count TBD | Startup time TBD | CPU
time TBD | Responsiveness/energy where reliable TBD | Correctness failures
TBD | Raw log path TBD.

Per-comparator extra fields are listed in Sections 5.1-5.3.

### Common integration-limitation fields

Verbatim-compatible with B-001 Section 8's common fields:

Modes not viable for this consumer + why TBD | Symbols not intercepted /
modules bypassing TBD | Android/bionic applicability (separate, unproven)
TBD | Page-size limitation TBD | Failure signature changed? (Y/N + evidence)
TBD | What this result does not prove TBD.

## 4. Selection and configuration for this route

- One comparator at a time, one variable at a time, baseline re-run
  interleaved (Section 3, item 5). A comparator number with no same-session
  baseline run is not a result.
- **System package vs pinned build are different comparators.** The system
  jemalloc on this Pi (libjemalloc2:arm64 5.3.0-1) has an uncontrolled
  version and configuration from this project's point of view; a pinned
  upstream build is a separate row with its own pin record. Section 10's
  probe is a system-package row.
- **Tuned vs untuned are separate rows.** Any MALLOC_CONF, arena, decay, or
  equivalent setting is recorded verbatim in the row it belongs to and is
  never merged into an untuned row. The Section 10 statistics run used
  MALLOC_CONF=stats_print:true, which changes reporting only; it is recorded
  as a configured corroboration run, not as a tuned measurement, and no
  tuned-performance row exists.
- Configuration is not selection. An allocator that is configured, present,
  or named on a link line has not been shown to service allocations until
  Section 3 item 1 evidence exists for the exact process under test.

## 5. Comparators

### 5.1 mimalloc -- P2-R04a

**Route ID / Status / Evidence label:** P2-R04a / Proposed / Proposed.
**Role:** established allocator as diagnostic comparator only. No project
dependency. **Baseline:** B-05 -- candidate only; no build or run recorded.
Not present on this Pi (B-001 Section 3 probe; re-verified live in this
slice, Section 10).

Naming control: this comparator is written in full as "mimalloc" in all
records. It is never shortened, abbreviated, or conflated with "MimirAlloc",
the project.

#### Exact questions

1. Does a pinned mimalloc build support this Pi's 16 KiB page configuration
   without fixed-page assumptions?
2. Which modes are true global replacement here, and which modes are a
   namespaced API requiring call-site adoption?
3. Research 11 question: what locality and retention trade-offs emerge on
   edge traces, including free-list sharding and local versus remote
   behaviour?

No answer to any question above is recorded in this slice. All answers are
TBD pending a pinned artifact and a run under a later slice.

#### Method / commands (DESIGN ONLY -- UNRUN)

```text
# LD_PRELOAD (dynamic, non-secure-execution consumers only):
LD_PRELOAD=<pinned-mimalloc.so> LD_DEBUG=bindings <test-binary>   # UNRUN
# Direct link (record DT_NEEDED and the link map):
cc <objs> -L<pin> -l<lib> -Wl,-Map=<map>                          # UNRUN
# Static replacement: pinned static archive at relink time,
# only for binaries this project rebuilds.                          # UNRUN
# Namespaced API: the allocator's prefixed API at call sites in a
# project-owned test program only; record adopted call sites by diff. # UNRUN
```

#### License to record

No license name is asserted for mimalloc in this slice.

- Pinned artifact actually used: TBD (none used).
- License field: TO-VERIFY from the pinned upstream release license file,
  at pin time; record version, copyright holder, notice-retention and
  redistribution terms in the provenance ledger.
- Diagnostic binary use / source inspection / source reuse: three separate
  questions, each TO-VERIFY; never collapsed into one statement.

The project license is MIT (LICENSE, (c) 2026 Volmarr Wyrd). It governs
project code only and does not license third-party material, mimalloc
included.

#### Linking constraints to record (ALL TBD at pin)

Shared vs static availability for AArch64 TBD | Symbol visibility/versioning
TBD | Whether all consumers must agree on one build configuration TBD | ABI
compatibility with the consumer's glibc and C++ runtime TBD. No mode is
treated as available until its constraint is resolved at pin time.

#### Modes assessed for this comparator

1. **LD_PRELOAD:** LD_PRELOAD=<pinned .so> with LD_DEBUG=bindings. Dynamic,
   non-secure consumers only. True global replacement for those consumers,
   if selection is proven.
2. **Direct link:** cc <objs> -L<pin> -l<lib> with -Wl,-Map recorded;
   record DT_NEEDED. Global replacement for the linked binary only.
3. **Static replacement:** pinned static archive at relink time; only for
   binaries this project rebuilds. Not applicable to shipped binaries.
4. **Namespaced API:** prefixed API at call sites in a project-owned test
   program only. Not global replacement, never reported as such; record
   adopted call sites by diff.

#### Measured-result fields (ALL TBD)

Pinned artifact identifier TBD | Build configuration TBD | Page
configuration result on 16 KiB pages TBD | Fixed-page assumption found TBD |
Mode exercised TBD | Selection-proof artifact TBD | Workload/class TBD |
Common fields (Section 3) TBD | Locality result on edge traces TBD |
Retention result on edge traces TBD | Free-list sharding behaviour TBD |
Local vs remote behaviour TBD | Correctness failures TBD | Raw log path TBD.

#### Integration-limitation fields

Modes not viable for this consumer + why TBD | Page-size limitation TBD
(the 16 KiB question above is this field's first entry when run) | Failure
signature changed? TBD | What this result does not prove TBD. Recorded now:
project dependency -- none, diagnostic comparator only; consumers in scope
for LD_PRELOAD -- dynamic, non-secure only; binaries in scope for static
replacement -- project-rebuilt only.

### 5.2 jemalloc -- P2-R04b

**Route ID / Status / Evidence label:** P2-R04b / Proposed / Proposed as a
route; the Section 10 probe is Device-tested for the probe process only.
**Role:** established allocator as diagnostic comparator only. No project
dependency. **Baseline:** B-04 -- candidate only; no jemalloc build,
configuration, or run was recorded before this slice.

**Set membership (decision recorded):** jemalloc IS in the Phase 2
comparator set, as P2-R04b. This follows B-001's operational resolution of
INC-3 and is recorded explicitly here, as B-001 Section 8.2 required
(see I-B004-1).

**Presence:** system libjemalloc.so.2 is present on this Pi, package
libjemalloc2:arm64 5.3.0-1 (B-001 Section 3; re-verified live in this slice,
Section 10 -- presence, artifact details, one bounded preload probe).
Presence alone would be inventory only; what the probe adds is stated
exactly in Section 10 and nowhere else.

#### Exact questions

1. Provenance J-1: public Mojo source selects jemalloc for the Linux Mojo
   driver while shipped wheels abort in TCMalloc -- can jemalloc selection
   in this project's own test program be proven independently of that
   unresolved question? (Section 10 answers this for a trivial standalone
   probe only: yes, with loader binding evidence. The Mojo provenance
   question itself remains unresolved.)
2. Research 11: how do concurrency, tuning, and retention compare (arenas,
   bins, decay, statistics)?
3. Does the system libjemalloc.so.2 suffice for a diagnostic run, or is a
   pinned upstream build required for a fair comparator (system package
   version/config are uncontrolled)?

#### Method / commands

```text
# LD_PRELOAD with the present system artifact -- RUN in this slice as a
# bounded probe only; see Section 10 for the exact commands, outputs, and
# limits of that run:
LD_PRELOAD=/lib/aarch64-linux-gnu/libjemalloc.so.2 /tmp/b004_probe
LD_PRELOAD=/lib/aarch64-linux-gnu/libjemalloc.so.2 LD_DEBUG=bindings /tmp/b004_probe
MALLOC_CONF=stats_print:true LD_PRELOAD=/lib/aarch64-linux-gnu/libjemalloc.so.2 /tmp/b004_probe
# Direct link (DESIGN ONLY -- UNRUN; and see the linking constraint below:
# -ljemalloc cannot resolve against the present artifact as-is):
cc <objs> -ljemalloc -Wl,-Map=<map>                               # UNRUN
# Namespaced API: jemalloc's prefixed/non-standard API and its
# statistics/configuration interfaces from a project-owned test
# program only.                                                     # UNRUN
# Static replacement: N/A until a pinned static build exists.
```

#### License to record

- **System artifact (the one present and probed): VERIFIED from the
  artifact on this Pi.** /usr/share/doc/libjemalloc2/copyright (Debian
  copyright-format 1.0, upstream jemalloc, source jemalloc.net) states, for
  Files: *: Copyright 2002-present Jason Evans, 2007-2012 Mozilla
  Foundation, 2009-present Facebook, Inc.; License: BSD-2-Clause, with the
  redistribution conditions printed in that file (retain/reproduce the
  copyright notices and disclaimer). Read on the Pi in this slice
  (Section 10). This discharges, for the system artifact only, the check
  B-001 Section 8.2 prescribed. It is a record of the file's contents, not
  a legal opinion.
- **Pinned upstream build, if one is ever made: TO-VERIFY** from that
  release's own license file at pin time; record which source governed any
  run actually made (system copyright file vs pinned release).
- Diagnostic binary use of the system library in a probe is the use the
  copyright file above describes; source inspection and source reuse were
  not performed and remain separate questions, TO-VERIFY if ever proposed.
  No jemalloc source was copied into this project.

The project license is MIT (LICENSE, (c) 2026 Volmarr Wyrd); it does not
license third-party material, jemalloc included.

#### Linking constraints (recorded from the Pi, Section 10)

- The inventoried system library is **shared-only as installed**: the file
  /lib/aarch64-linux-gnu/libjemalloc.so.2 (593,328 bytes, stripped,
  BuildID[sha1]=6de2f67281be05e21eacaf53174dc78e153a9d4f). There is **no**
  unversioned libjemalloc.so linker name, **no** headers under /usr/include,
  and **no** libjemalloc-dev package installed -- so -ljemalloc direct link
  cannot resolve as-is; direct link would need the dev package, an explicit
  path link, or a pinned build (all UNRUN).
- Exported surface of the present artifact (nm -D, T symbols): malloc,
  free, calloc, realloc, posix_memalign, aligned_alloc, memalign, valloc,
  malloc_usable_size -- the contracts.md core candidate set -- plus
  jemalloc's extended surface: mallctl, malloc_stats_print.
- /etc/malloc.conf (jemalloc's system-wide configuration file) is absent on
  this Pi; the probe ran with default configuration except where
  MALLOC_CONF is recorded.
- Static availability, version-symbol compatibility, and configuration for
  a pinned build: TBD at pin. A tuned run and an untuned run are different
  comparators (Section 4).

#### Modes assessed for this comparator

1. **LD_PRELOAD:** the only mode usable with the present artifact without
   relinking. **Exercised in this slice** against a trivial project-owned
   probe only (Section 10): selection proven by LD_DEBUG=bindings (the
   probe's malloc/free bound to libjemalloc.so.2), /proc/self/maps, the
   usable-size behavioural delta (40 under glibc vs 32 under jemalloc for
   malloc(32)), and jemalloc's own statistics header under
   MALLOC_CONF=stats_print:true. Not exercised against any Mojo or Aesir
   consumer.
2. **Direct link:** cc <objs> -ljemalloc with link map; record
   DT_NEEDED=libjemalloc.so.2. DESIGN ONLY, and blocked as-is by the
   missing linker name above; viable with a dev package or pinned build.
3. **Static replacement:** N/A until a pinned static build exists.
4. **Namespaced API:** jemalloc's prefixed/non-standard API and
   statistics/configuration interfaces in a project-owned test program
   only; statistics readout is corroboration, never sole selection proof.
   DESIGN ONLY -- the probe called no namespaced API; the statistics
   header in Section 10 was produced by jemalloc itself under an
   environment setting, not by an adopted call site, and is not reported
   as a namespaced-API result.

#### Measured-result fields

System-package, untuned, LD_PRELOAD, probe only (Section 10): Source --
system 5.3.0-1 | Mode -- LD_PRELOAD | Selection-proof artifact -- bindings
log + maps + behavioural delta + statistics header, quoted in Section 10 |
Workload/class -- none (trivial probe; no benchmark class claimed) |
Repetitions -- 3 fresh preload runs, exit 0, identical output | Correctness
-- probe completed its alloc/fill/free sequence and printed "probe success"
in all runs (this probe performs no pattern verification) | Timing/latency/throughput/
RSS/retention -- NOT measured; no performance claim.
All other rows TBD: Source (pinned upstream) TBD | Tuning/config rows TBD |
Arena/decay settings TBD | Concurrency result TBD | Retention result TBD |
Provenance J-1 for any real consumer TBD | Raw log path -- Section 10
quotes (probe artifacts live in /tmp; see I-B004-8).

#### Integration-limitation fields

Recorded now: preload interposition under the probe was process-wide --
the bindings log shows libc.so.6's and libstdc++.so.6's own malloc/free
(and C++ operator new/delete) bindings also resolving to libjemalloc.so.2;
a preload result therefore describes whole-process interposition, not a
single call site. Direct link blocked as-is (linker name absent). Static
consumers, hidden-symbol consumers, secure-execution contexts, and Android
app processes are not reached by this mode (Section 2). System package
version/config uncontrolled -- fairness vs a pinned build is TBD (question
3). Failure signature changed? -- not applicable: no Phase 1 failure packet
exists to compare against. What this result does not prove: anything about
Mojo, Aesir, performance, retention, or concurrency.

### 5.3 tcmalloc -- P2-R04c

**Route ID / Status / Evidence label:** P2-R04c / Proposed / Proposed.
**Role:** established allocator as diagnostic comparator only. No project
dependency. **Baselines:** B-02 (upstream-documented; no build, log, or
result in the repository) and B-03 (field experiment: summaries only, the
artifacts are NOT in this repository; a prior summary records a guarded
build reaching 264 of 300 Bazel actions, with no archive produced and no
allocator execution; Field 05 gates 1-6 remain open). Not present on this
Pi (B-001 Section 3; re-verified live, Section 10). No mode below has been
executed in this slice.

**Role warning:** TCMalloc is the implicated allocator, not a neutral
comparator. For each run, record whether the goal is failure reproduction
under a pinned TCMalloc or comparison against TCMalloc. The two goals are
not interchangeable and must not be reported as one result.

#### Exact questions

1. Address-width reproducibility: a Field 05 header-level summary records
   default 48 accepted, explicit 39 accepted, and 38 and 49 rejected. This
   is a prior summary, not re-measured in this slice. The B-03 source,
   log, and cache live outside the repository (J-2). Whether the result
   can be reproduced from a recorded pin is therefore unresolved.
2. Project identity: Google TCMalloc and gperftools are distinct projects
   and are not identical in API or implementation (Roadmap S11). Which
   project exactly is pinned must be recorded; the name "tcmalloc" alone
   does not settle it.
3. Linkage in the implicated consumer (Roadmap Q3): is TCMalloc statically
   or dynamically linked in the implicated consumer, and what prevents a
   configurable allocator from being selected in its place?

Upstream documentation stating "Linux AArch64 supported" (S1) is not
evidence for this build, operating system, ABI, or device.

#### License to record

TO-VERIFY from the pinned artifact at pin time. No license name is
asserted in this slice. Binary use, source inspection, and source reuse
are separate questions, to be answered separately for the exact artifact
pinned. Diagnostic use plans no source reuse.

#### Linking constraints to record (ALL TBD)

The intended route is a Bazel-based pinned build. Verbatim field warning
(BASELINE_INVENTORY Section 4.1): every TCMalloc translation unit and the
consumer must agree on the address-width setting; this agreement is a
build requirement, not a default to be assumed. Also to be recorded
before any run: static vs dynamic availability; rseq/platform requirements
(S7); C++ ABI. TCMalloc-specific symbols have been observed in real
binaries: TCMallocGlobals::tc_new / tc_delete, TCMallocInternal*, and weak
tcmalloc_internal::* symbols in libAsyncRTRuntimeGlobals.so (Integration).
Defined internal symbols prove that code is linked in; they do not prove
which path services an allocation at runtime.

#### Modes assessed for this comparator (DESIGN ONLY -- UNRUN)

1. **LD_PRELOAD:** viable only if a pinned shared build exists and the
   consumer is dynamic. A statically linked or bundled consumer (as
   recorded for parts of the installed Mojo runtime) cannot be reached
   this way; for such a consumer record mode-not-viable, not a TCMalloc
   failure.
2. **Direct link:** pinned library into a standalone test program; record
   DT_NEEDED plus the link map.
3. **Static replacement:** Bazel malloc attribute / linkopts / archive
   selection at rebuild of a project-controlled target (Integration M3).
   Not applicable to prebuilt shipped binaries.
4. **Namespaced API:** TCMalloc-specific extensions and profiling
   interfaces from a project-owned test program only. Success in this
   mode is not drop-in success.

**Extra proof bar:** any claim about TCMalloc in a consumer must separate
(a) TCMalloc-derived symbols defined inside a consumer library, (b)
standard allocation symbols undefined there and bound elsewhere, and (c) a
runtime binding audit showing which implementation serviced allocations.
No one of these substitutes for the others.

#### Measured-result fields (ALL TBD)

Project (google/tcmalloc vs gperftools) TBD | Pinned commit TBD |
Address-width setting, all units agree TBD | Bazel actions completed TBD |
Failure signature reproduced (Y/N + packet link) TBD | Common fields
(Section 3) TBD | Raw log path TBD.

#### Integration-limitation fields

Modes not viable for this consumer + why TBD (first entry when known:
LD_PRELOAD vs bundled/static consumers) | Address-width assumption TBD |
Failure signature changed? TBD | What this result does not prove TBD.
Recorded now: project dependency -- none; B-03 artifacts live outside the
repository, so this route cannot yet be reproduced from the repository
alone (J-2).

## 6. Four modes across this route -- summary

Mode vocabulary: this document uses B-001's four named modes throughout.
INTEGRATION_LAYER_MAP.md codes the same territory M1-M5, where its direct
link spans M2+M3 and static replacement is M3 in static form; the named
modes here are the join key to B-001 (see I-B004-7).

| Mode | P2-R04a mimalloc | P2-R04b jemalloc | P2-R04c tcmalloc |
|---|---|---|---|
| LD_PRELOAD | Viable in design; pin TBD; not run | Viable with present artifact; RUN vs trivial probe only (Sec. 10) | Only if a pinned shared build exists and consumer is dynamic; not-viable for bundled/static consumers |
| Direct link | Viable in design at pin; not run | Blocked as-is (no linker name/headers); needs dev package or pinned build; not run | Viable in design into a standalone test program; not run |
| Static replacement | Project-rebuilt binaries only; not run | N/A until a pinned static build exists | Bazel rebuild of project-controlled targets only; N/A for shipped binaries; not run |
| Namespaced API | Test program only; not global replacement; not run | Test program only; statistics = corroboration only; not run | Test program only; not drop-in success; not run |

Mode-selection rule for future runs of this route: default to LD_PRELOAD
on a standalone test program, escalate to another mode only with a
recorded reason, so rows stay comparable across a/b/c.

## 7. Comparator guardrails

These guardrails are B-004's own and govern every comparator in this
slice.

1. **Diagnostics only.** Comparators never cancel, replace, or declare
   redundant the independent MimirAlloc project, and no comparator is a
   project dependency.
2. **No selection without Section 11 proof.** Presence, name match, or
   compile success is never proof.
3. **No invented measurements and no invented licenses.** Every figure
   cites a run that happened and is logged; every license statement cites
   the artifact it was read from (Section 5.2) or reads TO-VERIFY.
4. **One variable at a time,** with the same workload and environment
   manifest as the B-01 baseline; tuned vs untuned and system vs pinned
   are separate rows.
5. **Mode honesty.** A namespaced-API success is not a drop-in
   replacement success; an LD_PRELOAD failure on a static consumer is a
   mode limitation, not an allocator verdict.
6. **Target honesty.** Pi/glibc results are not Android/bionic/musl/
   proot/tablet results; Android has no LD_PRELOAD route for installed
   apps (zygote namespaces, SELinux).
7. **Mobile objective governs.** Latency wins that raise memory pressure,
   retention, or stutter do not satisfy the objective; report negative
   results.
8. **Clean-room separation.** Comparator behaviour may be observed and
   documented; comparator implementation must not be copied into
   MimirAlloc; any source inspection is provenance-logged.
9. **Naming discipline.** "mimalloc" and "MimirAlloc" in full,
   everywhere.

## 8. Study-only landscape -- disposition

snmalloc, scalloc, TLSF, Scudo, and StarMalloc are Research 11 study
subjects (BASELINE_INVENTORY B-06). They are not Phase 2 comparators and
they are not baselines. No builds and no runs are recorded for any of
them, and Phase 2 does not name them. Promoting any one of them into
P2-R04 requires a recorded decision -- a future decision-ledger entry.
This slice makes no such decision and records no such promotion. Scudo is
listed in Research 11 separately and is never treated here as bionic's
allocator (B-002 Section 4).

| Subject | Phase 2 status | Builds / runs | Route to comparator status |
|---|---|---|---|
| snmalloc, scalloc, TLSF, Scudo, StarMalloc | Study-only; not comparators; not baselines | None recorded | Future decision-ledger entry only |

## 9. Phase gate, restated for this route

Roadmap Section 8, Phase 2 exit, quoted verbatim: "table of options with
commands, measured results and integration limitations. No claims based on
names alone." Section 11 of this document contributes this route's rows.
Phase 2 passes only when every route's rows carry an exact command, a
measured result, and integration limitations filled from evidence; an
empty, name-only, or compile-only row is Blocked, not a pass. No
comparator result, however strong, promotes any allocator to a project
dependency in Phase 2. The gate for using a comparator against the
original failure is the Phase 1 gate restated by B-002 Section 6: the
original failing command succeeds, with proof the comparator serviced the
allocations, on the exact pinned artifact in the exact failing
environment -- or a blocker is recorded. A hello-world does not close it.

## 10. Pi-observed facts (Team-04, 2026-11-06)

This is the ONLY measured section of this document. Everything above is
document-derived or design, except where it points here. All commands ran
on the Pi (purpleviking1) on 2026-11-06, light and read-only except the
bounded probe in 10.4, which ran in /tmp, single-process, sub-second.

### 10.1 Gate and manifest

Gate: free -g showed 15 total / 13 available before the probe; Samtal on
:8787 returned HTTP 200 before and after the probe. The Pi was not
loaded, restarted, or reconfigured by this slice; nothing was installed.

Manifest, verbatim values (this slice's runs):

- date: 2026-11-06T09:07:28-05:00
- uname -m: aarch64; uname -r: 6.12.112-v8-16k-hailo+
- getconf PAGESIZE: 16384
- Toolchain and glibc versions as recorded for this Pi in B-001 Section 3
  (glibc 2.36; gcc 12.2.0; GNU ld 2.40) -- not re-run in this slice.

### 10.2 Presence re-verification (B-001 Section 3 probes, re-run live)

```text
$ ldconfig -p | grep -i mimalloc   -> [no output] exit 1
$ ldconfig -p | grep -i jemalloc   -> libjemalloc.so.2 (libc6,AArch64) => /lib/aarch64-linux-gnu/libjemalloc.so.2
$ ldconfig -p | grep -i tcmalloc   -> [no output] exit 1
$ dpkg -l | grep -i jemalloc       -> ii libjemalloc2:arm64 5.3.0-1 arm64 general-purpose scalable concurrent malloc(3) implementation
$ dpkg -l | grep -i -E 'mimalloc|tcmalloc' -> [no output] exit 1
$ ls /usr/lib/aarch64-linux-gnu/ | grep -i -E 'mimalloc|jemalloc|tcmalloc' -> libjemalloc.so.2 only
$ ls /usr/include | grep -i -E 'mimalloc|jemalloc|tcmalloc' -> [no output] exit 1
```

Verdict: B-001's inventory stands exactly -- jemalloc present (system
package), mimalloc absent, tcmalloc absent.

### 10.3 jemalloc artifact, as installed

- File: /lib/aarch64-linux-gnu/libjemalloc.so.2 -- 593,328 bytes, ELF
  64-bit LSB shared object, ARM aarch64, dynamically linked, stripped,
  BuildID[sha1]=6de2f67281be05e21eacaf53174dc78e153a9d4f.
- No unversioned libjemalloc.so, no libjemalloc-dev package (dpkg query
  exit 1), no jemalloc headers under /usr/include.
- Exported dynamic symbols include (nm -D, T): aligned_alloc, calloc,
  free, mallctl, malloc, malloc_stats_print, malloc_usable_size, memalign,
  posix_memalign, realloc, valloc.
- /etc/malloc.conf: absent.
- License source read on the Pi: /usr/share/doc/libjemalloc2/copyright --
  Debian copyright-format 1.0; Upstream-Name: jemalloc; Source:
  jemalloc.net; Files: * -- Copyright 2002-present Jason Evans,
  2007-2012 Mozilla Foundation, 2009-present Facebook, Inc.; License:
  BSD-2-Clause (full conditions in that file). The same directory holds
  changelog.Debian.gz, changelog.gz, and a README.

### 10.4 Probe (the only comparator run in this slice)

Source: Appendix A. 511 bytes, sha256
2d30c06b5d46fe1100a410e1df9d6f90fb696e7b829ece1d96fc63369640064c.
Compile: gcc -O2 -Wall -Wextra -o /tmp/b004_probe /tmp/b004_probe.c --
exit 0, no warnings. The probe mallocs 32 bytes, pattern-fills, prints
malloc_usable_size, frees, then reports from /proc/self/maps whether
libjemalloc and libc are mapped.

Baseline (no preload), one run, exit 0:

```text
usable=40
maps jemalloc=0 libc=1
probe success
```

LD_PRELOAD=/lib/aarch64-linux-gnu/libjemalloc.so.2, three fresh runs,
exit 0, identical output each time:

```text
usable=32
maps jemalloc=1 libc=1
probe success
```

Selection evidence (Roadmap Section 11 forms):

- LD_DEBUG=bindings run, exit 0, same stdout as above. Log lines include:
  "binding file /tmp/b004_probe [0] to /lib/aarch64-linux-gnu/libjemalloc.so.2 [0]: normal symbol `malloc' [GLIBC_2.17]"
  and the same for `free'. The log also binds libc.so.6's and
  libstdc++.so.6's malloc/free -- and libstdc++.so.6's operator
  new/delete -- to libjemalloc.so.2 (53 libjemalloc binding lines in the
  log). The probe's own bindings are the selection proof; the libc/libstdc++
  bindings are recorded as interposition breadth (Section 5.2 limitation).
- Behavioural delta: malloc_usable_size(malloc(32)) = 40 under the glibc
  baseline (matching B-002 Section 7.2's usable req=32 -> 40) and 32
  under jemalloc -- different size-class behaviour in the servicing
  allocator, corroborating that jemalloc, not glibc, serviced the probe's
  allocation.
- Statistics corroboration: MALLOC_CONF=stats_print:true with the same
  preload, exit 0, additionally emitted jemalloc's own statistics header
  on stderr, beginning "___ Begin jemalloc statistics ___" and
  "Version: \"5.3.0-0-g54eaed1d8b56b1aa528be3bdd1877e59c56fa90c\"".
  Corroboration only, never sole proof (B-001 Section 8.2).

Conclusion: Device-tested, **for this probe process only** -- the system
jemalloc loads, binds, and services a trivial standalone program on this
Pi under LD_PRELOAD. No timing was recorded and no performance claim is
made; this was not a benchmark-class run under Section 3.

### 10.5 NOT run

No mimalloc or tcmalloc in any mode (both absent). No jemalloc direct
link, static, or namespaced-API run. No Mojo. No Aesir. No multithreaded
run. No benchmark class 1-8 run. No RSS/PSS, latency, retention, or
fragmentation measurement. No tablet, Termux, proot, bionic, or musl run.
None of those results exist, and none are claimed.

## 11. Phase 2 exit table

Roadmap Section 8, Phase 2 exit, quoted verbatim: "table of options with
commands, measured results and integration limitations. No claims based on
names alone." This slice contributes the P2-R04 rows. Format follows
B-002 Section 8's ten fields, in order, with two fields restored from
B-001 Section 9 -- License and Linking constraint -- because the Roadmap
Phase 2 task requires recording them and B-002's format omits them (see
I-B004-4), plus a Failure-signature field, which neither schema carries
(see I-B004-5).

### Row P2-R04a -- mimalloc (comparator)

| Field | Value |
|---|---|
| Option / route | P2-R04a mimalloc (established allocator, diagnostic comparator) |
| Target + libc | Linux AArch64, glibc 2.36 (Pi, first target; other targets/libcs separate and unproven) |
| Consumer / layer | Standalone test program first; no Mojo or Aesir consumer proven |
| Modes | All four, distinguished (Section 6); viability per mode in Section 5.1 |
| License | TO-VERIFY from the pinned artifact at pin time |
| Linking constraint | TBD at pin (Section 5.1) |
| Exact command | DESIGN ONLY -- Section 5.1; not run |
| Selection proof | TBD -- not run; Section 11 proof required (Section 3 item 1) |
| Measured result | TBD -- not run |
| Failure signature | TBD -- no Phase 1 packet exists |
| Integration limitation | Pin TBD; not present on Pi (Section 10.2); diagnostics only; does not establish MimirAlloc properties |
| Evidence level | Proposed |
| Artifact / log | This document; no raw log exists |

### Row P2-R04b (system, untuned) -- jemalloc (comparator)

| Field | Value |
|---|---|
| Option / route | P2-R04b jemalloc, system package, untuned (established allocator, diagnostic comparator) |
| Target + libc | Linux AArch64, glibc 2.36 (Pi) |
| Consumer / layer | Probe only (/tmp/b004_probe); no Mojo or Aesir consumer named |
| Modes | LD_PRELOAD exercised (probe); direct link blocked as-is; static N/A; namespaced API not run (Section 5.2) |
| License | System artifact: BSD-2-Clause per /usr/share/doc/libjemalloc2/copyright, read on the Pi (Sections 5.2, 10.3); pinned upstream: TO-VERIFY |
| Linking constraint | Shared-only as installed; no linker name, headers, or dev package (Section 10.3) |
| Exact command | `gcc -O2 -Wall -Wextra -o /tmp/b004_probe /tmp/b004_probe.c && LD_PRELOAD=/lib/aarch64-linux-gnu/libjemalloc.so.2 /tmp/b004_probe` |
| Selection proof | LD_DEBUG=bindings: probe malloc/free bound to libjemalloc.so.2; /proc/self/maps jemalloc=1; usable-size delta 40 -> 32; statistics header (Section 10.4) |
| Measured result | 3/3 preload runs exit 0, identical output; baseline usable=40 vs preload usable=32 for malloc(32); no performance claim, no timing recorded |
| Failure signature | Not applicable -- no Phase 1 packet exists; nothing scored |
| Integration limitation | Probe process only; whole-process interposition incl. libc/libstdc++ internal callers; system version/config uncontrolled; proves nothing about Mojo/Aesir/performance |
| Evidence level | Device-tested (probe only) / Proposed (route) |
| Artifact / log | This document Section 10 + Appendix A; probe source/binary/logs in /tmp only, no tracked harness (I-B004-8) |

### Row P2-R04b (pinned / tuned) -- jemalloc (comparator)

| Field | Value |
|---|---|
| Option / route | P2-R04b jemalloc, pinned upstream build and/or tuned configuration (separate row per Section 4) |
| Target + libc | Linux AArch64, glibc 2.36 (Pi, first target) |
| Consumer / layer | Standalone test program first; no consumer proven |
| Modes | All four, distinguished, as the pinned artifact allows |
| License | TO-VERIFY from the pinned release at pin time |
| Linking constraint | TBD at pin |
| Exact command | DESIGN ONLY -- Section 5.2; not run |
| Selection proof | TBD -- not run |
| Measured result | TBD -- not run |
| Failure signature | TBD -- no Phase 1 packet exists |
| Integration limitation | Pin decision TBD (Section 5.2 question 3); diagnostics only |
| Evidence level | Proposed |
| Artifact / log | This document; no raw log exists |

### Row P2-R04c -- tcmalloc (comparator / implicated)

| Field | Value |
|---|---|
| Option / route | P2-R04c tcmalloc (established allocator, diagnostic comparator; implicated allocator) |
| Target + libc | Linux AArch64, glibc 2.36 (Pi, first target; other targets/libcs separate and unproven) |
| Consumer / layer | Standalone test program first; no Mojo or Aesir consumer proven |
| Modes | All four, distinguished (Section 6); LD_PRELOAD not-viable for bundled/static consumers |
| License | TO-VERIFY from the pinned artifact at pin time |
| Linking constraint | Address-width agreement across all translation units and the consumer (Section 5.3); rest TBD |
| Exact command | DESIGN ONLY -- Section 5.3; not run |
| Selection proof | TBD -- not run; Section 11 proof plus the Section 5.3 extra proof bar required |
| Measured result | TBD -- not run |
| Failure signature | TBD -- per run, record reproduction-vs-comparison goal first (Section 5.3) |
| Integration limitation | Pin TBD; not present on Pi (Section 10.2); implicated, not neutral; B-03 artifacts outside the repo (J-2) |
| Evidence level | Proposed |
| Artifact / log | This document; no raw log exists |

### Row -- study-only landscape (disposition, NOT comparators)

| Field | Value |
|---|---|
| Option / route | snmalloc, scalloc, TLSF, Scudo, StarMalloc -- study-only disposition (Baseline B-06) |
| Target + libc | N/A -- no target assigned |
| Consumer / layer | None |
| Modes | None |
| License | Not examined; no use proposed |
| Linking constraint | N/A |
| Exact command | None -- no run is designed or authorised by this slice |
| Selection proof | None |
| Measured result | TBD -- not run |
| Failure signature | N/A |
| Integration limitation | Not comparators in route P2-R04; promotion needs a future decision-ledger entry (Section 8) |
| Evidence level | Proposed (as study subjects only) |
| Artifact / log | This document; no raw log exists |

## 12. Verdict and resumption

**Verdict: Proposed for all three comparators as routes** -- mimalloc
(P2-R04a), jemalloc (P2-R04b), tcmalloc (P2-R04c). The comparators are
diagnostics only and imply **no project dependency**; none is adopted,
ranked, or recommended by this slice. The single Device-tested fact is the
Section 10.4 probe: the system jemalloc, proven selected, serviced a
trivial standalone program on this Pi under LD_PRELOAD. That fact scores
no route against the original failure, because no Phase 1 failure packet
exists.

**Resumption requires, together:** (1) a real Phase 1 failure packet --
repeatable failing command, first relevant error and exit code, exact
host/target, tcmalloc and Mojo revisions, causal hypothesis; (2) pinned
builds (or, for jemalloc, a recorded decision on system package vs pinned
upstream build) for the comparators to be run; and (3) the B-005 measured-
results protocol, under which Section 3 runs are executed and their raw
logs preserved.

A **route verdict** may be recorded only from filled exit-table rows, and
only in this vocabulary: **Unblocks / Partially unblocks / Does not
unblock / Inconclusive / Blocked** (B-001 Sections 7 and 9). Null and
negative results are recorded, never hidden. No verdict in that vocabulary
is available to this slice.

## 13. Inconsistencies recorded

- **I-B004-1 (INC-3 / J-3 comparator-set conflict, carried into this
  route):** Roadmap Section 11 names the comparison set as platform libc,
  pinned tcmalloc, mimalloc, and MimirAlloc -- it omits jemalloc, the
  allocator the public Mojo source actually selects for its Linux driver,
  and it includes MimirAlloc, which does not exist as a verified
  implementation until Phase 3, after Phase 2. B-004 follows B-001's
  operational resolution, without editing the roadmap: MimirAlloc is
  excluded from the Phase 2 comparator set, and jemalloc is included as
  P2-R04b with its set membership recorded explicitly (Section 5.2).
- **I-B004-2 (B-002's stale header statement about B-001):** B-002's
  header states that B-001 had not banked and that its fields were not
  used. That was true when B-002 was written, on base 43ae545; it is
  false on this slice's base, where B-001 IS banked as af893ee. B-004
  therefore uses B-001's fields as assigned. No reconciliation is owed
  beyond this note; B-002's statement is a record of its own base, not an
  error to be rewritten.
- **I-B004-3 (Roadmap exemplar vs Section 11 set):** Roadmap Phase 2
  names "established allocators such as mimalloc" -- a singular exemplar
  -- while Section 11 names a different set (see I-B004-1). Route
  membership for P2-R04 therefore rests on B-001's plan and Research 11,
  not on that one roadmap sentence. Reading the exemplar sentence as the
  whole set would silently drop both jemalloc and the implicated
  tcmalloc; this slice does not do that.
- **I-B004-4 (Two incompatible Phase 2 exit schemas):** B-001 Section 9
  is an 11-column table including License, Linking constraint, and
  Verdict; B-002 Section 8 is a 10-field vertical table with none of
  those, but with Target + libc, Consumer / layer, Selection proof, and
  Artifact / log, which B-001 Section 9 lacks as columns. Roadmap
  Section 8 exit verbatim requires only "commands, measured results and
  integration limitations", and its Phase 2 task additionally requires
  recording "licenses, linking constraints and outcomes". B-004 follows
  B-002's field format as assigned and restores License and Linking
  constraint as explicit fields (Section 11); the two banked schemas
  remain unreconciled upstream of this slice, and B-005's synthesis will
  have to pick or merge them.
- **I-B004-5 (No failure-signature field in either schema):** B-001
  Sections 8/8.3 require recording "failure signature changed? (Y/N +
  evidence)" for every comparator and, for tcmalloc, the
  reproduction-vs-comparison goal plus "failure signature reproduced
  (Y/N + packet link)". Neither B-001 Section 9 nor B-002 Section 8 has a
  field for any of these -- the route's central diagnostic verdict has no
  home in either exit table. B-004 adds a Failure-signature field to
  every Section 11 row.
- **I-B004-6 (One row per comparator vs the separate-rows rule):** B-001
  Section 4 rule 5 and Section 8.2 require tuned vs untuned and
  system-package vs pinned-build to be separate rows, yet B-001
  Section 9 allocates exactly one row each to P2-R04a/b/c. B-004 splits
  jemalloc into system-untuned (probe) and pinned/tuned rows
  (Section 11); its row count therefore exceeds B-001 Section 9's three
  rows, by B-001's own protocol rule.
- **I-B004-7 (Mode vocabularies do not map 1:1):** B-001 uses the four
  named modes (LD_PRELOAD / direct link / static replacement / namespaced
  API). INTEGRATION_LAYER_MAP.md and B-002 Section 5 use codes M1-M5,
  where direct link is "M2 + M3" and static replacement is "M3, static
  form" -- M3 appears in both. B-004 uses B-001's named modes as its join
  key (Section 6) and records the mapping there; a reader joining on M3
  alone will conflate two modes. Also recorded: B-001 Section 9's "All
  four" for mimalloc and tcmalloc must not be read as "all four viable":
  jemalloc static is N/A and tcmalloc LD_PRELOAD is not-viable for
  bundled/static consumers (Section 6 states viability per mode).
- **I-B004-8 (Probe artifact not tracked):** as with B-002 (its
  I-B002-7), this slice's probe source, binary, and logs live only in
  /tmp, and the Section 13 repository layout is unsettled (INC-5). The
  probe source is preserved verbatim in Appendix A so the run is
  reproducible from this repository; the raw bindings/statistics logs are
  quoted at the lines relied on (Section 10.4) but are not themselves
  tracked artifacts. A tracked harness location is still owed.

## Appendix A. Probe C source

Verbatim from /tmp/b004_probe.c on the Pi, sha256
2d30c06b5d46fe1100a410e1df9d6f90fb696e7b829ece1d96fc63369640064c,
511 bytes:

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
int main(void){
  void *p=malloc(32); if(!p) return 1; memset(p,0xAB,32);
  printf("usable=%zu\n", malloc_usable_size(p)); free(p);
  FILE *f=fopen("/proc/self/maps","r"); char line[512]; int je=0, lc=0;
  while(fgets(line,sizeof line,f)){ if(strstr(line,"libjemalloc")) je=1; if(strstr(line,"libc.so")) lc=1; }
  fclose(f); printf("maps jemalloc=%d libc=%d\n", je, lc);
  printf("probe success\n"); return 0; }
```

## Appendix B. Acceptance mapping

Assignment acceptance, mapped: exact questions for this route -- Sections
5.1-5.3 (per comparator) and Section 3 (common). Method/commands --
Section 3 protocol; per-comparator commands in Sections 5.1-5.3, marked
DESIGN ONLY / UNRUN except the Section 10.4 probe, which is quoted with
its outputs. Licenses and linking constraints per comparator -- Section
5.1 (TO-VERIFY), Section 5.2 (system artifact verified from its Debian
copyright file; pinned TO-VERIFY), Section 5.3 (TO-VERIFY); linking in
Sections 5.1-5.3 and 10.3. Four integration modes assessed for this route
specifically -- per comparator in Sections 5.1-5.3, summarised in
Section 6. Measured-result fields -- Section 3 (common) and Sections
5.1-5.3 (per comparator). Integration-limitation fields -- Section 3
(common) and Sections 5.1-5.3. Phase 2 exit-table rows -- Section 11.
Diagnostics-only / no-dependency statement -- Sections 1, 7, 12.
Presence re-verified live before repeating B-001's probe notes --
Section 10.2. Roadmap inconsistencies recorded in-doc -- Section 13.
README.md untouched -- no change to README.md is part of this slice.

## Appendix C. Change log

| Date | Change |
|---|---|
| 2026-11-06 | B-004 initial evaluation: route P2-R04 (mimalloc, jemalloc, tcmalloc as diagnostic comparators only), B-001 fields with B-002 format, jemalloc system-artifact license verified from its on-Pi Debian copyright file, one bounded jemalloc LD_PRELOAD probe recorded in Section 10, exit rows in Section 11, inconsistencies I-B004-1..I-B004-8. |
