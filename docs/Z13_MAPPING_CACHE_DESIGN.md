# Z13 Mapping-Cache Design

Slice: Z-047 — Mapping-cache design (Wave 6). Status: design
written and priced on this host, 2026-11-12, team-01 worktree.
This slice changes NO allocator code. The implementation
slice is Z-048; the consumers of this design are Z-048
(implementation), Z-049 (THP for cached mappings), Z-050
(trim policy under pressure), Z-052 (mremap growth) and
Z-054 (Wave 6 verdict).
Artifacts: this doc (the design + the invariant list of
section 4); tests/perf/probe_z047.c, tests/perf/run_z047.sh,
tests/perf/summarize_z047.py (the pricing probe);
tests/perf/recorded_run_z13_z047.txt (the evidence run).

## 0. Position

The large path (every request above the 1024-byte cutoff,
src/core/mimir_core.c MIMIR_SMALL_CUTOFF) is one mmap per
allocation and one munmap per free, with a 96-byte descriptor
in a one-page reserve at the mapping's head (B-006 S7.1):
mapping_length = page_round_up(4096 + request), user pointer
= base + 4096. Z-046 priced that posture per decade: the
mapping price is nearly size-flat (op/mimir 1911 ns/op at
2048 rising to 4011 at 33554432), steady-state glibc serves
the same sizes from reused arena memory at ~8 ns/op, and the
dense-touch bleed scales with pages touched (38.07x at
1 MiB, 34.52x at 8 MiB; parity 1.01x only at 32 MiB, above
glibc's dynamic-threshold cap cliff). KL-C — a mapping per
allocation — is the Wave 6 hunt. This design attacks the
two terms a cache can reach: the mmap/munmap pair (Z-046's
raw pair ~0.98–1.08 us at every size) and the re-fault of
pages a previous identical allocation already faulted in.
It does not raise the cutoff, does not touch the span path,
and promises no verdict: Z-048's gate re-measures the built
cache against Z-009 and glibc, and Z-054 restates KL-C.

## 1. Evidence the design must serve

Every number below is from a banked recorded run named
beside it, or from this slice's own run of record
(tests/perf/recorded_run_z13_z047.txt; two full invocations
in a quiet window, load 0.09–0.60; headline = invocation 1,
agreement stated there).

- The pair is flat and the faults are the growth term
  (Z-046, tests/perf/recorded_run_z13_z046.txt): raw
  mmap+munmap pair ~980–1080 ns at every size 1024..
  33554432; fault+populated-teardown grows ~503 ns at 2048
  to ~2362 ns at 33554432; bookkeeping residual 401–840 ns
  at every size. A perfect cache caps its sparse saving
  near the pair+reserve-fault share (~55–75% of the
  single-block price at small sizes, less at large).
- The trim lesson (Z-046, the gltrim leg): reuse only pays
  if freed memory is RETAINED. An arena forced to map but
  trimmed at the default 128 KiB threshold falls off a
  cliff at 262144 and above (2061–3621 ns/op); retaining
  (both thresholds raised) holds ~8–10 ns/op at every size.
  For this design the consequence is controlling: the
  pool's retention bound IS the price. A pool that returns
  mappings too eagerly is the mapping path with extra
  bookkeeping.
- Syscall/VMA economics (Z-015, tests/perf/
  recorded_run_z015.txt): untouched mmap/munmap is
  sub-microsecond and size-flat; first-touch fault
  0.33–0.41 us per 4 KiB page; populated munmap teardown
  ~64 ns per resident page from 1 MiB up; interior hole
  punches split VMAs (+1 per hole), edge unmaps never
  split, exact-adjacent identical mappings MERGE;
  vm.max_map_count = 1048576, so count is not the binding
  constraint on this host — walk cost and fragmentation
  are.
- Fault curve (Z-014): the first-touch vs recycled split
  is the number the large path lives or dies by; a pooled
  mapping whose pages stayed resident is the recycled
  case, paid once instead of per allocation.
- THP (Z-013, tests/perf/recorded_run_z13_thp.txt):
  MADV_HUGEPAGE is a clean fault WIN at the 2 MiB and
  20 MiB decades (faults collapse 512→1 and 5120→10;
  bandwidth never worse). Z-049 owns applying it; this
  design only reserves the hook (section 3.9).
- The free path is the registry walk (Z-024): a large
  free today walks the mapping registry linearly (the
  walk is ~82–92% of mapping-free cost by the
  reverse-order controls). A pool that adds a second
  linear walk per free would hand back its own saving;
  pool lookup and pool insertion are therefore O(1) by
  construction in this design (section 3.2).
- RSS honesty (Z-006): MimirAlloc's current virtue is
  that AFTER_FREE returns to baseline because nothing is
  retained. Pooling spends that virtue deliberately, up
  to a hard bound, and must expose its retained bytes so
  the spend is auditable (sections 3.3, 3.7, invariant
  MC-14).
- Fork posture (Z-044): the allocator is fork-quiescent
  in proof; pooled state is ordinary process memory and
  inherits like it (section 3.6).

This slice's own probe (raw mmap in the exact large-path
geometry; no allocator code linked) adds four pricings:

