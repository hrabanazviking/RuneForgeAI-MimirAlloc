# Z13 Budget Enforcement (Z-079, decision D-014)

Roadmap slice Z-079 (Wave 9, L-03 / defect D-04):
Phase-9 soft budgets enforced at last — counters,
failure behaviour, and the budget-failure counter live
(INC-I001-14 discharged). The recorded decision is
D-014 in docs/DECISION_LEDGER.md; this document is its
evidence and procedure statement. The gate is exact:
enforcement tests green; over-budget behaviour proven
(tests/core/test_z079_budget.c; recorded run
tests/core/recorded_run_z13_z079.txt).

## 0. Position and honesty notes

- The design enforced here is not new: it is S3 of
  docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md (I-002),
  written in the first campaign and carried unbuilt
  ever since as INC-I001-14 — the Phase-10 exit
  evidence graded the budget counter "0-by-absence"
  because no budget existed. This slice builds exactly
  that design, with the quantities the banked tree has
  since made exact (Q-MAPPED as the Z-048 definition:
  live mapping bytes + live span bytes + pooled
  mapping-cache bytes, the same sum
  mimir_core_mapped_bytes reports).
- What this slice does NOT do: no public symbol is
  added (nm -D on libmimiralloc.so stays exactly ten,
  verified), the public statistics struct keeps its v1
  layout (S5), and no benchmark verdict is claimed —
  the gate is a correctness/enforcement gate, and every
  graded quantity is an exact counter, size, or return
  value, never a timing.
- docs/LIMITATIONS_AND_KNOWN_DEFECTS.md still states
  L-03 / D-04 in its pre-Z-079 form. D-014 supersedes
  those statements; amending that register is left to
  the same audit posture Z-078 took toward
  contracts.md (its rows are amended by Z-081's
  appendix audit, which owns the next revision of the
  contract-facing registers).

## 1. The decision in one paragraph

The Phase-9 soft budget over Q-MAPPED is adopted and
enforced (D-014): one size_t scalar under the global
lock, 0 = unset (the default, so unconfigured
behaviour is byte-for-byte the banked behaviour);
growth is judged before backing is acquired, at every
growth point; over budget the allocator reclaims what
it may release before it refuses; a refusal is exactly
NULL (ENOMEM for the posix shape), counted once in
Q-REFUSED-BUDGET, and in no other counter. Full text:
D-014.

## 2. Where the gate sits

`budget_admit_locked(new_length, may_reclaim)` in
src/core/mimir_core.c, evaluated under the allocator
lock at exactly the points where backing would grow:

- a fresh large mapping (alloc_locked, on the checked
  page round-up the pool probe computed — a pool hit
  grows nothing and never reaches the gate);
- a fresh aligned mapping (alloc_aligned_locked, same
  round-up);
- a fresh span (span_acquire_locked, one page, plus
  the Z-034 side page in an MIMIR_OOL_META build);
- an mremap growth (grow_mremap_locked, on the delta
  alone; a gate refusal there consumes nothing — the
  caller falls back to the banked move, whose own
  construction re-judges the budget and counts the
  refusal exactly once if it also refuses; a pool-hit
  move grows nothing and may still succeed).

Frees never run the gate. Reuse — TLS bin hits, span
free-list pops, pool hits — grows nothing and is
served regardless of the budget.

## 3. The decision procedure (I-002 S3.3)

1. No budget set: admit (one scalar read).
2. projected = Q-MAPPED + new_length, computed
   overflow-saturated; equality with the budget is
   admitted.
3. Over budget, reclaim before refusal: retained
   EMPTY spans are released one at a time through the
   one trim walk (trim_some_locked, the stop-at-need
   mechanism of I-002 S5.2.3), re-judging after each;
   when no EMPTY span remains, the Z-048 mapping pool
   is released whole and the projection re-judged
   once.
4. Still over: refuse. The gate never touches a live
   span or mapping, never blocks, never retries.

