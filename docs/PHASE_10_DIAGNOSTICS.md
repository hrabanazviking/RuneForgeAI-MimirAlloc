# Phase 10 - Allocation-Free Diagnostics / Error Reporting (Slice J-007)

Slice: **J-007 (team-07)**. Wave 10, Phase 10. The slice
J-001's plan said must be built from zero: an error-
reporting channel for the allocator that is **useful and
does not recursively allocate** (roadmap Phase 10 exit
sentence; J-001 exit item X6; J-001 S5.4). Base:
`1aab76258ccc8af8e29bb482c674484c47d8f117` (J-002 banked)
on branch `yrsa/mimiralloc-07`, worktree
`/home/pi/mimiralloc-team-07` on Raspberry Pi
`purpleviking1` (aarch64, system page size 16384).
Host state: `free -g` 13 GiB available before and after;
Samtal HTTP 200 on port 8787 before and after. Nothing
was stopped, configured or installed. All workloads are
**synthetic** (scripted test legs, fault injection).
Governing texts: `docs/PHASE_10_FOUNDATION_PLAN.md`
(J-001) S2.8, S5.4, X6, INC-J001-04; roadmap Phase 10;
`docs/contracts.md` MA-6.3-020..023 (the reporting rows);
`tests/core/AUDIT_6_3_C006.md` (C-006) incl. INC-C006-02.
Evidence: `tests/core/test_j007.c` (59 checks / 0
failures), `tests/core/recorded_run_j007.txt` (the real
Pi run), regression figures in S7.

> **BOTTOM LINE, STATED FIRST.** The channel exists,
> is wired, and is proven. `mimir_core_diag_report`
> (internal, shape (b)) formats ONE fixed-format line --
> `mimiralloc: error op=<op> cause=<cause> req=<bytes>
> failures=<n> posture=<m>` -- into a 224-byte stack
> buffer from fixed name tables and writes it with
> write(2) to fd 2: no malloc-family call, no L6 call,
> no stdio, no lock on the path, proven by a counting
> interposer while backing allocation was armed to fail
> (test-j007 L2/L5). A report attempted from inside a
> report's own write is suppressed and counted; maximum
> report depth is 1 (L8). Every genuine failure NULL of
> the three allocating entry points emits exactly one
> line with its true cause class (L3-L7, L11); posture
> NULLs, successes and invalid-pointer realloc emit
> nothing, and the J-002 counters keep their disjoint
> semantics (L9, L10, L12). **One recorded deviation
> governs emission: INC-J007-01.** J-001 S5.4's
> default-on reading collides with the banked C-006
> silence battery inside `make test` (MA-6.3-020..023
> graded at zero bytes); this slice keeps both banked
> gates by shipping the channel wired in production
> with emission enabled only when the process carries
> `MIMIRALLOC_DIAG=1` -- an allocation-free getenv gate.
> Flipping the default is owed to the banking
> reconciliation / J-009, named in S5, not hidden.

## 1. Design (the channel)

Location and shape follow the tree's own precedent:
J-004/J-005's debug detection reports from a fixed stack
buffer via write(2) to fd 2 because L6 publishes no
diagnostic channel (INC-J005-05). J-007 generalises that
shape into the production channel, in
`src/core/mimir_core.c` with declarations in the
internal header `src/core/mimir_core.h`:

- **Storage:** one `char buf[224]` on the stack; two
  fixed name tables (op: malloc/calloc/realloc; cause:
  none/backing/arithmetic/lock/page-size/internal);
  three fixed `uint64_t` channel counters (emitted,
  suppressed, max depth) and one `int` last-cause label
  and one `int` re-entry guard in static storage. No
  storage is obtained from the failing allocation path
  or anywhere else (MA-6.3-020's bound, stated: 224
  stack bytes + 40 static bytes, independent of the
  request, the heap and the number of prior reports;
  MA-6.3-023's per-report bound is the same figure for
  every report). The fixed format's worst case is 130
  bytes (longest names, three 20-digit numbers), so the
  bound never truncates a real report; the append
  helpers cap at the buffer regardless (MA-6.3-021's
  truncated-instead-of-unbounded posture, by
  construction).
- **Formatting:** hand-rolled append + decimal
  conversion. No printf family (stdio may allocate and
  locks), no string function that can allocate.
- **Emission:** a bounded write loop over write(2) to
  fd 2 (STDERR_FILENO); a write that returns <= 0 ends
  the attempt and the line is counted suppressed, never
  retried without bound and never escalated.
