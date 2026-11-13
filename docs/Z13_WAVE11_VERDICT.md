# Z13 Wave 11 Verdict

Slice: Z-099 - Wave 11 verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-12 by team-01. The six Mythic Engineering roles were run
SEQUENTIALLY by the lead itself (evidence audit, batching review,
trace review, tuning/flags review, process-economics review,
gate/commit-scope review) - no sub-agents were spawned this run, and
that is recorded plainly here and in CAMPAIGN_STATE.md.
Base: merge 7a773de of seat main 3894db0 (banked Z-001..Z-097) into
yrsa/mimiralloc-team-01 - clean merge, no conflicts; the merge added
exactly the banked Z-094, Z-095 and Z-096 artifacts (24 files) on top
of this branch's banked Z-097 commit b196ddb. Baseline standing gate
on the merged tree before any Z-099 edit: GREEN 4988 checks / 0 failed
/ 77 by-design skips (hidden_files/z099_baseline_gate.log).

STATUS: FINAL for Z-091..Z-097; OWED for Z-098, stated up front.
Wave 11 is 8 of 9 slices FINAL; one section remains OWED.
FINAL below: Z-091 (banked at the seat as 942369e), Z-092 (4188da8),
Z-093 (2b904be, fold-in 3e54613), Z-094 (203a469), Z-095 (11d41cc),
Z-096 (cfbca07) and Z-097 (3894db0) - all banked at the seat and
re-read first-hand from disk on the merged seat tree by this lead.
OWED: Z-098, the fair-fight leg (team-02, in flight, not banked;
verified at build time: seat main is 3894db0 with no Z-098 commit,
team-02 has merged the seat and holds no Z-098 artifact). Its
section (3.8) names what is missing and invents no verdict and no
number; a fold-in amendment completes it when the slice banks,
exactly as Z-090 handled Z-087/Z-088. No number anywhere in this
verdict rests on an unbanked source.
The headline the FINAL evidence supports: batching's second trial
is closed - batching LOSES on this machine, for one named reason
(the live-set metadata walk), and the batch API prototype TIES the
per-op loop it wraps and therefore LOSES as a batching answer.
The real traces are canonical and replay-proven on both allocator
legs; trace-driven tuning adopted NOTHING, because every candidate
that helped the traces broke the synthetic oracle. The wave's one
adoption is a build flag: -O3 replaces the shipped -O2 -flto set.
And preload's per-process tax is now priced: even a process that
never allocates pays +17% for the interposition library, and a
working short-lived process pays 2.30x.

## 1. The plain answer - Wave 11 per workload

BATCHING (final standing, section 4.1):
- Caller-side batching (grouping calls over the per-op API) LOSES
  at depth: 11.94x (64 B) and 84.66x (1024 B) against its own
  interleaved shape at burst512, and 33.8x / 120.2x against glibc
  on the identical shapes (Z-093). A mixed-class batch of 512
  loses too: 1137 ns/pair against glibc's 22 (51.7x).
- The Z-092 batch API prototype TIES the per-op loop at burst512
  and therefore LOSES as a batching answer: it still pays 10.9x /
  84.4x against its own interleaved shape, because it removes no
  walk (Z-093 fold-in, S7). Shallow batches (burst8) were already
  free for the per-op loop and stay free; the prototype's only
  real delta there (-23% / -21%) is the minor API-crossing term.
- Proposal P-1 (span retention across burst frees) is CLOSED,
  reaffirmed by two slices: its target prices at most ~11% of the
  loss on this machine (Z-091, reaffirmed Z-093). The only road
  to a batching WIN left standing is the Z-091 brief: span-grouped
  processing with one registry pass per span. No banked slice
  implements it; nothing here pre-judges it.

TRACES (final standing, section 4.2):
- The three canonical traces (python import storm, git object
  churn, Ollama session) replay MATCH on BOTH legs - glibc and
  MimirAlloc - with checksums, peaks and final residuals equal to
  the trace headers: 2,987,619 replayed ops, 0 failures (Z-094).
  As correctness artefacts the traces are a TIE: both allocators
  service the real request streams exactly.
- Trace-driven tuning adopted NOTHING (Z-095): the larger TLS
  cache (bin 32 / batch 16) won on two traces and on two oracle
  workloads but regressed small_churn +6.13% on the oracle, past
  the pre-registered 2% bar; the smaller cache and the respaced
  class table lost outright. The banked 20-class table and the
  Z-028 cache constants (bin 16 / batch 8) stand, by measurement.
