# Z13 Zero-Size and Errno Contract (Z-078, decision D-013)

Roadmap slice Z-078 (Wave 9, L-09): U-09 / U-12 / U-10
decided by recorded decision, implemented, and stated
in the public header as contract, not posture. The
recorded decision is D-013 in docs/DECISION_LEDGER.md;
this document is its evidence and row-by-row statement.
The gate is exact: header text and tests match the
decision byte for byte in behaviour
(tests/abi/test_z078.c; recorded run
tests/abi/recorded_run_z13_z078.txt).

## 0. Position and honesty notes

- Before this slice, zero-size behaviour, realloc-to-
  zero behaviour and the errno posture were the oldest
  open contract items in the tree: open since the
  Phase 3 design record (B-006 F-05, B-007 A1/A4/D.1.1,
  B-008 S3/S4), carried as postures by C-001 / C-005,
  given an interim pinned posture for the posix-shaped
  surface by the Z-073 contract (P3), and named as
  limitation L-09. Every one of those records said the
  same thing: the implementation's answers settle
  nothing until a per-target decision is recorded.
  This slice records that decision.
- The decision ADOPTS the implementation's existing
  answers as contract. No behaviour changes, no public
  symbol is added (the export list stays exactly ten),
  and the standing gate total does not move: the new
  suite is standalone by design, and the gate's own
  posture-era checks (test_c005 and the J-002 ledger
  tests) already expected exactly these outcomes --
  they now cite a contract instead of a posture.
- What this slice does NOT touch: docs/contracts.md
  still shows the U-09 / U-12 / U-10 rows as unresolved
  until Z-081's contracts-appendix audit amends that
  file; D-013 is the per-target record those rows wait
  for (their U-02 register entry for this target).
  Historical design documents (B-006/B-007/B-008 and
  the Phase 3 companions) keep their wording: they are
  the record of the question, this document and D-013
  are the record of the answer.
- The statistics field alloc_posture_nulls keeps its
  name. It is a field of the versioned public struct
  mimir_stats_v1 (Z-076); renaming it would break the
  v1 layout to fix a word. It now counts CONTRACT
  zero-size nulls; the header and D-013 say so wherever
  the name is explained.

## 1. The decision in one paragraph

Zero-size is a defined, non-failure refusal (exactly
NULL, or ENOMEM-with-*memptr-unchanged for the
posix-shaped entry); realloc-to-zero preserves the
original block (exactly NULL, block live and
caller-owned, never freed); errno is never promised
in either direction, and on the zero-size paths and
every checked refusal that never reaches the operating
system, errno is left exactly as it was on entry.
Full text: D-013; public statement: the "Settled
cases" block of include/mimir_alloc.h.

## 2. The measured contrast (this host, purpleraider)

