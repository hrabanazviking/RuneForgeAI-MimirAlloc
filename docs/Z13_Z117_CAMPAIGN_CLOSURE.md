# Z13 Z-Line Campaign Closure

Slice: Z-117 - Campaign closure (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-13 by team-03. The six Mythic Engineering roles were run
SEQUENTIALLY by the lead itself (evidence sweep, standings re-grade,
Wave 13 pool review, filings and owners review, plain-language
summary, gate and commit-scope review) - the finishing lead mustered six role agents (acknowledgements
only; every fact below was verified on-device by the lead) and
ran the roles itself, recorded plainly here and in
CAMPAIGN_STATE.md.
Base: merge 8940600 of seat main 627b1fa (banked Z-001..Z-116) into
yrsa/mimiralloc-team-03 - clean merge, no conflicts; the merged tree
was verified identical to the seat before any Z-117 edit (git diff
627b1fa HEAD empty apart from this document once written).
Baseline standing gate on the merged tree before any Z-117 edit:
GREEN 4988 checks / 0 failed / 77 by-design skips, exit 0
(hidden_files/z117_baseline_gate.log). Final gate on the slice
tree likewise GREEN (hidden_files/z117_gate.log).
This slice derives no new measurement. Its only fresh executions
are the baseline and final standing gates and the git sweeps
recorded in section 9. It adds one document and no code, no
tests, and no Makefile change.

STATUS: CLOSING SLICE. Sections 2 and 3 state the standings of
Z-001..Z-116 as FINAL from their banked records, re-read
first-hand from disk on the merged seat tree by this lead.
This document is the closing slice itself: when it banks, the
banked count from git is 117/117 plus the founding commit
(5ad882b), and the keeper verifies that count by ls-remote and
completes the seat-local stand-down entries (WAVE_PLAN.md,
PROJECT_MEMORY_MAP.md, the Current State board). The Z-108
verdict (docs/Z13_CAMPAIGN_VERDICT.md) is not overwritten and
remains the verdict of record through Z-108; this packet is
its successor for the whole Z line, Z-001..Z-117.

## 1. Scope and honesty header - read first

- This machine only: purpleraider (ASUS ROG Flow Z13, AMD
  Ryzen AI MAX+ 395 / Strix Halo, x86-64, 4 KiB pages, 128 GB
  UMA). Every number in this packet was measured on this host
  and traces to a banked recorded run or banked slice document
  in this tree. No number from any other machine is evidence
  here.
- This line only: the Z13 line (Z-001..Z-117, 13 waves of 9).
  The original MimirAlloc campaign (117/117 on a 16 KiB-page
  aarch64 Raspberry Pi) is separate, complete history. Its
  numbers were this line's questions, never its evidence, and
  nothing in this packet re-grades that line.
- Honest grading (roadmap Law 2): every standing below is
  stated in the campaign's own grades - WIN, TIE, LOSS (with
  the size of the loss), or STILL LOST - REASON NAMED. A proven
  loss is a banked result, not a failure of the campaign.
- The standing correctness gate never moved: 4988 checks /
  0 failed / 77 by-design skips at the founding baseline
  (Z-001) and at every banked slice through Z-116, including
  under ASan, UBSan and ASan+UBSan with zero sanitizer reports
  (Z-104). Roadmap Law 5 held: no speed slice traded away
  bounded behaviour, audit surfaces, or the no-silent-
  corruption posture without a recorded decision; the trades
  that were made are decisions D-007..D-017 in
  docs/DECISION_LEDGER.md, each with its evidence.
- Load honesty is inherited from the banked records: each
  standing carries the window its source run recorded
  (quiet, co-tenant, or shared). Numbers measured under
  cross-campaign contention were never promoted to baselines
  by their own slices, and this packet does not promote them
  either.

## 2. Final standings - what was won, tied, and lost vs glibc

