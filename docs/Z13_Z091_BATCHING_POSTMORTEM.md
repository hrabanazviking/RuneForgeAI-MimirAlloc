# Z-091 - Why batching lost: the K-007 post-mortem

Roadmap slice Z-091 (Wave 11 opener): re-read proposal
P-1 against this machine's constants - lock amortisation
vs locality vs API shape, priced per factor from Waves
2-3 evidence. Gate: post-mortem doc naming the dominant
loss factor. MET: this doc names it (S4), prices every
candidate factor from banked recorded runs (S3), adds a
this-machine re-run of the unmodified K-007 harness on
the current core (S2, recorded run
tests/perf/recorded_run_z13_z091.txt), and closes P-1
honestly with a redirected brief for Z-092 (S5).

Team note: the lead ran the six Mythic Engineering
roles sequentially itself (method design, pricing
audit, harness review, statistics/agreement audit,
ledger audit, risk review); no sub-agents were spawned
for this slice and that is recorded plainly here and in
the team CAMPAIGN_STATE.md.

## S1 What K-007 actually measured (history, not evidence)

K-007 (first campaign, Raspberry Pi, 16 KiB pages,
docs/PHASE_11_BATCHING_K007.md) grouped identical work
differently: inter1 (alloc/touch/free one at a time) vs
bursts of 8/32/128/512 (alloc B, touch all, free all).
Caller-side batching lost everywhere on MimirAlloc:
64 B inter1 93 ns/op rising monotonically to burst512
652 (7.01x); 1024 B 129 to 527 (4.09x), FIFO 847.
glibc lost the same shapes less steeply (64 B 1.27x,
1024 B 2.33x). K-007 named five losses: L-1 batching
always loses; L-2 a generic part (hot reuse defeated);
L-3 burst legs re-fault pages (minflt 261 at 512x64 B,
2130 at 512x1024 B vs 3 interleaved) because spans
released on free are faulted back next cycle; L-4 FIFO
penalty at 1024 B (+61%); L-5 RSS grows with batch.
Its interpretation pointed at L-3, and proposal P-1
(recorded only, register D-06, never implemented) was:
span retention across burst frees - defer unmap/return
of a span emptied by a burst so the next burst reuses
resident pages. A caller-facing batch API was rejected
by the same evidence. Those Pi numbers are this line's
questions, never its numbers (roadmap Law 1); S2
re-takes the question on this machine.

## S2 The Z13 re-run: batching loses worse here

The banked K-007 harness (tests/perf/bench_k007.c,
run_bench_k007.sh, summarize_k007.py, make bench-k007)
was run UNMODIFIED on the merged Z-089 tree (seat
9276fb8) on purpleraider: two full invocations, 7 reps
per leg per invocation, checksums identical per size
on both legs both times (64 B ck 3900000, 1024 B ck
974616 - the workloads are the same work, differently
grouped). Medians of invocation 1; invocation 2 agreed
within 7.8% on every mimir cell (worst cell burst512
64 B; all 1024 B cells within 2.9%). Load honesty:
SHARED HOST - loadavg 1.15-1.55 during the runs with
an Aesir forge GPU soak process holding one core at
~68% CPU throughout; the bench is single-threaded,
median-of-7-processes, and cross-invocation agreement
is the guard. Numbers are this-machine under that
stated load, not quiet baselines.

MimirAlloc, ns per alloc/free pair (invocation 1):

| size | inter1 | burst8 | burst32 | burst128 | burst512 | fifo512 |
|---|---|---|---|---|---|---|
| 64 B | 32 | 32 (1.00x) | 155 (4.84x) | 314 (9.81x) | 396 (12.38x) | 410 (12.81x) |
| 1024 B | 40 | 40 (1.00x) | 1474 (36.85x) | 3211 (80.28x) | 3706 (92.65x) | 3790 (94.75x) |

glibc control, same shapes: 64 B 5 -> 14 (2.8x at
burst512); 1024 B 9 -> 27 (3.0x). Minor faults per
pass (mimir medians): 64 B inter1 40, burst32 88,
burst128 400, burst512 2695; 1024 B inter1 42,
burst32 3186, burst128 9301, burst512 11529 (passes
are 120,000 pairs at 64 B, 30,000 at 1024 B).

