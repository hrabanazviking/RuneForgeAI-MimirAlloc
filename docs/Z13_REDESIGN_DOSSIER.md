# Z13 Redesign Dossier

Slice: Z-026 - Redesign dossier (Wave 3, the single-lock autopsy)
File: docs/Z13_REDESIGN_DOSSIER.md
Campaign: MimirAlloc Z13, purpleraider (ASUS ROG Flow Z13, Strix Halo)
Roadmap: docs/MIMIRALLOC_Z13_ROADMAP.md, slice Z-026
Decision of record: docs/DECISION_LEDGER.md, entry D-007

## Purpose and gate

Roadmap Z-026: "Candidate architectures (TLS freelist cache;
per-CCD arenas; per-thread arenas; sharded central locks) scored
against the Z-018 constants and Z-019..Z-025 evidence. Decision
recorded in docs/DECISION_LEDGER.md. No production code in this
slice. Gate: dossier + ledger decision naming Wave 4/5's chosen
roads."

This dossier is that scoring. It contains no production code and
changes no allocator behaviour. Every number below is cited to a
banked recorded run in-tree on this host (purpleraider). Inputs
that have not landed are marked OWED and are never cited as
results; section 3 lists them with the re-open trigger each one
carries.

## 1. Evidence of record (landed, banked Z-001..Z-019)

### 1.1 The losses the redesign must answer

Source: docs/Z13_WAVE01_BASELINE.md (Z-009), folding Z-003
(tests/perf/recorded_run_z13_k002.txt) and Z-005
(tests/perf/recorded_run_z13_z005.txt). Medians, ns/op:

| Workload | glibc | mimir | Factor | Verdict |
|---|---|---|---|---|
| small_churn | 9.02 | 93.43 | 10.4x | LOSS (KL-A) |
| mixed_batch | 9.90 | 398.42 | 40.2x | LOSS |
| large_batch | 1295.68 | 3064.69 | 2.4x | LOSS (KL-C) |
| mixed_lifecycle | 13.38 | 898.76 | 67.2x | LOSS |

Counter leg (Z-005, quiet window; ratios are the portable
result): mimir executes 3.6x..46x more instructions per op than
glibc (small_churn 10.4x, mixed_batch 31.1x, large_batch 3.6x,
mixed_lifecycle 45.9x). On small_churn, cache-misses per op are
near parity (0.0169 vs 0.0136) while cycles/op are 175.65 vs
17.83 (9.9x): the small path's wound is code-path length, not
cache behaviour. KL-A is an instruction-count wound.

RSS leg (Z-006, docs/Z13_WAVE01_BASELINE.md section 5): peak Pss
is TIE on small_churn (-4.3%) and large_batch (+2.1%), LOSS on
mixed_batch (+23.3%) and mixed_lifecycle (+40.2%); retention
after free-all is a WIN on three of four workloads (mixed_batch
the exception, LOSS at both points). Any redesign that raises
peak resident cost on the mixed workloads starts from a measured
deficit, not from parity.

Energy leg (Z-007, baseline section 6): LOSS in direction on
all four workloads (proven net factor 4.42x on large_batch).
Energy follows time and instructions on this host; a redesign
that removes instructions and lock traffic is also the energy
fix, and no separate energy mechanism is proposed here.

### 1.2 The lock autopsy (Z-019, banked)

Source: tests/perf/recorded_run_z13_z019.txt (instrumented
build only; production object byte-identical with the flag
off). TSC 2.9944 cyc/ns; stamp-pair floor median 30 cycles.

Single-thread, the lock itself is nearly free: uncontended WAIT
mean ~40 cycles (~13 ns), p50 exactly 30 cycles, p99 60, on
every entry point and every baseline workload. HOLD is the path
fingerprint (median-of-process mean hold, cycles / ns):