- **Recursion proof by construction:** the guard is an
  atomic exchange at entry; a nested attempt (a report
  triggered from inside a report) increments the
  suppressed counter and returns before touching the
  buffer, the counters it snapshots, or fd 2. Depth can
  therefore never exceed 1, and the path contains no
  call that can re-enter the allocator: no L6 entry, no
  entry point, no libc allocation entry is named.
- **Health independence (MA-6.3-021/022):** the path
  reads only its arguments and the two J-002 atomic
  counters (relaxed loads). It does not walk the
  registry, dereference allocator metadata, or take the
  global lock, so a report about corrupted or failed
  state does not depend on that state's health; there
  is no initialisation (statics are zero-initialised),
  so the bootstrap path is the same path.
- **Snapshot fields:** `failures` / `posture` are the
  J-002 counters loaded at emit time; on every wired
  path the failure being reported has already been
  counted by `mimir_stats_note_failure`, so `failures`
  is the post-increment value the caller could read
  from a J-002 snapshot -- the counter snapshot at the
  failure point, in the only form that needs no lock.
  `req` is the caller's requested bytes (for realloc,
  the new size); for a calloc product that overflowed
  there is no product value, and `req` carries the
  SIZE_MAX sentinel (INC-J007-08).

## 2. Wiring (which failures report, which never do)

Reports are emitted by the entry points, AFTER the
global lock is released (or, for pre-lock refusals,
where no lock was ever taken) -- never under the lock,
so a slow or blocked fd 2 can never stall the
allocator, and the cause a locked construction path
recorded (`MIMIR_DIAG_FAIL` at each NULL return of
`alloc_locked` / `span_acquire_locked`) is captured
before unlock on the realloc paths.

| Call site | Cause emitted | Proven by |
|---|---|---|
| malloc/calloc/realloc: global lock failure | `lock` | L6 |
| calloc product overflow (checked, pre-lock) | `arithmetic`, req=SIZE_MAX | L3 |
| large path: `size > SIZE_MAX - usable_offset` | `arithmetic` | L4 |
| L6 map acquire failure (large or span path) | `backing` | L5, L11 |
| L6 page-size discovery failure | `page-size` | L7 |
| layout/publication gates (A8/A9/A10, span shape) | `internal` | code inspection; L8 exercises the label directly |
| realloc transfer-step L6 release failure | `backing` (an OS release refusal) | code inspection (the I-004 release arm can drive it; not separately legged here -- recorded honestly) |

**Never reported** (deliberate, semantics-preserving):
zero-size malloc, calloc zero factor and realloc(ptr,0)
posture NULLs (counted in `alloc_posture_nulls`, L9);
invalid-pointer realloc's non-destructive refusal
(nothing counted by J-002, nothing reported, L12);
successful calls (L10); `mimir_free` in all cases
(INC-J007-04); the H-001 unload drain (INC-J007-05).

## 3. Proofs (tests/core/test_j007.c, 59 checks / 0 failures)

The test binary links the library objects plus TWO
test-only shim objects: the banked I-004 fault shim
(--wrap on the four L6 entries) and the new J-007
counting interposer `tests/core/mimir_diag_shim_j007.c`
(--wrap malloc/calloc/realloc/free/write). Neither shim
exists in any production binary.

- **Usefulness (L1, L3-L7, L11):** fd 2 is redirected
  to a pipe in-process; every emitted line is asserted
  FIELD BY FIELD (prefix, op token, cause token, req
  value, failures == the J-002 snapshot value the leg
  predicts, posture likewise, exactly one newline at
  the end, length < 224). Sample, from the recorded
  run: `mimiralloc: error op=realloc cause=backing
  req=300000 failures=7 posture=3`.
- **Zero allocation (L2, L5):** with the interposer
  counters zeroed, a direct emission moves NO
  malloc/calloc/realloc/free counter and NO L6
  counter, while write moves -- and this is repeated
  around a real fault-injected malloc failure (L5),
  where the I-004 acquire counter moves by EXACTLY 1
  (the genuine failed attempt): the report emitted
  from inside the allocation failure performed no
  backing call and no heap call of any kind. L2 also
  arms the I-004 acquire channel during a direct
  emission to show the report still emits while
  allocation is made to fail (the arm is never even
  consumed: the path attempts no acquire).
- **Recursion (L5, L8):** P1 -- L5's exactly-one-
  acquire figure above. P2 -- the interposer's write
  wrapper, armed by the test, attempts ONE nested
  `mimir_core_diag_report` from inside the outer
  report's own write(2): suppressed moves by exactly
  1, emitted by exactly 1, max depth stays 1, and the
  captured bytes are exactly the one outer line.
