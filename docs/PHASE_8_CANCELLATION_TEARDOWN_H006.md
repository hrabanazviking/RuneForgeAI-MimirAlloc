# Phase 8 - Thread Cancellation + Teardown, Fork Posture (Slice H-006)

Status: EXECUTED on this host (Raspberry Pi, aarch64, 4 cores,
16 KiB pages) in `tests/core/recorded_run_h006.txt` (real run,
Sat Nov 7 03:40 EST 2026, base dacb5e6, branch
yrsa/mimiralloc-06). Plain verdict: **deferred cancellation
is safe exactly where POSIX says it should be - between
allocator calls, and with consumer cleanup handlers that free
allocator blocks - and the two by-construction hazards are now
MEASURED, not just read: a fork timed mid-churn handed the
child a locked global mutex in 2 of 3 probe children, and
asynchronous cancellation of a thread hammering allocator
calls killed the allocator lock in 5 of 6 probe children.**
H-006 changes **no `src/` code and no header**: the gaps found
are contract postures decided in Section 4, not defects a
local patch can honestly fix. Phase 8 is NOT exited by this
slice; the exit packet is H-009's.

Companions: `docs/PHASE_8_FOUNDATION_PLAN_H002.md` (S5 owns
this slice; carried register O-03 fork / O-04 cancellation),
`docs/PHASE_7_THREAD_EXIT_G006.md` (G-006: fork INC-G006-02
and cancellation INC-G006-04 were inspection-only there;
this slice executes what can be executed safely),
`docs/PHASE_7_UNLOAD_DRAIN_H001.md` (the drain Phase C/E
teardown rests on), `docs/PHASE_8_CROSS_THREAD_FREES_H003.md`
(the cross-thread free proof the survivor frees rely on).

## 1. What was run (test_h006, 48 checks, 0 failures)

Method: one binary linking LIBOBJS (E-005 + G-003 audit
counters, H-001 drain) plus a dlopen leg against the .so.
All in-process cancellation is DEFERRED (the POSIX default)
delivered at a nanosleep cancellation point while the worker
waits OUTSIDE allocator calls, because the allocator itself
contains no cancellation point (G-006 INC-G006-04: no cond
waits, no pthread_testcancel; pthread_mutex_lock is not a
deferred cancellation point). Anything that could deadlock
ran in a disposable child process under a parent timeout:
a hang there is evidence, never a stuck test. Bounds:
<= 8 threads at once (max 4 workers + main per phase).

| Phase | Shape executed | Observed behaviour (recorded run) |
|---|---|---|
| A | 4 workers hold 16x64 small + 4x2048 mapping each (80 blocks), cancelled between calls; main survives | Join status PTHREAD_CANCELED 4/4; blocks still live after cancel (64 small / 16 mappings, exact counter deltas); all 80 patterns intact; main frees all; counters reconcile exactly (malloc 80, free 80, small 64/64, mappings 16/16) |
| B | 3 workers hold 8x128 small + 2x4096 mapping each (30 blocks), pthread cleanup handler verifies + frees own blocks; cancelled in consumer wait | PTHREAD_CANCELED 3/3; handlers ran 3/3 and freed 30/30 blocks themselves; handler-side pattern failures 0; counters reconcile exactly (malloc 30, free 30, small 24/24, mappings 6/6) - the frees were done BY the dying threads' cleanup handlers under the global lock, no deadlock, no double free, nothing left live |
| C | Teardown: exited thread's 8 + cancelled thread's 8 leftovers freed from main; 4-thread churn 300 ms; H-001 drain | Leftovers (16 blocks) intact, freed cross-thread, exact reconciliation (small 12/12, mappings 4/4); churn failures 0; nothing live after churn; drain released exactly the 3 retained empty spans live at that point, span published == retired after, second drain no-op, allocator usable after |
| D1 | Quiescent fork (no thread in a call): child verifies parent's 6 inherited blocks, allocates/frees 6 of its own, _exit(0) | Child exit 0; parent's blocks intact; parent counters moved ONLY by the parent's own 6 malloc / 6 free (child increments live and die in the child's copy) |
| D2 | Fork + exec /bin/true | Exit 0 |
| D3 | Fork timed mid-churn (4 churners hammering): child attempts one malloc/free under a 400 ms parent timeout, up to 24 tries | RECORDED RUN: hung on try 1 (tries=1 ok=0 hung=1 crash=0) - child inherited the locked global mutex and its first allocator call blocked; parent SIGKILLed the disposable child. First exploratory run: tries=2 ok=1 hung=1. Combined: **2 of 3 mid-churn fork children hung** |
| D4 | Async cancellation (PTHREAD_CANCEL_ASYNCHRONOUS) of a thread looping malloc/free, inside a disposable child; child's main then attempts malloc under a 5 s parent timeout; 3 trials | RECORDED RUN: ok=1 hung=2. First exploratory run: ok=0 hung=3. Combined: **5 of 6 children lock-died** (post-cancel allocator call never returned; parent SIGKILLed the child). The 1 survivor is the trial where cancellation landed between calls |
| E | dlclose teardown: .so instance, 3 holders cancelled between calls holding 30 blocks; leftovers verified + freed via the .so; dlclose x2 cycles | Cancelled 3/3; 30/30 patterns intact; freed via dlsym'd mimir_free; dlclose returns 0 and the image fully unmaps after EACH cycle (H-001 destructor drain at work); cycle 2 reopens, allocates, frees, unmaps cleanly |

