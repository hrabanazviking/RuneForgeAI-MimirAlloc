# Z-093 - Batch verdict run

Roadmap slice Z-093 (Wave 11): prototype vs per-op loop
vs glibc on batch-shaped workloads. Gate: verdict
recorded (Law 2) - including if batching loses again,
with the reason named and P-1 closed honestly.

VERDICT (this run): batching LOSES AGAIN on this
machine, for the reason Z-091 named - the live-set
metadata walk. The per-op loop over batch shapes pays
11.94x (64 B) and 84.66x (1024 B) at burst512 against
its own interleaved shape, and 33.8x / 120.2x against
glibc on the identical burst512 shapes. P-1 stays
CLOSED (reaffirmed, S4). The prototype leg is OWED:
the Z-092 batch API does not exist in this tree yet
(S1), so no prototype number is claimed anywhere in
this slice - the verdict covers every batching form
that exists on this tree today, and the prototype leg
must be measured when Z-092 merges (S5).

Team note: the lead ran the six Mythic Engineering
roles sequentially itself (method design, harness
build, statistics/agreement audit, verdict grading,
ledger audit, risk review); no sub-agents were spawned
for this slice and that is recorded plainly here and
in the team CAMPAIGN_STATE.md.

## S1 The prototype leg is absent - probe, not assumption

Z-093 was built ahead on the Z-091 base (seat main
9276fb8, banked Z-001..Z-089, plus team Z-091) while
Z-092 (Batch API prototype) is built concurrently on
another team branch; Z-092 is not banked and not in
this tree. The slice runner probes for the prototype
before measuring (tests/perf/run_z093.sh, log in the
team hidden_files/z093_run.log):

- public header include/mimir_alloc.h: 0 batch hits -
  no batch declaration of any kind;
- built libmimiralloc.so dynamic exports: 0 batch
  symbols (nm -D --defined-only).

So the roadmap's three legs reduce, on this tree, to
the two that exist: the MimirAlloc per-op loop and the
glibc per-op loop, driven over batch-shaped workloads
by the new harness tests/perf/bench_z093.c (K-007
geometry unmodified, plus one mixed-class batch
shape). Any Z-093 document claiming a prototype number
on this base would be inventing it; this one does not.

## S2 Method

bench_z093.c is built twice (glibc leg -DLEG_GLIBC,
mimir leg over the banked LIBOBJS, both -O2 shipped
flavour, bench_k007 precedent). Shapes: inter1 /
burst8 / burst32 / burst128 / burst512 (LIFO) /
burst512fifo at 64 B (120,000 pairs per leg) and
1024 B (30,000 pairs), plus mixbatch512 (batch of 512
cycling sizes 32/64/128/256/512/1024, LIFO, 30,720
pairs) - the mixed-class batch a real bulk caller
holds. One leg per process, warm-up 1/10 untimed,
whole-pass CLOCK_MONOTONIC, 7 reps per leg, TWO full
invocations. Checksums identical per size across legs,
shapes and invocations (64 B ck 3900000, 1024 B ck
974616, mix ck 998400) - the workloads are the same
work, differently grouped. Recorded run:
tests/perf/recorded_run_z13_z093.txt (raw + summaries
in the team hidden_files/).

Load honesty: loadavg 1.08-1.22 across the run window
on the shared host (Aesir / Lifthrasir / Tensile
co-tenants; team-02 building Z-092 concurrently).
Single-threaded median-of-7-processes with
two-invocation agreement as the guard: every mimir
median agreed within 3.7% between invocations (worst
cell mixbatch512; burst512 cells within 2.5%). Numbers
are this-machine under that stated load, not quiet
baselines.

## S3 Results (invocation 1 medians, ns per pair)

MimirAlloc per-op loop:

| size | inter1 | burst8 | burst32 | burst128 | burst512 | fifo512 |
|---|---|---|---|---|---|---|
| 64 B | 34 | 33 (0.97x) | 147 (4.32x) | 319 (9.38x) | 406 (11.94x) | 389 (11.44x) |
| 1024 B | 44 | 39 (0.89x) | 1511 (34.34x) | 3168 (72.00x) | 3725 (84.66x) | 3729 (84.75x) |