- The trace replay gap to glibc stands as context, not as a
  tuning grade: on Z-095's own captured streams MimirAlloc's
  replay medians are 343.63 / 558.04 / 2907.94 ns/op (git /
  ollama / python) against glibc's 31.58 / 16.66 / 12.37 - the
  same small-path LOSS the earlier waves measured, restated in
  section 4.2 so no reader mistakes "no tuning adopted" for
  "no gap".

BUILD FLAGS (final standing, section 4.3):
- ADOPTED: -O3 alone. The shipped set changes from -O2 -flto to
  -O3 in the Makefile (Z-096). Against -O2, -O3 is WIN on both
  small-path workloads (small_churn +12.37%, mixed_batch +6.85%
  in the run of record, replicated +12.45% / +8.55%) and TIE on
  large_batch and mixed_lifecycle; against the old shipped set
  it is WIN on both small-path workloads (+11.07% / +8.11%).
- NOT adopted: -march=native (mixed_batch LOSS -6.72%) and -flto
  (its Z-035 small-path wins are gone on the near-final core;
  the isolated LTO leg is TIE on every workload, so -flto is
  dropped, not merely not added).

SHORT-LIVED PROCESSES (final standing, section 4.4):
- The interposition tax is a LOSS on every shape (Z-097): +16.98%
  (1.17x) for a process that never allocates, +26.72% (1.27x) for
  one allocation, +71.08% (1.71x) for eight ladder allocations,
  +130.25% (2.30x) for a 1000-cycle working process. Projected:
  +203 ms per 1,000 churn-shaped processes, +2.03 s per 10,000 -
  the concrete price on shell scripts and build farms.

FAIR FIGHT (OWED, section 3.8):
- Z-098 (glibc under its own best tunables) is not banked. Every
  FINAL comparison in this verdict is against DEFAULT glibc.
  Whether tuning glibc narrows any gap above is an open question
  of record, not a gap this verdict papers over; the tuned-glibc
  column the Z-107 closing table needs does not exist yet.

## 2. Scope and honesty - read first

- Wave 11 (Z-091..Z-099) is batching's second trial plus the
  first real-trace line: a post-mortem that priced why batching
  lost (Z-091), a contract-first batch API prototype (Z-092), the
  verdict run that graded both (Z-093), canonical traces from
  real programs (Z-094), tuning measured against traces (Z-095),
  the final flag matrix (Z-096), the short-lived process tax
  (Z-097), the fair-fight leg (Z-098, OWED), and this verdict.
- The roadmap's Z-099 text asks for: batching's final standing
  and the trace standings, stated per workload with reasons;
  gate: verdict doc traceable to recorded runs. Section 1 is the
  plain statement, section 4 is the per-workload standings with
  reasons, and section 8 is the traceability index. The gate is
  met for every FINAL section; the OWED section names itself.
- Every FINAL number below traces to a banked recorded run,
  re-read first-hand from disk on the merged seat tree by this
  lead. Grades use the roadmap's own WIN / TIE / LOSS vocabulary,
  this machine only. Load stamps travel with their numbers:
  where a source run was contended, the standing says so and no
  baseline is claimed from it.
- Z-093 and Z-095 are cited TOGETHER wherever the trace-tuning
  OWED is handled (section 5), exactly as Z-095's own record
  directs ("Wave 11's verdict slice should cite both when they
  bank"). Z-093's batch verdict does not block Z-095 (no shared
  surface), and Z-095 does not soften Z-093: batching lost on
  synthetic batch shapes and the prototype removed no walk,
  independently of anything the traces say.
- This slice derives no new measurement. Its only fresh
  executions are the baseline and final standing gates.

## 3. What was built and what the runs found (FINAL unless named)

### 3.1 Z-091 - Why batching lost: the K-007 post-mortem (banked 942369e)