- Sparse hit ceiling: fresh pair+endpoint-fault costs
  1464 ns/op at 2048 rising to 3562 at 33554432; the
  no-syscall re-touch floor is 1.6–75 ns over the same
  ladder (the floor rises with size: endpoint pages fall
  out of cache/TLB). The gross sparse saving available to
  a hit is ~1.46–3.49 us/op before bookkeeping.
- Dense resident reuse: re-writing every byte of a pooled
  1 MiB / 8 MiB mapping whose pages stayed resident costs
  5909 / 70001 ns/op against 108634 / 892007 fresh —
  savings of 94.6% / 92.2% (invocation 2 agrees on the
  savings: 94.7% / 93.9%; the 8 MiB pooldense absolute is
  regime-split across invocations, 70.0k vs 55.1k, and is
  quoted as a range in the recorded run).
- DONTNEED pooling buys no time: the same legs with
  MADV_DONTNEED applied on every return to the pool cost
  107106 / 890136 ns/op — savings of 1.4% / 0.2%, i.e.
  the faults return in full and only the bare pair is
  saved. DONTNEED is an RSS tool, never a speed tool.
- Census (deterministic, identical in both invocations):
  eight separate 262144-byte mappings held live register
  as +1 VMA, not +8 — the kernel merged all eight
  adjacent identical mappings into one VMA, Z-015's merge
  behaviour at scale. Pooling them changes the VMA count
  not at all (+1); MADV_DONTNEED on the touched half
  drops RSS by exactly 1024 KiB (4 × 256 KiB of touched
  extents) with the VMA count unchanged; evicting all
  eight returns the count to base. Untouched pooled
  mappings cost no RSS (the pooled stage's RSS rise over
  live is exactly the touched half's 1024 KiB).

## 2. What is pooled

A pooled unit is one whole dead large-path mapping, exactly
as alloc_locked created it: base, mapping_length, reserve
page, and the 96-byte descriptor in the reserve page,
which stays the unit's metadata for its whole life (the
B-006 S7.1 self-hosting property — pool bookkeeping needs
no allocation, section 3.5). Nothing smaller than a whole
mapping is ever pooled: no sub-mapping cache, no splitting,
no coalescing of two dead mappings into one bigger unit.

Eligibility: a large-path free is pool-eligible iff its
mapping_length ≤ 16 MiB (16777216 bytes) AND the caps of
section 3.3 admit it. Above 16 MiB the free munmaps
immediately, always. Rationale: Z-046 found the top decade
is where the cache's relative prize is smallest (dense
parity at 32 MiB) and where a single retained unit would
eat a quarter of the byte cap; the eligibility line keeps
at least four maximum-size units admissible at once.
Re-open trigger: Z-054 may raise the line if Z-048/Z-053
evidence shows top-decade churn the pool is missing.

## 3. The design

### 3.1 Segregation: exact-length buckets

The reuse key is mapping_length itself. A request is
served from the pool only by a unit whose mapping_length
equals page_round_up(4096 + request) — the exact length
alloc_locked would have mapped. Consequences, all intended:

- Zero added waste: the unit's usable_size is already the
  request's own page-rounded extent; internal waste is
  byte-identical to the fresh path's (reserve page +
  round-up: 6144 B at a 2048 request falling to 0.01% at
  32 MiB, per Z-046 — unchanged, neither better nor worse).
