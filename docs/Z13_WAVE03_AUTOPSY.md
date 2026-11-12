# Z13 Wave 3 Autopsy Verdict

Slice: Z-027 - Autopsy verdict
File: docs/Z13_WAVE03_AUTOPSY.md
Campaign: MimirAlloc Z13, purpleraider (ASUS ROG Flow Z13, Strix Halo)
Roadmap: docs/MIMIRALLOC_Z13_ROADMAP.md, Wave 3 (Z-019 to Z-027)
Seat base at build: main 0b6c20f (banked Z-001..Z-019), merged into
this branch before drafting; team-04 HEAD before this slice:
d4809f9 (Z-018).

## Purpose and gate

This document is the Wave 3 verdict: the ranked loss table that
says where the nanoseconds go, per workload, and which wave owns
burning each down. Gate (roadmap Z-027): every Z-009 baseline gap
assigned to a named later slice. Section 6 is that assignment,
row by row; no Z-009 gap is left unowned.

Laws that govern this verdict (roadmap, Laws of this line):
Law 1 - measurement first, this machine only. Every number below
is cited to a recorded run in-tree on purpleraider. First-campaign
Pi figures are history and questions, never numbers here.
Law 2 - honest grading. Verdict words are WIN, TIE, LOSS (with
the size of the loss), or STILL LOST - REASON NAMED. The Z-009
losses stand exactly as recorded; this autopsy explains them, it
does not re-grade them.
Law 3 - pinned methodology. Cited runs embed the Z-002 host
header, follow the Z-004 protocol, pattern-touch every block,
and prove workload equivalence by identical checksums.

## 1. Evidence base - landed vs OWED

LANDED and read from disk by the team-04 lead before drafting:

| Input | Seat commit | Evidence used |
|---|---|---|
| Z-009 baseline of record | 2028f3d | docs/Z13_WAVE01_BASELINE.md (latency, counters, RSS, energy) |
| Z-018 design constants | 0b8abc0 | docs/Z13_DESIGN_CONSTANTS.md (fault, syscall, THP, CCD, alignment constants) |
| Z-019 lock histograms | 0b6c20f | tests/perf/recorded_run_z13_z019.txt (930 lines; hold/wait per entry point) |

OWED at commit time - being built in parallel by sibling teams,
none yet committed on its team branch when this verdict was
filed (verified from disk: every sibling worktree sits at its
merge of seat main 0b6c20f with no Z-02x commit; team-08 shows
profiling under way in build/z22test). These inputs are marked
OWED wherever this verdict would have used them; nothing from
them is invented, estimated, or imported. This document is
amended, not rewritten, as each lands.

| Slice | Owner team | Status at filing | What it will sharpen |
|---|---|---|---|
| Z-020 contention ladder (mimir) | team-06 | OWED - in flight | KL-B inversion point, 1..32 threads |
| Z-021 contention ladder (glibc) | team-07 | OWED - in flight | side-by-side glibc ladder |
| Z-022 small-path anatomy | team-08 | OWED - profiling under way | top-10 hot spots for Wave 4 |
| Z-023 large-path anatomy | team-09 | OWED - in flight | KL-C cost split percentages |
| Z-024 free-path anatomy | team-01 | OWED - in flight | free-cost table per geometry |
| Z-025 calloc/realloc anatomy | team-02 | OWED - in flight | zeroing cost, realloc cliffs |
| Z-026 redesign dossier | team-03 | OWED - in flight | architecture decision (DECISION_LEDGER) |

Method note on mixing windows: the Z-009 latency factors come
from the Z-003 window (Tensile contention, load 5.29-6.14); the
Z-019 holds come from an instrumented debug build in a later
window (shared host, load 2.50-2.68, drift at most 3.0 percent).
The holds therefore attribute shares INSIDE mimiralloc's own
cost - they are never subtracted from the Z-003 medians to
manufacture a residual verdict. Where a quiet-window absolute
is needed, the Z-005 window (load 0.61-0.85) is cited as such.

## 2. The ranked loss table (Z-009 baseline gaps)

Source: Z-009 section 2 (Z-003 recorded run,
tests/perf/recorded_run_z13_k002.txt; medians of 7 processes per
leg; one checksum per workload across both legs). Ranked by
loss factor; absolute excess (mimir minus glibc median ns/op)
shown because the two rankings disagree and both matter.

