# Phase 9 — Fragmentation and Reclaim-Overhead Measurement (I-006)

Slice I-006, Wave 9 (Phase 9 Memory-pressure behavior), team-06.
Base: 51d3ff69963e56a9938ea0fcb8892d6cf6154d29 (I-004 banked).
Evidence test: `tests/core/test_i006.c`, standalone via
`make test-i006` (NOT part of `make test`). Recorded run:
`tests/core/recorded_run_i006.txt`. All quantities below are
the I-002 definitions (docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md
Sections 3.2, 4.1 and 6.2); the trim mechanism measured is
I-003's (docs/PHASE_9_TRIM_IMPLEMENTATION.md,
tests/core/mimir_trim_i003.h). No src/ change; the public ABI
is unchanged (nm -D on the built .so stays at exactly 4).

## 1. Definitions — fixed BEFORE the run

Allocator quantities (I-002 S3.2), all read at quiescent
points (single thread, no allocation in flight, I-002 S5.3):

- **Q-MAPPED** = live mapping bytes + live span bytes
  (`mimir_core_mapped_bytes()`, I-003 surface). Allocator-
  managed bytes ONLY; process figures (RSS, /proc/self/maps)
  are kept separated per I-002 S4 — the single OS-side figure
  used is the anonymous rw-p byte sum in /proc/self/maps,
  used only as a cross-check of the full-trim drop, exactly
  as I-003 used it.
- **Q-RETAINED** = EMPTY spans x page size
  (`mimir_core_retained_bytes()` / `_retained_spans()`).
  The mapping path retains 0, always.
- **Q-LIVESPAN** = live spans (span_published - span_retired)
  x page size, from the G-003 counters.

Fragmentation figures (I-002 S6.2, F-1..F-6):

- **F-1 internal fragmentation, small path.** Two terms,
  reported separately per I-002: (a) sum over live small
  objects of (class_size - requested_size) — NOT derivable
  from allocator state (INC-I002-02: the span header keeps
  the class size, not the request), so this probe computes
  it exactly from the workload's own request record and
  labels it workload-side exact, never allocator-read;
  (b) per-span tail `page - 96 - capacity x class_size`
  with capacity = (16384 - 96) / class_size (derived
  arithmetic, I-002's own examples: class 16 tail 0 B,
  class 1024 tail 928 B).
- **F-2 internal fragmentation, mapping path.** Per live
  mapping, mapping_length - requested_size; mapping_length
  is read as the Q-MAPPED delta of the allocation (the
  allocator holds no other figure for it), requested_size
  is the workload's own record.
- **F-3 free-but-unusable bytes** = (live spans' free slots
  x their class_size) + Q-RETAINED: bytes mapped, not live,
  and not releasable without emptying their span. Free
  slots are computed from the workload record (spans per
  class x capacity - live objects per class); the totals
  are cross-checked exactly against the G-003 object/span
  counters and Q-MAPPED, so a fill-order surprise would
  fail the run, not silently change the figure.
