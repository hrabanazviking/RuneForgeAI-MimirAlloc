# Phase 8 — Contention Controls: Evaluation + Measured Decision (Slice H-004)

Status: MEASURED DECISION on this host (Raspberry Pi, aarch64, 4 cores,
16 KiB pages) in `tests/perf/recorded_run_h004.txt` (real run,
Sat Nov 7 03:11 EST 2026, base 7c31df9, branch yrsa/mimiralloc-04).
Plain verdict: **keep the single global lock; defer sharding; reject the
bounded remote-free queue as a standalone mechanism.** The measured
models say striped locks and a deferred-free queue both cut pure
lock cost a lot — but they measure lock cost only, and each control
breaks a banked invariant (retention bound, thread-exit, or the
two-registry free path) that no model here can re-prove. H-004 changes
**no `src/` code and no header**: it is the evaluation + decision
slice, per the E-006 precedent. The H-002 baseline it judges against
is `docs/PHASE_8_FOUNDATION_PLAN_H002.md` (INC-H002-01 commissions
this slice).

Companions: `docs/MimirAlloc_Research_and_Development_Roadmap.md`
Phase 8 (roadmap: "Implement and verify cross-thread frees and
contention controls; then consider sharding or bounded remote-free
queues."), `tests/perf/probe_h002.c` (baseline method, reused
exactly), `docs/PHASE_7_RECLAIM_POLICY_G005.md` (RP-1..RP-5),
`docs/PHASE_7_THREAD_EXIT_G006.md` (thread exit), `docs/PHASE_7_UNLOAD_DRAIN_H001.md` (H-001 drain).

## 1. The decision, in one sentence

**Keep the single global lock now, defer sharding until an end-to-end
sharded prototype beats the real allocator by 2x at 8 threads with the
RP-5 bound intact, and reject a bounded remote-free queue as a
standalone control because under the measured burst shape about
two-thirds of cross-thread frees overflow its bound and take the
direct locked fallback anyway.**

## 2. What was measured, and how honestly

`tests/perf/probe_h004.c` (`make test-h004`, standalone, NOT in
`make test`) runs three variants at 1/2/4/8 threads, each line the
median of 3 internal repetitions, and the recorded run repeats the
whole probe 3 times; the medians below are the medians of those
3 recorded runs. The reduction is the H-002 one exactly (sum of
per-thread elapsed, divided by total alloc/free pairs). Timing is
never a correctness oracle: every variant carries independent
pattern checks (seeded byte patterns re-derived by the freeing
thread) and count reconciliation (allocs == frees == slots
returned; 0 pattern failures, 0 null failures, 0 unreturned
slots in every recorded rep).

| Variant | What it is | What it measures |
|---|---|---|
| V0 real | The banked allocator through the public API (status quo single global lock), H-002 Phase B shape (64 B, every 10th 2048 B) | End-to-end contention cost, directly comparable to H-002 |
| V1 shard model | Test-only fixed 64 B slot pool, 4 shards, one mutex per shard; cross-thread free locks the owning shard | Striped-lock cost only; NOT an allocator |
| V2 queue model | Test-only fixed 64 B slot pool; each thread owns a freelist (no lock for own alloc/free); a cross-thread free pushes onto the owner's bounded queue (cap 512) under the owner's queue mutex; the owner drains at its next alloc; on a full queue the freer returns the slot directly into the owner's freelist under the same lock (counted as a fallback, never dropped, never blocking) | Deferred remote-free cost only; NOT an allocator |

## 3. Measured comparison (medians, ns per alloc/free pair)

| Threads | V0 real (status quo) | V1 shard model | V2 queue model | V0 vs 1 thread |
|---|---|---|---|---|
| 1 | 1,967 | 358 | 320 | 1.0x |
| 2 | 3,058 | 515 | 502 | 1.6x |
| 4 | 5,699 | 710 | 677 | 2.9x |
| 8 | 9,969 | 1,163 | 842 | 5.1x |

Reading the numbers honestly:

