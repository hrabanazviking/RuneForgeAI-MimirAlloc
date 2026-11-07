# Phase 7 - Exit Evidence Packet

Slice: **G-009 (team-09)**. Wave 7, final slice. Phase 7 exit
assembly: this packet originates no design and no
implementation; it tallies the banked Wave 7 evidence
(G-001..G-008) against the exit-evidence matrix G-001
defined, re-confirms it with one fresh recorded run at this
base, and carries every still-open item forward by name.
Base: `31308c6` (G-008 banked; Wave 7 = 8 of 9) on branch
`yrsa/mimiralloc-09`, worktree `/home/pi/mimiralloc-team-09`
on Raspberry Pi `purpleviking1` (aarch64, page size 16384,
Samtal live on port 8787 throughout; `free -g` 15 GiB total,
13 GiB available before and after the fresh run).
Governing texts: Roadmap Section 8 Phase 7;
`docs/PHASE_7_FOUNDATION_PLAN.md` (G-001, matrix X1-X7);
the banked slice docs G-002..G-008 with their INC lists;
WAVE_PLAN.md in the coordinator checkout. Fresh run:
`tests/core/recorded_run_g009.txt` (this slice, this base).
`README.md` and `include/mimir_alloc.h` byte-identical to
base (sha256 in the recorded run); no Makefile or `src/`
change in this slice.

> **BOTTOM LINE, STATED FIRST.** The Roadmap Phase 7 exit
> sentence is: *"no regression in correctness; documented
> allocation latency, retained memory and responsiveness
> versus Phase 3 and the platform allocator."* This packet
> grades it **PARTIAL — not a clean exit.** Correctness
> (X1) and both latency rows (X2, X3) and tuning (X7) are
> MET. Retained memory (X4, X5) and responsiveness (X6)
> are PARTIAL, for one measured reason that this packet
> does not grade around: **INC-G008-02 — `dlclose` orphans
> retained spans.** Every load/unload cycle that warms
> small classes permanently leaves their retained spans
> mapped until process exit, measured at exactly
> **49,152 bytes (3 pages) per cycle** for the 3 warmed
> classes and reproduced byte-exactly by this packet's
> fresh run (Section 4). The remedy is commissioned, not
> left: **H-001**, a destructor/unload drain for retained
> spans, stands at the head of the carried-owed register
> (Section 9) as the first slice of the next wave, with
> its own evidence test defined there. Phase 7's
> in-process story is proven and strong (Sections 6-8);
> its unload story is a real defect, and the exit tally
> says so.

## 1. Verdict vocabulary and reading rules