glibc per-op loop, same shapes: 64 B inter1 5 ->
burst512 12 (2.40x); 1024 B inter1 9 -> burst512 31
(3.44x). Mixed-class batch (mixbatch512): mimir 1137
ns/pair vs glibc 22 ns/pair (51.7x). Minor faults per
pass (mimir medians) reproduce the Z-091 shape: 64 B
inter1 40, burst512 2693; 1024 B inter1 42, burst512
11527.

Findings:

- The Z-091 verdict reproduces on the current core
  with a dedicated harness: burst cost grows
  monotonically in batch size at fixed total ops;
  burst8 is again free (0.97x / 0.89x); the loss
  switches on between batch 8 and 32.
- Against glibc on identical shapes the gap widens
  with depth: at burst512 mimir pays 33.8x (64 B) and
  120.2x (1024 B) the glibc per-pair cost, because
  glibc's tcache free is O(1) in live-set size while
  the mimir per-op cost walks the live set (Z-091
  F-4, Z-024 controls).
- The mixed-class batch is not a rescue: spreading a
  batch across six classes still manufactures a
  512-deep live set, and mimir pays 1137 ns/pair
  against glibc's 22.
- FIFO vs LIFO at burst512 is again inside noise
  (-4.2% at 64 B, +0.1% at 1024 B): free ORDER is not
  a priced factor on this machine (Z-091 finding
  stands).

## S4 Verdict in roadmap grades, and P-1

- Batching as it exists on this tree (caller-side
  grouping over the per-op API, any class mix):
  LOSS - 11.94x / 84.66x against its own interleaved
  shape at burst512, 33.8x / 120.2x against glibc.
- Reason named (Law 2): the live-set metadata walk -
  per-op cost is a function of live-set size (linear
  registry/span walks per alloc and free, Z-024), and
  a batch is precisely the pattern that maximises
  live-set size. Lock amortisation and page faults
  remain the priced minor terms of Z-091 S3
  (<=~50 ns/pair; <=~5% faults, ~11% with syscalls at
  1024 B); nothing in this run moves those prices.
- P-1 (span retention across burst frees): CLOSED,
  reaffirmed. Z-091 closed it as mis-aimed for this
  machine (target <=~11% of the loss; general form
  already banked; Z-014 recycling ratio 0.979-0.999).
  This run's fault counts match Z-091's, so the
  pricing that closed P-1 stands unchanged. It is not
  re-opened and not implemented.
- Prototype leg (Z-092 internal batch API): NO GRADE
  - not measurable on this tree (S1). This is an owed
  measurement, not a silent pass: see S5.

## S5 OWED - what must happen when Z-092 merges

1. Add the prototype leg to bench_z093.c against the
   merged Z-092 API (one lock acquisition for the
   batch, pointers grouped by owning span, one
   registry pass per span - the Z-091 S5 brief),
   keeping every other shape, total and checksum
   identical, and re-run run_z093.sh (two invocations,
   7 reps) on the merged tree.
2. Grade the prototype in the roadmap's grades
   against the per-op numbers recorded here: per
   Z-091, any win must show up as walk removal - a
   win smaller than the walk share is the minor terms
   (lock, faults) being collected, not batching
   vindicated.
3. File the re-run as an amendment to the recorded
   run; until then, Wave 11's batching verdict is:
   per-op batching LOSS (this doc), prototype OWED.

## S6 Verification

- Slice runner tests/perf/run_z093.sh PASS, exit 0:
  prototype probe recorded, both invocations complete
  (182 raw lines each, as expected), checksums OK.
- Standing gate scripts/z13/run_gate.sh GREEN
  4988 checks / 0 failed / 77 by-design skips on the
  base tree before the run and on the committed tree
  after (logs in the team hidden_files/).
- No src/, API or ABI change; the K-007 harness is
  untouched; README.md untouched. New files only:
  the bench, its runner, its summariser, the slice
  runner, this doc, the recorded run.
- Bugs: none discovered in repo code. One harness
  design note, recorded not hidden: bench_z093's mix
  shape has no interleaved counterpart by design (a
  mixed live set is the shape under test); its grade
  is stated against glibc on the identical shape and
  against the same-class burst cells, never against a
  fabricated mixed inter1 baseline.