The unmodified K-007 harness was re-run on this machine (two
invocations, checksums identical per size; loadavg 1.15-1.55 with
an Aesir GPU soak co-tenant - stated load, agreement-guarded, not
a quiet baseline). Batching loses WORSE here than on the Pi:
burst512 vs interleaved is 12.38x at 64 B (32 -> 396 ns/pair) and
92.65x at 1024 B (40 -> 3706), against glibc's ~2.8x / ~3.0x on
the same shapes. burst8 is free (1.00x at both sizes); the loss
switches on between batch 8 and 32. The Pi's FIFO penalty does
NOT reproduce (+3.5% / +2.3%, inside agreement): free order is
not a priced factor here.
Every candidate factor was then priced as a ceiling from banked
Wave 2-3 runs. Lock amortisation: at most ~50 ns/pair - at most
14% of the 64 B burst512 penalty, 1.4% at 1024 B. Page faults
and span release (proposal P-1's target): at most ~3.1% / ~5.3%
as faults, ~11% at 1024 B with the full release/recreate syscall
round trip; and Z-014's recycling ratio (0.979-0.999) says page
residency itself buys almost nothing on this host. The residual,
and the only factor that scales like the loss, is F-4: per-op
cost is a function of live-set size because every alloc and free
pays linear registry/span walks over the live set (Z-024
reverse-order controls price the walk at ~82-92% of free cost at
deep live sets). DOMINANT FACTOR, named per the gate: the
live-set metadata walk. Batching maximises live-set size;
interleaving minimises it. P-1 CLOSED as mis-aimed for this
machine (its general form - retention bound, mapping pool,
trim law - is already banked). The Z-092 brief was redirected:
attack the walk, or lose again for this reason.

### 3.2 Z-092 - Batch API prototype (banked 4188da8)

Bulk same-class alloc/free over the Wave-4 cache, contract tests
first. Gate MET on both clauses: the contract suite is green
(62 checks / 0 failed, RESULT: PASS, recorded run
tests/contract/recorded_run_z092.txt) and the prototype is NOT a
public symbol (nm proof: exactly the 13 public exports, zero
mimir_batch_* names in the shared library). The contract is
all-or-nothing (cap 4096, D-013 zero-size refusal per element,
reverse-order rollback on any element failure; free in LIFO
order, NULL-skipping, counted).
Honest scope, in the slice's own words (its doc S2): the shipped
prototype does NOT remove the F-4 walk. It loops the banked
per-op core entries, adds no allocator state, no span grouping
and no shared registry pass; for cache-served classes the cost
is the TLS cache's, for misses and above-cutoff sizes every
element pays the full central cost Z-091 priced. It was
therefore EXPECTED to lose or tie in Z-093 except where the
cache absorbs the burst - which is exactly what Z-093 found.

### 3.3 Z-093 - Batch verdict run (banked 2b904be, fold-in 3e54613)

Prototype vs per-op loop vs glibc on batch-shaped workloads
(K-007 geometry unmodified plus a mixed-class shape; two
invocations, 7 reps, checksums identical across legs, shapes
and invocations; loadavg 1.08-1.22, shared host, agreement
within 3.7% on every mimir cell). Per-op loop, invocation 1
medians: burst512 is 406 ns/pair at 64 B (11.94x its own
inter1) and 3725 at 1024 B (84.66x); against glibc on the
identical burst512 shapes that is 33.8x / 120.2x. mixbatch512:
mimir 1137 vs glibc 22 ns/pair (51.7x). burst8 again free
(0.97x / 0.89x); FIFO vs LIFO inside noise again. GRADE:
batching LOSS again, reason named = the live-set metadata walk
(Z-091 F-4). P-1 CLOSED, reaffirmed: this run's fault counts
match Z-091's, so the pricing that closed P-1 stands.
The prototype leg was OWED at the original run (Z-092 not yet
in that tree - probed, not assumed: 0 batch hits in the public
header, 0 batch exports) and is DISCHARGED by the fold-in run
on the merged tree (quiet window, loadavg 0.28-0.48; prototype
cells agreed within 2.9% across invocations): burst512 proto
383 vs filed per-op 406 ns/pair at 64 B (-5.7%) and 3544 vs
3725 at 1024 B (-4.9%), with same-run per-op at 388 / 3548 -
inside noise on both invocations: TIE against the per-op loop.
inter1 TIE; mixbatch512 TIE (1021 vs same-run 1016); burst8
-23% / -21% is the minor API-crossing term in the walk-free
zone only. As a batching answer the prototype is a LOSS: it
pays 10.9x / 84.4x against its own interleaved shape and
~32x / ~114x against glibc's filed burst512, because it removes
no walk (Z-092 doc S2). Minor faults identical to the per-op
leg (2693 / 11528 at burst512): the P-1 pricing is unmoved.
No OWED item remains for Z-093.

### 3.4 Z-094 - Real workload traces (banked 203a469)

Three canonical traces captured on this host via the audit-hook
route (an LD_PRELOAD shim over plain glibc; MimirAlloc is not
in the capture path; the trace records what real programs ask
for, leg-independently; boundary named: exactly eight
PLT-boundary entry points, strdup-class and libc-internal
allocations outside by construction). Gate MET: make test-z094
replays each trace on BOTH legs with checksum, op count, peak
and final live residual matching the headers exactly -
2,987,619 replayed ops, 0 failures, 0 dirty callocs.
- python_import_storm: 28,694 ops, 1 process, peak live
  8,645,075 B, FNV-1a64 478338dba68de321. MATCH / MATCH.
- git_object_churn: 576,623 ops, 622 processes, peak live
  508,627,126 B, FNV-1a64 2ff165b183a9a269. MATCH / MATCH
  (mimir leg counts 303 D-013 zero-size normalisations).
- ollama_session: 2,382,302 ops, 11 processes, peak live
  594,768,279 B, FNV-1a64 8c99dd8de5e6658d. MATCH / MATCH
  (318 D-013 normalisations). The session is proven real in
  the capture log (a 12-token generation, done_reason length),
  not assumed.
Multi-process peak/final figures are the sequential-
concatenation sums over process segments - an upper accounting
the replay reproduces exactly, never a claim about one instant
of the original run (slice doc S2). No timing or benchmark
number is claimed anywhere in this slice: the traces are
correctness artefacts, and that restraint is what makes the
replay MATCH a clean TIE. Four defects were found in the
slice's own new tooling and fixed in scope (fork inheritance
poisoning multi-process streams; torn final records in killed
processes; the private Ollama serve missing the model store;
a smoke consumer overwriting a live pointer with a failing
realloc's NULL). None was in the banked core.

### 3.5 Z-095 - Trace-driven tuning (banked 11d41cc)

Class table and cache sizes re-tuned against traces, before/
after measured on traces AND the synthetic oracle, under a
pre-registered adoption bar (trace median improves >= 3% on at
least 2 of 3 traces, no trace regresses > 2%, and no oracle
workload regresses > 2%). Dependency posture, stated in the
slice's own record: Z-094 was not banked at build time, so the
slice captured its OWN three real traces with the same workload
shapes (python 261,664 replayed ops; git 70,957 of 73,801
records; ollama first 300,000 of 2,064,384) and recorded
re-validation against Z-094's canonical traces as OWED -
carried, not discharged, in section 5 of this verdict.
Quiet window throughout (loadavg 0.22-0.83). Results:
- v1, TLS bin 32 / batch 16: trace WIN on ollama (-13.87%) and
  python (-5.37%), TIE on git (-1.34%); oracle WIN on
  mixed_lifecycle (-45.29%) and mixed_batch (-4.21%), TIE on
  large_batch - but small_churn REGRESSES +6.13%, past the bar.
  NOT ADOPTED; the regression is reported, not hidden. v1 is
  the recorded near-miss, and its evidence packet is the case
  for a per-class bin sizing slice (section 5).
- v2, TLS bin 8 / batch 4: LOSS everywhere it matters (traces
  +8.87% / +26.66% / +5.64%; oracle mixed_batch +1582.09%).
  REJECTED.
- v3, class respace (exact 56/88 classes for Ollama's hottest
  sizes): trace TIE-to-WIN (ollama -6.61%, python -0.63%, git
  +1.16%) but oracle mixed_batch REGRESSES +649.56% and
  mixed_lifecycle +10.05%. REJECTED, regression in full.
- ADOPTED TUNING: NONE. The banked 20-class table and the
  Z-028 TLS constants stand unchanged - not by inertia but by
  measurement: every candidate that improved the traces broke
  the oracle, and the gate's rule is that a regression in
  either suite blocks adoption.
Context the slice itself files: on its traces the MimirAlloc
replay medians (v0) are git 343.63, ollama 558.04, python
2907.94 ns/op against glibc's 31.58 / 16.66 / 12.37 - the
replay LOSS vs glibc stands; this slice graded tuning deltas,
not the leg gap. Two harness defects were found and fixed in
scope before the run of record; no allocator defect was found.

### 3.6 Z-096 - Final build-flag matrix (banked cfbca07)

The Z-035 flag study repeated on the near-final core (full
2x2x2 factorial over -O2/-O3 x -march=native x -flto, 9 reps
interleaved, bench TU -O2 in every leg; two clean runs agree,
a third contended run corroborates adoption only; quiet window,
loadavg 0.14-0.63 in the runs of record). Adoption rule,
registered before grading: an isolated flag is adopted only
if WIN on BOTH small-path workloads, drift inside bounds,
checksums identical, and no other workload LOSS.
- -O3: small_churn 33.72 -> 29.55 ns/op (+12.37% WIN) and
  mixed_batch 30.79 -> 28.68 (+6.85% WIN) vs -O2 in run A2;
  replicated +12.45% / +8.55% in run A3; large_batch and
  mixed_lifecycle TIE. ADOPT.
- -march=native: mixed_batch -6.72% LOSS in A2 (-4.82% in A3).
  DO NOT ADOPT.
- -flto in isolation: TIE on every workload in both clean
  runs. DO NOT ADOPT - its Z-035 small-path wins are gone on
  this core.
FINAL ADOPTED SET: -O3. The Makefile shipped set changes from
-O2 -flto to -O3 (CFLAGS and the .so link lines lose -flto
with it). Against the old shipped set, -O3 is WIN on both
small-path workloads (+11.07% small_churn, +8.11% mixed_batch,
run A2). No source or ABI change. This is Wave 11's only
adoption, and it is a flag, not an allocator change - the
standings in section 4.3 say exactly that and no more.

### 3.7 Z-097 - Short-lived process economics (banked 3894db0)

Interposition .so load and first-allocation cost across a
fork+exec storm (the shell-script / build-farm shape: children
spawned sequentially, each waited; 200 children per storm, 100
for churn, 7 storms per world/shape, two full invocations,
every cell agreed within 0.9%; QUIET WINDOW, loadavg 0.38 flat;
all 19,600 measured children exited 0 on checksum-verified
work). Per-process tax vs glibc, invocation 1 medians:
- none (never allocates): 148,168 vs 173,322 ns = +25,154
  (+16.98%, 1.17x) - the pure load/init tax. LOSS.
- first (one allocation): 155,202 vs 196,676 = +41,474
  (+26.72%, 1.27x). The first-touch increment over the no-alloc
  leg is 23,354 ns under MimirAlloc vs 7,034 under glibc: a
  first-allocation tax of +16,320 beyond the load tax. LOSS.
- multi (eight allocations across the class ladder, top rung
  on the mapping path): 158,689 vs 271,493 = +112,804
  (+71.08%, 1.71x). LOSS.
- churn (1000 small alloc/touch/free cycles): 155,831 vs
  358,797 = +202,966 (+130.25%, 2.30x). LOSS. Projection:
  +203 ms per 1,000 processes, +2.03 s per 10,000.
Reason named (Law 2): two stacked terms - (1) a fixed
per-process load/init term (+25.2 us on the none leg; preload
minor faults 84.4 vs glibc 73.0 per process: the extra .so
mapping, relocation and allocator initialisation every exec
pays whether or not it allocates), and (2) the per-allocation
small-path cost documented since Z-003/Z-024 (the same
registry/span walks as F-4), isolated by the increments:
+16,320 for the first touch, +87,650 for the eight ladder
allocations, +177,812 for the 1000 churn pairs, with preload
minor faults rising 84 (none) to 234 (churn) vs a flat 73-79
under glibc. The tax grows with exactly the work a short-lived
process does; it is not a load-time-only curiosity. No defect
was found in repo code by this slice.

### 3.8 Z-098 - The fair-fight leg - OWED (not banked)

OWED, in the Z-090 pattern. Z-098 (team-02) measures glibc
under its own tunables (tcache size/count, mmap threshold) at
their best for each workload, so the final tables face TUNED
glibc, not only default glibc; its gate includes the
tuned-glibc column existing in the Z-107 closing table's raw
data. At this verdict's build time the slice is IN FLIGHT and
NOT banked: seat main is 3894db0 (Z-001..Z-097), team-02 has
merged the seat and holds no Z-098 artifact, and no Z-098
number exists anywhere this lead may cite. This verdict
therefore claims NOTHING about tuned glibc: every FINAL
comparison above is against default glibc, and the fair-fight
standing - whether tuning narrows, erases, or leaves any gap
in sections 4.1-4.4 - is OWED in full. The fold-in protocol is
section 6. Inventing a tuned-glibc figure here, or quietly
letting default-glibc comparisons pose as the fair fight,
would break the traceability gate this slice is graded on.

## 4. Standings per workload, with reasons (the Z-099 gate)

### 4.1 Batching standings (Z-091 + Z-092 + Z-093, FINAL)

| Workload | Grade | Numbers of record | Reason |
|---|---|---|---|
| Caller batch 64 B, burst512 vs own inter1 | LOSS | 406 vs 34 ns/pair = 11.94x (Z-093); re-run shape 396 vs 32 = 12.38x (Z-091) | Live-set walk (F-4) |
| Caller batch 1024 B, burst512 vs own inter1 | LOSS | 3725 vs 44 = 84.66x (Z-093); 3706 vs 40 = 92.65x (Z-091) | Live-set walk (F-4) |
| Caller batch burst512 vs glibc, same shape | LOSS | 33.8x (64 B) / 120.2x (1024 B) (Z-093) | glibc tcache free is O(1) in live-set size; mimir per-op cost walks the live set |
| Mixed-class batch512 vs glibc | LOSS | 1137 vs 22 ns/pair = 51.7x (Z-093) | A mixed batch still manufactures a 512-deep live set |
| Shallow batch, burst8 | TIE (free) | 0.97x / 0.89x per-op (Z-093); 1.00x both sizes (Z-091) | Live set still fits the hot metadata neighbourhood |
| Prototype burst512 vs per-op loop | TIE | 383 vs 406 filed, 388 same-run (64 B); 3544 vs 3725 filed, 3548 same-run (1024 B) (Z-093 S7) | Same walk, same price; inside noise |
| Prototype as a batching answer | LOSS | 10.9x / 84.4x vs own inter1; ~32x / ~114x vs glibc burst512 (Z-093 S7) | The prototype removes no walk (Z-092 S2) |
| P-1 span retention proposal | CLOSED | Target at most ~11% of the loss at 1024 B (Z-091, reaffirmed Z-093) | Mis-aimed for this machine; general form already banked |

### 4.2 Trace standings (Z-094 + Z-095, FINAL)

| Workload | Standing | Numbers of record | Reason |
|---|---|---|---|
| python import storm - canonical replay | TIE (correctness) | 28,694 ops, MATCH on glibc and mimir legs, 0 failures (Z-094) | Both legs service the real stream exactly; checksums/peaks/residuals equal |
| git object churn - canonical replay | TIE (correctness) | 576,623 ops / 622 procs, MATCH / MATCH (Z-094) | Same; 303 D-013 normalisations counted on the mimir leg, never hidden |
| Ollama session - canonical replay | TIE (correctness) | 2,382,302 ops / 11 procs, MATCH / MATCH (Z-094) | Same; 318 D-013 normalisations counted |
| Trace replay speed, mimir vs glibc | LOSS (context) | git 343.63 vs 31.58, ollama 558.04 vs 16.66, python 2907.94 vs 12.37 ns/op (Z-095 v0) | The standing small-path gap; Z-095 graded tuning deltas, not this gap |
| Tuning v1 (bin 32 / batch 16) | WIN cells, NOT ADOPTED | Traces ollama -13.87%, python -5.37%, git TIE; oracle small_churn +6.13% (Z-095) | The oracle regression blocks adoption under the pre-registered bar |
| Tuning v2 (bin 8 / batch 4) | LOSS, REJECTED | Traces +8.87% / +26.66% / +5.64%; oracle mixed_batch +1582.09% (Z-095) | Shallow bins starve the mixed cycle onto the locked core path |
| Tuning v3 (class respace) | LOSS on oracle, REJECTED | Traces ollama -6.61%, python -0.63%, git +1.16%; oracle mixed_batch +649.56% (Z-095) | Dropped mid classes punish the synthetic cycle more than exact-size wins repay |
| Adopted tuning | NONE | Banked 20-class table and bin 16 / batch 8 stand (Z-095) | Every improver broke the other suite; regressions block adoption |

### 4.3 Build-flag standings (Z-096, FINAL)

| Flag (isolated, vs -O2) | Standing | Numbers of record |
|---|---|---|
| -O3 | ADOPTED (WIN) | small_churn +12.37%, mixed_batch +6.85% (A2), replicated +12.45% / +8.55% (A3); large_batch, mixed_lifecycle TIE |
| -march=native | NOT ADOPTED (LOSS cell) | mixed_batch -6.72% (A2), -4.82% (A3); other workloads TIE |
| -flto | NOT RE-ADOPTED (TIE) | TIE on every workload in both clean runs; Z-035 wins gone on this core |
| Shipped set | -O2 -flto -> -O3 | -O3 vs shipped: small_churn +11.07% WIN, mixed_batch +8.11% WIN (A2) |

### 4.4 Short-lived process standings (Z-097, FINAL)

| Shape | Grade | Tax of record |
|---|---|---|
| none (never allocates) | LOSS | +25,154 ns/proc, +16.98%, 1.17x |
| first (one allocation) | LOSS | +41,474 ns/proc, +26.72%, 1.27x; first-touch term +16,320 beyond load |
| multi (eight ladder allocations) | LOSS | +112,804 ns/proc, +71.08%, 1.71x |
| churn (1000 small cycles) | LOSS | +202,966 ns/proc, +130.25%, 2.30x; +203 ms / 1,000 procs, +2.03 s / 10,000 |

### 4.5 Fair-fight standings (Z-098)

OWED - no standing exists. See sections 3.8 and 6.

## 5. Carried items - named, with owners, not hidden

CARRIED OWED 1 - Z-095's canonical re-validation (owner: a
follow-up measurement slice; NOT discharged by this verdict).
Z-095 tuned against its own three captured traces because
Z-094 was unbanked at its build time, and its record files the
re-validation as OWED: re-run run_z095.sh measure with Z-094's
canonical traces and confirm the no-adoption verdict and v1's
near-miss under the canonical streams. Z-094 has since banked
(203a469), so the input now exists - but no banked slice has
performed that re-run, and this verdict slice derives no new
measurement by design. The OWED is therefore CARRIED
explicitly, not discharged: until the re-run happens, Z-095's
no-adoption verdict rests on its own three traces (named with
their provenance in its recorded run), and any future claim
that the verdict was confirmed on canonical traces without
the re-run would be invention. Discharge requires exactly the
re-run Z-095 names, graded under its pre-registered bar.
Per Z-095's direction this handling cites Z-093 and Z-095
together: the batch verdict (Z-093) neither blocks nor
confirms the tuning verdict, and the tuning near-miss (v1)
neither blocks nor softens the batch LOSS - they share no
surface, and both standings in section 4 are FINAL on their
own evidence while this one re-validation stays owed.

