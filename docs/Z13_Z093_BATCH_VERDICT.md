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
CLOSED (reaffirmed, S4). The prototype leg was owed
at the original run - the Z-092 batch API did not
exist in that tree yet (S1) - and is DISCHARGED by
the fold-in run (S7): the prototype grades TIE
against the per-op loop at burst512 and LOSS as a
batching answer, for the same named reason (it
removes no walk). No owed item remains for Z-093.

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

Fold-in note: Z-092 has since banked (seat 4188da8)
and merged into this tree (ff2b6fc); the absence
recorded above was true of the original tree only.
The prototype leg is measured and graded in S7.

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
- Prototype leg (Z-092 internal batch API): TIE
  against the per-op loop at burst512 in the fold-in
  (S7: 383 vs 406 filed ns/pair at 64 B, 3544 vs
  3725 at 1024 B; same-run per-op 388 / 3548, inside
  noise on both invocations), and therefore LOSS as
  a batching answer: it still pays 10.9x / 84.4x
  against its own interleaved shape and ~32x / ~114x
  against glibc's filed burst512, because the
  shipped prototype loops the per-op core entries
  and removes no walk (Z-092 doc S2). Per the Z-091
  rule, a batching win must show as walk removal;
  this one does not. Grade discharged in the
  fold-in; the S5 protocol was followed exactly and
  is CLOSED.

## S5 Re-run protocol - DISCHARGED (fold-in, S7)

Filed at the original run, this protocol required:
(1) add the prototype leg to bench_z093.c against
the merged Z-092 API, keeping every shape, total
and checksum identical; (2) re-run run_z093.sh
(two invocations, 7 reps) on the merged tree;
(3) grade against the per-op numbers recorded
here, any win to show as walk removal; (4) file
the re-run as an amendment to the recorded run.
All four steps were executed in the fold-in after
Z-092 banked (seat 4188da8, merged as ff2b6fc):
results in S7, amendment in the recorded run. This
section is CLOSED - nothing is owed for Z-093.

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

## S7 Fold-in: the prototype leg, measured and graded

Fold-in run of record: 2026-11-12, on the merged
tree (team HEAD ff2b6fc = seat main 4188da8, banked
Z-001..Z-092, merged into this branch on top of the
original Z-093 commit ab2aa5f). Runner
tests/perf/run_z093.sh PASS, exit 0 (team
hidden_files/z093_foldin_run.log; raw
z093_raw1.txt / z093_raw2.txt, summaries
z093_sum1.txt / z093_sum2.txt - the original run's
raw and summaries are preserved alongside as
*_orig.txt).

Probe of record (extended in the fold-in): public
header batch hits 0 and .so batch exports 0 - the
prototype is NOT a public symbol, by the Z-092
design; contract header
tests/contract/mimir_batch_z092.h present; core
header batch decls 5; core object batch symbols 3
(T mimir_batch_alloc, T mimir_batch_free,
T mimir_batch_snapshot in build/mimir_core.o).
Harness defect found and fixed in-slice: the first
fold-in attempt's probe read 0 core-object symbols
because make lib builds the .so from dedicated PIC
objects and build/mimir_core.o did not exist yet at
probe time; run_z093.sh now builds
build/mimir_core.o before probing it, and the run
of record was re-executed from a cold object with
the true probe above. The superseded attempt's
bench medians agreed with the run of record within
noise on every headline cell.

Method unchanged: identical shapes, totals and
checksums (64 B ck 3900000, 1024 B ck 974616, mix
ck 998400 - identical across all THREE legs);
273 raw lines per invocation (39 keys x 7 reps);
two invocations, 7 reps. Load honesty: SHARED HOST
but a quiet window - loadavg 0.28/0.56/1.01 before,
0.39/0.58/1.01 mid, 0.48/0.59/1.01 after; numbers
are this-machine under that stated load.
Two-invocation agreement (prototype leg): every
cell within 2.9% (burst512 exact at 64 B: 383/383;
3544/3543 at 1024 B; mix 1021/1011).

Prototype leg, invocation 1 medians (ns per pair):

| size | inter1 | burst8 | burst32 | burst128 | burst512 | fifo512 |
|---|---|---|---|---|---|---|
| 64 B | 35 | 20 | 140 | 303 | 383 | 378 |
| 1024 B | 42 | 26 | 1457 | 3072 | 3544 | 3578 |

mixbatch512: proto 1021 ns/pair (same-run mimir
1016, filed mimir 1137, glibc 20 same-run / 22
filed). Minor faults identical to the per-op leg
(burst512 minflt medians 2693 at 64 B, 11528 at
1024 B, both legs) - the fault term is unmoved, so
the Z-091 P-1 pricing stands untouched.

Grade (against the filed per-op numbers, per S5):

- burst512: proto 383 vs filed mimir 406 at 64 B
  (-5.7%) and 3544 vs 3725 at 1024 B (-4.9%);
  same-run per-op 388 / 3548 puts the prototype
  inside noise of the loop it wraps (-1.3% / -0.1%):
  TIE against the per-op loop.
- inter1: proto 35 / 42 vs filed mimir 34 / 44:
  TIE (a batch of 1 is the per-op call plus the
  batch entry, exactly as the contract shapes it).
- burst8: proto 20 / 26 vs same-run mimir 26 / 33
  (-23% / -21%) - the one real delta: at batch 8
  the walk is negligible, so the saving is the
  minor term (API crossings), collectable but not
  the batching question. burst8 was already free
  for the per-op loop (S3).
- As a batching answer the prototype is a LOSS,
  reason named (Law 2): the live-set metadata
  walk is unremoved. The shipped prototype loops
  the per-op core entries (its own doc, Z-092 S2:
  it removes no walk), so at burst512 it pays
  10.9x (64 B) and 84.4x (1024 B) against its own
  interleaved shape and ~32x / ~114x against
  glibc's filed burst512 (12 / 31 ns/pair) - the
  same loss the per-op loop pays, at the same
  price, because it is the same walk. A future
  batching design (span-grouped, one registry pass
  per span - the Z-091 S5 brief) is a different
  slice's work; nothing here pre-judges it.
- Wave 11 batching verdict, final: per-op batching
  LOSS (S4), prototype batching LOSS (this
  section), P-1 CLOSED. The owed prototype
  measurement is DISCHARGED; no item remains open
  for Z-093.

Verification: slice runner PASS (above); standing
gate scripts/z13/run_gate.sh on the final tree is
recorded in the team CAMPAIGN_STATE.md fold-in
entry. No src/, API or ABI change in the fold-in;
bench_k007 harness untouched; README.md untouched.
Fold-in changes are confined to
tests/perf/bench_z093.c (prototype leg),
tests/perf/run_bench_z093.sh (three legs),
tests/perf/run_z093.sh (extended probe, prototype
build, probe object-build fix), this doc, and the
recorded run amendment.