glibc's answers, probed on this host at slice time
(small C probe against the system glibc, run
2026-11-12; addresses are the run's own and vary):

| Call | glibc on this host | MimirAlloc (D-013) |
|---|---|---|
| malloc(0) | distinct non-null pointer per call, freeable | NULL |
| calloc(0, 16) | non-null pointer | NULL |
| aligned_alloc(64, 0) | non-null pointer | NULL |
| posix_memalign(&m, 8, 0) | returns 0, *m a non-null pointer | ENOMEM, *m unchanged |
| realloc(p, 0) | NULL (the glibc manual: p is freed) | NULL, p LIVE and caller-owned |
| malloc(SIZE_MAX) | NULL with errno = ENOMEM (12) | NULL, errno untouched |
| posix_memalign errno | sentinel 42 preserved across the zero-size call | sentinel preserved (S5 of the test) |

The probe does not dereference p after realloc(p, 0):
under the glibc reading that would be use-after-free,
and under this target's reading it is merely
unnecessary -- the MimirAlloc side proves preservation
positively instead (contents byte-intact, usable size
unchanged, block still realloc-able and freeable).

Why not simply copy glibc, as Z-073/Z-074 did for the
alignment rules? Three recorded grounds (D-013):
(1) a live MimirAlloc block always has a non-zero
usable extent (D-012), so a zero-size live block
cannot be represented honestly; (2) a uniform
realloc NULL (original always preserved) leaves no
exception for a caller to audit, and the freeing
reading is the classic ownership trap -- C23 made
realloc(p, 0) undefined for exactly this divergence;
(3) several failure classes here (lock refusal,
arithmetic refusal, detected corruption) have no
honest errno value, so a partial ENOMEM convention
would promise what the allocator cannot mean.

## 3. Zero-size rows (U-09 settled)

Every row: no storage allocated, no ownership passes,
errno left exactly as on entry, and the J-002 ledger
moves as stated (postures = alloc_posture_nulls,
failures = alloc_failures).

| Call | Result | Ledger |
|---|---|---|
| mimir_malloc(0) | NULL | postures +1 |
| mimir_calloc(0, n) / (n, 0) / (0, 0) | NULL | postures +1 each |
| mimir_realloc(NULL, 0) | NULL (the mimir_malloc(0) case by inheritance) | postures +1 |
| mimir_reallocarray(NULL, 0, n) / (NULL, n, 0) | NULL | postures +1 each |
| mimir_aligned_alloc(valid alignment, 0) | NULL | postures +1 |
| mimir_posix_memalign(valid alignment, 0) | ENOMEM, *memptr unchanged | neither counter (the entry returns before any ledger note, as it does for EINVAL) |

Validation precedence is unchanged and pinned:
alignment validity is judged before size, so
mimir_aligned_alloc(0, 0) and (3, 100) are invalid-
alignment FAILURES (failures +1, exactly NULL), never
zero-size refusals; mimir_posix_memalign(NULL, ...)
and an invalid alignment return EINVAL with *memptr
unchanged, ahead of any size consideration.

## 4. Realloc-to-zero rows (U-12 settled)

| Call | Result | Original block | Ledger |
|---|---|---|---|
| mimir_realloc(p, 0), p small (class path) | NULL | live, contents byte-intact, usable size unchanged, freeable | postures +1 |
| mimir_realloc(p, 0), p large (mapping path) | NULL | same | postures +1 |
| mimir_reallocarray(p, 0, n) / (p, n, 0) | NULL | same | postures +1 each |

The test proves "live" positively after each refusal:
the pattern is re-read, mimir_usable_size answers the
pre-call value, a same-size realloc returns the block,
and mimir_free retires it exactly once.

## 5. Errno rows (U-10 settled)

- No entry point promises errno in either direction;
  the allocator's own code never assigns it (the name
  appears under src/ only in includes, in comments,
  and as the EINVAL / ENOMEM return VALUES of the
  posix-shaped entry).
- Pinned by sentinel (errno set to a known value
  before the call, compared after): every S3/S4 zero-
  size call; the checked failures mimir_malloc
  (SIZE_MAX), mimir_calloc(SIZE_MAX, 2) and
  (2, SIZE_MAX), mimir_reallocarray(NULL, SIZE_MAX, 2)
  and mimir_realloc(p, SIZE_MAX) (original preserved);
  invalid-alignment refusals; and every
  mimir_posix_memalign refusal (EINVAL and ENOMEM
  alike -- its numbers are return values, not errno).
- NOT pinned, by contract: errno after a genuine
  operating-system failure (unspecified; an OS call
  may have set it incidentally) and after successful
  calls (no statement is made).

## 6. Audit counters

No new counters. The test reconciles the six existing
L1 counters of the affected entries (malloc, calloc,
realloc, reallocarray, aligned_alloc, posix_memalign)
exactly against its counting wrappers, the E-005
discipline carried through Z-074..Z-077.

## 7. Evidence

tests/abi/test_z078.c: the S3/S4/S5 rows above with
exact ledger deltas (zero-size postures against
genuine failures, the two never mixed), errno
sentinels on every pinned path, failure preservation
for the arithmetic refusals, a worker-thread leg, and
the audit reconciliation. The dynamic consumer
re-proves the rows against the .so alone (including
the ledger deltas through the PUBLIC statistics
struct), the C++ consumer proves the declarations
from C++, and the header-only units prove the edited
header is self-contained for C and C++. The recorded
run files the nm -D / DT_NEEDED / absence proofs:
exports stay exactly the ten public symbols. The
standing correctness gate (scripts/z13/run_gate.sh)
is run on the final tree and its counts are recorded
in the slice's CAMPAIGN_STATE.md entry; the gate
total does not move, because this suite is standalone
by design.