| Rank | Workload | glibc ns/op | mimir ns/op | Factor | Excess ns/op | Verdict |
|---|---|---|---|---|---|---|
| 1 | mixed_lifecycle | 13.38 | 898.76 | 67.2x | 885.38 | LOSS |
| 2 | mixed_batch | 9.90 | 398.42 | 40.2x | 388.52 | LOSS |
| 3 | small_churn | 9.02 | 93.43 | 10.4x | 84.41 | LOSS |
| 4 | large_batch | 1295.68 | 3064.69 | 2.4x | 1769.01 | LOSS |

Reading: by ratio, mixed_lifecycle is the deepest wound on this
machine. By absolute nanoseconds per op, large_batch carries
the largest excess (1769 ns/op) despite the smallest factor -
glibc is simply slow there too, so the ratio flatters mimiralloc.
No workload is a WIN or a TIE on latency. The counter, RSS and
energy standings of the same baseline are autopsied in
sections 3 to 5; every row is assigned an owner in section 6.

## 3. Where the nanoseconds go - per workload

Hold/wait figures: Z-019 recorded run (instrumented build;
median-of-process mean hold, cycles converted at the run's own
2.9944 cyc/ns; stamp-pair floor 30 cycles). Single-thread WAIT
sits at the measurement floor on every entry point and workload
(mean about 40 cycles, about 13 ns; p50 exactly 30 cycles, one
TSC tick; p99 60 cycles). Finding F-1: single-threaded, nobody
waits for a free lock - the lock mechanism is not the wound;
the work done UNDER the lock is. HOLD is therefore the path
fingerprint used below.

### 3.1 mixed_lifecycle - LOSS 67.2x (rank 1)

Z-019 holds: malloc mean 624.7 cyc (208.6 ns), bimodal - p50
about 61 cyc for small-class hits, p90 about 2490 cyc where the
large minority pays the mapping path; free mean 1777.1 cyc
(593.5 ns), p50 about 1332 cyc. Z-005 counters (quiet window):
cycles/op 4374.61 vs glibc 35.28 (124.0x), instructions/op
9550.16 vs 208.22 (45.9x), cache-misses/op 4.90669 vs 0.01048
(about 469x) - the deepest counter wound of the baseline.
Autopsy: lifecycle churn pays BOTH wounds at once - the small
path's code length on every op and the mapping path's hold on
every large excursion, plus a cache-miss rate glibc does not
have. The free path (593.5 ns mean hold) costs about 2.8x the
malloc path here. Z-024 (free-path anatomy) and Z-025
(calloc/realloc anatomy) will split this further; both OWED.

### 3.2 mixed_batch - LOSS 40.2x (rank 2)

Z-019 holds: malloc mean 143.1 cyc (47.8 ns), bimodal - p50
about 61 cyc on span hits, p99 about 2926 cyc on the span-refill
tail; free mean 601.2 cyc (200.8 ns), p50 about 550 cyc.
Z-005 counters: cycles/op 1173.78 vs 18.31 (64.1x),
instructions/op 3125.60 vs 100.61 (31.1x), cache-misses/op
0.45442 vs 0.01806 (about 25x), branch-misses/op 2.73256 vs
0.01468. Autopsy: batch allocation repeatedly falls off the
fast span path into refill, and free pays registry/span
validation on every block; the wound is path length plus
refill frequency, not lock overhead (wait at floor). This is
also the KL-E workload family: batching lost here as it lost
in K-007. Z-022 will name the hot spots; OWED.

### 3.3 small_churn - LOSS 10.4x (rank 3; KL-A, the canonical wound)

Z-019 holds: malloc mean 60 cyc (20.1 ns), free mean 85.9 cyc
(28.7 ns) - free is the more expensive small entry, paying
registry/span validation. Z-005 counters (quiet window):
cycles/op 175.65 vs 17.83 (9.9x), instructions/op 861.97 vs
82.99 (10.4x), cache-misses/op near parity (0.01688 vs 0.01362).
Autopsy, in Z-005's own reading: the small path's loss is
instruction count, not cache behaviour - KL-A is a code-path-
length wound, executed under one global lock. Z-017 adds a
fixable geometry defect on this path: the 48 B class splits
cachelines on 49.41 percent of blocks and the 64 B class on
100.00 percent (every pointer at ptr mod 64 = 32); both classes
are named for Wave 4. Z-022's top-10 hot-spot list will rank
the source regions; OWED at filing.

