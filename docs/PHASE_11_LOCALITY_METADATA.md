# Phase 11 Locality + Metadata Placement (K-005)

Slice K-005 (team-05), Wave 11, Phase 11. Base e04dd8b
(Wave 11: K-001..K-003 banked). Worktree
/home/pi/mimiralloc-team-05, Pi purpleviking1.
Deliverables: tests/perf/bench_k005.c,
run_bench_k005.sh, summarize_k005.py, run_perf_k005.sh,
recorded_run_k005.txt, Makefile bench-k005 block
(standalone, not in make test), this doc. No src/,
README.md or public-header change.

> **VERDICT (plain first). No placement change is made.**
> The measured legs give no gain a change could claim:
> random access costs only ~10% over sequential at the
> 64-byte class and is flat at 1024 bytes; grouping a
> mixed live set by class is SLOWER than allocation
> order (~49% at median); dTLB miss rate is ~0.00% on
> every access leg. The one large measured locality
> cost is structural and already known: the large
> mapping-per-allocation path faults 2 fresh pages per
> churn op and costs ~2.2x the small-span churn op.
> That is the mapping design itself (Phase 3/7 banked),
> not a metadata-placement defect this slice may
> redesign on this evidence. Honest no-change verdict
> per roadmap: profile before adding complexity.

## 0. Flavour note (K-004 reconciliation owed)

K-004 (team-04, shipped -O2 CFLAGS) is banking in
parallel and is NOT in this base. Every bench leg here
links THIS base's shipped objects: LIBOBJS built with
the banked CFLAGS carrying no -O level (-O0); the
bench harness alone is -O2 (bench_k002 precedent).
All figures describe the -O0 shipped build. If K-004
lands first, the keeper's banking of this slice owes a
reconciliation note: absolute ns figures will move
under -O2; the within-allocator pattern comparisons
(seq vs rand, grouped vs alloc-order, small vs large
churn, TLB rates) are the claims, not the absolutes.

## 1. Locality map (SOURCE ANALYSIS, not measurement)

Read from src/core/mimir_core.c + mimir_core.h at base.
Page size on this Pi: 16384 (16 KiB).

### 1.1 Small path (requests 1..1024, 20 classes)
- One span = exactly one 16 KiB page. Span header:
  96 bytes at offset 0 of that same page (struct
  mimir_span_header, static-asserted 96 B): magic,
  mapping_base/length, usable_offset=96, class_size,
  usable_size, state, class_index, registry
  list_next/list_prev, free_head, live_count,
  carve_count.
- User objects are carved sequentially from offset 96
  at the class stride. Header and user data are
  IN-PAGE, usually the same page and within the first
  cache lines of it: header = 96 B = 1.5x 64 B lines.
- Free list is IN-BAND: free_head in the header;
  each free object's first 8 bytes hold the next
  pointer (*(void**)obj). A free object costs 0 extra
  metadata bytes; a live object carries no in-band
  metadata at all.
- Capacity per span: (16384-96)/class: 254 objects at
  64 B, 15 at 1024 B. Header amortised cost: 0.38 B
  per 64 B block, 6.4 B per 1024 B block.
- Plausible cache footprint of one small allocation:
  span-header lines (touched under the global lock on
  alloc/free) + the user block (64 B class = exactly
  one 64 B line for a full-block touch; first-byte
  touch = 1 line at any class). No separate metadata
  page, no extra TLB entry: header and block share
  the span's single page/TLB entry.
- Retention: at most 2 EMPTY spans per class retained
  by default (20 classes x 2 x 16 KiB = 640 KiB max).

### 1.2 Large path (requests >1024, mapping-per-alloc)
- Each allocation is its own mapping:
  [reserve page 16 KiB][usable region page-rounded].
  The 96-byte descriptor (struct mimir_descriptor,
  same field discipline) sits at offset 0 of the
  reserve page; the allocating-thread record sits at
  offset 96 of that same page (J-002). User data
  starts exactly one page after the descriptor.
- Metadata placement is OUT-OF-PAGE relative to user
  data: descriptor and user block never share a page,
  cache line, or TLB entry. Metadata cost per large
  allocation: one whole 16 KiB page, of which 96 B
  (+thread record) is used - plus the global registry
  list threaded through the descriptors themselves.
- Consequence by construction (analysis): every large
  alloc/free pair is at least one map + one unmap and
  touches at least 2 pages; no reuse across ops.

### 1.3 What is NOT in either path
No per-object header prefix, no footer, no out-of-band
per-object table for small blocks, no thread-local
cache (INC-G003-04 posture stands). Global registry
anchors/counters are fixed static storage.

## 2. Measured legs (recorded_run_k005.txt)