- **Ratios (of the quantities above, nothing invented):**
  R-EXT = F-3 / Q-MAPPED at the fragmented point (the
  free-but-unusable share of everything mapped);
  R-INT = (sum of live spans' F-1(b) tails) / Q-LIVESPAN
  at the same point.
- **F-4 reclaim overhead.** For each trim operation: the
  operation kind (full `mimir_core_trim_retained`, partial
  `mimir_core_trim_retained_some(k)`, no-op trim on an
  empty cache, and the free-path immediate release that
  happens when an emptying span exceeds the retention
  bound), the exact span count returned, the exact
  Q-RETAINED / Q-MAPPED / span_retired deltas (THE
  evidence), the count of span releases (span_retired
  delta; each retired span is exactly one L6 release call
  by construction in span_release_locked), and wall time
  from clock_gettime(CLOCK_MONOTONIC) reported as total
  and per span. Wall time is labelled **timing-never-an-
  oracle** (campaign precedent): it is reported, never
  asserted. F-4's follow-on cost leg: first-allocation-
  after-trim latency vs retained-hit latency, 7 samples
  each, reported as samples + median + min + max, with
  the span_published delta (+1 miss / +0 hit) as the
  exact evidence of which path each sample took.
- **F-6 Q-MAPPED trajectory.** Q-MAPPED and Q-RETAINED
  sampled at every phase point of THIS probe and printed
  as a trajectory table (Section 3). The I-005 cross-slice
  leg of F-6 (sampling across I-005's constrained runs)
  is NOT measured here — see Section 5.

## 2. Method — the stated workload

Page 16384 B; 20 small classes 16..1024 B; span = 1 page,
96 B header; retention bound default 2 (read back from
`mimir_tune_get_retention_bound()`), max 16.

- **Fragmented state (reproducible):** for each of classes
  64, 256, 1024 B (capacities 254, 63, 15), allocate
  exactly 3 spans' worth of blocks at exactly the class
  size (762 / 189 / 45 blocks), fill each block with a
  per-block byte pattern, then free every block at an odd
  allocation index. Result by construction: 9 live spans,
  every span partly live (no span empties), interleaved
  live/free holes in allocation order. Two large blocks
  (20000 B and 40000 B, mapping path) are allocated before
  the pattern and stay live across every reclaim step.
- **Cross-class proof:** with the fragmented state held,
  one allocation of a never-warmed class (128 B) must
  acquire a fresh span (span_published +1) despite the
  F-3 bytes lying free; one allocation of a fragmented
  class (64 B) must reuse a hole (span_published +0).
  Both are then freed and the stray retained span trimmed.
- **Reclaim steps, in order:** free every remaining small
  block in allocation order (the third emptying span per
  class exceeds the bound of 2 and is released on the free
  path itself); partial trim of 2 spans; full trim of the
  rest; a further full trim on the empty cache (no-op).
  After EVERY step, every still-live block (large blocks,
  and small blocks while they live) is verified byte-
  identical against its pattern.
- **Overhead trials:** 5 full-trim trials (warm class 256
  with 2 spans, free all, trim the 2 retained) and 3
  partial-trim trials (same warm, trim_retained_some(1)),
  each timed; then the F-4 latency legs (7 hit samples,
  7 miss samples, class 512).
- **F-1(a) leg:** 30 blocks of 1000 B (class 1024, waste
  24 B each by the workload record), free the odd-indexed
  half, read the live half's term, free the rest, trim.
- **Bounds:** 996 + 32 small blocks and 2 large blocks in
  total; peak Q-MAPPED under 300 KiB; no system-wide
  limits, no stress, single thread. Samtal-safe.

## 3. Measured results

(Filled from tests/core/recorded_run_i006.txt after the
run; every number below is read from that log.)

Run of record: `make test-i006` on the Pi, 120 checks /
0 failures, RESULT: PASS (tests/core/recorded_run_i006.txt).
Totals at final reconciliation: span_published =
span_retired = 36; small_alloc = small_free = 2051;
map_published = map_retired = 2; Q-MAPPED = Q-RETAINED = 0.

### 3.1 The fragmented state (exact, from allocator counters)

- Spans published for the pattern: exactly 9 (3 per class);
  objects handed out: 996; after the odd-index frees: 499
  live small objects (64: 381, 256: 95, 1024: 23), no span
  emptied (Q-RETAINED stayed 0).
- Q-MAPPED at the fragmented point: 262,144 B (= 114,688 B
  large mappings + 9 x 16,384 B spans).
- **F-3 free-but-unusable = 70,976 B** (free slots: class
  64: 381 x 64 = 24,384; class 256: 94 x 256 = 24,064;
  class 1024: 22 x 1024 = 22,528; Q-RETAINED term 0).
- **R-EXT = F-3 / Q-MAPPED = 0.270752** — just over a
  quarter of everything the allocator held mapped at that
  point was free inside spans yet unusable for any other
  class and unreleasable without emptying a span.
- **F-1(b) span tails total = 3,360 B** (3 spans x
  (32 + 160 + 928) B); **R-INT = 3,360 / 147,456 =
  0.022786** of Q-LIVESPAN.
- **F-1(a)** on the fragmented classes is 0 B by
  construction (requests were exactly the class sizes);
  on the separate 1000 B leg (class 1024): 15 live blocks
  x 24 B = **360 B**, workload-record exact (Section 5
  for why allocator state cannot give this term).
- **F-2 mapping path = 54,688 B**: 20,000 B request ->
  mapping_length 49,152 (overhead 29,152); 40,000 B
  request -> 65,536 (overhead 25,536). Both mappings
  stayed live across every reclaim step below.
- Cross-class proof, exact: with F-3 = 70,976 B lying
  free, a first allocation of class 128 acquired a fresh
  span (span_published +1, Q-MAPPED +16,384); an
  allocation of class 64 reused a hole (span_published
  +0). The emptied 128 span was then trimmed (released
  exactly 1, Q-MAPPED restored to 262,144) with every
  live block verified byte-identical afterwards.

### 3.2 Reclaim overhead (F-4) — exact deltas are the evidence

Wall times are CLOCK_MONOTONIC nanoseconds, reported as
timing-never-an-oracle (never asserted, single Pi, light
load):

| Operation | Released | Q-RETAINED delta | span_retired delta | Wall (ns) | Per span (ns) |
|---|---|---|---|---|---|
| Frag-state full trim (1 retained) | 1 | -16,384 | +1 | 9,963 | 9,963 |
| Free-path release at bound (3 classes freed) | 3 | n/a (never retained) | +3 | not timed (inside free) | - |
| Partial trim_some(2) | 2 | -32,768 | +2 | 9,166 | 4,583 |
| Full trim (4 retained) | 4 | -65,536 | +4 | 13,315 | 3,328 |
| Full trim, empty cache (no-op) | 0 | 0 | 0 | 148 | - |
| Partial trim_some(3), empty cache | 0 | 0 | 0 | 148 | - |

- The full trim's OS cross-check agreed byte-exactly:
  the anonymous rw-p sum in /proc/self/maps dropped by
  exactly 65,536 B (4 pages) across the trim — the one
  process figure used, kept separated per I-002 S4.
- Free-path reclaim: freeing the last live block of the
  third span of each class released that span inside the
  free itself (span_retired +3 across the free-all), so
  Q-RETAINED after free-all was exactly 98,304 B
  (6 spans = bound 2 x 3 classes), not 9 spans' worth.
- Trials (class 256 warmed to 2 retained spans each):
  full trim of 2 spans, 5 trials: 5,259 / 4,648 / 6,129 /
  4,944 / 4,204 ns (median 4,944 ns, ~2,472 ns per span);
  partial trim_some(1), 3 trials: 2,704 / 2,445 / 2,630 ns
  (median 2,630 ns for the single span). The very first
  trim of the run (9,963 ns for 1 span) is visibly the
  cold outlier; the warmed per-span cost clusters near
  2.1-3.1 us. Reported, not graded.
- Follow-on cost (the reuse the cache exists to provide),
  class 512, 7 samples each, path proven by the exact
  span_published delta of every sample (hit +0, miss +1):
  retained-hit allocation samples 185 / 166 / 130 / 129 /
  130 / 130 / 111 ns (median 130 ns); first allocation
  after trim 2,592 / 2,574 / 2,537 / 2,482 / 2,482 /
  2,389 / 2,463 ns (median 2,482 ns) — a fresh span
  acquisition costs roughly 19x a retained hit in wall
  time on this host, and exactly one new page mapped,
  which is the durable fact; the ratio is timing only.
- Live-block integrity: every live small block and both
  large blocks were verified byte-identical after the
  frag-state trim, after free-all, and after the full
  trim (pattern checks at each step, all ok).

### 3.3 Q-MAPPED trajectory (F-6, this probe)

| Point | Q-MAPPED | Q-RETAINED | Live spans |
|---|---|---|---|
| baseline | 0 | 0 | 0 |
| after large | 114,688 | 0 | 0 |
| fragmented | 262,144 | 0 | 9 |
| after free-all small | 212,992 | 98,304 | 6 |
| after partial trim(2) | 180,224 | 65,536 | 4 |
| after full trim | 114,688 | 0 | 0 |
| after F-1(a) leg | 114,688 | 0 | 0 |
| final (large freed) | 0 | 0 | 0 |

## 4. What the numbers do and do not say

The fragmentation this allocator can hold at the stated
workload is bounded and fully accounted: every mapped
byte at the fragmented point is either live, F-3
free-but-unusable, span tail, header, or large-mapping
overhead, and the probe's exact counter checks leave no
unexplained remainder. Reclaim is cheap, exact and
non-destructive to live data in every measured form:
free-path release past the bound, partial trim, full
trim, and no-op trim all returned exactly the counts the
I-002/I-003 definitions predict, and the OS saw the full
trim's pages go. Nothing here measures behaviour under
real Android memory pressure (no claim to prevent
low-memory kills, per the Wave 9 exit wording).

## 5. NOT measured — honest register

- **F-5 (budget-step-3 cost) NOT measured:** no budget
enforcement exists at this base (INC-I001-14: budget
enforcement is chartered to no slice; there are no
refusal or admitted-after-trim events to count). Reported
as absent, not approximated.
- **F-6 cross-slice leg NOT measured:** Q-MAPPED sampling
across I-005's constrained-memory runs is owed to
whichever slice runs after I-005 banks; this probe's
trajectory (S3.3) covers only its own run. I-005 was
being built in parallel from the same base.
- **F-1(a) from allocator state NOT measurable:** the
span header retains the class size, not each object's
request (INC-I002-02). The 360 B figure is exact from
the workload's own request record only.
- **Locked hold time inside trim NOT distinguished:**
F-4 asks for it "where the harness can measure it"; this
harness measures whole-call wall time only — the core
exposes no lock-hold instrumentation at this base.
- **Free-path release wall time NOT measured:** the
release happens inside mimir_free among 499 other frees;
only its exact counts were taken.
- **Address-space (external) fragmentation NOT
measured:** a single-threaded in-process probe holding
under 300 KiB cannot speak for address-space layout;
X7's address-space term stays declared-not-measured
with that reason.
- No multi-threaded, multi-process, or Android figures;
no soak; wall times are one light-load Pi sample set,
never an oracle.

## 6. INC-I006 register

- **INC-I006-01 (F-5 unmeasurable at this base):** no
budget exists to refuse, so F-5's counts have no
events. Carried from INC-I001-14; closes only when a
future slice implements budget enforcement.
- **INC-I006-02 (F-6 owed cross-slice):** the I-005
trajectory leg remains owed after I-005 banks; I-009's
exit packet should grade F-6 as this-probe-only until
then.
- **INC-I006-03 (first-trim cold cost):** the run's
first trim measured 9,963 ns for one span against a
warmed cluster of ~2.1-3.1 us per span. Recorded as an
observation (cold caches/instruction path), not a
defect; no code change proposed.
- **INC-I006-04 (no defect found):** no allocator defect
was discovered by this slice. Every predicted count
matched exactly on the first recorded run (one compile
fix only: an unused local in the test itself, removed
before the run of record). No bug was left unfixed
because none was found; the unused-variable fix is in
the test source as committed.