- No size field is ever rewritten downward; descriptor
  relations (usable_size == mapping_length −
  usable_offset, requested_size ≤ usable_size) hold at
  reuse with only requested_size updated (section 3.4).
- Fragmentation cannot form inside the pool: a unit is
  exactly one future allocation of one exact length, or
  it is evicted whole. Z-053's fragmentation study reads
  pool behaviour without a best-fit variable.

Rejected here: best-fit/range reuse (serve a smaller
request from a larger dead unit). It would cut cold misses
across neighbouring lengths, but it rewrites usable
extents, strands the tail of every over-size unit for
exactly the sizes Z-053 must audit, and its gain is
bounded by the ladder's length variety, which no banked
run has yet measured in a real mix. Re-open trigger: if
Z-048's churn gate shows pool misses dominated by
near-length neighbours, range reuse returns as its own
design slice, not as a Z-048 improvisation.

### 3.2 Bucket structure: O(1) by construction

Two tiers, all heads in static storage (no allocation —
B-006 S7.2 bootstrap discipline; the pool exists before
the first malloc and must never need one):

- Direct tier: lengths up to 1048576 + 4096 (257 pages)
  index a static array of 257 bucket heads by page count
  (mapping_length / 4096). Covers every request up to
  1 MiB — the decades where Z-046 shows the mapping price
  is worst relative to size and where churn concentrates.
- Large tier: lengths above that, up to the 16 MiB
  eligibility line, are held on per-length lists whose
  heads live in a static table of at most 32 distinct
  lengths. A free whose length would be the 33rd distinct
  large length is not pooled (munmap); a lookup scans at
  most 32 table entries comparing lengths — a bounded
  constant, not a walk over units.

Each bucket is a LIFO stack linked through the pooled
descriptors' own list_next/list_prev fields. LIFO is
deliberate: the most recently freed unit is the one whose
pages are likeliest still warm in cache/TLB (the probe's
poolhit floor rising with size is the cold-endpoint price,
and LIFO minimises exactly that), and LIFO makes the
oldest units the natural eviction end for Z-050's trim.

Pool operations never walk the live registry and never
walk more than one bucket: insertion is a push, reuse is
a pop, eviction is a pop from a named bucket. This is the
Z-024 lesson applied — the registry walk already prices
the free path; the pool adds no second walk to either
side.

### 3.3 Bounds: the retention price, fixed in advance

Three caps, all checked under the lock before admission,
all counted in mapping_length bytes:

- per-bucket count cap: 4 units per exact length;
- global count cap: 64 units;
- global byte cap: 64 MiB (67108864) of pooled
  mapping_length.

64 MiB is ~9× the largest banked large-path peak live set
(Z-006: large_batch peak 7024640 B) and 0.05% of this
host's 128 GiB UMA — large enough to shelter every churn
pattern the banked workloads contain, small enough that
the worst-case RSS spend of section 3.7 is a bounded,
statable number. The count cap bounds the worst-case VMA
exposure at +64 (section 3.8) and the trim walk at 64
units.

Admission rule on a pool-eligible free: if the bucket
already holds 4, or the pool holds 64 units, or pooled
bytes + this length would exceed 64 MiB, the free munmaps
— it does NOT evict another unit to make room. Free stays
O(1), eviction policy lives in exactly one place (trim,
Z-050), and a burst of frees of one length can never flush
the shelter of every other length.

### 3.4 Lifecycle and state machine

The descriptor state field gains one value beside LIVE (1)
and RETIRED (2): POOLED (3). Legal transitions, each taken
exactly once, under the lock, by the named actor only:

- (fresh) → LIVE: alloc_locked publication, unchanged.
- LIVE → POOLED: the large free path, after
  descriptor_is_live validation passes, in place of
  retire_locked's munmap. The descriptor is unlinked
  from the live registry, live counters decremented,
  state set POOLED, unit pushed on its bucket, pooled
  counters incremented — one locked section, the same
  publication discipline as today.
- POOLED → LIVE (reuse): alloc_locked's new first step.
  Pop the bucket head; validate the unit (section 3.5);
  rewrite requested_size to the new request (and the
  J-002 owner-thread record at reserve offset 96);
  zero the usable extent iff this is a calloc
  construction; set state LIVE; link into the live
  registry; move the counters. No mmap, no munmap, no
  madvise on this path — MC-07 makes that auditable.
