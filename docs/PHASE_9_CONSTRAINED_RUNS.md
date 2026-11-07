# Phase 9 Constrained-Memory Runs - Slice I-005

Status: complete on Linux (Raspberry Pi Mimir, aarch64,
16 KiB pages). Evidence: tests/core/test_i005.c, recorded
run tests/core/recorded_run_i005.txt (`make test-i005`,
standalone, NOT part of `make test`).

## 1. What this slice proves (exit item X5)

X5 asks for recorded runs under a stated address-space
limit, in the C-008 Section 9.3-2 child form or stronger,
with behaviour documented and bounded: refusal exactly
NULL, bounded in time, live blocks non-overlapping, and
the allocator healthy afterwards.

Form used (stronger than C-008 in two ways): each leg is
a forked child that calibrates its per-process RLIMIT_AS
from its OWN measured VmSize baseline (/proc/self/status)
plus a headroom stated in exact page arithmetic, instead
of C-008's single fixed 96 MiB figure; and the parent
enforces a stated 30 s wall bound per child (poll
waitpid, SIGKILL on expiry) instead of relying on the
script merely finishing.

Page arithmetic (measured, asserted in Leg 0): page
16384 B; small cutoff 1024 B; a large 4096 B block costs
2 pages of VA (1 reserve page + 1 usable page); a small
span costs 1 page. Per-process RLIMIT_AS only - no
system-wide limit was ever set; Samtal (HTTP 200) and
free memory (13 GiB available) were checked before and
after the recorded run and were undisturbed.

## 2. Legs and observed results (recorded run)

- A (64 pages headroom): large-fill served exactly 32
  blocks of 4096 B before a clean NULL - exactly
  64 pages / 2 pages per block. All live blocks pairwise
  non-overlapping, patterns intact, allocator served
  again after all were freed. 6/6 checks.
- B (64 pages): after filling to NULL, freeing two large
  blocks (4 pages) let a large request (2 pages) and then
  a small 64 B request (1 span page) both be served in
  the freed headroom. 3/3 checks.
- C (16 pages): realloc of a 4096 B block to 8 MiB under
  the limit returned exactly NULL and left the original
  byte-identical and freeable; allocator healthy after.
  4/4 checks.
- D (128 pages, largest level; trim under pressure):
  1200 small 64 B blocks freed left 5 retained empty
  spans (81920 B, exactly 5 pages, retention bound raised
  to 16 via the I-003/G-007 test-only tune). Large-fill
  then served exactly 61 blocks - floor((128-5)/2) -
  before NULL. mimir_core_trim_retained() released
  exactly the 5 retained spans, retained bytes went to
  0, and a large request was then served under the same
  limit. 8/8 checks.
- E (64 pages): raw 1-page mmap filled the headroom with
  exactly 64 mappings before MAP_FAILED; both a large
  and a small mimir request returned exactly NULL while
  VA was exhausted by mappings the allocator does not
  own; after munmap the allocator was healthy. 4/4 checks.
- F (parent, unlimited): fresh-process health probe -
  malloc pattern and calloc zeroing both correct after
  all constrained children. 4/4 checks (parent total
  10 checks including Leg 0 and the five leg verdicts).

Totals: 35 checks, 0 failures; every child finished in
~50 ms against the 30 s bound (poll granularity 50 ms,
so times are upper bounds at that resolution). Full
`make test` exit 0; nm -D on libmimiralloc.so exactly 4;
README.md and include/mimir_alloc.h byte-identical to
base. No src/ change in this slice.

## 3. Evidence class - read this before citing

The failures in this slice are REAL OS failures: the
kernel refused mmap because the process hit RLIMIT_AS.
That is a different evidence class from I-004's INJECTED
failures (link-time --wrap shim failing the Nth platform
call). I-004 proves the core's failure-handling contract
against a controlled fault; I-005 proves the same
contract surface (exactly-NULL refusal, no overlap,
failure-atomic realloc, post-failure health) against
the operating system itself, plus bounded behaviour
when VA is consumed by mappings outside the allocator
(Leg E). Neither substitutes for the other.

## 4. Honest grading of X5

X5's own text scopes the deliverable to Linux, with the
Android and proot tracks named as not run. On that
scope: the Linux requirement is now MET by designed,
calibrated, recorded runs (this slice), superseding
C-008 S93-2's small-scale partial form. Android: NOT
RUN. proot: NOT RUN. Both are named here exactly as X5
prescribes; no result is claimed for them. The
PHASE_9_FOUNDATION_PLAN.md X5 row update is the
coordinator's wave-bank edit, not this slice's.

## 5. INC-I005 register

- INC-I005-01 (build portability, fixed in-slice):
  MAP_ANONYMOUS is undeclared when _POSIX_C_SOURCE is
  set without _DEFAULT_SOURCE on this glibc; the test
  now defines _DEFAULT_SOURCE. No production code was
  affected (the core does not use the macro).
- INC-I005-02 (test-design defect, fixed in-slice, NOT
  a core defect): Leg B's first draft freed one large
  block (2 pages) and expected both a small span
  (1 page) and another large block (2 pages) to fit;
  the small span starved the large request, exactly as
  the arithmetic says it should. The leg now frees two
  blocks (4 pages) and orders large-then-small. The
  allocator behaved correctly throughout.
- INC-I005-03 (exact VA accounting): observed success
  counts match the stated per-allocation VA costs
  exactly in Legs A, D and E - no unaccounted VA
  consumption by the allocator was observed within
  these legs' resolution (whole pages).
- INC-I005-04 (bound granularity): child wall times
  are reported at the parent's 50 ms poll granularity;
  "~50 ms" means "finished before the second poll",
  not a measured 50 ms duration.
- INC-I005-05 (tracks not run): Android and proot
  constrained runs remain unrun and unclaimed (see
  Section 4).
