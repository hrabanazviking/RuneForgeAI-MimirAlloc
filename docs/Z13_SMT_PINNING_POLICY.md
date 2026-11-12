# Z13 SMT and Pinning Policy (Z-041)

Status: PROVISIONAL - superseded automatically for the arena variant when the Z-038 arena ladder banks (see Owed, below).
Scope: allocator-heavy loads on purpleraider (ASUS ROG Flow Z13, AMD Ryzen AI MAX+ 395, 16 physical cores / 32 threads, 2 CCDs, SMT on - the shipped host state). Workload classes measured: small-object churn and mixed-size batch alloc/free (K-002 small_churn / mixed_batch), same-thread alloc/free only.
Evidence: tests/perf/recorded_run_z13_z041.txt (two full invocations, Z-004 protocol), on top of banked Z-011 (bandwidth saturation), Z-012 (CCD/SMT behaviour), Z-020/Z-021 (contention ladders). Grades: mimir pinned figures PROVEN (cross-invocation agreement <= 2.41%); glibc magnitudes INDICATIVE (short passes, Z-021-class variance), glibc directions proven where they hold in all measured pairs.

## Policy table

Aggregate throughput, median ops/sec (mimir: invocation a2/a3; glibc: indicative range across invocations).

| Posture | mimir small_churn | mimir mixed_batch | glibc small_churn | glibc mixed_batch | Variance (spread) | Verdict |
|---|---|---|---|---|---|---|
| Floating, 8 threads | 16.9M / 26.0M (bimodal) | 16.6M / 15.6M | ~1.27B | ~1.17-1.29B | 41-94% (mimir) | REJECTED at low thread counts |
| Pinned, 8 threads, one CCD (CPUs 0-7) | 31.6M / 30.9M | 31.1M / 30.7M | 1.31-2.26B | 1.36-1.74B | <= 1.6% (mimir) | RECOMMENDED for mimir-class efficiency |
| Floating, 16 threads | 22.3M / 22.6M | 21.5M / 22.1M | ~2.50-2.67B | ~2.37-2.38B | 29-53% (mimir) | Tolerable for mimir; rejected for latency-sensitive work |
| Pinned, 16 threads, all physical (0-15) | 22.8M / 22.2M | 21.3M / 21.1M | 2.93-4.40B | 2.76-4.26B | <= 5.9% (mimir) | REJECTED for mimir-class (worst pinned posture); fine for scalable allocators |
| Floating, 32 threads | 33.2M / 32.4M | 32.8M / 37.2M | ~2.91-3.19B | ~2.72-3.15B | 6-31% (mimir) | Tolerable aggregate; migration wholesale (26-30 of 32 workers migrate) |
| Pinned, 32 threads, all logical (0-31) | 33.2M / 33.8M | 39.9M / 39.7M | 4.62-5.45B | 3.31-4.49B | <= 6.6% (mimir) | RECOMMENDED default for allocator-heavy loads |

Ratios of record (a2/a3): SMT pinned 32/16 - mimir 1.46/1.52 (small_churn), 1.87/1.88 (mixed_batch), PROVEN; glibc direction 4/4 pairs positive (1.05-1.58, indicative). CCD pinned 16/8 - mimir 0.72/0.72 and 0.69/0.69, PROVEN (spreading a sharing-heavy allocator across both CCDs at 16 threads LOSES ~30%); glibc direction 4/4 positive (1.30-3.37, indicative). Pin/float at equal thread count - pinned >= floating in 20 of 24 leg/workload/invocation pairs; all four exceptions are mimir cells at 16-32 threads inside a 0.95-1.00 tie band, and glibc is 12 of 12 for pinned.

## Recommendations

Default posture for allocator-heavy loads on this machine: PIN, and size by allocator class.

1. Pin threads to CPUs (identity order: physical cores first 0-15, SMT siblings 16-31). Pinning never measured worse than floating beyond a tie band, removes the floating variance (mimir spreads <= 6.6% pinned vs up to 93.7% floating), and at low thread counts prevents the scheduler bimodal packing collapse (floating 8-thread mimir reps split ~16M/~31M inside one invocation).
2. Scalable allocators (glibc class - per-thread caches, no central serialisation): run 32 threads pinned. Scaling is monotonic 8 -> 16 -> 32 in every measured pair; SMT is a proven-direction gain on top of physical-core scaling.
3. The current banked MimirAlloc core (TLS cache over shared central structures): do NOT run 16 threads pinned across both CCDs - it is the worst pinned posture (0.69-0.72x of the 8-thread one-CCD posture, proven twice). Choose per goal: 8 threads pinned inside one CCD (CPUs 0-7) for per-thread efficiency (~3.9M ops/sec/thread), or 32 threads pinned for best aggregate (39.7-39.9M mixed_batch ops/sec, 1.28-1.29x the 8-thread aggregate).
4. Bandwidth-bound work is a different class and keeps the Z-011 answer: saturation at 16 physical threads, SMT 4-10% below - do not read recommendation 2/3 onto streaming loads. SMT pays when the bottleneck is serialisation/cache latency (the allocator path), not when it is DRAM bandwidth.
5. Floating is acceptable only as a fallback at 16-32 threads for mimir-class loads (median tie band) and never for latency-sensitive allocator work: a median 26-30 of 32 floating workers migrate mid-pass, and floating spreads reach 94%.

## What not to do

- Do not impose one machine-wide thread cap: the 16-thread answer has opposite signs for the two allocator classes (finding: the disagreement is allocator-attributable, not hardware-attributable).
- Do not quote glibc magnitudes from the Z-041 run as precise: they are indicative; only their directions are proven.
- Do not apply this policy to layouts not in the matrix (below) or to remote-free-heavy / producer-consumer patterns - Z-043 owns those.

## Not tested (named, not inferred)

- SMT disabled at host level (no host setting was changed; this policy covers the shipped SMT-on state only).
- Sibling-packed 16 threads (16 threads on the 8 physical cores of one CCD, CPUs 0-7 + 16-23).
- NUMA-style first-touch placement policies (this host is a single NUMA node, UMA).

## Owed

- OWED-Z041-01: re-run this matrix on the Z-037/Z-038 per-CCD arena variant before quoting any posture for it; this document is superseded for that variant when the Z-038 ladder banks.
- OWED-Z041-02: the sibling-packed 16-thread layout above, if a future slice needs a 16-thread mimir posture better than the one-CCD 8.

## Decision ledger entry

Z-041 | Z13 SMT/pinning policy | PROVISIONAL
Evidence: Z-011, Z-012, Z-020/Z-021, Z-041 matrix (purpleraider only, two invocations, Z-004 protocol)
Decision: pin by default; 32 pinned for scalable allocators and for best mimir aggregate; 8 pinned one-CCD for mimir per-thread efficiency; 16 pinned cross-CCD expressly rejected for the current mimir core
Scope: small_churn + mixed_batch, same-thread alloc/free, SMT-on host state, layouts in the policy table only
Superseded-by: Z-038 arena ladder (for the arena variant)
Owed: OWED-Z041-01, OWED-Z041-02