CARRIED PROPOSAL 2 - per-class TLS bin sizing (owner: a future
tuning slice, proposed by Z-095, endorsed here as the honest
next step and nothing more). v1's trace wins with its
small_churn oracle regression named is the evidence packet:
a uniform bin of 32 pays on real streams and costs the
small-class churn leg, so sizing bins per class is the design
that could keep both. No number for it exists; this verdict
files the proposal, it does not grade it.

CLOSED in this wave, for the record: P-1 (Z-091, reaffirmed
Z-093); the Z-093 prototype-leg OWED (discharged by its
fold-in, S7); the Z-092-to-Z-093 grading OWED (discharged by
Z-093). The only OWED items leaving Wave 11 are Z-098
(section 3.8) and Carried OWED 1 above.

## 6. Fold-in protocol for Z-098 (when it banks)

In the Z-090 pattern (its Z-087/Z-088 fold-ins are the
precedent): when Z-098 banks at the seat, a fold-in commit on
this branch ("Z-099: Wave 11 verdict (fold-in)") will -
(1) merge the seat main that carries Z-098 into this branch;
(2) re-read the banked Z-098 doc and recorded run first-hand;
(3) rewrite section 3.8 as FINAL with the tunables matrix and
the tuned-glibc standings, add the fair-fight rows to
section 4 (a new 4.5 replacing the OWED note), and restate
any section 1 / section 9 line the tuned numbers change -
including, if tuned glibc narrows a gap, saying so at the
same prominence as the original gap;
(4) run the standing gate on the fold-in tree (expected GREEN
4988/0/77) and record it in the fold-in recorded-run
amendment.
Until that commit exists, this verdict stands as FINAL for
Z-091..Z-097 and OWED for Z-098, and no citation of it may
drop the OWED qualifier for the fair-fight leg.