| Series | Hold | Note |
|---|---|---|
| small_churn malloc | 60 / 20 | small path, span hit |
| small_churn free | 86 / 29 | free dearer than malloc |
| mixed_batch malloc | 143 / 48 | bimodal: p50 61 span hits, refill tail p99 ~2926 |
| mixed_batch free | 601 / 201 | |
| large_batch malloc | 2542 / 849 | mmap held under the lock (S9.4) |
| large_batch free | 4490 / 1499 | munmap held under the lock (S9.4) |
| mixed_lifecycle malloc | 625 / 209 | bimodal small+large mix |
| mixed_lifecycle free | 1777 / 594 | |
| calloc (supplementary) | 65 / 22 | malloc's shape plus zeroing |
| realloc (supplementary) | 759 / 254 | p50 ~95 stay-in-class; grow minority pays large path |

Under contention the picture inverts (supplementary specimen in
the Z-019 run, small_churn shape, one CCD): at 4 threads hold
barely moves (malloc 132, free 203 cycles) but mean WAIT jumps
to 1597 (malloc) / 1688 (free) cycles, about 12x the hold, p99
~14700-15200 cycles (~5 us). At 8 threads: hold 164 / 279,
mean wait 4190 / 4292 cycles (~1.4 us), about 26x the hold,
p99 ~29700-30200 cycles (~10 us).

Z-019's verdict, adopted by this dossier as its central design
law: threads spend their lives queueing for a lock whose actual
work takes 100-300 cycles; the scaling wound (KL-B) is
serialised HOLD summed across threads, not lock overhead and
not hold growth. A redesign is therefore judged first by how
much hold it removes from the free and small-malloc paths, and
from the large path's mapping-under-lock.

### 1.3 Constants of record the scoring must obey (Z-018)

Source: docs/Z13_DESIGN_CONSTANTS.md; every constant cited
there to its Wave 2 recorded run. Condensed to the rows the
candidate scoring turns on:

- Topology: 2 CCDs; 16 physical cores, 32 logical (SMT 2);
  CCD0 = physical 0-7 (+ siblings 16-23), CCD1 = physical
  8-15 (+ siblings 24-31); single NUMA node, UMA, 128 GiB.
- Cache: line 64 B; L1d 48 KiB/core; L2 1 MiB/core; L3 32 MiB
  per CCD, NOT unified across CCDs; DRAM plateau from 64 MiB.
- Bandwidth (Z-011): one core 38.2 GB/s; saturation at 16
  threads, peak 112.76 GB/s; single-CCD plateau 61.7-61.8 GB/s
  at 4 threads; 32 threads (SMT) 4-10% BELOW 16 for streaming.
- CCD/SMT (Z-012): intra-CCD shared-line transfer 32.5/32.2 ns;
  cross-CCD 103.0/103.7 ns; penalty factor 3.2x; SMT sibling
  ~1.03x intra-CCD transfer, scaling factor 0.18 (a sibling is
  ~0.2 of a core for memory work). Arena rule of record:
  ARENA_PER_CCD = YES; CROSS_CCD_SHARING = AVOID on hot paths;
  PREFER_INTRA_CCD = YES. Thread sizing rule: size
  thread-count-sensitive structures for 16 physical cores.
- Pages (Z-002/Z-013): 4 KiB base pages; THP host mode
  madvise; MADV_HUGEPAGE adopted at the 2 MiB and 20 MiB
  decades (fault -89..91%, minflt collapses); 200 MiB pooled
  adoption OWED (section 3).
- Faults (Z-014): ~345-380 ns/page to 16 MiB, ~466-505 ns/page
  at 64 MiB..1 GiB; recycled mappings do NOT fault cheaper
  (ratio ~0.999/0.979) - no fault-cost saving may be booked
  for pooled mappings; pooling pays only in syscall/VMA
  economics.
- Syscalls/VMA (Z-015): untouched mmap flat ~0.38-0.54 us at
  any size; populated teardown ~63-65 ns/page from 1 MiB up;
  mremap shrink ~0.61-0.65 us flat; interior unmaps split VMAs
  (+1 per hole), holes never merge.