- **Gate (L0):** with `MIMIRALLOC_DIAG` unset, a
  genuine oversize failure and a direct report call
  emit zero bytes and move no channel counter, while
  `alloc_failures` still counts the failure (J-002
  semantics untouched by the gate).
- **Silence (L9, L10, L12):** posture battery emits
  0 bytes with posture +3 / failures +0; success
  battery 0 bytes; invalid-pointer realloc 0 bytes,
  no counter movement.

## 4. Contract bounds, non-vacuous at last

C-006 graded MA-6.3-020..023 INSPECTED (vacuous bound:
no reporting existed) and recorded INC-C006-02 for
MA-6.3-021's presupposition of a reporting facility.
With the gate enabled, this slice's channel meets the
four rows substantively: bounded storage not from the
failing path (S1: 224 + 40 bytes, fixed), independence
from the reported state's health (no metadata read, no
lock), the same bound on the bootstrap path (no init
exists), and no accumulation across cascading reports
(fixed statics only). The rows' full re-grade belongs
to J-009; nothing here rewrites C-006's audit or its
test (S5 explains why the battery still passes).

## 5. INC-J007-01 -- the enable gate (headline item)

J-001 S5.4 rules that J-007's reporting ships in
production and that the production failure is reported
"in fewer words if need be -- never no words", which
reads as default-on. The banked C-006 evidence test
inside `make test` (test_c006, silence battery)
asserts the opposite observable: zero bytes on fds 1/2
across its failure/success battery -- the Phase 3
baseline fact its audit recorded. Both cannot hold at
once, J-001's plan nowhere flags the collision, and
this slice's commission fixes `make test` at 4590/0.
Resolution taken, recorded rather than smoothed over:
**the channel ships in the production build, wired at
every site in S2 and proven as in S3, with emission
gated on the process environment carrying
`MIMIRALLOC_DIAG=1`.** The gate check is one getenv +
one two-byte compare inside `mimir_core_diag_report`:
getenv allocates nothing (a bounded scan of environ),
takes no lock and stores nothing, so every bound in
S1/S4 covers the gated path whole; a closed gate is a
plain return (no write, no channel-counter movement;
the J-002 counters still move exactly as banked).
What is OWED: a ruling -- at this slice's banking
reconciliation or in J-009 -- on whether the default
flips to on (with C-006's battery amended by its own
reconciliation slice to scope its silence claim to
the gate-closed posture), or the gate stands as the
permanent production posture and J-001 S5.4 is
amended. Until that ruling, X6 grades at most PARTIAL
on its "production emits it" clause: production
*can* emit, identically to the proven test posture,
but does not emit by default.

## 6. Carried items pointing at this slice -- dispositions

- **INC-J005-05 (J-005's write(2) in core, re-home
  owed here): DISPOSITION -- the channel is the
  re-home, and J-005's call site CAN adopt it at
  banking reconciliation, with two recorded
  differences the reconciliation must handle.** (1)
  J-005's debug detection reports and then ABORTS
  (fail-clearly in a debug build); the channel never
  aborts -- adoption means J-005 calls
  `mimir_core_diag_report` (with its own op/cause
  values, e.g. a future OP for quarantine verification)
  for the emission and keeps its abort as its own
  next statement. (2) J-005's debug reports are
  ungated today; channel emission is gated per
  INC-J007-01 -- the reconciliation must decide
  whether corruption detections bypass the gate (they
  are detections, not allocation failures). This
  slice edits NO J-005 file; at this base the debug
  code does not exist, so adoption is necessarily a
  banking-time act.
- **J-006's note (loud failure reporting remains
  J-007's channel): DISPOSITION -- the channel now
  exists as the loud-reporting home J-006 named, but
  J-006's own detected-corruption refusals are NOT
  wired by this slice.** J-006's branch is not in this
  base; its detection sites sit in locked sections of
  code this base does not contain, and wiring them
  needs the same capture-cause-then-emit-after-unlock
  pattern used in S2, applied at banking
  reconciliation when J-006's code and this channel
  first coexist. Until then J-006's refusals remain
  exactly as silent as J-006 recorded. Nothing in
  J-006's branch was edited.
- **INC-J001-04 (no reporting channel; refusal is
  silent):** discharged for the allocating entry
  points' genuine failures, subject to INC-J007-01's
  gate; its second half (invalid/corrupt-path
  loudness) remains open per the J-006 disposition
  above.