Full suite in the same recorded run: `make clean && make
test` exit 0 (4,590 checks, 0 failed); `make test-h006`
exit 0 (**48 checks, 0 failures, RESULT: PASS**);
`nm -D` exactly 4 public symbols; Samtal HTTP 200 before
and after; README and public header byte-identical to HEAD
(sha256 bd7f6614... / 55107f05..., matching `git show HEAD:`).

## 2. Why cancellation INSIDE a call was never provoked in-process

Deferred cancellation can only be delivered at a
cancellation point, and an allocator call contains none
(Section 1), so a deferred cancel requested while a thread
happens to be inside mimir_malloc/mimir_free is simply
pending until the call returns and the thread reaches its
next cancellation point: by construction it cannot
interrupt the call or strand the lock. Provoking the only
remaining inside-a-call case therefore requires
ASYNCHRONOUS cancellation, whose failure mode is known by
construction before any run: if it lands while the thread
holds the one global mutex, the mutex is never unlocked
and every later allocator call in the process blocks
forever. Running that in-process would hang this test (and
risk the Pi's other tenants gaining nothing). It was run
instead as D4, in disposable children under timeouts,
where the predicted lock-death was observed in 5 of 6
trials - the reasoning and the measurement agree.

## 3. Cancellation + teardown contract posture (decided)

On the executed evidence above, the allocator's posture is:

- **Deferred cancellation is supported.** A thread may be
  cancelled between allocator calls while holding live
  blocks: the blocks stay live and intact in the global
  registries and any other thread may verify and free them
  (Phase A; the cancellation analogue of G-006's exit
  proof - cancellation at a wait point outside calls is,
  to the allocator, indistinguishable from exit).
- **Cleanup handlers may free allocator blocks.** A
  consumer cleanup handler that calls mimir_free during
  cancellation unwinding works exactly as observed in
  Phase B: the handler runs in the dying thread, the lock
  is free at that moment (the thread was not inside a
  call), frees reconcile exactly. This was the dangerous
  case H-002 Section 5 named; executed, it is benign
  under deferred cancellation.
- **Asynchronous cancellation is UNSUPPORTED** anywhere
  it can land inside an allocator call (D4: 5/6 lock
  deaths). A consumer must not enable asynchronous
  cancellation on a thread that calls this allocator.
  This is a posture, not a fixable defect: no lock-based
  allocator survives async cancel inside its critical
  section, and no in-slice hack (trylock loops, signal
  masks around calls) was attempted - masking the risk
  would change semantics for every caller to defend a
  usage POSIX itself declares unsafe with non-async-safe
  functions.
- **Teardown is orderly shutdown, already built:** free
  everything (from any thread, including leftovers of
  exited/cancelled threads - Phase C), then the H-001
  drain releases retained empty spans, directly via
  mimir_core_drain_retained() in-process or automatically
  via the image destructor at dlclose (Phase E). Live
  blocks at dlclose keep the H-001 posture: their spans
  are NOT drained (INC-G008-03 stands).

## 4. Fork posture - DECISION (closes O-03 as a decision)

**Decision: fork is supported only in two shapes -
(1) fork from a quiescent state (no other thread inside
an allocator call, e.g. a single-threaded phase), after
which the child may allocate and free normally (D1,
executed); and (2) fork followed promptly by exec (D2,
executed). A fork that can race another thread's
in-flight allocator call is UNSUPPORTED for allocator
use in the child: the child may inherit the global mutex
locked by a thread that does not exist in the child
(D3, executed: 2 of 3 children hung on their first call),
so such a child must not call the allocator - it may
exec, or _exit.** Rationale: the hazard is real and now
measured, but the supported shapes cover the honest use
cases (worker spawn from a quiet parent, fork+exec), and
the standard remedy - a pthread_atfork handler that
reinvents lock state in the child - cannot be made sound
for a mutex that may be mid-mutation at fork time without
a redesign of the locking discipline; it is recorded as
future work (INC-H006-03), not bolted on here. The public
header is UNCHANGED: it states no fork claim (U-17..U-21),
and this decision lives in this slice record as the
project's posture until a contract slice promotes it -
the keeper is told explicitly: no header change was made
or is required by this decision.

## 5. Incidents (INC-H006-NN)

- **INC-H006-01 (mid-churn fork inherits locked mutex -
  EXECUTED, posture decided).** D3: 2 of 3 disposable
  children forked under churn blocked forever on their
  first allocator call and were SIGKILLed by the parent
  timeout. This is the G-006 INC-G006-02 / G-009 O-03
  hazard, no longer inspection-only. Disposition: the
  Section 4 decision (unsupported shape), not a code fix.
- **INC-H006-02 (async cancellation is lock-fatal -
  EXECUTED, posture decided).** D4: 5 of 6 disposable
  children never returned from the post-cancel allocator
  call. Closes G-006 INC-G006-04's open question by
  measurement for the async case; deferred cancellation
  (the default) is fully proven by Phases A/B. Disposition:
  Section 3 posture (unsupported), no code change.
- **INC-H006-03 (no pthread_atfork handler - accepted,
  future work).** src/ still registers no atfork handler
  (grep-verified posture from G-006 stands). If a future
  contract slice wants fork-while-busy support, it must
  redesign the lock discipline first (e.g. atfork prepare/
  parent/child handlers around a lock whose invariants
  survive copying); until then Section 4 governs.
- **INC-H006-04 (no hardening gap found in scope).** No
  defect was found in the supported shapes, so this slice
  changes no src/ - mirroring INC-G006-01. The absence of
  a src/ diff is deliberate: A/B/C/E all passed as built.
- **INC-H006-05 (evidence bounds).** Cancellation proof is
  bounded at <= 4 concurrent workers (+ main), fork probes
  at 3 (D3) and 6 (D4) disposable children across two runs
  on this Pi. The hang RATIOS (2/3, 5/6) are observations
  from small samples, quoted as counts, not rates; the
  qualitative verdict (both hazards are real) needs only
  one hang each and has two and five respectively.

## 6. Phase 8 exit status (honest)

Phase 8 is **NOT exited** by this slice. H-006 discharges
its own queue item completely: roadmap S5 (cancellation
and teardown) is executed and the O-03 fork posture is
decided, O-04 is closed by measurement. Remaining for the
exit packet (H-009): H-005 owner death, H-007 stress under
oversubscription, H-008 latency/memory reports; exit
criteria E1/E2 are H-009's to grade on the whole wave.

## 7. Slice discipline

Files: this doc, `tests/core/test_h006.c`,
`tests/core/recorded_run_h006.txt`, one additive Makefile
block (+24 lines, `test-h006` standalone, `make test`
composition unchanged). No src/ change; public header and
README byte-identical to HEAD (recorded-run sha256 proof).
Public ABI unchanged: nm -D exactly 4. Banking order:
H-005 first, this slice after, per the keeper.