- False sharing (Z-016): same-line penalty 2.86x at 2 writers
  rising to ~11x at 8-16 writers; one counter per 64 B line
  captures the entire win. PADDING_RULE of record:
  MIN_HOT_COUNTER_STRIDE = 64 B, GUARD_LINES_NEEDED = 0 -
  every field written by more than one thread gets its own
  64 B line; read-mostly and single-writer fields exempt.
- Alignment (Z-017): the 16-byte guarantee holds everywhere
  (0 sub-16 pointers in 163,840 blocks per leg). MimirAlloc
  cacheline splits at request <= 64: 16 B 0.00%, 32 B 0.00%,
  48 B 49.41%, 64 B 100.00% (every 64 B pointer at ptr mod 64
  = 32; zero cacheline-aligned starts). Every MimirAlloc
  class that is a multiple of 64 (64..1024, twelve classes)
  returns pointers exclusively at ptr mod 64 = 32. Classes
  NAMED FOR WAVE 4 by Z-017: 48 B and 64 B. Classes above
  64 B span lines by size, not by misalignment, and are not
  alignment defects.

### 1.4 The architecture being redesigned (of record)

From src/core/mimir_core.c / mimir_core.h at the banked seat:
one global lock (L6) guards all allocator metadata and IS held
across mapping acquire/release (B-006 S9.4 decision); requests
of 1..1024 bytes are served from 20 size-class spans (one
system page each, 96-byte header, bounded reuse, per-class
empty-span retention bound 0..16); requests above the 1024
cutoff take the mapping-per-allocation path (own OS mapping,
96-byte descriptor in a one-page reserve). There is no
per-thread state, no arena structure, and no lock sharding in
the banked core. Cross-thread frees are already a measured
reality on the mapping path (the J-002 statistics surface
counts them there); on the small path there is no per-object
owner record at all.

## 2. Scoring method

Each candidate is scored against seven criteria drawn from the
landed evidence, not from taste:

- C1 KL-A: does it remove single-thread small-path cost (the
  10.4x instruction wound, Z-003/Z-005; hold 60/86 cycles,
  Z-019)?
- C2 KL-B: does it remove serialised hold, the contended-wait
  multiplier (wait ~12x hold at 4 threads, ~26x at 8, Z-019)?
- C3 CCD fit: does it respect the Z-012 constants (cross-CCD
  3.2x penalty, per-CCD L3, ARENA_PER_CCD = YES,
  CROSS_CCD_SHARING = AVOID)?
- C4 False-sharing exposure: can it be built inside the Z-016
  padding rule (one hot writer per 64 B line, 0 guard lines)?
- C5 RSS/fragmentation risk: does it worsen the measured
  Z-006 deficits (peak Pss LOSS on both mixed workloads)?
- C6 Contract and lifecycle risk: thread-exit drain,
  cross-thread free routing, fork posture, teardown accounting
  (the surfaces Z-028/Z-030/Z-042 must pin down).
- C7 Alignment compatibility: does it preserve the 16-byte
  guarantee and give Wave 4 a road to fix the named 48 B /
  64 B splits (Z-017)?

Grades per criterion: STRONG (directly answers the cited
evidence), PARTIAL (helps, with a named limit), WEAK (little
effect on the cited evidence), NEGATIVE (works against a cited
constant or a measured standing).

## 3. Inputs OWED - not landed, never cited as results

Checked on disk during this build (2026-11-12): the seat is
banked through Z-019 (seat main 0b6c20f). Sibling worktrees
team-06, team-07, team-08, team-09, team-01, team-02 have each
merged the seat main and carry NO committed recorded run for
Z-020..Z-025. The six Wave 3 anatomy/contention slices are
therefore OWED inputs to this dossier:

| Slice | Owner | What it will settle | Downstream design it informs |
|---|---|---|---|
| Z-020 contention ladder (banked allocator) | team-06 | per-thread throughput 1..32, exact inversion thread count | urgency and target geometry of the Wave 5 arena road (Z-037/Z-038) |
| Z-021 contention ladder (glibc) | team-07 | the comparator's scaling shape on the same protocol | the bar the Z-038 three-way ladder is judged against |
| Z-022 small-path anatomy | team-08 | perf top-10 hot spots on small_churn mapped to source regions | which source regions the TLS cache must bypass (Z-029) |
| Z-023 large-path anatomy | team-09 | large-path cost split: mmap/munmap vs bookkeeping vs faults | Wave 6 mapping-cache design (Z-047); sizes KL-C exactly |
| Z-024 free-path anatomy | team-01 | cross-thread and remote-free costs per path and thread geometry | remote-free queue geometry for Z-030 |
| Z-025 calloc/realloc anatomy | team-02 | zeroing cost by size; realloc growth ladders; cliffs at class boundaries and the 1024 cutoff | cache admission for calloc/realloc paths in Z-028's contract |

Also still OWED from Z-018 (docs/Z13_DESIGN_CONSTANTS.md
section 11): THP adoption at 200 MiB for pooled mappings. No
landed run has taken that measurement; no candidate below is
scored as if it existed, and no Wave 4/5 road may assume it.

Why deciding now is defensible: the decision turns on Z-019's
histograms and the Z-018 constants, both landed and banked.
Z-019 already contains a contended specimen (section 1.2) that
sizes KL-B at histogram level; Z-020/Z-021 will refine the
ladder, not reverse the mechanism - hold does not shrink when
more threads queue, on any ladder shape. Z-022 could in
principle name a hot spot outside the lock/span machinery; if
it does, that is a named re-open trigger (section 7), handled
by the ledger's append-only supersession, not by editing this
dossier. The remaining owed slices inform design detail inside
the chosen roads (queue geometry, cache admission, arena
targets) at the slices that own those details (Z-028, Z-030,
Z-037), each of which reads the then-landed evidence before
building.

## 4. Candidate scoring

### 4.0 Matrix

| Criterion | TLS freelist cache | Per-CCD arenas (2) | Per-thread arenas | Sharded central locks |
|---|---|---|---|---|
| C1 KL-A single-thread | STRONG | WEAK | PARTIAL | WEAK |
| C2 KL-B hold removal | STRONG | PARTIAL | STRONG (in theory) | PARTIAL |
| C3 CCD fit (Z-012) | PARTIAL (neutral alone) | STRONG | NEGATIVE (as default) | WEAK |
| C4 False sharing (Z-016) | STRONG | PARTIAL | PARTIAL | PARTIAL |
| C5 RSS risk (Z-006) | PARTIAL | PARTIAL | NEGATIVE | STRONG (no new retention) |
| C6 Lifecycle risk | PARTIAL (new surfaces) | PARTIAL | NEGATIVE (churn) | STRONG (smallest delta) |
| C7 Alignment (Z-017) | STRONG (carve control) | PARTIAL | PARTIAL | WEAK (no carve change) |

### 4.1 TLS freelist cache - ADOPT as Wave 4's road

Design as scored: per-thread bins for the hot size classes
over the banked span machinery; lock-free on a cache hit;
batched refill/drain to the central core on miss (roadmap
Z-028/Z-029); remote frees routed to the owner thread's queue
with bounded drain (Z-030).

- C1 STRONG. A cache hit removes the lock acquisition AND the
  central span/registry walk from the operation entirely. That
  is the only candidate that attacks both halves of the
  landed diagnosis at once: the instruction excess (Z-005:
  small path is 10.4x instructions at cache-miss parity) and
  the hold itself (Z-019: small malloc hold 60 cycles, free
  86, every cycle of which is also paid by queued threads
  under contention). glibc's 9.02 ns/op on small_churn is only
  reachable on a path that does not take a global lock per
  operation.
- C2 STRONG. Every hit is hold that never enters the
  serialised sum. Z-019's law - judged by hold removed from
  the free and small-malloc paths - names this candidate's
  territory explicitly ("TLS cache territory, Z-028/Z-029"
  in the recorded verdict).