Findings against the Pi shape:
- The loss reproduces and is FAR worse at 1024 B
  (92.65x vs the Pi's 4.09x) and worse at 64 B
  (12.38x vs 7.01x), even though the interleaved
  baseline itself got much faster on this machine
  (32/40 ns vs the Pi's 93/129). Batching did not
  get relatively better as the core improved; the
  burst penalty grew.
- burst8 is FREE here (1.00x at both sizes; the Pi
  already paid +9-10%): the loss switches on between
  batch 8 and batch 32, i.e. when the live set stops
  fitting the hottest metadata/cache neighbourhood.
- The Pi's L-4 FIFO penalty does NOT reproduce:
  fifo512 vs burst512 is +3.5% at 64 B and +2.3% at
  1024 B, inside cross-invocation agreement. On this
  machine free ORDER is not a priced factor; both
  orders pay the same deep-live-set cost. Any Z-092
  contract must still state order behaviour (INC-
  K007-02 stands as a warning), but it may not blame
  order for the loss here.
- The generic (glibc) share of the burst effect is
  ~3x at burst512 on this machine. MimirAlloc pays
  12.38x / 92.65x. The amplifier is MimirAlloc-side,
  by a factor of ~4x (64 B) to ~31x (1024 B) over the
  generic effect.

## S3 Pricing the three candidate factors

Each factor is priced as a CEILING from banked Z13
recorded runs: the most it could possibly contribute
to the burst512 penalty over inter1 (64 B: +364 ns/pair;
1024 B: +3666 ns/pair, S2). A factor whose ceiling is
a small share of the penalty cannot be dominant, no
matter how real it is.

### F-1 Lock amortisation - ceiling ~50 ns/pair; NOT dominant

The batching hope: pay the one global lock once per
batch instead of once per op. Two facts kill it.
First, K-007's caller-side batching never amortised
anything: every alloc and free still took the lock
individually - the API shape grouped the CALLS, not
the lock acquisitions (see F-3). Second, even a perfect
internal batch pays the lock at most once per op less:
Z-019 (tests/perf/recorded_run_z13_z019.txt) measured
uncontended WAIT at the floor (mean ~40 cycles, ~13 ns;
p50 exactly one TSC tick) and HOLD at 60 cycles (20 ns)
for small malloc and 86 cycles (29 ns) for small free.
Lock + wait together are ~35-45 ns per op; amortising
511 of 512 acquisitions saves at most ~50 ns/pair -
<=14% of the 64 B burst512 penalty and <=1.4% of the
1024 B penalty. Under contention the wait grows
(Z-019: mean wait ~12x hold at 4 threads, ~26x at 8),
but K-007's shapes are single-threaded and Z-091's
re-run is single-threaded: there is no queue to
amortise away. Verdict: real, small, NOT dominant.

### F-2 Page faults and span release (P-1's target) - ceiling ~5% faults, ~11% with syscalls; NOT dominant on this machine

P-1 aimed at K-007's L-3: spans released on burst
frees are re-faulted next cycle. The faults are real
here too (S2: 2695 faults/pass at 512x64 B, 11529 at
512x1024 B vs 40/42 interleaved). Priced at this
machine's fault cost (Z-014: ~345-505 ns/page, 4 KiB
pages, median of 7 processes, quiet window), they buy
8-11 ns/pair at 64 B (<=3.1% of the penalty) and
133-194 ns/pair at 1024 B (<=5.3%). Adding the full
span release-and-recreate syscall round trip per
released span (Z-015: mmap flat ~0.38-0.54 us, populated
munmap ~64 ns/page on a one-page span; ~169 of 171
spans released per 512-pair cycle at 1024 B, ~7 of 9
at 64 B) raises the ceiling to roughly ~11% at 1024 B
and ~5% at 64 B. Two further facts demote P-1's
mechanism specifically: (a) Z-014's recycling verdict
- recycled mappings fault at 0.979-0.999x of fresh on
this machine, so page residency by itself is worth
almost nothing here; retention only helps by skipping
the fault entirely, and only for the spans it covers;
(b) retention in general form is ALREADY banked - the
retention bound (2 empty spans per class, tunable)
plus the Z-048 mapping pool and Z-050/Z-060 trim and
pressure law - and a burst512 at 1024 B needs ~171
live spans of one class, so the banked bound covers
~1% of a burst's span demand. P-1's fault target was
the Pi's story (16 KiB pages, different span
geometry); on this machine it is a minor term.
Verdict: real, priced, NOT dominant.

### F-3 API shape - explains why F-1/F-2 fixes cannot be reached from outside; a multiplier, not the cost itself

A caller-facing batch API (alloc_n/free_n over a
pointer array) changes nothing inside the allocator:
each pointer still needs its own registry validation,
span lookup and free-list threading under the lock
(the contract is not for sale, roadmap Law 5), so the
per-op metadata work is identical - plus array
traffic and a forced wide live set (L-5: RSS grows
with batch by construction; S2 maxrss deltas agree).
K-007 rejected the caller-facing API by evidence and
S2 re-proves the rejection on this machine: grouping
calls cannot win while the per-op cost itself grows
with live-set size (F-4). What an INTERNAL batch entry
could change is only: one lock acquisition (F-1,
~50 ns ceiling) and one shared metadata pass (F-4).
Verdict: API shape decides WHICH factors a design can
touch; it is not itself where the time goes.