Under MIMIR_LOCK_SHARD the span acquire runs under one
shard lock, where the cross-class reclaim walk would
break shard discipline, so step 3 is skipped there
(may_reclaim = 0): refusal is still exact, only
reclaim-first is not run under a shard lock. The
production build runs the full procedure.

## 4. Counting (I-002 S3.2, FC-4)

The refusal flag is thread-local (the Z-066 label
discipline): a locked construction path sets it when
the gate refuses, and the entry point of the same call
consumes it after unlocking — exactly one refusal per
refused call, tallied on a pre-lock _Alignas(64)
atomic (the Z-064/Z-065 posture) as Q-REFUSED-BUDGET.
A budget refusal is disjoint from alloc_failures
(Q-FAILED-OS) and from alloc_posture_nulls, and emits
no J-007 line: the J-007 channel is wired only to
genuine failures, and a refusal stays distinguishable
from them by counters alone. Entry points covered:
mimir_core_alloc, mimir_core_calloc,
mimir_core_aligned_alloc (and through it
mimir_core_posix_memalign, which answers ENOMEM with
*memptr unchanged), mimir_core_realloc (both grow
courses; the original block stays live, unchanged and
caller-owned — FC-2), and through realloc,
mimir_core_reallocarray. The TLS refill course
consumes the flag on its own path (tls_alloc), so a
small-class span refusal is counted exactly once too.

## 5. Surface (I-002 S3.6, the G-007 pattern)

Internal only, INC-E001-07 shape (b), exactly like the
G-007 tuning pair:

- `mimir_budget_set_mapped_bytes(v)`: stores the
  budget under the global lock; 0 unsets; returns 0 on
  success, -2 on lock failure with nothing changed.
  Setting performs NO release (I-002 S3.4): a budget
  set below current Q-MAPPED binds only future
  acquisitions.
- `mimir_budget_get_mapped_bytes()`: the effective
  budget under the same lock; (size_t)-1 only if the
  lock cannot be taken.
- `mimir_core_budget_failures()`: the lock-free read
  of Q-REFUSED-BUDGET (the Z-064 getter pattern).

Declared in src/core/mimir_core.h, re-declared for
tests only in tests/core/mimir_budget_z079.h, bound
local in the shared library. The internal J-002
snapshot gains budget_failures and budget_mapped_bytes
APPENDED at the end of struct mimir_stats (the Z-064
append discipline; earlier offsets unchanged). The
public mimir_stats_v1 layout is NOT extended: its
versioning rule reserves appends for a version bump,
so the budget pair lives in the internal snapshot
only. Promotion of any of this surface to the public
ABI stays owed to a future contract slice
(INC-I002-03 / INC-I002-05 posture).

## 6. Evidence

make test-z079 (standalone, NOT part of make test):
70 checks, 0 failed, reproduced identically across
three runs on the final tree. Legs: unset baseline and
set/get round-trip; unbounded while unset; the large
path at exact-equality admission, second mapping
refused (+1, OS counter unmoved, nothing acquired),
pooled reuse served, pool reclaimed before a
below-current refusal (+1); retained-EMPTY-span
reclaim before refusal (+1); TLS bin hit served at a
1-byte budget while a fresh class is refused at its
span acquire (+1); calloc (+1) and realloc grow (+1,
original byte-intact) refusals; counter separation
(zero-size moves only posture, oversize moves only
Q-FAILED-OS); the aligned surface (+2, posix ENOMEM
with *memptr unchanged); final identity — exactly 8
refusals, Q-MAPPED reconciled to the drained baseline.
Core compiles -Werror-clean under MIMIR_LOCK_SHARD /
MIMIR_LOCK_HIST / MIMIR_DEBUG / MIMIR_ARENA_CCD /
MIMIR_OOL_META. nm -D: exactly 10 dynamic symbols, no
budget symbol among them. Standing gate
scripts/z13/run_gate.sh on the final tree: GREEN,
4988 checks / 0 failed / 77 by-design skips —
identical to the merged baseline. Full numbers:
tests/core/recorded_run_z13_z079.txt.