- C3 PARTIAL. Thread-local state is CCD-neutral by itself: a
  thread migrated across CCDs carries its bins with it, and
  refill batches drawn from the central core can cross CCDs.
  The cache neither creates nor cures cross-CCD traffic;
  that is the arena road's job (4.2), and Z-037 is specified
  to share this cache machinery, so the two roads compose
  instead of competing.
- C4 STRONG. Bins are written by exactly one thread; the
  Z-016 rule (single-writer fields exempt, one hot writer per
  line otherwise) is satisfied by construction on the hit
  path. The shared surfaces the design adds - refill/drain
  batches and the remote-free queue - are few and named, and
  each queue head/counter takes its own 64 B line under the
  padding rule.
- C5 PARTIAL. Cached free blocks are retained memory: a bin
  cap per class is a hard design requirement, sized so the
  Z-006 mixed-workload peak deficits (+23.3% / +40.2% Pss)
  are not deepened. The banked core already bounds span
  retention (0..16 empty spans per class); the cache's bound
  must be stated in bytes in Z-028's contract and re-measured
  by Z-031's verdict run against the Z-009 RSS leg.
- C6 PARTIAL - the real cost of this road, paid openly. Three
  new lifecycle surfaces: (i) thread-exit drain - bins must
  return their blocks to the central core at thread exit,
  byte-accounted (proven later by Z-042's storm: 10,000
  short-lived threads, retained-bytes-after-storm figure);
  (ii) cross-thread free routing - a block allocated by one
  thread and freed by another must reach its owner's bin
  without taking the owner's lock per free (Z-030's bounded
  remote-free queue), and the small path gains the per-object
  owner record it currently lacks; (iii) fork posture - TLS
  state in a forked child must follow the Phase 8 posture
  proven for this host by Z-044. Z-028 exists precisely to
  pin these contracts before code; the contract is written
  against this dossier's decision and must be red on the
  current core. The contract must pin, at minimum: per-class
  bin capacity and max cached bytes per thread; refill/drain
  batch sizes and watermarks; the remote-free routing
  interface Z-030 implements; thread-exit drain destination
  and destructor ordering; the pthread_atfork posture; and
  the early-init/reentrancy rule (first allocation before
  TLS initialisation, and allocation from inside TLS
  destructors, must not recurse).
- C7 STRONG. Refill carves blocks from spans under the
  cache's own control, which is the natural place to fix the
  carve geometry Z-017 named: the 64 B class must gain
  cacheline-aligned starts (today 0.00% aligned, 100% split)
  and the 48 B class's 49.41% split must be answered, in
  concert with the Z-032 size-class spacing study and the
  conditional Z-033 retune. The 16-byte guarantee is
  preserved by construction (bins hand out span blocks
  unchanged in alignment or better).

What it does NOT fix: the large path (KL-C). Holds of
2542/4490 cycles for large malloc/free are the mmap/munmap
pair under the S9.4 lock; a small-path cache never touches
them. Wave 6 owns that wound (Z-046..Z-054); this dossier
records that Waves 4/5 change the large path only by not
regressing it.

### 4.2 Per-CCD arenas (2 arenas) - ADOPT as Wave 5's first road

Design as scored: two arenas, one per CCD, selectable at build
time behind a flag, sharing the Wave 4 TLS cache machinery
(roadmap Z-037); threads draw central service from their
CCD's arena; the Z-020/Z-021 ladders re-run three-way
(banked / arena / glibc) at Z-038.

- C3 STRONG - this candidate is the Z-012 constants written
  as architecture. Cross-CCD shared-line transfer costs
  103 ns against 32.5 ns intra-CCD (3.2x); L3 is 32 MiB per
  CCD and not unified; the arena rule of record is literally
  ARENA_PER_CCD = YES, CROSS_CCD_SHARING = AVOID,
  PREFER_INTRA_CCD = YES. Two arenas is the only candidate
  count that matches the silicon: one arena per L3 domain.