- The status-quo scaling shape reproduces H-002 (near-linear rise
  with thread count, ~5x at 8 threads here vs ~6.2x there). The
  ABSOLUTE values are lower here than H-002 recorded (1,967 vs
  4,184 at 1 thread; 9,969 vs 25,778 at 8) — see INC-H004-01:
  host load at measurement time differs, so absolute numbers must
  NOT be compared across slices; only the within-run comparisons
  (V0 vs V1 vs V2, and each variant's scaling) are evidence.
- Both models cut the measured lock cost sharply: at 8 threads the
  shard model is ~8.6x and the queue model ~11.8x below the real
  allocator per pair — but those ratios are lock-model ratios, not
  allocator ratios. The real pair also pays the registry walk,
  span acquire/release, and (every 10th) the mapping path, all of
  which any real control must still pay or re-engineer. No
  end-to-end speedup claim is made from these models.
- The queue model's headline number hides INC-H004-02: under the
  H-002 Phase A burst shape (a thread frees 1,500 blocks owned by
  one other thread in one leg, while the owner is itself freeing
  elsewhere and never allocs), the bounded queue overflows
  immediately. Median fallbacks per recorded rep: 2 threads
  1,964 of 3,000 cross frees (65%), 4 threads 3,718 of 6,000
  (62%), 8 threads 7,766 of 12,000 (65%). The queue helps only
  when the owner interleaves allocations; in a burst it is mostly
  a direct locked return with extra bookkeeping.
- The shard model also degrades at 8 threads (1,163 vs 710 at 4):
  4 shards on a 4-core host with 8 threads means two threads per
  shard plus oversubscription — sharding wins while shards
  outnumber contenders and stops winning when they do not.

## 4. Evaluation table (correctness risk + complexity, per control)

### (a) Status quo single global lock — RETAIN (adopt)

| | |
|---|---|
| Correctness risk | None new. It is the design every banked proof assumes: one lock, two global registries, no per-thread state (H-002 Section 2). The two-registry free path, RP-1..RP-5, H-001 drain, and the audit counters are all proven under it. |
| Complexity cost | Zero. No code, no state, nothing to drain at thread exit or unload. |
| Measured cost | The contention price (Section 3): ~5.1x per-op at 8 threads on this run, ~6.2x in H-002. That is the price, stated plainly, not hidden. |
| Decision | RETAIN. The cost is real but it is the only control whose correctness is already proven at this base. |

### (b) Sharding (per-class or striped locks / sharded span registries) — DEFER

| | |
|---|---|
| Correctness risk | HIGH. It is the first per-shard state this allocator has ever had, and it touches every banked invariant: (1) the two-registry free path must find a block's span/mapping without knowing its shard — either the header gains a shard tag (a format change reaching every consumer of the header layout) or every free searches all shards (a lock-per-shard walk that re-introduces the contention it was meant to remove); (2) RP-2 is a per-class retention bound — sharded per-class bounds multiply the RP-5 global bound by the shard count (INC-H004-03) unless retention is accounted globally, which re-introduces a shared counter/lock; (3) the H-001 unload drain must walk every shard under every shard lock in a safe order (new lock-ordering proof); (4) audit counters become per-shard sums whose quiescent read needs a new discipline; (5) G-006's "thread exit is a non-event" proof rests on there being no per-thread state — per-thread shard affinity would void it and need re-proof by H-005/H-006. |
| Complexity cost | HIGH. New lock-ordering discipline, shard routing, per-shard retention accounting, drain and counter changes — a real redesign of src/core, not a local tweak. |
| Measured evidence | The model cuts lock cost ~8.6x at 8 threads (Section 3) — but the model has no registries, no spans, no mmap path, and no retention bound to keep, so it cannot say what a real sharded allocator costs. |
| Decision | DEFER. Do not build it now. |
| Reopen trigger | An end-to-end sharded prototype (test harness first, or a branch) that (i) beats the V0 median at 8 threads by >= 2x on this host across 3 recorded runs, (ii) keeps the RP-5 bound global and measured exactly as G-005 measures it, (iii) passes `make test` plus the G-003/G-005/H-001 suites unchanged, and (iv) carries a lock-ordering and drain proof reviewed against H-001/H-006. Until all four hold, sharding stays deferred. |

### (c) Bounded remote-free queue — REJECT as standalone; DEFER the hybrid

| | |
|---|---|
| Correctness risk | HIGH as a standalone mechanism, MEDIUM as queue-plus-direct-fallback. A queue is the first per-thread state this allocator has ever had: a thread dying with a non-empty queue loses ownership of the queued blocks (G-006 re-proof required; owner-death is H-005's slice), a queue left undrained at dlclose conflicts with the H-001 drain (queued blocks are live by the registries' lights but invisible to the drain), and a full queue must either block the freer (new deadlock surface under contention) or drop (lost ownership — not acceptable) or fall back to a direct locked return (measured below). Cancellation while holding a queue lock is H-006's territory. |
| Complexity cost | MEDIUM-HIGH. Per-thread queues, drain points, exit drain, bound/back-pressure policy, fallback path — a new subsystem with its own failure modes, for a gain that only exists in non-burst shapes. |
| Measured evidence | The model is fast when the queue is used (842 ns/pair at 8 threads) — but under the H-002 burst shape 62-65% of cross frees take the direct fallback (INC-H004-02), so the measured "queue" number is mostly a direct-return number with queue bookkeeping on top. No non-burst interleaved workload was measured; H-007/H-008 own that shape. |
| Decision | REJECT as a standalone control (a bounded queue alone cannot carry burst cross-thread frees without blocking or dropping). The hybrid (queue + direct fallback) is DEFERRED, not adopted: it is the only queue shape that is correct, and its benefit is unproven in any interleaved workload. |
| Reopen trigger | A workload measurement (H-007/H-008) showing cross-thread frees in an interleaved (non-burst) shape where queue occupancy stays below cap with < 10% fallbacks across 3 recorded runs, PLUS a thread-exit drain design for queued blocks reviewed against G-006/H-005, PLUS the H-001 drain extended to queued blocks. Until then the queue stays out of src/. |

## 5. Incidents recorded by this slice (INC-H004-NN)

- **INC-H004-01 (absolute medians are not comparable across slices).**
  This slice's V0 medians (1,967 / 3,058 / 5,699 / 9,969 ns/pair)
  are lower than H-002's recorded medians (4,184 / 8,275 / 16,436 /
  25,778) while the scaling shape matches (~5.1x vs ~6.2x at 8
  threads). Same method, same host, different host load at
  measurement time (parallel teams were running during H-002).
  Consequence: only within-run comparisons are evidence; never
  quote H-002's absolute numbers against H-004's. Same class as
  INC-H002-02.