The standings of record are the Z-107 closing head-to-head
(banked at seat 85b856f, run of record
tests/perf/recorded_run_z13_z107.txt, 2026-11-13, co-tenant
window with controls passing), as carried FINAL by the Z-108
verdict and re-confirmed in direction by every Wave 13 slice
that re-measured a leg (Z-113, Z-114, Z-115). No Wave 13 slice
moved a headline grade; where a Wave 13 number differs in
detail it is filed under section 3, not silently substituted
here.

### 2.1 Latency - the oracle (bench_k002, Z-107 closing table)

Against DEFAULT glibc - LOSS on three of four:
- small_churn: mimir 29.43 vs glibc 3.35 ns/op - LOSS 8.79x.
- mixed_batch: mimir 29.12 vs glibc 3.72 ns/op - LOSS 7.83x.
- large_batch: mimir 98.45 vs glibc 710.47 ns/op - WIN 0.14x.
- mixed_lifecycle: mimir 531.01 vs glibc 7.12 ns/op -
  LOSS 74.58x.
Against the BEST TUNED glibc per workload - LOSS on all four:
- small_churn: LOSS 8.79x (29.43 vs 3.35).
- mixed_batch: LOSS 7.85x (29.12 vs 3.71).
- large_batch: LOSS 10.84x (98.45 vs 9.08) - with its cache
  raised, tuned glibc runs this workload about 11x faster
  than MimirAlloc; the default-glibc WIN does not survive
  the fair fight (Z-098, confirmed at the close by Z-107).
- mixed_lifecycle: LOSS 168.04x (531.01 vs 3.16).
Against its own Z-009 founding baseline the final core
improved on every workload (-68.5% small_churn, -92.7%
mixed_batch, -96.8% large_batch, -40.9% mixed_lifecycle)
and still loses three of four to default glibc. Counters
grade the same pattern (Z-107): large_batch cycles and
instructions WIN 0.15x against default glibc and LOSS
4.39x / 4.30x against tuned; every other counter cell is
a LOSS.

WON (latency, exactly one cell): repeated large allocations
served from the mapping pool beat default glibc roughly
seven- to tenfold (0.14x at Z-107; ~0.10x indicative at
Z-054). Traced to Z-048 (bounded exact-length mapping pool,
64 mappings / 32 MiB, removing 97.6% of the mapping-path
price on cycling traffic), graded at Z-054, Z-098 and Z-107,
and re-confirmed under a GPU co-tenant at Z-113 (WIN 0.167x
baseline phase, 0.133x under load - the WIN widens under
load because glibc pays proportionally more, +49.9% vs
mimir +19.5%).
TIED (latency): none on the four oracle workloads against
either glibc column at the close. Ties live elsewhere -
see 2.4.
LOST (latency): small_churn, mixed_batch and
mixed_lifecycle against both columns; large_batch against
tuned glibc. The loss classes are graded in 2.2.

### 2.2 The five loss classes (Z-108 section 2, FINAL, plus Wave 13)