## 7. What this verdict does NOT prove

- Anything about TUNED glibc (Z-098 OWED, sections 3.8/6).
- That trace tuning could never win: Z-095 proves only that
  its three candidates, under its pre-registered bar, on its
  own traces, did not earn adoption; the canonical
  re-validation is still owed (section 5) and the per-class
  bin proposal is unmeasured.
- That batching could never win: the Z-091 brief (span-grouped,
  one registry pass per span) is unimplemented. What is proven
  is that caller-side grouping and a per-op-looping prototype
  lose, for the named reason, on this machine.
- Quiet-machine baselines for the contended source runs:
  Z-091 (loadavg 1.15-1.55) and Z-093's original run (1.08-1.22)
  are agreement-guarded shared-host measurements, graded as
  such; their margins (12x-120x) are far outside contention
  noise, and the fold-in re-measured the prototype leg in a
  quiet window - but no cell here is re-labelled a baseline.
- Performance beyond the filed workloads: the trace standings
  cover the three canonical workloads and Z-095's three
  streams; the process tax covers four shapes of short-lived
  process; nothing here grades long-running or multi-hour
  behaviour (Wave 12's soak line owns that).

## 8. Evidence index (traceability - the Z-099 gate)

FINAL sources, re-read first-hand on the merged tree:
- Z-091 seat 942369e: docs/Z13_Z091_BATCHING_POSTMORTEM.md;
  tests/perf/recorded_run_z13_z091.txt (re-run 12.38x / 92.65x
  at burst512; F-4 dominant; P-1 CLOSED; loadavg 1.15-1.55).
- Z-092 seat 4188da8: docs/Z13_Z092_BATCH_API_PROTOTYPE.md;
  tests/contract/recorded_run_z092.txt (contract 62/0 PASS;
  13 exports, zero batch symbols; S2: prototype removes no
  walk).
- Z-093 seat 2b904be + fold-in 3e54613:
  docs/Z13_Z093_BATCH_VERDICT.md;
  tests/perf/recorded_run_z13_z093.txt (per-op LOSS 11.94x /
  84.66x, vs glibc 33.8x / 120.2x, mix 51.7x; fold-in S7:
  prototype TIE vs per-op, LOSS as a batching answer).
- Z-094 seat 203a469: docs/Z13_Z094_REAL_WORKLOAD_TRACES.md;
  tests/perf/recorded_run_z13_z094.txt (three traces in-tree,
  replay MATCH on both legs, 2,987,619 ops, 0 failures;
  capture loadavg 0.91-1.29, no timings claimed).
- Z-095 seat 11d41cc: tests/perf/recorded_run_z13_z095.txt
  (no adoption; v1 near-miss small_churn +6.13%; v2/v3
  rejected; OWED canonical re-validation; quiet window).
- Z-096 seat cfbca07: tests/perf/recorded_run_z13_z096.txt
  (run A2 of record + A3 confirmation: -O3 ADOPTED;
  -march=native and -flto not adopted) and the Makefile
  change it banked (shipped set -O2 -flto -> -O3).
- Z-097 seat 3894db0: tests/perf/recorded_run_z13_z097.txt
  (tax table LOSS on all four shapes; 19,600 children, all
  exit 0; quiet window loadavg 0.38; two-invocation agreement
  within 0.9%).
OWED source:
- Z-098: no banked artifact exists at build time (seat main
  3894db0, verified by git log on the seat and by team-02's
  state). Nothing is cited from it.
This slice's own fresh executions: the baseline gate on the
merged tree and the standing gate on the final tree, both
GREEN 4988/0/77 (recorded run,
tests/perf/recorded_run_z13_z099.txt).
No new measurement is derived by this slice: every number is
a synthesis of the sources above, in the Z-090 verdict pattern.

## 9. Verdict - the Wave 11 standings of record

- Batching's final standing: LOSS, FINAL. Caller-side batching
  pays 11.94x / 84.66x against its own interleaved shape at
  burst512 and 33.8x / 120.2x against glibc (Z-093, re-proving
  Z-091); the reason is named and priced - the live-set
  metadata walk (F-4), with lock amortisation (at most ~50
  ns/pair) and faults/span release (at most ~11%) as minor
  terms. P-1 is CLOSED.
- The batch API prototype's standing: contract-green and
  honestly scoped (Z-092); TIE against the per-op loop and
  LOSS as a batching answer (Z-093 fold-in). A future batching
  design must show walk removal to win (the Z-091 brief);
  none is graded here.
- The trace standings: canonical traces replay MATCH on both
  legs - correctness TIE (Z-094); trace-driven tuning adopted
  NONE, with v1 as a recorded near-miss and v2/v3 rejected on
  named regressions (Z-095); the replay speed gap to glibc
  stands as a LOSS of record. Z-095's canonical re-validation
  is CARRIED as an OWED (section 5), cited with Z-093 as its
  record directs.
- Build flags: -O3 ADOPTED as the shipped set (from -O2
  -flto); -march=native and -flto not adopted, each on named
  cells (Z-096). FINAL.
- Short-lived process economics: the interposition tax is a
  LOSS on every shape, +16.98% (never allocates) to +130.25%
  (working process), reasons decomposed into the load/init
  term and the per-allocation walk term (Z-097). FINAL.
- The fair fight: OWED (Z-098 in flight, not banked). Wave 11
  therefore stands 8 of 9 slices FINAL at this writing,
  counting this verdict; the fold-in of section 6 completes
  it when Z-098 banks.

Bugs found/fixed by this slice: none in the allocator, the
suite, or any source slice. The defects Wave 11 itself found
were fixed inside their own slices (Z-094's four tooling
defects, Z-095's two harness defects, Z-093 fold-in's probe
object-build fix, Z-092's stale comment and export-count
fixes) and are restated where their sections carry them.
This synthesis discovered no new defect; the gaps it carries
(Z-098, the Z-095 canonical re-validation, the per-class bin
proposal) are named with their owners in sections 3.8 and 5.