### F-4 Live-set metadata walk (locality of metadata) - the residual, and the only factor that scales like the loss; DOMINANT

What is left is what Z-024 (tests/perf/
recorded_run_z13_z024.txt) proved about the free
path: a small free walks the mapping registry, then
the span registry, and small_free_locked walks the
span registry a SECOND time - two linear walks per
free, priced directly by reverse-order controls:
64 B emptying oldest-first 497.5 vs newest-first
75.1 ns (walk ~85%); 512 B 6333.5 vs 523.2 (~92%);
mapping 8192 6390.9 vs 1176.0 (~82%). Free cost
tracks LIVE-SET SIZE and depth, not class size:
512 B steady free is 5668.0 ns with 1171 spans live
vs 444.1 ns for 64 B with 133 spans. The alloc side
has the same shape (span search and refill over the
live span set, Z-022/Z-019: mixed_batch malloc hold
is bimodal, p50 61 cycles on a span hit with a refill
tail to ~2926). Interleaved alloc/free is the
degenerate best case for this design: live set of
one, walk of length ~zero, one hot block and span
(Z-010 plateaus: L1d 0.79-0.88 ns vs DRAM 108-170 ns
when a working set spills). A burst is the exact
opposite: it manufactures the deepest live set the
workload ever holds - at 1024 B, ~171 spans of one
class live at once, every free and alloc walking
registry depth the interleaved shape never pays,
with the data working set (512 KiB at burst512)
spilling past L1/L2 on top. That is why the penalty
grows monotonically in batch size at fixed total ops
(S2), why it is ~31x the generic glibc effect at
1024 B (glibc's tcache free is O(1) in live-set
size), and why FIFO/LIFO no longer matters here
(both orders walk the same deep set). F-1 + F-2 +
the generic locality share price at most ~15-25% of
the burst512 penalty; the walk is the rest.

## S4 Verdict - the dominant loss factor

DOMINANT FACTOR: per-op cost on this design is a
function of live-set size, because every alloc and
free pays linear registry/span walks over the live
set (F-4). Batching lost - on the Pi, and worse on
this machine - because batching is precisely the
call pattern that maximises live-set size, while
interleaved reuse minimises it. Lock amortisation
(F-1, <=~50 ns/pair) and page faults / span release
(F-2, P-1's target, <=~5% faults, ~11% with syscalls
at 1024 B) are real but minor terms on this machine;
API shape (F-3) only decides whether a design can
reach the walk at all. Grade for batching as built
and as proposed in P-1: LOSS, re-proven on this
machine (12.38x at 64 B, 92.65x at 1024 B, burst512
vs interleaved, S2).

## S5 P-1 closed honestly; the Z-092 brief

Proposal P-1 (span retention across burst frees) is
CLOSED as mis-aimed for this machine: its target
prices at <=~11% of the loss here (S3 F-2), its
general form is already banked (retention bound,
mapping pool, trim/pressure law), and Z-014 proved
page residency itself buys ~nothing on this host.
Register D-06's P-1 stands answered by this doc: not
implemented as proposed, with the reason named
(roadmap Law 2). This doc does not amend the register
file itself; the keeper folds the closure at banking.

What Z-092 inherits instead (its roadmap line reads:
Z-091's dominant factor; contract tests first): a
batch prototype can only win by attacking the WALK -
one lock acquisition for the batch, pointers grouped
by owning span, ONE registry pass per span instead
of one (two) per block, per-span free-list threading
done once. Retention tweaks and lock-only batching
are pre-priced losers (<=~11% and <=~1.4-14%) and
Z-093's verdict run should grade the prototype
against exactly that expectation: any win must show
up as walk removal, or batching loses again for the
reason named here.

## S6 Verification

- Harness unmodified: bench_k007.c, run_bench_k007.sh,
  summarize_k007.py and the Makefile bench-k007 block
  are untouched by this slice (empty git diff); the
  slice changes no src/, no API, no ABI.
- Two full invocations, checksums identical per size
  on both legs; agreement within 7.8% per cell (S2).
- Standing gate: scripts/z13/run_gate.sh GREEN
  4988 checks / 0 failed / 77 by-design skips on the
  merged tree before the re-run and on the committed
  tree after (logs in the team hidden_files/).
- Every Wave 2-3 number cited above was read from
  its banked recorded run on this host during this
  slice (Z-010, Z-014, Z-015, Z-019, Z-024); the Pi
  numbers in S1 are labelled history throughout.
- Bugs: none discovered in repo code. One evidence
  correction is recorded, not hidden: K-007's L-4
  FIFO penalty does not reproduce on this machine
  (S2), and K-007's L-3 interpretation (faults as
  the burst mechanism) is demoted by this machine's
  pricing (S3 F-2) - both are findings of this
  post-mortem, not defects in the banked K-007 run,
  whose own numbers stand as measured on its host.