- C2 PARTIAL. Two arena locks halve the queue per lock at
  best, and only for the central traffic the TLS cache has
  not already removed. That is exactly why this road is
  second: applied first, arenas would spend Wave 4's effort
  splitting a lock whose hold the cache is about to delete
  on the hot classes; applied after the cache (Z-037's own
  specification shares the cache machinery), arenas split
  only the residual central traffic - refill/drain batches,
  span supply, the mapping path - where locality also pays.
- C1 WEAK. Single-thread, an arena is still a locked central
  path; it removes no instructions from an uncontended
  operation. It is not a KL-A answer and is not scored as one.
- C4 PARTIAL. Arena headers, stats and free queues are shared
  structures written from up to 8 physical cores per CCD:
  every hot field takes its own 64 B line under the Z-016
  rule, and Z-040 (false-sharing purge) is the named wave-5
  slice that applies the rule to every structure the
  profiles name.
- C5 PARTIAL. Two span pools retain up to twice the empty
  spans of one if retention bounds are copied naively; the
  per-class retention bound must be split across arenas, not
  duplicated, and Z-038's ladder run must carry the Z-006
  RSS leg so any peak regression is measured, not assumed.
- C6 PARTIAL. Thread-to-arena assignment must survive thread
  migration: a thread that migrates across CCDs must not
  silently pay cross-CCD prices on every refill, so
  assignment is by current CCD at refill time, never a
  birth-CCD pin; SMT siblings (~0.2 of a core, Z-012 sizing
  rule) share their physical core's arena - structures are
  sized for 16 physical cores, not 32 logical ones. The
  Z-037 prototype must measure the thread migration rate
  and the remote-free ratio alongside throughput: an arena
  win bought by assuming threads never migrate is not a
  win on this host until those two numbers are recorded.
- C7 PARTIAL. Arenas inherit span carve geometry; the Wave 4
  alignment fix (4.1, C7) lands first and carries over.

### 4.3 Per-thread arenas - NOT ADOPTED as a primary road; held as Z-039's conditional

Design as scored: one arena per thread, each with its own
span pool and lock (or lock-free ownership).