- **INC-C006-02 (MA-6.3-021 presupposes reporting):**
  the facility the row presupposes now exists (S4);
  its re-grade is J-009's, with INC-J007-01 attached.

## 7. Verification (all on the Pi, this slice)

From `tests/core/recorded_run_j007.txt` (real run):

- `make test-j007`: **59 checks / 0 failures**; the
  nine captured report lines printed leg by leg.
- `make test`: **4590 checks / 0 failures** (CHECKS
  line, all binaries RESULT PASS), stderr 0 bytes --
  the gate keeps the C-006 silence battery green.
- `make test-j002`: 550 / 0. `make test-i004`: 248 / 0.
  `make test-i003`: 72 / 0.
- `make lib`: `nm -D --defined-only` exactly **4**
  (mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc); diag symbols appear only as LOCAL
  symbols in the .so (hidden visibility + version
  script), 0 in the dynamic table. Only the
  production flavour exists at this base (the
  MIMIR_DEBUG flavour is J-004/J-005's, not in base);
  its nm re-verification at banking is INC-J007-07.
- `README.md` and `include/mimir_alloc.h`
  byte-identical to base (sha256 in the recorded run).
- Samtal HTTP 200 before and after; `free -g` 13 GiB
  available throughout.

## 8. INC register (this slice)

- **INC-J007-01** (headline, S5): emission gate
  `MIMIRALLOC_DIAG=1` reconciles J-001 S5.4 with the
  banked C-006 silence battery; default-flip ruling
  owed at banking reconciliation / J-009. X6 PARTIAL
  on the production-emits clause until ruled.
- **INC-J007-02**: INC-J005-05 adoption is possible
  and specified (S6) but is a banking-time act; the
  gate-bypass question for debug detections is part
  of the same ruling.
- **INC-J007-03**: J-006's detected-corruption sites
  are not wired (not in base); wiring owed at banking
  reconciliation with the S2 pattern (S6).
- **INC-J007-04**: `mimir_core_free` lock failure
  stays silent: free has no NULL return, J-002 counts
  no free failure, and inventing a counter semantic
  was refused. A future slice may wire an op for free
  if it first defines what is counted.
- **INC-J007-05**: the H-001 unload drain still
  discards its released-span count (INC-J001-04's
  second clause, drain half): destructor-time
  emission was not wired or claimed; at unload there
  is no failing caller to inform, and the drain's
  result remains available to any in-process caller
  of `mimir_core_drain_retained`.
- **INC-J007-06** (honest label limit): the cause
  label travels through one static
  (`mimir_diag_last_cause`) written under the lock
  and read by the
  same call after unlock. If two threads fail
  concurrently, one call's report can carry the other
  failure's cause class; op, req and both counter
  fields are always the reporting call's own, and any
  label carried is a genuine concurrent failure's.
  Removing the limit means returning the cause
  through `alloc_any_locked`'s signature -- a
  reconciliation choice for a future slice, refused
  here as churn beyond the slice's need.
- **INC-J007-07** (banking reconciliation): J-004
  and J-005 also modify `src/core/mimir_core.c` and
  append Makefile blocks from this same base. Their
  regions (debug sections under MIMIR_DEBUG) are
  disjoint from this slice's channel section and
  wiring sites by inspection of their banked-held
  diffs, but the union merge at banking must preserve
  the gate + channel whole, and the MIMIR_DEBUG
  flavour's `nm -D` = 4 must be re-verified with the
  channel present. J-003 (docs/tests only) has no
  interaction. J-006's core.c walks are INC-J007-03.
- **INC-J007-08**: `req=SIZE_MAX` in a calloc report
  is a sentinel meaning "the product nmemb*size was
  unrepresentable; no requested-bytes value exists" --
  a reading convention stated in `mimir_core.h`, not
  a literal request size.

## 9. Files

- `src/core/mimir_core.c` -- channel + wiring (S1/S2).
- `src/core/mimir_core.h` -- channel declarations,
  op/cause enums, gate + sentinel documentation.
- `tests/core/mimir_diag_j007.h` -- test-only surface
  (shape (b), includes the core header, J-002 pattern).
- `tests/core/mimir_diag_shim_j007.c` +
  `tests/core/mimir_diag_shim_j007.h` -- counting /
  re-entry interposer (test-only).
- `tests/core/test_j007.c` -- evidence test (59 / 0).
- `tests/core/recorded_run_j007.txt` -- the real run.
- `docs/PHASE_10_DIAGNOSTICS.md` -- this record.
- `Makefile` -- additive test-j007 block at the end
  only. `README.md`, `include/mimir_alloc.h` untouched.