Contract: M-1 7 fresh processes (all 8 workloads per
process, fixed order - see INC-K005-02), M-2 full
repeat distribution + median/min/max in the recorded
run, M-3 direct linkage to banked LIBOBJS named,
M-5 conditions in the recorded run (kernel
6.12.112-v8-16k, page 16384, gcc 12.2.0, governor
ondemand, temp 57.85 C at perf start, free 13G avail,
Samtal 200 before/after), M-6 warm-up per process
(1 access pass / 1/10 churn), M-7 raw data committed.
Checksums identical across seq/rand and across
alloc-order/grouped legs in every rep (equivalence
checkable from the recorded run). perf: present,
perf_event_paranoid=2, user counters permitted and
actually read (cycles/cache/dTLB, 3 reps per access
workload, whole-process counters - that scope is a
caveat: build+access+free are all counted).

Medians (ns per access/op, 7 reps):
| Leg | Median | Min-Max | Note |
|---|---|---|---|
| seq64 | 1.87 | 1.73-3.61 | 4096 live 64B, 200 passes |
| rand64 | 2.05 | 2.03-2.48 | same set, permuted |
| seq1024 | 4.32 | 4.02-5.13 | 1024 live 1024B |
| rand1024 | 4.22 | 3.85-6.57 | FLAT vs seq (loss case for the random-costs hypothesis) |
| mixed_allocorder | 3.03 | 2.94-4.22 | interleaved 64/1024 set |
| mixed_grouped | 4.52 | 4.31-6.23 | grouped SLOWER ~49% |
| churn64 | 2851.71 | 2813.74-2931.36 | 100k alloc/touch/free |
| churn8192 | 6233.15 | 6182.27-6472.17 | 5000 mapping-path ops |

Perf medians-of-3 (whole process): cache-miss rate
seq64 1.24-1.31% of refs vs rand64 5.03-5.04%
(~4x the miss RATE, yet only ~10% time: the 256 KiB
set fits L2/L3, so misses are served on-chip);
seq1024 6.00-6.01% vs rand1024 6.13% (flat, matching
the flat timing); mixed grouped 3.71-3.75% vs
alloc-order 3.55% (slightly worse, matching timing).
dTLB-load-miss rate: 0.00% on every leg (absolute
misses ~0.8-0.9k per whole process incl. startup).
Page-fault legs (ru_minflt deltas, medians): churn64
timed loop 0 faults in 100k ops (one retained span
serves all); churn8192 timed loop 10000 faults in
5000 ops = exactly 2.0/op (reserve + usable page,
Section 1.2 by construction, now measured); live-set
builds: 64B set 20 faults for 17 spans, 1024 set 69
for 69 spans, mixed 70 - span counts from Section
1.1 arithmetic, faults track them closely.

## 3. Findings
- F1. Span packing is the locality story: 4096 live
  64 B blocks occupy 17 pages / 17 TLB entries. No
  TLB pressure exists at these scales on 16 KiB
  pages; a TLB-motivated placement change has no
  measured problem to solve here.
- F2. Metadata/user adjacency in the small path is
  already maximal (same page, header 96 B, in-band
  free list, 0 B per live object). There is no
  out-of-band small metadata to move closer.
- F3. The large path's separate reserve page costs
  measurably (2 faults/op, ~2.2x churn ns/op vs the
  span path) but that is the banked mapping design's
  isolation price, and the comparison crosses size
  classes and paths - it does not isolate metadata
  placement as the cause (memset of 8192 B and the
  map/unmap syscalls dominate by construction).
  Recorded as evidence for a future design slice,
  not acted on here (one-change/M-11 discipline).
- F4. Access-order effects are small or negative:
  random ~+10% at 64 B, flat at 1024 B, and
  class-grouped access to a mixed set LOSES to
  allocation order. Any caller-side or allocator-
  side reordering scheme is contradicted, not
  supported, by this data. (Mechanism for the
  grouped loss is NOT established; stated as a
  measured result only.)
- F5. Metadata costed: small = 96 B/span (0.38-6.4 B
  per block amortised) + 8 in-band bytes per FREE
  block only; large = 16384 B reserve page per live
  mapping. In ns: no separable small-path metadata
  timing exists in these legs - alloc/free cost is
  the lock + path cost already baselined by K-002.

## 4. INC-K005 register
- INC-K005-01: K-004 flavour reconciliation owed at
  banking (Section 0): absolutes are -O0-base only.
- INC-K005-02: workloads run in fixed order within
  each process (M-1 alternation applies across
  processes only for single-allocator legs); drift
  within a process cannot be fully excluded - the
  seq/rand gap (~10%) is near this noise class and
  is claimed only as small, never as precise.
- INC-K005-03: perf counters are whole-process, not
  per-phase; per-phase counter attribution is owed
  to any future slice that needs it.
- INC-K005-04: F3's large-path cost is not isolated
  to metadata placement; a design slice wanting to
  act on it must first isolate map/unmap vs memset
  vs descriptor-touch costs.
No bug was discovered in allocator source by this
slice; the one build failure met (misleading-
indentation in this slice's own harness) was fixed
in the harness before any run was recorded.