- POOLED → RETIRED: eviction — by trim, by unload drain,
  or by Z-050's pressure policy, never by admission
  (3.3). Counters move pooled → released; the munmap
  uses the exact (base, mapping_length) pair captured
  into locals before the call, retire_locked's existing
  discipline verbatim.
- LIVE → RETIRED: unchanged retire_locked, for
  ineligible or cap-refused frees.

realloc: a growing large realloc constructs its new block
through the same alloc path (so it may reuse a pooled
unit of the new length) and retires its old block through
the same free path (so the old unit may pool). A shrinking
large realloc keeps today's in-place bookkeeping and
never pools a part of itself — there is no part; the
mapping stays LIVE and whole. Pooled units are never
mremap sources or targets in this design; Z-052 builds
growth on live mappings only.

### 3.5 Validation and the corruption course

A pooled descriptor has sat in dead memory. Before any
reuse, descriptor_is_live's J-006 relations are re-checked
in their POOLED form: magic intact, state == POOLED,
mapping_base == the descriptor's own address,
usable_offset == one page, usable_size == mapping_length
− usable_offset, mapping_length == the bucket's length,
requested_size non-zero and ≤ usable_size. A unit that
fails is NOT served and NOT munmapped by a guessed length
(the J-006 posture: a wrong-length release is the failure
being defended against). It is unlinked from its bucket,
counted as a quarantined leak (a new stats counter, so the
leak is a number, never a silence), and construction
falls through to the fresh mmap path. The quarantined
mapping stays mapped and accounted in the pooled counters
minus its bucket membership — the invariant MC-03
statement below carries the quarantine term explicitly so
the books still balance.

Reading a pooled descriptor can itself fault its reserve
page back in if the kernel reclaimed it under pressure:
bounded at one fault per reuse, accepted, and one more
reason the hit price in section 5 is a ceiling, not a
promise.

### 3.6 Contents, calloc, fork, unload

- malloc reuse returns the unit's bytes as they lie:
  contents unspecified (B-007 C1) — but the honest
  difference from today is named: a fresh mapping happens
  to read as zero, a reused one carries its previous
  owner's bytes. That is contract-legal and in-process
  only (units never cross a process boundary), and it is
  recorded here so Z-054's verdict and any future security
  review meet it as a decision, not a discovery. No
  scrubbing on the free or reuse path in Z-048: scrubbing
  a dense unit costs the full re-touch the cache exists
  to avoid (probe: 5.9–70 us of memset per dense unit),
  and calloc — the one entry point that promises zeros —
  already zeroes the whole usable extent inside
  alloc_locked, on reuse exactly as on fresh.
- Fork: the pool duplicates with the address space. Each
  side's units are private copies with valid descriptors;
  either side may reuse, evict, or drain its own pool with
  no cross-process coordination (there is none today for
  live mappings either). The Z-044 fork-quiescent posture
  is unchanged: at a quiescent fork the pool is simply
  more quiescent memory.
- Unload: the H-001 destructor drain must drain the pool
  with the retained spans — mimir_core_drain_retained is
  extended to evict every pooled unit (POOLED → RETIRED
  for each) so a dlclose orphans no pooled mapping and
  loses no descriptor's owner.

### 3.7 Residency and RSS: resident by default