KL-A - single-thread small-op latency (small path vs glibc
tcache). STILL LOST - REASON NAMED. Closing grade LOSS
8.79x (Z-107). The autopsy (Z-027) named the wound:
instruction count, not cache behaviour - 861.97 vs 82.99
instructions/op at baseline (Z-005), free the dearer small
entry (Z-019). Wave 4's TLS cache (Z-028/Z-029), LTO
(Z-035) and -O3 (Z-096) cut the absolute from 93.43 to
~29 ns/op; no banked run ever brought the grade inside
the tie band. Wave 13 re-tested the last tuning road:
Z-111's per-class bin candidate removed the oracle
regression entirely and still failed its trace bar
(section 3) - bin depth is not the wound. Two geometry
defects named at Z-017 remain open and are carried in
section 4: the 48 B class splits cachelines on 49.41% of
blocks and the 64 B class on 100% (a carve-offset,
span-header item no slice took).
KL-B - multithread scaling under the one global lock.
STILL LOST - REASON NAMED, SHAPE CHANGED. Before: inversion
at 2 threads, scaling 0.043 / 0.033 of solo at 32 threads
(Z-020). After the TLS cache: no 2-thread inversion, honest
scaling to one CCD, then a cross-CCD cliff at 12 threads
(Z-045). Against glibc: LOSS at every rung 1..32 on both
ladder workloads (Z-021). The per-CCD arena partition is
TIE vs the banked core at 21 of 24 rungs with a top-end
mixed_batch WIN only (Z-038); lock-sharding was chosen by
Z-039's R2 rule and banked (decision D-008) - no banked
verdict run re-graded the assembled scaling curves after
it, and none is claimed here. The cross-free flood prices
the surviving wound: direct remote free LOSES to glibc at
every percentile (Z-043); the Z-030 remote-free queue was
proven and priced (freer-side WIN, pair total LOSS) and
was never wired into mimir_free.
KL-C - large allocations. DISCHARGED for repeated (pool-
hit) traffic; STILL LOST on miss traffic - REASON NAMED.
The pool WIN is 2.1's won cell. On a pool miss (first,
unique, or over-bound sizes) the full fresh-mapping price
stands, the ~80x cliff at 1025 B stands with it, and
untouched huge alloc/free pairs stand at LOSS 1.10-1.22x
(Z-051). The cutoff of record never moved: 1024 bytes
(Z-054). Wave 13 priced the miss path directly: Z-112's
huge-page variant WINS every miss shape measured
(touched total -76.85% at 2 MiB to -84.32% at 40 MiB,
faults collapsing from one per page to one per PMD) but
was NOT ADOPTED as a pool replacement, because it evicts
the pool-hit traffic the WIN rests on (witness regression
488x-1386x). The follow-on it names is carried in
sections 4 and 5. Supporting standings: THP ADOPTED on
pooled mappings at usable sizes >= 2 MiB, rejected below
(Z-049, discharging Z-018's owed row); huge allocations
TIE 0.995..1.018 fully touched, 256 MiB..8 GiB (Z-051);
realloc growth by mremap TIE 1.004x vs glibc, from 3.77x
LOSS (Z-052); pool retention cost is a bounded LOSS on
holed RSS patterns (Z-053).
KL-D - mixed lifecycle. STILL LOST - REASON NAMED (fast
half); slow half DISCHARGED. Fast half: LOSS 74.58x
against default glibc and 168.04x against tuned at the
close (Z-107) - the deepest standing wound of the line.
It exercises every path at once, and the paths it
exercises most (the small path, KL-A; lifecycle
bookkeeping across classes) were never rebuilt to glibc's
per-op price. Slow half: one hour (Z-057) and then eight
hours (Z-100) of its own workload show no drift, no leak,
no fragmentation growth - resident shape is a WIN
(net_factor median 1.988 vs glibc 2.500 at one hour;
1.895 vs 2.125 at eight hours), retention bounded and
returned to exactly 0 under pressure cycles (Z-060).
Placement cannot soften the shape: best-fit LOSS,
first-fit TIE, DECISION D-009 NO-ADOPTION (Z-059). The
fragmentation mechanism is architectural - class-locked
free bytes, no split/coalesce below the page, whole-page
release the only healing (Z-055, Z-058).
KL-E - batching. CLOSED as a LOSS - REASON NAMED. The
dominant factor is the live-set metadata walk: per-op
cost grows with live set because every alloc and free
pays linear registry/span walks (Z-091; walk priced at
~82-92% of free cost at deep live sets). Caller-side
batching at burst512 loses 11.94x (64 B) and 84.66x
(1024 B) against its own interleaved shape (Z-093); the
Z-092 batch API prototype TIES the per-op loop and
therefore LOSES as a batching answer (it removes no
walk). Proposal P-1 is CLOSED, reaffirmed by two slices.
Z-111 re-confirmed the same wall from the trace side:
at canonical live-set scale (508-594 MB peak live) per-op
cost is dominated by live-set effects, which is why no
bin variant could pass its trace bar.

### 2.3 Real-program standings (not the synthetic oracle)

- Real traces (Z-094): the three canonical traces (python
  import storm, git object churn, Ollama session) replay
  MATCH on both legs - 2,987,619 ops, 0 failures: as
  correctness artefacts a TIE. On replay speed MimirAlloc
  stands at 343.63 / 558.04 / 2907.94 ns/op (git / ollama /
  python) against glibc 31.58 / 16.66 / 12.37 (Z-095) - the
  same small-path LOSS.
- Preload on real programs (Z-085, quiet window): Python
  total LOSS 5.01x, Python peak RSS LOSS +11.5%; Ollama
  model load LOSS ~22-24x per cold load; Ollama generation
  TIE. Correctness PASS everywhere tried (section 2.5).
- Short-lived processes (Z-097): the interposition tax is
  a LOSS on every shape: +16.98% (never allocates) to
  +130.25% / 2.30x (1000-cycle working process).
- Batch shapes (Z-093): batching LOSS at depth (KL-E).
- KV-cache shapes (Z-115, Wave 13): LOSS on every shape
  against both glibc columns - kv_append 4.573x / 4.906x,
  kv_blocks 260.919x / 395.744x, kv_mixed 3.852x / 3.850x
  (default / tuned). No specialisation adopted.

### 2.4 What was TIED (each traced to its banked slice)

- Huge allocations fully touched, 256 MiB..8 GiB: TIE
  0.995..1.018 (Z-051).
- Realloc growth by mremap: TIE 1.004x vs glibc (Z-052).
- - First-fit placement: TIE (Z-059; best-fit LOSS, D-009
  NO-ADOPTION).
- Per-CCD arena partition vs the banked core: TIE at 21 of
  24 rungs, top-end mixed_batch WIN only (Z-038).
- Real-trace replay correctness: MATCH on both legs,
  2,987,619 ops, 0 failures (Z-094) - a TIE as correctness
  artefacts (speed is a LOSS, section 2.3).
- Ollama generation under preload: TIE (Z-085).
- Aesir engine correctness under preload: TIE, identical
  token streams (Z-087).
- THP fault behaviour at adopted sizes and the pooled-
  mapping standings of Z-049/Z-051 as recorded there.

### 2.5 Correctness and non-latency standings (FINAL)

- The contract was not sold: standing gate 4988 / 0 / 77 at
  every banked slice Z-001..Z-116; ASan, UBSan and both
  together report zero sanitizer findings (Z-104).
- Corruption: 19 of 25 J-006 probes DETECTED (Z-072); at
  scale, 0.0% false positives, descriptor flips DETECTED
  96.7% single-bit and 100% consistent-pair, span flips 0
  silent among completed trials (Z-103). Two silent paths
  filed at Z-088 (F-Z088-1, F-Z088-2) and stand as filed.
- Resident memory at the close (Z-107): PEAK Pss LOSS on
  small_churn (+333.0%), mixed_batch (+542.5% / +531.6%)
  and mixed_lifecycle (+269.3% / +272.7%); large_batch PEAK
  the only TIE (+3.5% / +3.2%). AFTER_FREE_ALL LOSS nearly
  everywhere (large_batch +2519.9% vs default, TIE vs
  tuned). The founding after-free WIN (Z-006) does NOT
  survive to the final core - the honest regression of the
  close, in Z-107 own words. The soak resident-shape WIN
  (KL-D slow half) is a different workload and stands.
- Energy at the close (Z-107, co-tenant window):
  small_churn LOSS 2.84x / 3.26x net nJ/op; large_batch WIN
  0.20x vs default glibc; three cells ungraded (reference
  below idle-subtraction resolution). The quiet-window
  re-run is Z-110, still open (section 4).
- Public surface: 13 namespaced exports on the default
  library, ABI epoch 0, surface still Proposed; 1.0
  withheld (Z-105, D-017). The opt-in interposing variant
  (20 exports) is CORRECT everywhere tried and SLOW where
  graded, with one known break (D-013 zero-size vs glibc
  anchored regcomp, T-084-1). The nine-entry never-preload
  list is ops law (Z-089).

## 3. Wave 13 one by one (Z-109 .. Z-116, all FINAL)

Wave 13 was verdict-driven reserve: each pool choice was
recorded in WAVE_PLAN.md before its slice started. No Wave
13 slice moved a headline Z-107 grade; each is FINAL from
its banked doc and recorded run in this tree.

- Z-109 API fuzz at scale - PASS. 530,000 stream ops,
  72,860,018 checks, 0 failed; 13/13 public exports covered;
  734 injected mmap refusals on the acquire channel with
  disarmed-retry discipline; no crash, hang or misbehaviour;
  no allocator defect found. D-09 discharged for the acquire
  channel only. Filed: INC-Z109-01/02/03 (section 4).
  (docs/Z13_Z109_API_FUZZ.md; tests/fuzz/recorded_run_z13_z109.txt)
- Z-110 Quiet-window energy re-run - BLOCKED-CONTENTION,
  NO GRADE FILED. Attempt 1 blocked by the quiet gate;
  attempt 2 ran contended in fact (sibling Z-111 probe at
  ~100% CPU inside the window), so its grades are preserved
  verbatim in the recorded run and NOT ADOPTED. Z-107 energy
  grades remain the grades of record. STILL OPEN (section 4).
  (docs/Z13_Z110_QUIET_WINDOW_ENERGY.md;
  tests/perf/recorded_run_z13_z110.txt)
- Z-111 Profile-guided class table v2 - NO-ADOPTION.
  Z-095 bars applied unchanged against the banked canonical
  Z-094 traces: v2 passes the oracle clause (worst -0.10%
  stage 2, +1.01% recheck) and keeps most of the canonical
  ollama win (-7.93%), but improves only 1 of 3 traces past
  the 3% bar, so the trace clause FAILS. v1 and wA also NOT
  ADOPTED. Adopted tuning: NONE; bin 16 / batch 8 stand.
  Cause named: at canonical live-set scale (508-594 MB peak
  live) cost sits in the live-set machinery, not bin depth.
  (tests/perf/recorded_run_z13_z111.txt)
- Z-112 Huge-page-only large-pool variant - miss-shape WIN,
  NO-ADOPTION as a replacement. Touched total improves
  76.85% (2 MiB) to 84.32% (40 MiB); faults collapse from
  one per page to one per PMD; untouched pairs no regression.
  Adoption fails on the pool-hit witness: fresh price vs
  cached reuse is 488x-1386x on the banked pool home traffic.
  Follow-on named: PMD-align and huge-advise FRESH mappings
  on the miss path, keeping the exact-length pool (section 4).
  (docs/Z13_Z112_HUGE_PAGE_POOL.md;
  tests/perf/recorded_run_z13_z112.txt)
- Z-113 GPU-shared UMA pool behaviour - standings UNCHANGED
  in direction under a real GPU co-tenant (ollama qwen2.5:7b
  generation, gpu_busy 94-98%): small_churn LOSS 8.81x ->
  9.19x, mixed_batch LOSS 7.56x -> 7.83x, large_batch WIN
  0.167x -> 0.133x, mixed_lifecycle LOSS 58.6x -> 58.9x.
  Both allocators degrade on every workload; on large_batch
  glibc pays proportionally more (+49.9%) than mimir
  (+19.5%), so the WIN widens under load. Shared host, not a
  quiet window; one co-tenant shape only.
  (docs/Z13_Z113_GPU_SHARED_UMA.md;
  tests/perf/recorded_run_z13_z113.txt)
- Z-114 Perf-per-watt at the 70 W envelope - WIN (tuned vs
  untuned). Frequency cap 3.5 GHz (the only working lever;
  RAPL is measurement-only here, EPP writes refused EBUSY).
  Perf-per-watt 1.914x / 1.722x / 2.255x / 1.948x on the
  four oracle workloads; tuned batches held the envelope
  (medians 28.0-38.2 W) where untuned exceeded it
  (72.5-80.9 W). Cost: throughput fell 25-27% - a ceiling
  trade, not a free win. Co-tenant window; Z-110 NOT
  discharged by this slice.
  (docs/Z13_Z114_PERF_PER_WATT_70W.md;
  tests/perf/recorded_run_z13_z114.txt)
- Z-115 Aesir KV-cache-shaped specialisation - LOSS on
  every shape vs both glibc columns: kv_append 4.573x /
  4.906x, kv_blocks 260.919x / 395.744x, kv_mixed 3.852x /
  3.850x (default / tuned). No specialisation adopted.
  B-087-1/2/3 re-checked on unchanged artifacts: ALL THREE
  STILL STAND (section 4). Synthetic shapes, low-load
  window, single-threaded.
  (docs/Z13_Z115_KV_CACHE_AESIR.md;
  tests/perf/recorded_run_z13_z115.txt)
- Z-116 Energy-proportional idle behaviour - STABLE and
  TRIM-RESPONSIVE, NOT autonomous. After a 49.7 MiB load and
  free-all, retained footprint (229,376 B spans, 2,727,936 B
  mapped, RSS +4456 KiB) is exactly flat over a 120 s idle
  window (drift +0 over 12 samples); retained-idle package
  watts (7.23 / 6.62 W) sit below the pre-load sample - no
  measurable idle power cost at this resolution. One
  explicit mimir_trim(0) releases everything to 0. Idle
  return is opt-in by the Z-050 caller-driven design;
  observation filed with owner (section 4). Z-110 remains
  OPEN and is not discharged here.
  (docs/Z13_Z116_ENERGY_PROPORTIONAL_IDLE.md;
  tests/perf/recorded_run_z13_z116.txt)

## 4. Carried-open-items ledger (owners; nothing discharged that is not)

Every item below was re-checked against its banked record
on this tree by this lead. Carried means carried: no item
here is claimed fixed, closed or discharged by this slice.

- Z-110 quiet-window energy hand-off - STILL OPEN. Owner:
  the next quiet-window energy slice on this line (one
  unchanged invocation of tests/perf/run_z110.sh in a
  verified quiet window). Z-114 and Z-116 both state
  plainly that they do not discharge it. The Z-107 energy
  grades remain the grades of record until it runs.
- F-Z105-1 - interpose export gap, OPEN. The interposing
  variant still carries the pre-Z-083 10-name namespaced
  set; mimir_memalign / mimir_valloc / mimir_pvalloc and
  their standard-name compositions are absent from its
  exactly-20 exports. Owner: the production roadmap queue,
  as a gap-closure slice with its own decision entry on
  D-015 exactly-20 contract (via Z-108 section 8 hand-off).
- F-Z105-2 - stale Pi 0.1.0 release record, OPEN until the
  cut. Owner: the D-017 0.2.0 cut checklist, which
  regenerates REPRODUCIBLE_BUILD.md pins and SHA256SUMS.
- F-104-1 - clang portability LOSS, OPEN. clang 22.1.8
  cannot compile mimir_core.c (122 __atomic call sites);
  the shipped gcc build is unaffected. Owner: the next
  core-touching portability slice (PBR-053 line).
- INC-Z109-01 - fuzz injection channels release / lock /
  page-size compiled but never armed; only acquire (D-09)
  was discharged. Owner: the next fuzz slice on this line;
  failing that, this hand-off carries it to the production
  roadmap queue.
- INC-Z109-02 - MT fuzz covers concurrent streams, cross-
  thread frees and inbox transfers, not arbitrary
  unsynchronised interleavings over shared blocks. Owner:
  a soak-class follow-on (M-005 territory) if commissioned.
- INC-Z109-03 - the undefined-behaviour surface is never
  invoked by design; no verdict is claimed there.
  Corruption detection rates remain Z-103 territory.
  Owner: none - a stated scope boundary, not a defect.
- Z-112 miss-path follow-on - OPEN, named not built.
  PMD-align and huge-advise FRESH large mappings on the
  pool-miss path while keeping the banked exact-length
  pool for hit traffic (priced miss win 76.85-84.32%).
  Costs on record: up to +2 MiB virtual padding per fresh
  mapping, one madvise per fresh mapping, a reserve-layout
  change in the core. Owner: a future large-path slice.
- Z-116 opt-in idle-return observation - OPEN by design.
  An application that never calls mimir_trim and never
  ticks the Z-050 policy holds the drained residue for its
  whole life (caller-driven design of record, no background
  thread). Owner: API consumers; any autonomous idle-return
  change is a future design slice, not a fix owed.
- B-087-1 / B-087-2 / B-087-3 - Aesir blockers STILL
  STANDING, re-checked at Z-115 on unchanged artifacts
  (identity reads only; no Aesir code ran): root aesir_main
  and aesir_engine/aesir_main binaries and the 24-byte
  model.gguf placeholder are as Z-087 found them. Owner:
  the Aesir campaign; this line only records the boundary.
- D-017 - the 0.2.0 cut is DECIDED, NOT EXECUTED. VERSION
  stays 0.1.0 in-tree until the cut; cutting a public
  release is Volmarr act. Signing deferred-by-absence,
  owner Volmarr. This slice does not cut anything.
- Earlier carried rigour items, still open (Z-108 section
  6, re-carried whole): the carve-offset geometry item
  (48 B class splits cachelines on 49.41% of blocks, 64 B
  class on 100% - KL-A); the Z-034 free-list liveness walk
  O(carve_count) in find_span_live_locked; Z-043 realloc-
  retire residual (strict finder can return NULL in the D2
  transient window, original preserved, retryable);
  INC-Z048-01 standalone-battery repairs (i003/h001/g003/
  h003/j006 red at the pristine baseline, outside the
  standing gate composition); Wave 8 section 8 suite-
  modernisation and the approved flag-gated MIMIR_CANARY
  stride variant for classes 320-1024 (unbuilt); F-Z088-1
  and F-Z088-2 silent-path filings from the failure drills.

## 5. What it would take to change the standings

Named by measurement, not promised: (1) the small path
needs its instruction count rebuilt toward glibc per-op
price - the one architectural road left standing is
span-grouped processing with one registry pass per span
(Z-091 brief), which also prices part of KL-D and KL-E;
(2) the fair fight is the honest target - any future speed
claim must grade against tuned glibc (Z-098 columns);
(3) the Z-112 miss-path follow-on takes the priced huge-
page win without evicting pool-hit traffic; (4) the quiet-
window energy grades need a verified quiet host (Z-110).
Nothing else tried moved the small-path grade: cache
(necessary, not sufficient), arenas (TIE), placement
(NO-ADOPTION), bin tuning (Z-111 NO-ADOPTION), flags
(-O3 adopted; gain real, grade unmoved).

## 6. Closing summary for Volmarr, in plain language

Volmarr - the Z13 line is done: all 117 slices are built,
measured and banked on your Z13, and this document is the
last one. Here is the honest shape of it.

What was won: one clear speed win. When a program asks for
large blocks of memory over and over, MimirAlloc serves
them from its pool about seven to ten times faster than
the system allocator in its everyday settings - and that
win gets bigger, not smaller, when the graphics chip is
busy sharing the same memory. Long-running programs also
hold a steadier memory footprint over hours than the
system allocator does, idle memory sits perfectly still
and comes back the moment it is asked for, and capping
the chip frequency nearly doubles the work done per unit
of energy while staying inside a 70-watt budget.

What was tied: very large one-off allocations, growing a
block in place, replaying real program traces without a
single error, and generating text with Ollama under the
preload library - on those, MimirAlloc matches the system
allocator.

What remains lost: ordinary small, fast memory work - the
everyday churn most programs do most of the time - is
still roughly eight to nine times slower than the system
allocator, mixed work is worse, and against the system
allocator at its best tuning even the large-block win
disappears. The campaign made MimirAlloc dramatically
faster than its own starting point on every workload and
proved, workload by workload, exactly why the remaining
gap is there: it does more bookkeeping per operation, and
no tuning trick measured here removes that. Correctness
was never traded for speed: the full test gate stayed
green at every single slice, and the fuzzing, corruption
and soak batteries all passed.

What is still open is listed in section 4 with owners -
most importantly the quiet-machine energy re-run, which
still needs a truly quiet window on the shared machine,
and the 0.2.0 release cut, which is yours to make and
nobody else. Nothing open is hidden, and nothing in this
packet claims more than the recorded runs prove.

## 7. Traceability index

- Standings of record: docs/Z13_CAMPAIGN_VERDICT.md (Z-108)
  over tests/perf/recorded_run_z13_z107.txt (Z-107 closing
  head-to-head, seat 85b856f); wave verdict docs
  Z13_WAVE01_BASELINE, WAVE03_AUTOPSY, WAVE04/05/06/07/08/
  10/11_VERDICT in docs/, each over its own recorded runs.
- Wave 13: docs/Z13_Z109_API_FUZZ.md, Z13_Z110_QUIET_
  WINDOW_ENERGY.md, Z13_Z112_HUGE_PAGE_POOL.md, Z13_Z113_
  GPU_SHARED_UMA.md, Z13_Z114_PERF_PER_WATT_70W.md,
  Z13_Z115_KV_CACHE_AESIR.md, Z13_Z116_ENERGY_PROPORTIONAL_
  IDLE.md, and tests/perf/recorded_run_z13_z111.txt -
  all present on the merged seat tree and re-read here.
- Decisions: docs/DECISION_LEDGER.md (D-007..D-017).
- Roadmap gate for this slice: docs/MIMIRALLOC_Z13_
  ROADMAP.md, Z-117 entry.

## 8. Correctness gate for this slice

Baseline standing gate on the merged tree before any Z-117
edit: GREEN 4988 checks / 0 failed / 77 by-design skips,
exit 0 (hidden_files/z117_baseline_gate.log). Final standing
gate on the slice tree (bash scripts/z13/run_gate.sh):
GREEN 4988 checks / 0 failed / 77 by-design skips, counts
read back from hidden_files/z117_gate.log. This slice adds
one document and no code, no tests, and no Makefile change;
the gate guards the tree the packet describes. Bugs
discovered during this slice: none in allocator code.

## 9. Stand-down checklist and git sweeps

Git sweeps by this lead on the merged tree, 2026-11-13:
seat /home/volmarr/mimiralloc-z13-forge HEAD 627b1fa equals
ls-remote origin refs/heads/main 627b1fa4c9863913f8e1d87782576abdc1cb07b7; unique slice subjects on the seat log are
exactly Z-001..Z-116, gapless (116 subjects), over the
founding commit 5ad882b (the Z13 roadmap commit). Team-03
base for this slice: merge 8940600 of seat 627b1fa.

For the keeper, when this slice banks:
[ ] Banked count from git = 117/117 plus the founding
    commit, verified by ls-remote (seat HEAD equals
    ls-remote refs/heads/main; unique Z subjects
    Z-001..Z-117 gapless, one slice per commit).
[ ] This packet committed as exactly one file,
    docs/Z13_Z117_CAMPAIGN_CLOSURE.md, subject
    Z-117: Campaign closure; no CAMPAIGN_STATE.md or
    partial file in the commit.
[ ] Standing gate GREEN 4988 / 0 / 77 on the seat tree
    after the bank, counts read from the seat gate log.
[ ] WAVE_PLAN.md and PROJECT_MEMORY_MAP.md carry the
    Z-117 entry and the campaign-completion line
    (seat-local bookkeeping, never banked).
[ ] Current State board banked count updated to 117/117.
[ ] Section 4 filings each carry a named owner into the
    production roadmap queue or Volmarr hands - none
    silently dropped; the Z-110 hand-off stays OPEN.
[ ] The keeper stands down at this bank: the campaign is
    complete at 117/117. The D-017 0.2.0 cut remains
    Volmarr act and is not part of stand-down.
