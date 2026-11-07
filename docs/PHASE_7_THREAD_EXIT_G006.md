# Phase 7 — Thread Exit Handling (Slice G-006)

Status: VERIFIED BY EXECUTION and by source inspection on this
host (Raspberry Pi, 16 KiB pages) in
`tests/core/recorded_run_g006.txt` (real run, Sat Nov 7
01:03:27 AM EST 2026, base 5e3987b, branch
yrsa/mimiralloc-06). Plain verdict: **thread exit in
MimirAlloc is a non-event by construction, and this slice
proves it** — the core holds no per-thread state, registers
no thread-exit hook of any kind, and keeps every allocation
in process-global registries under one global lock, so a
thread may exit holding live small and large allocations
and those allocations stay live, intact, and freeable from
any other thread through the two-registry free path.
`pthread_exit` and plain return are observably identical.
G-006 changes **no core source**: no defect was found, so
the deliverable is the proof (`tests/core/test_g006.c`,
standalone `make test-g006`, **44 checks, 0 failures**)
plus this record — exactly the proof-side scope INC-G005-04
assigned. Phase 7 is NOT exited by this slice; the exit
assembly remains G-009's.

Companions: `docs/PHASE_7_FOUNDATION_PLAN.md` (G-001),
`docs/PHASE_7_SMALL_OBJECT_PATH_G003.md` (G-003, the
implementation under proof),
`docs/PHASE_7_RECLAIM_POLICY_G005.md` (G-005, whose
INC-G005-04 scoped this slice and whose RP-1..RP-5 this
slice's results rest on).

## 1. The thread-exit model as implemented

One sentence: **ownership is process-global, never
thread-local — there is nothing per-thread to drain at
exit, and nothing in the allocator runs at thread exit at
all.**

Source evidence (all inspected in this slice; the grep
legs are recorded verbatim in the recorded run):

- **One global lock.** The only lock is the L6 global
  mutex: `src/platform/mimir_platform.c:140`
  `static pthread_mutex_t mimir_global_lock =
  PTHREAD_MUTEX_INITIALIZER;`, consumed through
  `mimir_platform_global_lock/unlock` (lines 144/152).
  Every core entry point takes it, mutates, and releases
  it before returning (B-006 S9 discipline); it is never
  held across a return to a caller, so a thread can never
  exit *while holding* the allocator lock from inside an
  allocator call — the lock is released before any
  allocator call returns, and thread exit cannot happen
  "inside" a call the thread is no longer executing.
- **Two global registries.** All live state lives in two
  file-static registries in `src/core/mimir_core.c`: the
  mapping registry (`mimir_registry_head`, line 192) and
  the span registry (`span_registry_insert/remove`, lines
  627/637), plus the audit counters. No registry entry,
  span, or counter is owned by, keyed to, or annotated
  with the allocating thread.
- **No per-thread state — exhaustive negative search.**
  Recorded-run greps over all of `src/` and `include/`
  for `_Thread_local`, `__thread`, `pthread_key`,
  `pthread_setspecific`, and `pthread_getspecific` all
  exit 1 (no match anywhere). Retention is therefore the
  G-005 global per-class bound
  (`MIMIR_MAX_EMPTY_SPANS_PER_CLASS` = 2, core line 544;
  RP-4), never a per-thread cache.
- **No exit-time code at all.** The same recorded search
  finds no `atexit`, no `pthread_atfork`, and no
  `__attribute__((constructor))` or `((destructor))`
  anywhere in `src/` or `include/`. The allocator
  registers nothing that could run at thread exit or at
  process exit, so there is no destructor-order surface
  of its own (see INC-G006-03).

Consequences, as designed:

1. An allocation's lifetime is bound only to its registry
   entry. The allocating thread exiting — by return, by
   `pthread_exit`, or (for the process's last thread) by
   process exit paths the contract does not claim — does
   not free, move, corrupt, or orphan it.
2. Reclaim still happens exactly where G-005 put it: at
   free time, synchronously, under the lock (RP-3). Exit
   is never a reclaim trigger, because there is nothing
   exit could reclaim that free has not already handled.
3. Freeing is thread-agnostic: `mimir_free` consults the
   mapping registry, then the span registry (G-002 R-4),
   with no check of which thread allocated the block.
   Cross-thread free is not a special case; it is the
   only case the code knows.

## 2. Test coverage map (test_g006, 44 checks)

Method, stated honestly: all counts are exact
audit-counter readings (E-005 mapping counters + G-003
span/object counters, INC-E001-07 shape (b)) taken only
at quiescent points — after every thread of the phase has
been joined; no counter is sampled mid-flight for a
verdict. Small-object and mapping reconciliations are
EXACT (delta == 0); span readings are bound checks
(<= 40, the RP-5 ceiling), because retained empty spans
legitimately stay mapped (RP-2). Patterns are
seeded byte patterns verified over the full requested
extent. Bounds: at most 8 threads live at once, 60 thread
lifecycles in total, 1,421 small allocations and 424
mapping allocations across the whole run — small and
bounded for this Pi, as commissioned.

| Task case | Test phase | What was executed |
|---|---|---|
| (a) exit holding live allocs, freed from main | **T1** | 4 threads: two hold 32 blocks of size 64, two hold 16 blocks of size 256 (span path), each also holds 4 blocks of 2048 (mapping path); all exit still holding. Post-join live deltas exactly **96 small / 16 mappings**; every held pattern intact; main frees all 112 blocks via the two-registry path; counters back to baseline exactly; live spans 2 <= 40. |
| (b) exits while others allocate/free | **T2** | 3 churner threads x 200 iterations (alternating 128 span-path / 4096 mapping-path, alloc+touch+free) run concurrently with 3 holder threads that allocate 16x64 + 2x2048 and exit holding. Churners: **0 failures**; held patterns intact after exit; deltas while held exactly **48 small / 6 mappings**; exact reconciliation after main frees. |
| (c) many short-lived threads | **T3** | 48 lifecycles in 6 batches of 8 concurrent; each thread allocates 20 small (32/96/512) + 2 large (2048), verifies patterns, frees everything itself, exits. After **every** batch: live small == 0, live mappings == 0, live spans == 7 <= 40; total per-thread failures **0**. |
| (d) pthread_exit vs return | **T4** | Identical holder shape (8x64 + 2x2048) run twice: once returning, once via `pthread_exit(NULL)`. Deltas **identical** (small 8, mappings 2 in both forms), patterns intact in both, both reconcile exactly after main frees. The design does not distinguish the forms — it cannot, having no exit hook (section 1) — and execution shows no observable difference. |
| final accounting | **T5** | Fresh small + large allocation after all exits succeeds; totals: small_alloc == small_free == **1421**, mapping published == retired == **424**, live small 0, live mappings 0, live spans **7** (the classes this run touched, x <= 2 retained each — far under the 40 ceiling). |

Recorded-run totals: `make test-g006` exit 0, **44 checks,
0 failures, RESULT: PASS**. Full suite in the same run:
`make clean && make test` exit 0 (final accounting 4590
checks, 0 failed); predecessor regression
`make test-g005` exit 0 (97 checks, 0 failures);
`make lib` exit 0 with **nm -D exactly 4** (mimir_malloc,
mimir_free, mimir_calloc, mimir_realloc). Public header
and README byte-identical to HEAD (sha256
55107f05... for the header, bd7f6614... for README, both
matching `git show HEAD:` in the recorded run). Makefile
diff: +29 additive lines for the standalone test-g006
target only; `make test` composition unchanged.

## 3. Comparison with G-005's idle-thread result

G-005's P3 measured threads that allocate, free
everything, and then sit **idle**: live spans stayed at
the 40-span bound state while idle, and after join the
reading was identical — exit reclaimed nothing further
because reclaim had already happened at free time (RP-3)
and no per-thread state existed (RP-4). G-006 is the
complementary half of the same posture, and the two agree
exactly:

- **What G-005 proved:** exit after freeing is a no-op —
  idle threads contribute zero retention beyond the
  global bound.
- **What G-006 adds:** exit *before* freeing is equally
  safe. Live allocations survive their thread untouched
  (T1/T2/T4: patterns and exact counter deltas), remain
  the process's property in the global registries, and
  free cleanly from a different thread. The bound story
  is unchanged: this run's heaviest exit churn (T3, 48
  lifecycles) never pushed live spans past **7**, and
  retention remains the RP-5 global ceiling, independent
  of how many threads lived and died — thread-count
  independence is now proven at exit, not only at idle.
- **No discrepancy** with G-005 was found in any phase.

## 4. Gaps, surprises, and owed work (INC-G006-NN)

- **INC-G006-01 (no core change — deliberate).** No
  defect was found by any phase, so this slice changes
  no core source, mirroring INC-G005-01. The absence of
  a src/ diff is the finding: the banked design's
  thread-exit posture needed proof, not repair.
- **INC-G006-02 (fork/exec — inspection-covered only,
  NOT executed).** No fork or exec case is exercised by
  this slice, and none is claimed. The exposure, recorded
  honestly: the allocator registers no `pthread_atfork`
  handler (recorded grep), and the public header makes
  no fork-safety claim (U-17..U-21 remain open there).
  If a process forks while another thread is inside an
  allocator call, the child inherits the global mutex in
  whatever state it was in — potentially locked, with no
  owner thread in the child — and the child's first
  allocator call could block. This is the standard
  single-global-lock fork hazard, it is inherited from
  the banked B-006 S9 design rather than introduced by
  Phase 7, and proving or fixing it belongs to a fork
  slice with an explicit contract decision, not to this
  slice's scope. Labelled: INSPECTION ONLY.
- **INC-G006-03 (destructor order — by construction,
  inspection-covered).** Because the allocator registers
  no destructors, no atexit handlers, and no TLS
  destructors (recorded greps), there is no
  allocator-side destructor-order interaction to
  exercise: a *consumer's* destructor that calls
  `mimir_free` on a block it still owns is just a free
  under the lock, covered by T1/T2's cross-thread frees
  in substance. The residual case — a consumer freeing
  from a TLS destructor during another thread's exit —
  reduces to ordinary concurrent free, which the global
  lock serialises; it is not separately executed here.
  Labelled: INSPECTION + the executed concurrent-free
  evidence of T2.
- **INC-G006-04 (thread cancellation — NOT exercised).**
  `pthread_cancel` is never used by this slice's tests.
  Inspection note: allocator code contains no
  cancellation points of its own (no condition waits, no
  `pthread_testcancel`; its syscalls are `mmap`/`munmap`
  and the mutex, of which `pthread_mutex_lock` is not a
  cancellation point under POSIX deferred cancellation),
  so a cancellation delivered to a thread executing an
  allocator call should not be acted on mid-call; but
  "should" here is a POSIX reading, not a measurement,
  and asynchronous cancellation anywhere remains as
  unsafe as it is for any lock-based allocator. Recorded
  as unproven surface, not as a defect found.
- **INC-G006-05 (scale bound of the evidence).** The
  executed evidence is bounded by commissioning: <= 8
  concurrent threads, 60 lifecycles, ~1.8k allocations.
  The design argument (one lock, global registries, no
  per-thread state) has no thread-count term, and T3's
  flat span readings across 48 lifecycles support it,
  but no claim is made at hundreds-of-threads scale on
  this Pi; a scale leg, if ever wanted, belongs with
  G-008's responsiveness work, not here.
- **INC-G006-06 (signal-context exits — out of scope by
  contract).** A thread exiting from within a signal
  handler, or allocator calls made in signal context,
  are outside the banked posture: the public header
  states no async-signal-safety for any entry point
  (MA-6.3-030..032), and this slice adds no claim.
  Inspection only; nothing executed, nothing owed by
  this slice.