### 3.4 large_batch - LOSS 2.4x (rank 4 by factor, rank 1 by excess)

Z-019 holds: malloc mean 2541.6 cyc (848.8 ns), free mean
4489.8 cyc (1499.4 ns) - the largest holds in the table; the
mmap/munmap pair, held across the calls, dominates. Z-018
constants size the parts: fresh fault cost about 345-380
ns/page to 16 MiB, rising to about 466-505 ns/page at 64 MiB
to 1 GiB (Z-014); untouched mmap is flat at about 0.38-0.54 us
at any size and populated teardown costs about 63-65 ns/page
from 1 MiB up (Z-015). Two would-be savings are DISPROVEN by
landed evidence: recycling does not make faults cheaper at
large sizes (Z-014 recycled-vs-fresh about 0.999/0.979), so
pooling may be argued only on syscall/VMA economics; and the
cutoff design (a mapping per allocation past 1024 bytes, KL-C)
pays the full syscall-plus-fault price on every large op where
glibc's threshold-and-cache does not. Z-023 will record the
exact cost split percentages; OWED at filing.

### 3.5 Contention - KL-B sized at histogram level

Z-019 contended specimen (small_churn shape, one CCD): at 4
threads hold barely moves (malloc 131.7 cyc, free 203.1 cyc)
but mean WAIT jumps to 1597 cyc (malloc) / 1688 cyc (free),
about 12x the hold, p99 about 14.7k-15.2k cyc (about 5 us).
At 8 threads: hold 163.6 / 279.4 cyc, mean wait 4190 / 4292
cyc (about 1.4 us), about 26x the hold, p99 about 29.7k-30.2k
cyc (about 10 us). Finding F-2: threads spend their lives
queueing for a lock whose actual work takes 100-300 cycles -
the scaling wound is serialised HOLD summed across threads,
not lock overhead and not hold growth. Every cycle of hold
removed is therefore paid back once per queued thread. The
full 1..32-thread ladders and the inversion point belong to
Z-020/Z-021; OWED at filing - this verdict claims the
histogram-level sizing only, exactly as Z-019 filed it.

### 3.6 calloc and realloc (supplementary, Z-019 phases)

Not baseline workloads, recorded in the same Z-019 run:
calloc hold mean 65.2 cyc (21.8 ns) - malloc's shape plus
zeroing at small sizes; realloc hold mean 759.4 cyc (253.6
ns), stay-in-class at p50 about 95 cyc, the grow-to-4096
minority paying the large path; the realloc phase's free of
the grown block holds 2772.7 cyc (926.0 ns), the mapping path
again. Full anatomy is Z-025; OWED at filing.

## 4. RSS / PSS autopsy (Z-009 section 5, from Z-006)

Load state of the source run: shared host, loadavg 2.96/1.89/
1.59 at start, 2.51/1.89/1.60 at end. PEAK Pss standings:
small_churn TIE (-4.3 percent), large_batch TIE (+2.1
percent), mixed_batch LOSS (+23.3 percent Pss, +24.5 percent
Pss_Anon), mixed_lifecycle LOSS (+40.2 percent Pss, +41.8
percent Pss_Anon). Retention after free-all reverses on three
of four: WIN small_churn (-15.3 percent), WIN large_batch
(-39.9 percent), WIN mixed_lifecycle (-64.4 percent);
mixed_batch is the exception, LOSS at both points (+16.3
percent after free-all). Autopsy: at peak the mixed workloads
hold materially more resident memory than glibc; the
mixed_batch path both peaks higher and retains more - a
metadata/footprint wound on the mixed-size path, not a
retention-policy wound (retention elsewhere is a WIN).

## 5. Energy autopsy (Z-009 section 6, from Z-007)

