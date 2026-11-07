# Phase 11 — K-006: False-Sharing + Concurrency Scaling Measurement

Slice K-006 (Wave 11, Phase 11 PERFORMANCE RESEARCH). Base:
4ea9f128f46ab498476d0d50b7a1853ee68457cb (K-004 banked; shipped
-O2 build). Branch: yrsa/mimiralloc-06. All measurements run on
the Raspberry Pi Mimir (aarch64, 4 cores, 64-byte L1 data cache
lines per `getconf LEVEL1_DCACHE_LINESIZE`), recorded in
`tests/perf/recorded_run_k006.txt`. Samtal HTTP 200
(127.0.0.1:8787) before and after; free memory 13 GiB available
before and after. Zero failures in all 224 recorded rows: every
scale pattern verified byte-for-byte, every fs counter reached
its exact final count.

## Design

One probe source, two binaries (K-002 pattern):
`tests/perf/probe_k006.c` built as `-DLEG_GLIBC` (host glibc
malloc/free) and as the MimirAlloc leg linked against the
banked production objects in the shipped -O2 flavour (K-004).
Five workloads, thread counts 1/2/4/8:

- `scale` — H-002 Phase B replica: each worker loops 1500
  times alloc / fill with a seeded pattern / verify / free its
  OWN blocks against the shared allocator. Sizes 64, every
  10th 2048 (the H-002 Phase B mix). Reported ns/op is the
  H-002 figure: SUM of per-thread elapsed / total ops, so a
  flat line means perfect scaling and a rising line is
  contention loss.
- `fs_packed` — false-sharing control, no allocator in the
  loop: thread counters packed 8 bytes apart in one shared
  array; each thread increments only its own counter.
- `fs_padded` — same, each counter alone in a 128-byte slot
  (own cache line).
- `fs_alloc16` — each thread allocates ONE 16-byte object
  through the leg allocator, then increments its first word
  in the timed loop. The probe prints the object addresses
  (ADDR lines in the recorded run) so adjacency is checkable
  from the evidence itself.
- `fs_alloc128` — same with 128-byte objects: the
  padded-by-size control for the allocator legs.

Method rules (K-001): every repetition is a fresh process
(7 reps, driven by `tests/perf/run_bench_k006.sh`, legs
alternating within a rep so slow machine drift lands on both
legs); every process runs an untimed warm-up at 1/10 scale
before its timed pass; distributions are summarised by
`tests/perf/summarize_k006.py`. Headline figures are medians
of 7; a single timing is never a verdict. Timing:
CLOCK_MONOTONIC.

## Measured results (medians of 7 fresh-process reps)

### Concurrency scaling — `scale`, sum-metric ns/op

| threads | glibc | MimirAlloc |
|--------:|------:|-----------:|
| 1 | 258 (257–268) | 887 (882–897) |
| 2 | 258 (257–274) | 2293 (2199–2314) |
| 4 | 264 (258–300) | 3569 (3174–3896) |
| 8 | 261 (258–266) | 5536 (4810–6100) |

Wall-clock throughput tells the same story: glibc rises
2.90 → 5.47 → 5.74 → 7.65 M ops/s with thread count, while
MimirAlloc wall throughput is flat at 0.96 / 0.76 / 0.79 /
0.82 M ops/s — no scaling gain at all. That flat wall line
plus a sum-metric ns/op rising 6.2x from 1 to 8 threads is
the signature of a serialised allocator: one global lock
(`mimir_global_lock`, src/platform/mimir_platform.c, recorded
in the H-002 architecture greps) admits one thread at a time,
so added threads only queue. Single-thread cost on the
shipped -O2 build is 887 ns/op vs glibc 258 (3.4x).

### Raw false sharing — fs legs, median wall ops/s

| threads | fs_packed | fs_padded | packed/padded |
|--------:|----------:|----------:|--------------:|
| 1 | 263 M | 282 M | 0.93 |
| 2 | 304 M | 513 M | 0.59 |
| 4 | 420 M | 584 M | 0.72 |
| 8 | 434 M | 616 M | 0.70 |

A real false-sharing effect, honestly sized: packed counters
saturate near 434 M ops/s while padded counters keep scaling
to 616 M — about 1.4x at 4–8 threads, not an order of
magnitude. At 1 thread there is no measurable effect (0.93,
inside the rep spread): with a single writer there is no line
bouncing to lose. This is the loss case stated plainly — the
effect exists only under cross-core contention.