- C2 STRONG in theory: contention on owned state vanishes.
  But C3 is NEGATIVE as a default: with thread counts sized
  by callers (up to 32 logical on this host, SMT siblings at
  ~0.2 of a core), per-thread pools multiply span retention
  by thread count, and a producing thread's arena is routinely
  freed into by consumers on the other CCD - the cross-CCD
  traffic Z-012 says to AVOID becomes the free path's normal
  case unless ownership routing (Z-030's queues) does the
  work, at which point the design has converged back to a TLS
  cache over shared arenas with extra pools.
- C5 NEGATIVE: per-thread retention is the worst case for the
  Z-006 standings; every idle thread's pool is peak RSS that
  serves nobody, on workloads already LOSS at peak.
- C6 NEGATIVE: thread churn is this host's named storm case
  (Z-042: 10,000 short-lived threads). Per-thread arenas pay
  arena birth/death and drain per thread; TLS bins pay it
  too, but bins are bounded freelists over shared spans, not
  whole pools with their own span supply.
- Verdict: not a primary road for either wave. The roadmap
  itself makes this candidate conditional (Z-039: "Per-thread
  arenas if Z-038 argues for them, else lock-sharding") and
  this dossier keeps exactly that conditional: per-thread
  arenas may be taken up at Z-039 ONLY if the Z-038 three-way
  ladder shows the per-CCD arena variant still
  contention-bound on landed numbers. Absent that showing,
  the road not taken is this one, with this section as its
  recorded reason.

### 4.4 Sharded central locks - NOT ADOPTED as a primary road; held as Z-039's named fallback

Design as scored: the central core's one lock split into N
shard locks (by class or by hash), span machinery otherwise
unchanged.

- C1 WEAK, and this is the disqualifying row for a primary
  road: sharding removes no instructions from the
  single-thread path. Z-019 shows uncontended wait already at
  the measurement floor (~13 ns mean, p50 one TSC tick) -
  there is no lock-acquisition cost left to shard away - and
  Z-005 shows the wound is the 10.4x instruction count under
  the lock, which every shard still executes on every
  operation. A sharded core still loses small_churn by
  roughly the Z-003 factor.
- C2 PARTIAL: queueing divides by shard count for spread
  workloads, but hot classes concentrate on hot shards, and
  every operation still pays hold inside the serialised sum.
- C3 WEAK: a shard's spans are served to both CCDs unless
  shards are CCD-partitioned - at which point the design is
  per-CCD arenas with more, smaller locks and none of the
  locality discipline.
- C5/C6 STRONG: smallest delta, no new retention, no new
  lifecycle surface. That safety is why it is the fallback
  and not the discard pile.
- Verdict: not a primary road. Held, exactly as the roadmap's
  Z-039 frames it, as the fallback geometry: if Z-038 does
  not argue for per-thread arenas, Z-039 takes lock-sharding
  of the central core as Wave 5's second geometry. Its
  recorded merit is safety; its recorded defect is that it
  cannot answer KL-A.

## 5. Decision (summary; the record is DECISION_LEDGER D-007)

Wave 4's chosen road is the TLS freelist cache: Z-028 writes
its contract against this decision (ownership, cross-thread
free routing, thread-exit drain), Z-029 implements per-thread
bins for the hot classes, lock-free on cache hit, with batched
refill/drain to the central core on miss, and Z-030 routes
cross-thread frees into TLS-owned blocks through a bounded
remote-free queue per owner thread. The 48 B and 64 B
alignment defects named by Z-017 are Wave 4 work on this road:
the cache's refill carve, with the Z-032 spacing study and the
conditional Z-033 retune, must give the 64 B class
cacheline-aligned starts and answer the 48 B class's 49.41%
split.

Wave 5's chosen first road is per-CCD arenas: Z-037 builds
the 2-arena variant behind a build flag, sharing the Wave 4
cache machinery, and Z-038 re-runs the contention ladders
three-way (banked / arena / glibc). Wave 5's second geometry
is decided at Z-039 on Z-038's landed numbers, exactly as the
roadmap frames it: per-thread arenas only if the ladder
argues for them, else lock-sharding of the central core. This
dossier adopts neither as a primary road and records the
reasons in sections 4.3 and 4.4.

The large path is not redesigned by Waves 4/5. KL-C (a mapping
per allocation above the 1024 cutoff; large malloc/free holds
of 2542/4490 cycles under the S9.4 lock) is Wave 6's wound
(Z-046..Z-054, mapping cache per Z-047's design). Waves 4/5
must not regress it, and the TLS/arena work must leave the
mapping path's contracts intact for Z-047 to build on.

## 6. Constraints the chosen roads must build inside

Imposed by the Z-018 constants on Z-028..Z-039, stated here so
the consuming slices can cite them:

1. Line discipline (Z-016): every structure written by more
   than one thread - arena headers, shard or arena locks,
   remote-free queue heads, statistics counters - places each
   hot field on its own 64 B line; GUARD_LINES_NEEDED = 0, no
   padding beyond the line. TLS bins are single-writer and
   exempt on the hit path; their refill/drain batch headers
   are not exempt when shared.
2. CCD discipline (Z-012): no hot path shares state across
   CCDs by default (CROSS_CCD_SHARING = AVOID); arena
   assignment follows the thread's current CCD at refill
   time; structures sized for 16 physical cores, with SMT
   siblings treated as ~0.2 of a core, never as independent
   arena or bin owners by default.
3. Locality budget (Z-010/Z-011): per-CCD working state
   should live inside the 32 MiB per-CCD L3 at desktop
   working-set sizes; bandwidth saturation at 16 threads
   means no design may assume SMT threads add memory
   throughput.
4. Retention budget (Z-006): cache bin caps and arena span
   retention are stated in bytes in the Z-028/Z-037 designs
   and re-measured against the Z-009 RSS leg by the wave
   verdict runs (Z-031/Z-038); the mixed_batch and
   mixed_lifecycle peak standings (LOSS +23.3% / +40.2% Pss)
   may not be deepened without a recorded reason.
5. No assumed savings (Z-014/Z-015): no fault-cost saving is
   booked for any pooling the new roads introduce; pooled or
   cached mappings justify themselves on syscall/VMA
   economics only. THP at 200 MiB for pooled mappings remains
   OWED and unassumed (section 3).
6. Alignment (Z-017/Z-002): the 16-byte guarantee is
   inviolable on every new path, cache hits included; the
   64 B class carve fix (section 5) is part of Wave 4's
   definition of done, verified by re-running the Z-017
   audit on the new core at Z-031 or Z-032.
7. Growth extents (Z-013): where the new roads grow pooled
   or arena storage, extents align to the THP decades of
   record (2 MiB / 20 MiB); no extent is sized on 200 MiB
   huge pages (owed, section 3). And no hot lock, counter or
   queue head may be placed inside a 48 B or 64 B allocation
   on the assumption that it owns its cacheline - Z-017
   proves those classes split lines (49.41% / 100%); shared
   control structures live in dedicated, line-padded storage,
   never in class allocations.
8. Zero-cost-when-off precedent (Z-019): any instrumentation
   or variant machinery the new roads add follows the
   MIMIR_LOCK_HIST precedent - build-flag gated, with the
   production object proven unaffected, as Z-037's flag
   already requires.

## 7. Re-open conditions

This decision is made on landed evidence and is provisional
in exactly the following named ways. Any one of these, when
its slice lands, requires a superseding ledger entry (D-008
or later) before the affected downstream slice builds further:

- Z-022 names a top-10 hot spot on the small path that lies
  OUTSIDE the lock/span/registry machinery the TLS cache
  bypasses (for example, zeroing or pattern-touch cost
  dominating; as a working threshold, lock/central-path share
  under a quarter of small-path samples): re-open the Wave 4
  road choice.
- Z-020's ladder shows the banked allocator's scaling
  inversion at or below 2 threads, or no inversion through
  32: re-open the Wave 5 sequencing (arena urgency).
- Z-021 shows glibc inverting at the same thread count as
  the banked allocator on the identical ladder: the scaling
  limit is then hardware, not allocator structure, and the
  Wave 5 arena expectation is re-opened.
- Z-024 shows remote-free cost dominated by the mapping path
  rather than the small path (or remote frees as the common
  case at more than twice the local-free cost): re-scope
  Z-030 (design detail, not the architecture decision) and
  record the change.
- Z-025 names a calloc or realloc cliff that a freelist cache
  would widen (for example at the 1024 cutoff): Z-028's
  contract must carve that path out of cache admission, and
  the carve-out is recorded in the ledger entry's wake.
- Z-023's large-path split shows bookkeeping, not the
  mmap/munmap pair, dominating KL-C: Wave 6's design order
  changes; Waves 4/5 are unaffected and no re-open of this
  decision is needed.

## 8. Gate statement

Roadmap gate for Z-026: "dossier + ledger decision naming
Wave 4/5's chosen roads." This file is the dossier; the
decision is recorded as entry D-007 in
docs/DECISION_LEDGER.md (appended per that ledger's rules;
entries D-001..D-006 untouched). No production code is in
this slice: the commit carries this dossier and the ledger
append only.

Correctness gate for this slice: scripts/z13/run_gate.sh on
the final tree, log hidden_files/z026_gate.log (team-local,
not banked): GATE COUNTS checks=4988 failed=0 skips=77, GATE
RESULT: GREEN - identical to the Z-001 baseline and to the
expected counts for a dossier slice. Machine load during the
build was light-to-shared (load average ~1.75-3.16 on the
32-thread host, other campaigns' teams active); no benchmark
verdict is claimed by this slice, so no quiet-machine number
was needed.

Bugs discovered: none in the allocator. One process note: the
seat and this branch carried identical test-z013 / test-z017
Makefile hunks into the pre-build merge of seat main
(0b6c20f); the merge completed clean with each target present
exactly once, verified in the merged Makefile - no conflict
resolution was required.