Load state of the source run: contended window, loadavg
8.65/3.39/2.15 at start decaying to 3.67/3.12/2.17; NOT a
quiet baseline. Standings: LOSS in direction on all four
workloads. Proven net factor: large_batch 4.42x (glibc
11861.56 nJ/op, mimiralloc 52394.16 nJ/op). small_churn
factor 17.82x is INDICATIVE only (glibc median near the
idle-subtraction floor). mixed_batch and mixed_lifecycle net
factors are BELOW idle-subtraction resolution in that window
(direction proven by disjoint gross ranges); their quiet-
window re-run is owed work recorded by Z-007 itself and is
assigned in section 6 - it is not invented here. Autopsy:
energy follows the counter leg - more instructions and more
cycles per op is more joules per op; no separate energy
defect is indicated by landed evidence.

## 6. Gate assignment - every Z-009 gap owned by a named later slice

| # | Z-009 baseline gap | Owning slice(s) | Wave |
|---|---|---|---|
| G-1 | small_churn latency LOSS 10.4x (KL-A) | Z-028/Z-029 TLS cache; verdict Z-031 | 4 |
| G-2 | mixed_batch latency LOSS 40.2x | Z-028/Z-029 (path), Z-034 (diet); batching root Z-091, prototype Z-092, verdict Z-093 | 4, 11 |
| G-3 | large_batch latency LOSS 2.4x, excess 1769 ns/op (KL-C) | Z-046 threshold, Z-047/Z-048 mapping cache, Z-049 THP, Z-050 trim, Z-052 mremap | 6 |
| G-4 | mixed_lifecycle latency LOSS 67.2x (KL-D) | Z-058 split/coalesce, Z-059 placement, audited at Z-063; setup Z-055/Z-057 | 7 |
| G-5 | Multithread scaling under one lock (KL-B) | Z-037/Z-038/Z-039 arenas, Z-040 false-sharing purge, Z-041 SMT/pinning; proven Z-042/Z-043, verdict Z-045 | 5 |
| G-6 | Instruction excess 3.6x-46x per op (Z-005) | Z-029 (small), Z-034 metadata diet, Z-035 build flags; re-audit Z-096 | 4, 11 |
| G-7 | Cache-miss wounds, mixed workloads (25x / about 469x per op) | Z-032/Z-033 size classes, Z-040 purge, Z-062 reuse-distance/L3 | 4, 5, 7 |
| G-8 | Alignment splits: 48 B 49.41 percent, 64 B 100 percent (Z-017) | Z-032 study, Z-033 retune | 4 |
| G-9 | RSS PEAK LOSS mixed_batch +23.3 percent, mixed_lifecycle +40.2 percent | Z-034 (batch footprint), Z-050 trim, Z-058/Z-059 (lifecycle) | 4, 6, 7 |
| G-10 | RSS retention LOSS mixed_batch +16.3 percent after free-all | Z-034 metadata diet, Z-050 trim policy | 4, 6 |
| G-11 | Energy LOSS in direction, all 4 workloads; proven 4.42x large_batch | Z-031/Z-036 verdict re-measurement; closing Z-107 | 4, 12 |
| G-12 | Energy net factors OWED for mixed_batch / mixed_lifecycle (below resolution, Z-007) | quiet-window re-run owned by Z-036 (Wave 4 verdict) | 4 |
| G-13 | THP adoption at 200 MiB for pooled mappings OWED (Z-018 open row) | Z-049 THP for cached mappings | 6 |
| G-14 | Batching loss family (KL-E, K-007 pattern in G-2) | Z-091 post-mortem, Z-092 prototype, Z-093 verdict run | 11 |

Correctness is not a gap: the Z-009 correctness baseline
(4988 / 0 / 77) stands and is guarded on every later slice by
the Z-008 standing gate; it is listed here so the assignment
is complete, not because ground is owed.

Ownership honesty note (per the roadmap text itself): KL-D
(G-4) has no single dedicated burn-down slice - its owners
are the Wave 7 audit slices named above, with the Wave 6
large-path work (G-3) carrying its large excursions. The
Wave 3 anatomy slices Z-020..Z-026 are EVIDENCE slices for
the rows above, not owners; the redesign choice among their
findings is recorded by Z-026 in docs/DECISION_LEDGER.md
(OWED at filing) and is re-judged at the terminal verdict
Z-108, with this slice revisited there.