- **INC-H004-02 (bounded queue overflows under burst cross frees).**
  With cap 512 and the H-002 Phase A burst (1,500 blocks freed
  to one owner in one leg), 62-65% of cross frees at >= 2 threads
  take the direct locked fallback (Section 3). A queue without a
  fallback would block or drop; either is unacceptable. Design
  datum for any future queue work — the fallback is load-bearing,
  not a nicety. Found by the probe's own counter, not by a
  failure: 0 pattern/null/unreturned failures in every rep.
- **INC-H004-03 (sharding multiplies the RP-5 retention bound).**
  RP-2 retains at most 2 empty spans per size class globally;
  sharded per-class bounds would retain 2 per class per shard,
  multiplying the RP-5 worst case (655,360 bytes at 16 KiB pages)
  by the shard count unless retention is accounted globally.
  Recorded as the blocking invariant question for sharding; no
  src/ change was made and none is proposed here.

No other bug or inconsistency was found in src/ during this slice.
No src/ file was touched.

## 6. Phase 8 exit status — NOT exited

Roadmap Phase 8 exit needs "no corrupted live allocations or lost
ownership across repeated stress scenarios; latency and memory
reports archived." This slice does not claim it: H-003 (cross-thread
proof), H-005 (owner death), H-006 (cancellation/teardown), H-007
(stress) and H-008 (latency/memory reports) are still to run, and
H-009 grades the exit. What this slice adds to the exit evidence:
the contention price is measured and the two candidate controls are
judged with reopen triggers, so the wave cannot silently adopt a
control that breaks the retention bound, thread exit, or the drain.

## 7. Slice discipline

No `src/` change; no header change; README untouched
(byte-identical, verified at commit). Files: this doc,
`tests/perf/probe_h004.c`, `tests/perf/recorded_run_h004.txt`, and
one additive Makefile block (`test-h004`, standalone, `make test`
composition unchanged). Public ABI unchanged: `nm -D` exactly 4
(recorded). Banking order: after H-003, per the keeper.