- **MET** - the evidence the item specifies exists, in the
  form it specifies, in the banked repo (or, for fresh-run
  items, in this packet's recorded run).
- **UNMET** - some evidence exists, but the item as written
  is not satisfied.
- **PARTIAL** - one named part of the item is met and
  another named part is discharged only in a weaker,
  explicitly recorded sense, or is owed.
- **OWED** - the evidence is wholly absent at this base;
  the row names exactly what is missing.
- Counts are copied from the recorded-run files named in
  each row, never restated from memory or from another
  slice's summary. Where this packet's fresh run reproduces
  a banked count, both are cited.
- Every Wave 7 claim is single-target (Pi-native AArch64
  Linux/glibc, 16 KiB pages) unless a row says otherwise.
  No target is promoted to "supported" (Roadmap Section 5
  sense) by anything in this packet.
- Claim labels follow G-001: **PROVEN on the Pi** (run or
  inspected by a named slice), **documented** (a banked
  source says so), **OWED**, **BLOCKED**.

## 2. Fresh verification run for this packet (G-009)

One fresh run on the Pi at base `31308c6`, recorded in
`tests/core/recorded_run_g009.txt` (1631 lines). Counts
below are this run's only; no banked number is copied
into this section.

| Step | Exit | Fresh result |
|---|---|---|
| `make clean` | 0 | - |
| `make test` | 0 | 8/8 suites PASS; final accounting **4590 checks run, 0 failed** (smoke 18/18, core 29, b007 69, c005 95, c006 23, c007 122, c008 42 with 7 skipped-not-yet, d004 final 4590) |
| `make test-g001` | 0 | Probe legs complete, fails=0 throughout: mimir p50 148 ns (sizes 8, 16), 166 (32), 185 (64), 203 (128), 204 (256), 223 (512), 241 (1024), 5889 (4096, large path); libc p50 56 ns (8..1024), 74 (4096) |
| `make test-g003` | 0 | **54 checks, 0 failures**; small_allocs == small_frees == 5650 |
| `make test-g004` | 0 | **64 checks, 0 failures**; mappings published == retired == 8, small 562 == 562 |
| `make test-g005` | 0 | **97 checks, 0 failures**; final live_spans == 40, retained mapped bytes == **655,360** (RP-5 worst case, reached exactly) |
| `make test-g006` | 0 | **44 checks, 0 failures**; counters reconcile exactly |
| `make test-g007` | 0 | **132 checks, 0 failures**; final retained 37 spans / 606,208 B at the test's designed mixed-bound end state |
| `make test-g008` | 0 | **214 checks, 0 failures**; INC-G008-02 reproduced byte-exactly (Section 4); churn 14,656,864 ops, 0 fails; probe 60/60 Samtal samples HTTP 200 |
| `make test-e005-dyn` | 0 | **34 checks, 0 failures** (final retained=49,152 B = 3 pages, the retention bound in the dynamic leg's accounting) |
| `make lib` + `nm -D` | 0 | Exactly 4 defined dynamic symbols: `mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc` |

Integrity, same run: `include/mimir_alloc.h` and
`README.md` sha256-identical to HEAD `31308c6`; Samtal
`GET /` HTTP 200 before, during (60/60 probe samples) and
after the run; `free -g` unchanged (13 GiB available).

## 3. Banked evidence inventory (G-001..G-008)

| Slice | Deliverable in one line | Headline banked evidence |
|---|---|---|
| G-001 | Foundation plan + measured Phase 3 baseline | Phase 3 p50 4889-5167 ns, size-independent; 32 KiB mapped per allocation on this host; retained-after-free 0; libc leg p50 56 ns. Matrix X1-X7 defined |
| G-002 | Size-class table design | 20 classes 16..1024, cutoff 1024, one-page spans, routing R-1..R-6; census-derived, distribution honestly unmeasured (INC-G002-02) |
| G-003 | Small-object path implementation | Span path live with central fallback; measured p50 148-253 ns at 8..1024; 256 live size-64 blocks cost 16 KiB RSS delta vs 8192 KiB baseline |
| G-004 | Central fallback audit + full correctness re-run | R-1..R-6 all PASS (test_g004 64/0); INC-G003-02 executed: test-e005-dyn re-taught span-aware (34/0); same-spirit re-teaches d003/e008/F-005 |
| G-005 | Reclaim policy RP-1..RP-5 + idle-thread measurement | Policy found already implemented by G-003 (INC-G005-01); test_g005 97/0: exactly +2 spans/class, 40 spans = 655,360 B worst case reached, never exceeded; idle threads add zero retention |
| G-006 | Thread exit handling (proof slice) | No per-thread state exists (recorded greps); test_g006 44/0: exit-while-holding freed from main, counters reconcile 1421/1421 and 424/424; live spans flat at 7 across 48 lifecycles |
| G-007 | Tuning parameters | RP-2 retention bound now one locked scalar, internal/test-only setter/getter, valid 0..16, invalid refused -1 with previous value kept, default exactly 2; test_g007 132/0; public ABI unmoved |
| G-008 | Repeated load/unload + responsiveness record | 50 dlopen/dlsym/dlclose cycles clean (214/0); image unmaps every cycle; **INC-G008-02 orphan finding (Section 4)**; Samtal p50 0.896 ms baseline vs 0.996 ms under churn, 60/60 HTTP 200, no bound claimed |

## 4. The headline finding, restated in full (INC-G008-02)

G-008 measured, and this packet's fresh run reproduced
byte-exactly, the following defect in the unload path.
Nothing in this section is softened.

- The library image fully unmaps at every `dlclose`
  (`img_after_close=0` in all 50 cycles, both runs); the
  library's registries and counters live in the image, so
  each fresh load starts from a fresh registry, and every
  cycle's workload verifies clean against it.
- **Retained spans do NOT unmap.** Anonymous rw bytes
  after each close grow by **exactly 49,152 bytes =
  3 pages per cycle, every cycle**: 98,304 B after
  cycle 0 to **2,506,752 B after cycle 49** (growth
  cycles 1-50 = 2,408,448 B = 147 pages = 49 x 3). The
  3 pages are exactly the retained spans of the 3 small
  classes the workload warms (256, 512 via the realloc
  grow, 1024 — one span retained per class in this
  workload, under the RP-2 bound of 2). VmRSS grows in
  step (+48 KiB per cycle; fresh run 1376 KiB start to
  3840 KiB end, delta +2464 KiB including the orphan
  probe and reload legs; banked run 1360 to 3824 KiB,
  same delta).
- Orphan probe, both runs: a 256 B block leaked across a
  final `dlclose` stays mapped after the close and its
  pattern reads back byte-intact — but no defined path
  can ever free or reuse it, because the registry that
  owned it was destroyed with the unmap (INC-G008-03).
- Mechanism, by G-008's recorded inspection: `src/`
  registers **no destructor** and no atexit/atfork or
  thread-local cleanup, so `dlclose` unmaps the image
  but nothing ever munmaps the span mappings the image's
  code created. They stay mapped until process exit.
  This is the load/unload face of INC-G005-03's
  no-reaper design.
- **Derived, not measured** (G-008's arithmetic, carried
  as derived): a consumer that warms all 20 classes to
  the RP-2 bound in each load orphans the full RP-5
  worst case — 40 spans = **640 KiB per load/unload
  cycle**, unbounded across cycles, until process exit.
- The large (mapping) path contributes nothing: its
  mappings are retired at free, as in Phase 3.

**Grading consequence, stated once and applied in
Section 5:** while loaded, the Phase 7 allocator's
retention is bounded, proven and small. Across an
unload, retention is neither bounded nor reclaimed.
Any exit row whose wording covers retained memory or
repeated load/unload therefore cannot be graded MET
while this stands. The fix is commissioned as **H-001**
(Section 9, first entry): a destructor/unload drain for
retained spans, proven by re-running the G-008 harness
to zero growth. G-008 deliberately made no core change
(record the property, do not silently change it); this
packet likewise changes no code — it grades, and it
commissions.

## 5. Exit tally against the G-001 matrix (X1-X7)

| # | Exit element (G-001 Section 5) | Grade | Evidence in one line |
|---|---|---|---|
| X1 | No regression in correctness | **MET** | G-004 full re-run R-1..R-6 PASS; fresh run Section 2: make test 4590/0 plus all standalones 0 failures |
| X2 | Documented allocation latency vs Phase 3 | **MET** | Baseline 4889-5167 ns p50 (G-001) vs Phase 7 148-253 ns p50 (G-003), same probe shape re-read in the fresh run at 148-241 ns |
| X3 | Documented allocation latency vs platform allocator | **MET** | Same-shape libc leg p50 56 ns (G-001), reproduced exactly in the fresh run; Phase 7 small path documented at ~2.6-4.5x that p50, not parity |
| X4 | Documented retained memory vs Phase 3 | **PARTIAL** | In-process side MET and proven exactly (RP-5, G-005); unload side UNMET: INC-G008-02 orphans 49,152 B/cycle where Phase 3 retained 0 in every dimension |
| X5 | Documented retained memory vs platform allocator | **PARTIAL** | Same-shape probe readings documented both sides (G-001 leg, reproduced fresh), but process-level RSS only (INC-G001-06) and the unload dimension never measured against the platform allocator |
| X6 | Documented responsiveness (multitasking, repeated load/unload) | **PARTIAL** | Metric defined and measured by G-008 and reproduced fresh: device responsiveness shows no material change under churn, no bound claimed; the repeated load/unload half carries the INC-G008-02 defect |
| X7 | Tuning parameters configurable, validated, visible | **MET** | G-007: one parameter settable, validated (0..16, invalid refused, previous kept), visible (exact read-back), on the internal/test surface; public promotion remains a proposal (INC-G007-01) |

### X1 — MET

The matrix assigns X1 to G-004's full correctness re-run,
re-confirmed by G-009. G-004 audited the central-fallback
routing R-1..R-6 route by route (test_g004 64 checks,
final accounting small 562 == 562, mappings 8 == 8),
executed the INC-G003-02 re-teach of the dynamic E-005
leg, and re-taught the three further banked programs the
same coupling had broken (INC-G004-02/-03/-05). This
packet's fresh run re-confirms the whole surface at the
exit base: `make test` 4590 checks, 0 failed, and every
standalone target at 0 failures (Section 2). Scope note,
recorded so the grade is not over-read: X1 grades
allocation correctness — no live allocation is corrupted,
lost, or mis-served anywhere in the evidence, including
across 50 load/unload cycles whose workloads all verify
clean. INC-G008-02 is a retention defect in the unload
path; it is graded where the matrix puts its substance —
X4 and X6 — and in the bottom line (Section 10), not
hidden inside X1 and not used to inflate it.

### X2 — MET

G-001 measured the Phase 3 side on the Pi: p50 between
4889 ns and 5167 ns at every size 8..4096, the cost of
one mapping per allocation. G-003 measured the Phase 7
side in the identical probe shape: p50 148-253 ns across
sizes 8..1024 (148 at 8 and 16; 241 at 1024 in the fresh
re-reading; G-003's banked run reads to 253 ns). That is
roughly a 20-35x reduction at p50, documented on both
sides, and this packet's fresh run (Section 2) re-read
the same shape in the same session: mimir 148-241 ns at
8..1024, and 5889 ns at 4096 — the large path keeps the
Phase 3 mapping shape, as designed. The readings are
micro-probe readings on a shared host (INC-G003-10);
the exit sentence asks for documented latency, and the
documentation exists, is banked, and is reproducible.

### X3 — MET

The platform side of the same probe shape: libc p50
56 ns at sizes 8..1024 and 74 ns at 4096 (G-001 banked
leg), reproduced exactly by this packet's fresh run
(56 ns; 74 ns). Against it, the Phase 7 small path's
148-253 ns p50 is documented honestly: roughly
2.6-4.5x the platform allocator's p50 in this shape —
a large improvement over Phase 3's ~90x, and not
parity, and not claimed as parity anywhere in Wave 7.
INC-G001-02 (no banked platform measurement existed at
G-001's base) is discharged for latency by the two
consistent same-shape legs now on record.

### X4 — PARTIAL

Met part, proven exactly: Phase 3 retained 0 after free
and mapped 32 KiB per live allocation on this host
(G-001). Phase 7, while loaded, retains at most the
RP-5 bound — 20 classes x 2 spans x one page =
**655,360 B** — and G-005 proved the bound is reached
exactly and never exceeded (97/0: +2 spans in every one
of the 20 classes individually, churn-stable at 40
spans, idle threads adding zero); this packet's fresh
test-g005 re-run confirms 40 spans / 655,360 B. Live
footprint fell correspondingly (256 live size-64 blocks:
one page vs the baseline's 8192 KiB, G-003).
Unmet part: across `dlclose`, the comparison reverses.
Phase 3's zero-retention held in every dimension
because nothing was ever cached. Phase 7's retained
spans orphan at exactly 49,152 B per cycle (Section 4),
derived 640 KiB per cycle at full warming, with no
bound across cycles short of process exit. The exit
sentence's retained-memory wording carries no
"while loaded" qualifier, so X4 is PARTIAL until
H-001 lands.

### X5 — PARTIAL

Documented part: G-001's libc leg recorded arena
retention in the same probe shape (256-block batches:
retained-after-free 0 / 16 / 64 / 160 KiB at sizes
16 / 64 / 256 / 1024), and this packet's fresh run
reproduced those readings exactly. Against them,
Phase 7's in-process retention is smaller in kind: a
fixed 40-page ceiling at default, independent of churn
and of thread count by construction (RP-4; G-005
Section 3). Why not MET: the comparison rests on
process-level RSS on both sides (INC-G001-06,
INC-G005-02 — RSS cannot attribute bytes to the
allocator alone), no allocator-attributable instrument
exists for the platform side, and the unload dimension
of retention (Section 4) has no platform-allocator
counterpart measurement at all. What is missing for
exit grade is named in the register (O-14).

### X6 — PARTIAL

G-001 recorded that "responsiveness" had no metric
anywhere (INC-G001-03); G-008 defined one before
measuring, as instructed: Samtal `GET /` latency and
`/bin/true` fork+exec time, 15 samples per leg, baseline
vs synthetic churn (2 threads, 12 s). Banked record:
Samtal p50 0.896 ms baseline vs 0.996 ms under churn,
mean 1.537 vs 1.903 ms; `/bin/true` p50 and mean moved
down under churn; churn leg 14,795,488 ops, 0 failures;
60/60 Samtal samples HTTP 200. This packet's fresh run
reproduced the shape with its own numbers: Samtal p50
1.006 ms baseline vs 0.956 ms churn, mean 1.382 vs
1.253 ms; `/bin/true` p50 1.333 ms in both legs; churn
14,656,864 ops, 0 failures; 60/60 HTTP 200; post-churn
Samtal HTTP 200. Reading, carefully and as G-008 read
it: at this bounded churn level the medians show no
material responsiveness change in either direction,
and the largest spikes sit in the baseline legs — this
host has background noise of that order with or without
the allocator churning. **No latency bound is claimed**
(INC-G008-05: single-run observations on a shared,
Samtal-carrying home). The PARTIAL grade is for the
row's second half: repeated load/unload is a named
Phase 7 test element inside this row's evidence, and
its measured behaviour is the Section 4 defect. Device
responsiveness is documented and fine; the load/unload
behaviour it was measured alongside is not.

### X7 — MET

G-001 left the channel an open design question
(INC-G001-05). G-007 closed it without silent ABI
expansion: the RP-2 retention bound is one scalar under
the global lock, settable and readable through one
internal/test-only pair (`mimir_tune_set/get_retention_
bound`, INC-E001-07 shape (b)); valid range 0..16,
invalid values refused -1 before any state is touched
with the previous value kept; default exactly 2 (the
banked G-003/G-005 behaviour, re-proven by test-g005
passing unchanged against the tunable code); applies
at free time with no eager sweep (INC-G007-03, a
stated property). Fresh run: test-g007 132/0. Two
carries, neither of which lowers the grade as the
matrix words it: the range cap 16 is G-007's own
decision, which it asked G-009 to carry or re-decide —
**this packet carries it as decided** (worst case at
cap: 20 x 16 x page = 5,242,880 B on this host;
1,310,720 B at 4 KiB pages), recorded at O-06; and a
public tuning ABI remains a proposal for a future
contract slice (INC-G007-01), not a promotion.

## 6. Latency story (summary)

Phase 3 mapping-per-allocation: p50 4889-5167 ns,
size-independent (G-001, PROVEN on the Pi). Phase 7
small-object path: p50 148-253 ns at sizes 8..1024
(G-003 banked; fresh re-reading 148-241 ns). Platform
allocator, same shape: p50 56 ns (both legs, both
runs). Large path unchanged at the Phase 3 shape
(fresh reading 5889 ns at 4096). The Phase 7 design
bought a 20-35x latency improvement over its own
baseline and stands documented at ~2.6-4.5x the
platform p50 in this micro-probe shape. All readings
are single-session micro-probe readings on a shared
host (INC-G003-10); they are the exit documentation
the roadmap asks for, not a Phase 11 benchmark result.

## 7. Retained-memory story (summary)

Phase 3: 32 KiB mapped per live allocation on this
16 KiB-page host, 0 retained after free (G-001).
Phase 7 while loaded: spans pack small objects into
one-page spans (256 live size-64 blocks = one page,
G-003); retention is the deliberate, bounded price of
reuse — RP-1..RP-5 (G-005): at most 2 empty spans per
class retained, released whole and synchronously at
free time beyond the bound, no per-thread state, hard
ceiling 655,360 B on this host, proven reached-exactly
and never exceeded, stable under churn, and unchanged
by idle threads (G-005) or by thread exit (G-006).
Platform allocator, same shape: arena retention equal
to its live delta (to 160 KiB at size 1024), no fixed
ceiling of this kind (G-001 leg, reproduced fresh).
Phase 7 across unload: **the Section 4 defect** —
retained spans orphan at 49,152 B per cycle for
3 warmed classes, derived 640 KiB per cycle at full
RP-5 warming, until process exit (G-008, reproduced
byte-exactly by this packet's fresh run). H-001 is
the commissioned remedy.

## 8. Responsiveness story (summary)

Metric defined by G-008 (discharging INC-G001-03):
Samtal `GET /` and `/bin/true` fork+exec, baseline vs
2-thread synthetic churn for 12 s, 15 samples per leg.
Two independent records now agree in shape (banked
G-008; this packet's fresh leg — numbers in X6):
Samtal stays at p50 ~0.9-1.0 ms in both legs, 60/60
samples HTTP 200 in both runs, churn completes
~14.7M ops with 0 verify failures in both runs, and
baseline spikes exceed every churn-leg median shift.
No responsiveness bound is claimed for Phase 7 exit;
the numbers are single-run observations on a working
home, stated as observations. Probe coverage is the
Samtal read path only (INC-G008-06).

## 9. Carried-owed register

Nothing in this register is silently dropped. H-001
heads it by commission. "Closed here" means this
packet itself discharges the item, with the reason
stated.

| # | Owed / carried item | Source | Future home / trigger |
|---|---|---|---|
| **H-001** | **COMMISSIONED FIX — unload drain for retained spans (INC-G008-02).** Scope: add a destructor (or an explicit shutdown entry point, if a contract decision prefers it) that, at `dlclose`, releases every retained empty span through L6 before the image unmaps, under the existing global lock discipline. Must also: decide the INC-G008-03 posture for blocks still live at unload (document the consumer duty, or define the shutdown semantics); re-examine INC-G006-03 (a registered destructor creates the destructor-order surface G-006 proved absent). Evidence test, required: re-run the G-008 harness shape (50 cycles, same workload) and record anonymous rw growth of **exactly 0 B per cycle** and `img_after_close=0` throughout, plus `make test` and test-g005 green at the new bound behaviour. Until H-001 banks, X4 and X6 stay PARTIAL and Phase 7 stays unexited | INC-G008-02; INC-G005-03 (load/unload face) | **Head of the Wave 8 queue**, first slice, ahead of Phase 8 content |
| O-01 | Live allocations at `dlclose` are unrecoverable (span and contents survive; no defined free path exists) | INC-G008-03 | Decided inside H-001's scope (above) |
| O-02 | Destructor-order interaction: none exists today by construction; H-001's destructor reopens the question | INC-G006-03 | Re-examined inside H-001 |
| O-03 | Fork/exec: inspection-covered only, never executed; the single-global-lock fork hazard stands (child may inherit a locked mutex); header makes no fork claim (U-17..U-21 open) | INC-G006-02 | A fork slice with an explicit contract decision |
| O-04 | Thread cancellation never exercised; POSIX reading only | INC-G006-04 | Roadmap Phase 8 (cancellation and teardown are named there) |
| O-05 | Thread evidence scale bound: <= 8 concurrent threads, ~1.8k allocations; no hundreds-of-threads claim | INC-G006-05 | Phase 8 stress work |
| O-06 | Tuning range cap 16: **carried as DECIDED by this packet** (G-007 asked G-009 to carry or re-decide); ceiling arithmetic at O-06 of Section 5/X7. Revisitable only by a future tuning slice with reasons | INC-G007-02 | Closed here as a decision; the parameter itself stays tunable 0..16 |
| O-07 | Public tuning ABI: promote the proven setter/getter pair to a public surface only via a contract slice settling U-03 | INC-G007-01 | Future contract slice |
| O-08 | Tuning lock-failure paths (setter -2, getter sentinel) unexercised; no lock-failure injection hook exists | INC-G007-05; same posture INC-G005-06, INC-G004-07 | A fault-injection slice, which also discharges the RP-3 rollback branch and the R-1 backing-failure sub-case by execution |
| O-09 | Multithreaded tuning (setting the bound while other threads allocate) unmeasured; race-free by lock construction only | INC-G007-06 | A measurement leg in a future responsiveness or tuning slice |
| O-10 | Stale bound name tidy: `tests/core/test_g005.c` line 18 (verified present at this base by this slice) and the G-005 doc still name the retired constant `MIMIR_MAX_EMPTY_SPANS_PER_CLASS`. Cosmetic only. This packet may not fix it: the packet scope guard allows exactly two files, and editing banked evidence files is outside it | INC-G007-08 | The keeper, or the next slice that touches test_g005.c for cause |
| O-11 | Stale citation tidy: `tests/core/test_g004.c` line 63 (verified present at this base by this slice) cites INC-G004-02 for the span backing-failure code audit; the audit is G-004 doc Section 2 | INC-G004-06 | The next slice that touches test_g004.c for cause |
| O-12 | No idle-time reaper, in-process face: a warmed class keeps its 2 retained spans for the life of the process (bounded by RP-5). Stated property, by design; its unload face is H-001 | INC-G005-03 | Roadmap Phase 9 (trimming and cache release are named there) |
| O-13 | Responsiveness numbers are single-run observations; a tighter statement needs repeated runs, ideally at a quieter hour. Samtal write paths and long-lived connections under churn uncovered | INC-G008-05; INC-G008-06 | Any future slice that wants a bound; not required by the exit sentence as graded here |
| O-14 | Exit-grade retained-memory comparison vs the platform allocator: needs an allocator-attributable instrument for the platform side; RSS-only readings are what exist (X5) | INC-G001-06; INC-G005-02; INC-G001-02 (retained half) | A future measurement slice; Phase 10 counters are the natural instrument on the mimir side |
| O-15 | Runtime size histogram still unmeasured: the class table is census/grid-derived, not distribution-derived; any future table revision claiming workload evidence needs it | INC-G002-01, -02, -03; INC-G001-01 | Phase 10 counters or Phase 12 Aesir traces (the real workload family) |
| O-16 | Shrink-across-class retained-slack cost under churn unmeasured; two-page span for class 1024 evaluated and not adopted (928 B tail waste per span stands); free-path O(free-list length) cost noted, bitmap optimisation named if ever indicted | INC-G002-06, -07; INC-G003-06, -07, -08 | Phase 11 performance research, with evidence |
| O-17 | B-008 "extent" wording reconciliation (mapping usable extent vs class extent) owed to the next B-008-touching slice | INC-G002-08; INC-G003-09 | Next B-008-touching slice |
| O-18 | Alignment floor dependency: 16 B floor proven for glibc/AArch64 only; D-008 musl leg BLOCKED, bionic leg OWED; U-15 open at contract level | INC-G002-10 | The D-008 legs' own resumption points |
| O-19 | Audit counters unobservable through the `.so` by design (shape (b)); dlopen-side verdicts rest on maps/RSS evidence | INC-G008-01 | Standing design fact; changes only with a contract decision on counter visibility |
| O-20 | Function-pointer instability across dlclose measured (same address in 4 of 49 later cycles, both runs' shape); nothing may be cached across a close | INC-G008-04 | Standing consumer discipline; no allocator action owed |
| O-21 | Signal-context allocation/exit: outside the banked posture, no claim made or owed by Wave 7 | INC-G006-06 | Contract-level only (MA-6.3-030..032) |
| O-22 | **Phase 3 exit UNMET**: the x86-64 suite run is still OWED | C-009 packet, carried by G-001 and F-009 O-03 | An x86-64 sysroot + suite run; a 4096-page environment run |
| O-23 | **Phase 4 exit UNMET**: physical-device allocate/fill/free + multithread tests on Android still OWED | E-009 O-16, carried by G-001 and F-009 O-02 | An Android NDK (install reserved for Volmarr's word), an NDK build, and a physical device run |
| O-24 | **Phase 1 exit unmet; Phase 6 Clause A UNMET / X1 BLOCKED** (blocker-record alternative only) — restated by G-001's header and carried unchanged; nothing in Wave 7 discharges any of it | G-001 header; F-009 Sections 6-8 | Their own recorded resumption points |

## 10. Bottom line

Phase 7 built what the roadmap asked it to build:
measured size classes (G-002), bounded local reuse
with central fallback (G-003, audited G-004), a
reclaim policy with an exactly proven bound (G-005),
thread-exit handling proven against a design with no
per-thread state to drain (G-006), tuning parameters
configurable, validated and visible (G-007), and a
device multitasking + repeated load/unload record
(G-008). Correctness did not regress (X1, fresh-run
re-confirmed). Latency and retained memory are
documented against both Phase 3 and the platform
allocator, and responsiveness is documented against a
metric the wave defined before measuring (X2, X3,
X7 MET; X5, X6 documented with named limits).

The exit is nevertheless **PARTIAL, not clean**,
because of the one finding the wave itself surfaced:
retained memory that is bounded and proven while the
library is loaded becomes unbounded per-cycle leakage
across `dlclose` (INC-G008-02, Section 4). The roadmap
sentence does not offer a blocker-record alternative
for Phase 7, and this packet does not invent one. The
honest verdict is: **Phase 7 exit PARTIAL — X4 and X6
PARTIAL on the unload dimension — with the remedy
already commissioned as H-001 at the head of the next
wave.** When H-001 banks with its zero-growth harness
record, X4 and X6 can be re-graded on that evidence
and the exit declared; until then it is not.

Carried debts from earlier phases stand exactly as
G-001 restated them (O-22..O-24): Phase 3 exit UNMET,
Phase 4 exit UNMET, Phase 1 exit unmet, Phase 6 met
via the blocker-record alternative only.

## 11. Inconsistencies recorded by this slice (INC-G009-NN)

- **INC-G009-01 (the exit verdict depends on the
  unload reading, stated openly).** The exit sentence
  does not qualify "retained memory" by load state.
  This packet reads it unqualified — retention is
  retention, loaded or unloaded — and therefore grades
  X4/X6 PARTIAL and the exit PARTIAL. A reader who
  scopes the sentence to in-process behaviour alone
  would grade X4 MET and X6 MET on the same evidence
  and declare the exit; this packet records that the
  difference is a reading, names its own reading, and
  notes that H-001 removes the divergence either way.
- **INC-G009-02 (INC-G008-02 is deterministic, fresh
  evidence).** This packet's fresh test-g008 run
  reproduced the banked orphan numbers byte-exactly
  (98,304 B after cycle 0; 2,506,752 B after cycle 49;
  +49,152 B per cycle; RSS delta +2464 KiB in both
  runs). The defect is a structural property of the
  unload path, not contention noise on a shared host;
  recorded so no later slice re-opens it as a
  measurement question. The fix question is H-001.
- **INC-G009-03 (two tidies owed to G-009 could not be
  discharged by G-009).** INC-G007-08 and INC-G004-06
  both name G-009 or "a future slice" as a possible
  home, but this packet's scope is exactly two files
  (this doc + the recorded run) with no test-file
  edits; both items are verified still present at
  this base (Section 9, O-10/O-11) and carried to the
  keeper / next touching slice rather than fixed out
  of scope or silently dropped.
- **INC-G009-04 (fresh probe numbers differ from
  banked ones, by design of the evidence).** The fresh
  run's responsiveness and churn counts (Samtal p50
  1.006/0.956 ms; churn 14,656,864 ops) differ from
  G-008's banked record (0.896/0.996 ms; 14,795,488
  ops). Both are single-run observations on a shared
  host (INC-G008-05); the packet cites each run's
  numbers only for that run and claims agreement in
  shape, never identity of values.
- **INC-G009-05 (X5's evidence grade is the weakest
  in the tally, recorded).** Every other row rests on
  an instrument that measures the allocator directly
  (counters, per-op clocks, maps). X5's platform side
  rests on process-level RSS with the INC-G001-06
  caveat and has no unload dimension; it is graded
  PARTIAL for that reason, and O-14 names what an
  exit-grade version would need.

## 12. Files of this slice

- `docs/PHASE_7_EXIT_EVIDENCE.md` — this packet (new).
- `tests/core/recorded_run_g009.txt` — the fresh run
  (new, 1631 lines).

Exactly two files, one commit on `yrsa/mimiralloc-09`
with parent `31308c6`. No Makefile change, no `src/`
change, no test change. `README.md` and
`include/mimir_alloc.h` byte-identical to base,
verified in the recorded run. No push, no merge — the
keeper banks last and completes Wave 7.