### Allocator-adjacent sharing — fs_alloc16 vs fs_alloc128

ADDR evidence from the recorded run (7 reps, consistent):
MimirAlloc hands out consecutive 16-byte objects at an exact
16-byte stride (e.g. …060, …070, …080, …090), so up to four
thread counters share one 64-byte cache line. glibc's 16-byte
objects sit at a 32-byte stride (two per line). For 128-byte
objects MimirAlloc's stride is exactly 128 (glibc 144), so
every counter owns its line in both legs — the control.

Median wall ops/s:

| threads | glibc a16 | glibc a128 | mimir a16 | mimir a128 |
|--------:|----------:|-----------:|----------:|-----------:|
| 1 | 300 M | 306 M | 302 M | 307 M |
| 2 | 545 M | 549 M | 307 M | 539 M |
| 4 | 588 M | 595 M | 435 M | 595 M |
| 8 | 531 M | 773 M | 469 M | 632 M |

At 1 thread the legs are indistinguishable (no contention,
no sharing loss). Under contention, alloc16 falls behind
alloc128 in both allocators (8T ratios 0.69 glibc, 0.74
MimirAlloc), and MimirAlloc's tighter 16-byte packing shows
the deepest dip: at 2 threads its alloc16 throughput is
307 M against its own alloc128 control at 539 M (0.57).
Finding: false sharing on allocator-adjacent small objects
is measurable in BOTH allocators on this workload, and
MimirAlloc's denser small-object packing puts more counters
per cache line. No code change is made on this evidence:
the slice is measurement-only, and any packing change is a
design decision for a later slice, not a defect fix.

## Comparison to the H-002 baseline (cross-slice)

H-002 Phase B (banked, pre--O2 build) medians of its 3 reps,
same sum-metric: 4184 / 8275 / 16436 / 25778 ns/op at
1/2/4/8 threads — a 6.2x rise. K-006's MimirAlloc leg rises
887 → 5536, also 6.2x. The contention SHAPE is unchanged;
the absolute level is lower under the shipped -O2 build in
an own-blocks-only loop. See INC-K006-03 for why the absolute
figures must not be compared directly.

## Inconsistencies and method limits

- INC-K006-01: the fs legs' ns_per_op is integer wall/ops
  and lands at 1–3 ns — too coarse to rank anything. All fs
  conclusions above rest on median wall throughput (ops/s),
  where the separation is far outside the rep spread.
- INC-K006-02: fs wall time starts before pthread_create
  and includes the 1/10-scale warm-up, so absolute fs ops/s
  is understated. Every fs comparison in this document is a
  like-for-like ratio between legs measured the same way.
- INC-K006-03: the H-002 comparison is cross-slice and
  method-shifted — H-002 Phase B interleaved cross-thread
  frees in the same processes and ran the pre--O2 build.
  Only the scaling shape (ratio) is compared; the absolute
  drop is consistent with K-004's -O2 change plus the
  own-block-only loop, and is not attributed further.
- INC-K006-04: the Pi has 4 cores, so the 8-thread legs
  oversubscribe 2:1 by the assignment's fixed thread set
  {1,2,4,8}. The 8T figures are oversubscribed-contention
  figures and are labelled as measured, not as 8-core
  scaling.

## Verification

- `make bench-k006` exit 0; 224 CSV rows + 112 ADDR lines,
  0 failed rows (`tests/perf/recorded_run_k006.txt`).
- `make test` exit 0 — CHECKS: 4590 run, 0 failed.
- `nm -D --defined-only build/libmimiralloc.so` = exactly 4
  symbols (mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc).
- README.md and include/mimir_alloc.h byte-identical to
  base; no src/ changes in this slice.
- Samtal HTTP 200 before and after; free memory unchanged
  (13 GiB available).

Deliverables: Makefile (additive bench-k006/test-k006 block
only), docs/PHASE_11_FALSE_SHARING_SCALING.md,
tests/perf/probe_k006.c, tests/perf/run_bench_k006.sh,
tests/perf/summarize_k006.py, tests/perf/recorded_run_k006.txt.