## 7. Verdict headlines

V-1. The single lock is not slow; the work under it is.
Single-thread wait is at the measurement floor everywhere
(Z-019); hold time IS the path cost, entry by entry.
V-2. Free is the most expensive small entry on every
workload (28.7 ns small, 200.8 ns mixed_batch, 593.5 ns
mixed_lifecycle, 1499.4 ns large) - validation and, on the
mapping path, munmap under the lock.
V-3. The large path pays syscall-plus-fault on every op
past the 1024-byte cutoff; recycling cannot save the faults
(Z-014), so only threshold, caching and THP economics
(Z-046..Z-049) can close G-3.
V-4. Under contention the wound inverts: wait reaches about
26x hold at 8 threads with p99 about 10 us - serialised hold,
summed across queued threads, is the scaling loss (KL-B).
V-5. Wave 4 is judged first by one number, per Z-019: how
much hold the TLS cache removes from the small-malloc and
free paths. Wave 5 by whether arenas cut the wait; Wave 6 by
whether the large path stops paying per-op mapping costs.
V-6. Nothing in Wave 3's landed evidence overturns a Z-009
standing: all four latency standings remain LOSS at the sizes
recorded, RSS remains split (PEAK losses on the mixed
workloads, retention wins elsewhere), energy remains LOSS in
direction on all four. A proven loss, autopsied, is a
bankable result (roadmap Law 2).

## 8. Traceability index

| Section | Numbers from | Recorded run in-tree | Load state of the run |
|---|---|---|---|
| 2 Ranked table | Z-003 via Z-009 sec. 2 | tests/perf/recorded_run_z13_k002.txt | 5.82/4.42/2.28 at build; 5.29 at end; Tensile contention |
| 3 Holds/waits | Z-019 | tests/perf/recorded_run_z13_z019.txt | 2.50/3.57/2.74 start; 2.68/3.49/2.74 end; shared host, Aesir + Tensile co-tenants |
| 3 Counters | Z-005 via Z-009 sec. 4 | tests/perf/recorded_run_z13_z005.txt | 0.61/2.25/2.43 first check; 0.85/1.56/2.05 end; quiet window |
| 3.3 Alignment | Z-017 via Z-018 sec. 10 | tests/perf/recorded_run_z13_z017.txt | Tensile contention 6.99-7.71 (distributions proven load-independent) |
| 3.4 Fault/syscall | Z-014, Z-015 via Z-018 sec. 7-8 | tests/perf/recorded_run_z13_z014.txt; tests/perf/recorded_run_z015.txt | Z-014 quiet 1.05-1.29; Z-015 contended 1.26-1.74, labelled indicative |
| 4 RSS/PSS | Z-006 via Z-009 sec. 5 | tests/perf/recorded_run_z13_z006.txt | 2.96/1.89/1.59 start; 2.51/1.89/1.60 end; shared host |
| 5 Energy | Z-007 via Z-009 sec. 6 | tests/perf/recorded_run_z13_z007.txt | contended: 8.65/3.39/2.15 start; 3.67/3.12/2.17 end |
| 1, 6 Ownership | roadmap text | docs/MIMIRALLOC_Z13_ROADMAP.md | n/a (slice titles and gates as written) |

Drafting load state (this slice, no bench run of its own):
loadavg 1.71-2.31 across the build window with Aesir forge
and Tensile lab co-tenants - a document slice; the only run
it owns is the standing correctness gate below.

## 9. Correctness gate for this slice

Standing gate (Z-008): scripts/z13/run_gate.sh on this tree,
log hidden_files/z027_gate.log - counts read from that log:
4,988 checks / 0 failed / 77 by-design skips, GATE RESULT: GREEN, gate exit 0 (loadavg 4.53/2.59/2.34 at start, shared host). This slice adds no code and no
tests; the gate guards the merged tree the verdict describes.
Bugs discovered during this slice: none in repo code. One
evidence-chain defect was caught and corrected before filing:
a sibling-landing scout report claimed a Z-022 commit that
does not exist on disk (verified by git log and file search
in the team-08 worktree); the claim was discarded, Z-022 is
recorded OWED above, and the discrepancy is logged in this
team's CAMPAIGN_STATE.md.