Free and reuse perform no madvise. A pooled unit keeps
exactly the residency it died with: touched pages stay
resident (that residency IS the dense prize — 92–95% of
the fresh dense price), untouched pages cost nothing
(census: the untouched half of the pooled stage added
0 KiB). MADV_DONTNEED is reserved to Z-050's trim, where
it is the middle course between keeping a unit and
unmapping it — applied there to a unit's whole usable
extent only, and recorded in the unit's flags so a later
reuse knows its pages are gone (its price is then the
pooldontneed leg's: pair saved, faults repaid).

The spend, stated plainly: with the pool live, AFTER_FREE
RSS no longer returns to baseline; it returns to baseline
+ resident pooled bytes, bounded by the 64 MiB cap and
exposed exactly by the pooled-bytes gauge (MC-14). Z-048's
RSS leg (Z-006's method) must file the with-pool AFTER_FREE
figure beside the banked one so the trade is a filed
number, and Z-050 exists to give the bytes back under
pressure. A design that hid this would be repeating, in
allocator form, the retention glibc is criticised for in
Z-006 — the difference here is the cap is hard, small,
and audited.

### 3.8 VMA posture: no surgery, no counting

The pool performs exactly two kernel operations on its
units over their pooled life: none (keep), and munmap of
the exact acquired pair (evict). It never punches, splits,
shrinks, or partially unmaps a unit — Z-015 priced every
one of those, and none is needed. Merging is the kernel's
business and this design neither exploits nor forbids it:
the census proves adjacent identical mappings merge
(8 units, +1 VMA), so VMA count is NOT a function of pool
size and must never be used to audit it — the pool's own
counters are the audit (MC-03/MC-14). The design budgets
the worst case anyway: if nothing merges, the count cap
bounds pooled VMAs at +64 against vm.max_map_count =
1048576, and Z-015's per-VMA walk price is bounded by the
same cap. One corollary for Z-049: a pooled unit's usable
extent may sit inside a merged VMA shared with live
neighbours; madvise on the extent is range-based and
safe, but no later slice may assume a pooled unit IS a
VMA.

### 3.9 THP hook (reserved for Z-049)

Descriptor flags bit 0 is reserved by this design as
"THP advised on this unit's usable extent". Z-048 always
leaves it 0 and applies no madvise. Z-049, per Z-013's
verdict, is expected to set MADV_HUGEPAGE on pool-in for
units at the 2 MiB decade and above, set the bit in the
same locked section, and clear/keep it by its own recorded
rule on reuse — no layout change is needed when it lands,
which is the point of reserving the bit now.

### 3.10 Locking

The pool lives entirely under the existing global lock:
bucket heads are allocator metadata in static storage,
guarded exactly as the registries are (B-006 S9). In
MIMIR_LOCK_SHARD builds the large path already acquires
the all-shard-then-global superset (Z-039), and the pool
adds no lock and no new ordering edge. The TLS cache is
not involved: large requests bypass it today and still
do; there is no per-thread large pool in this design
(cross-thread large frees pool centrally under the lock,
as they retire centrally today). Re-open trigger: a
Z-043-class storm profile showing the central lock, not
the syscalls, pricing large churn after Z-048 lands.

## 4. Invariants the implementation must keep

Numbered for Z-048's gate to test against. "Unit" = one
pooled mapping with its descriptor.

- MC-01 Exact-pair integrity: every munmap of a pooled
  unit uses the (base, mapping_length) pair captured from
  its validated descriptor into locals before the call;
  no other length is ever unmapped.
- MC-02 Single residence: at every instant, every large
  mapping is in exactly one of {live registry, one pool
  bucket, quarantined set, unmapped}. Never two, never
  none while still mapped (quarantined units are the
  counted exception that keeps this honest).
- MC-03 Counter conservation: published − retired ==
  live + pooled + quarantined, and the same identity for
  bytes, at every locked snapshot; pooled_mappings and
  pooled_bytes gauges equal the buckets' true contents.
- MC-04 Caps are hard: per-bucket ≤ 4, global ≤ 64 units,
  pooled bytes ≤ 64 MiB, eligibility length ≤ 16 MiB —
  checked before admission, never exceeded at any
  instant, including under concurrent frees.
- MC-05 Validation before service: no pooled unit is
  handed out without the full section-3.5 check passing
  in the same locked section that pops it.
- MC-06 Calloc zeros on reuse: a calloc construction
  served from the pool zeroes the whole usable extent
  before publication, identically to the fresh path.
- MC-07 Hit purity: the reuse path performs zero mmap,
  zero munmap, zero madvise calls (auditable by syscall
  count in Z-048's churn gate).
- MC-08 Free cannot fail on the pool: cap refusal and
  ineligibility fall back to munmap; a munmap failure on
  a free leaves the unit POOLED and accounted (never
  half-retired, never lost), with the failure counted —
  the free itself still completes, because the block is
  dead on either branch.
- MC-09 No allocation for bookkeeping: pool metadata is
  static storage plus the units' own descriptors; the
  pool is fully functional before the first malloc
  returns and allocates nothing, ever.
- MC-10 Alignment identity: a reused unit returns
  base + 4096, page-aligned exactly as a fresh unit
  (MA-6.1-MALLOC-01 by analogy, as today).
- MC-11 No VMA surgery: pool code never calls munmap on
  a sub-range of a unit and never calls madvise in
  Z-048 at all.
- MC-12 Contract surface unchanged: nm -D on the shared
  library stays exactly 4; include/mimir_alloc.h is
  untouched; pooled gauges reach tests only through the
  INC-E001-07 shape-(b) internal surface, like drain/trim.
- MC-13 Drain completeness: unload drain leaves zero
  pooled units and zero pooled bytes; trim (Z-050's
  surface, prepared here) can reach every pooled unit.
- MC-14 RSS auditability: pooled resident bytes are
  never deniable — the pooled-bytes gauge plus the caps
  let any RSS reading state the pool's maximum possible
  share (64 MiB) and its current share exactly.
- MC-15 THP honesty: flags bit 0 is 0 on every unit
  until Z-049 sets it, and thereafter means the madvise
  was actually applied to that unit's usable extent.
- MC-16 LIFO per bucket: reuse pops the most recently
  pooled unit of the exact length; eviction for trim
  takes the least recently pooled first.

## 5. Pricing summary — ceilings, not promises

What a hit can save, from the evidence of section 1 (all
this-machine, this slice's quiet-window run plus Z-046's
residual): sparse, the gross ~1.46–3.49 us/op of pair +
endpoint faults, minus the bookkeeping a real hit still
pays (lock, bucket pop, descriptor validation/rewrite,
registry insert — expected in the low hundreds of ns, in
the family of Z-046's 401–840 ns residual, which also
carried the mmap-side work a hit deletes). Z-046's ceiling
analysis stands: expect the built cache to land near
50–75% off the single-block sparse price at the small
decades, less at the large ones — and Z-048's large_batch
re-measurement, not this paragraph, is the grade. Dense,
the ceiling is the probe's 92–95%: a workload that
re-touches its large blocks is where this design earns
most, and mixed_lifecycle (Z-003: LOSS 67.2x) is the
standing it most plausibly moves. What the pool cannot
fix, named so the verdict can be honest: cold misses (the
first allocation of any length pays the full fresh price),
live-set memory cost, the span path's losses (Wave 4's
closed book), and any size above the 16 MiB eligibility
line.

## 6. Rejected alternatives, with their reasons

- Best-fit/range reuse: rejected for Z-048 (3.1);
  re-open on Z-048 churn evidence.
- MADV_DONTNEED on free (RSS-clean pooling): rejected by
  this slice's probe — it repays every fault and saves
  0.2–1.4%; it survives only as Z-050's trim tool.
- MADV_FREE on free: not adopted, not rejected — unmeasured
  on this kernel in any banked run; it is Z-050's candidate
  to price, and Z-048's binary resident-or-gone posture
  keeps its own measurement clean.
- Per-thread large pools: deferred (3.10); the central
  lock is not yet the measured price of large churn.
- Raising the cutoff instead (multi-page spans): priced
  in Z-046 (cutoff 131072 worth 37.2% uniform / 61.3%
  large_batch in the ideal-proxy model) — a real road,
  but a different one: it changes span geometry for every
  small size to reach sizes this cache reaches by keeping
  what the kernel already built. The two compose; they do
  not substitute. The roadmap's order (cache first,
  Z-047/Z-048) is followed.
- Unbounded or time-only retention: rejected by the trim
  lesson (1); a pool without hard caps is glibc's arena
  posture re-implemented, minus thirty years of tuning.

## 7. Open items handed forward, named

- OWED-Z047-01 (to Z-048): pool hit/miss/ins/evictions
  counters join the J-002 stats snapshot as internal
  fields; the churn gate files them per run.
- OWED-Z047-02 (to Z-048): the with-pool AFTER_FREE RSS
  figure (Z-006 method) filed beside the banked figure.
- OWED-Z047-03 (to Z-049): flags-bit-0 protocol (3.9).
- OWED-Z047-04 (to Z-050): eviction order is LRU within
  the byte/count budget (MC-16 gives trim the order);
  DONTNEED as the middle course, priced in section 1.
- OWED-Z047-05 (to Z-052): pooled units are out of mremap
  scope (3.4); growth works on live mappings.
- OWED-Z047-06 (to Z-054): the unscrubbed-reuse posture
  (3.6) is restated in the Wave 6 verdict, not buried.
