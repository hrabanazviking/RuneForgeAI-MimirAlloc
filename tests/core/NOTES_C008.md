# C-008 notes — unsupported-behaviour enforcement audit and
# roadmap §9.3 controlled-failure tests: slice record and
# inconsistencies

Slice C-008 audits the banked implementation (C-001..C-007,
base **94e1a8d**) against the unsupported-behaviour record of
`docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`
(**B-008**) Sections 6–7, following the C-004 / C-006 audit
pattern, and adds the controlled-failure evidence of roadmap
Section 9.3 that the Pi can produce through the public API
only. The audit itself is `tests/core/AUDIT_B008_C008.md`;
the evidence program is `tests/core/test_c008.c`, run on the
Pi via `make test` (see `tests/core/recorded_run_c008.txt`).
Host: Raspberry Pi `purpleviking1`, runtime system page size
16384 bytes.

## Outcome in one paragraph

**No gap in any B-008 Defined refusal was found, and no
`src/` or header change is made by this slice.** Every
Defined refusal of B-008 §6.4 (oversize / overflow, 
unsatisfiable, realloc failure with the original preserved)
is already enforced by the banked core and is re-verified
under §9.3 controlled-failure framing in `test_c008.c`, each
induced failure followed by sentinel verification and a
recovery probe. The §6.1 not-present bucket is verified at
link level (recorded `nm` output in the audit): the L1
object defines exactly the four supported names and nothing
else. The §6.2 UB bucket and the §6.3 unresolved bucket are
handled exactly as B-008 prescribes — no posture is invented
for any open U-item; the audit records them posture-only /
BLOCKED with the register ID named. Per the C-006 precedent,
no work was invented to fill the slice: enforcement that
already exists is evidenced, not rewritten.

## What this slice adds

- `tests/core/AUDIT_B008_C008.md` — item → evidence →
  verdict for every row of B-008 §6.1 (N-1..N-11), §6.2
  (U-1..U-8), §6.3 (X-1..X-8), §6.4 (all Defined refusals),
  the §7.1 documentation channels, and the §7.2 supported
  list, plus the roadmap §9.3 bullet-by-bullet disposition.
- `tests/core/test_c008.c` — §9.3 controlled failures
  through the public API only: page/alignment/integer
  boundaries with the §6.4 refusals and recovery probes;
  an RLIMIT_AS child that allocates to genuine OS refusal
  and recovers under the same limit; a fragmented-address-
  space case with pairwise non-overlap verification; a
  kill/restart case (process isolation only, INC-C008-02);
  a 120-round failure storm. Injection-dependent and
  off-host cases print SKIP lines with reasons, never a
  faked run.
- `Makefile` — additive wiring for `test_c008` in `all` and
  `test`, following the C-005/C-006 pattern.
- `tests/core/recorded_run_c008.txt` — the full
  `make clean && make test` session on the Pi.

## Inconsistencies and postures recorded

The register lives in `tests/core/AUDIT_B008_C008.md`
(INC-C008-01 .. INC-C008-06). Summary:

1. INC-C008-01 — §9.3 injection dependence (INC-C007-01
   carried): no fault-injection shim exists, so the
   fail-the-Nth sweep and EMFILE/EAGAIN classes are
   SKIPPED/NOT-YET; only public-API, rlimit and process
   forms run.
2. INC-C008-02 — §9.3's kill/restart bullet is
   application-level; the allocator holds no cross-process
   state, so the run case proves isolation/restart health
   only.
3. INC-C008-03 — the §6.2 UB bucket rests on inspection
   under the NO-TEST-UB discipline; the baseline's
   non-destructive course for invalid free/realloc is not
   a Defined refusal and must never be cited as one.
4. INC-C008-04 — Android / proot §9.3 tracks and the
   x86-64 leg (INC-C007-02 carried) cannot run on this
   host; NOT-YET, no result claimed.
5. INC-C008-05 — the RLIMIT_AS failure threshold is
   environment-shaped; the case asserts the failure +
   recovery shape, never a byte-exact threshold.
6. INC-C008-06 — teardown interval (X-8 / INC-B008-8
   carried) still has no contract row; C-008 adds no
   evidence and makes no claim.

Carried unchanged, not re-opened: U-03 (namespaced gap),
U-09 / U-12 (zero cases, postures recorded by C-005,
INC-C005-01), U-10 (errno), INC-C006-01 (discarded unlock
results), INC-C007-01..05 except as cited above.

README.md untouched. No new public API. The C-007 suite is
not redone; it runs unchanged in the same `make test`
session and stays green (see the recorded run).

*End of C-008 notes.*
