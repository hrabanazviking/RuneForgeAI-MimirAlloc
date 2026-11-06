# C-005 notes — calloc/realloc per B-008: implementation record,
# inconsistencies, and postures

Slice C-005 implements `mimir_calloc` and `mimir_realloc`
exactly per `docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`
(B-008, including its Section 7.2 supported-list amendment as
reconciled by B-009), over the banked C-003 core as audited by
C-004. Declarations are in `include/mimir_alloc.h` (the
declaration the C-001 note INC-C001 left owed), following the
contract IDs of `docs/contracts.md` (A-008) as B-008 maps them
— by analogy under U-03 in every case, since the namespaced
names have no contract rows of their own. Evidence:
`tests/core/test_c005.c`, run on the Pi via `make test` (see
`tests/core/recorded_run_c005.txt`). Existing smoke, core and
B-007 suites are unchanged in expectation and stay green; the
smoke test gained only additive checks (item 7 below).

Implementation shape (all in `src/core/mimir_core.c`, behind
the thin L1 forwarders in `src/api/mimir_alloc.c`):

- The C-003 `mimir_core_alloc` body is refactored, behaviour
  unchanged, into a lock-held construction helper
  `alloc_locked(size, zero_user, &desc)` shared by malloc,
  calloc and realloc's move. Registry owner determination
  (`find_candidate_locked` + `descriptor_is_live`) and the
  retire-exactly-once step (`retire_locked`, with its F-06
  rollback) are likewise shared helpers instead of
  free-private code; `mimir_core_free` uses them with
  identical outcomes.
- calloc: zero-factor gate first (B-008 S3.2 step 0), then
  the product checked before the multiplication is relied on
  (`nmemb > SIZE_MAX / size`, B-007 A6 multiply form), then
  the product admitted exactly as a malloc of that size, with
  the usable extent zeroed explicitly inside `alloc_locked`
  after the mapping succeeds and before registry publication
  (B-008 S3.2 steps 3–6, S3.3 mechanism decision).
- realloc: NULL delegates to the malloc path (S4.7); the
  zero-size gate fires before any state is touched (item 1);
  same-size returns the input pointer (S4.3 design decision);
  shrink updates only the descriptor's `requested_size`
  record and retains the whole mapping (S4.4); grow always
  moves — construct the new block completely, copy
  `min(old, new)`, retire the old block exactly once — and
  any failure before the transfer returns NULL with the
  original LIVE, unchanged and caller-owned (S4.5,
  MA-6.1-REALLOC-02 / MA-6.2-022).

## Inconsistencies and postures recorded

1. INC-C005-01 — ZERO CASES: NO SOURCE STATES A BEHAVIOUR,
   A BEHAVIOUR WAS REQUIRED ANYWAY (U-09 / U-12).
   B-008 states no behaviour for the calloc zero factor
   (S3.6, MA-6.1-CALLOC-04, [UNRESOLVED: U-09]) or for
   `realloc(p, 0)` (S4.8, MA-6.1-REALLOC-04, [UNRESOLVED:
   U-12]); both are outside the S7.2 supported list. B-009
   goes further: P3-T-CAL-031 and P3-T-REA-041 write **no
   pass criterion** for either case (P3-T-REA-041: "in
   particular neither the return value nor the ownership /
   freeability of p is asserted"), and hook C-006 stands
   BLOCKED until a per-target decision is recorded. An
   implementation nevertheless exhibits *some* behaviour,
   and this slice's tasking requires the functions plus a
   test of `realloc(ptr, 0)` "per the B-008-documented
   posture". Postures chosen, following the house precedent
   set for `mimir_malloc(0)` by C-003 (NOTES.md item 4):
   - calloc zero factor → NULL, continuous with malloc(0).
   - `realloc(NULL, 0)` → NULL, inheriting malloc(0) by
     B-008 S4.7's own inheritance rule.
   - `realloc(p, 0)` → NULL with the original block left
     LIVE, unchanged in contents and ownership, and freeable
     exactly as before. Rationale: zero is not admitted at
     L2 (B-007 A1: no behaviour, no arithmetic on an
     unadmitted size), so the refusal is decided before any
     allocator state is touched — the same failure-atomic
     shape as every other admission refusal in this core,
     and the only candidate under which the call can neither
     leak ownership silently nor destroy a block the caller
     may still own. It is one of the V-04 candidate shapes
     ("returning null") with p's fate settled *by this
     implementation only*, not by any source.
   Every zero-case check in test_c005.c is labelled
   "posture only" in its label text. None of them settles
   U-09 or U-12, none passes hook C-006 or P3-T-CAL-031 /
   P3-T-REA-041, and none may be cited as a per-target
   policy. If a per-target annex later chooses differently,
   the implementation and these posture checks change with
   it; the Defined-case checks are unaffected.

2. INC-C005-02 — ZEROING EXTENT: CONSTRUCTION vs ORACLE
   (INC-B008-4 carried). B-008 S3.3 resolves the
   roadmap/contracts wording tension in favour of the
   stronger wording: the zeroing step covers the whole
   usable extent. The implementation zeroes `usable_size`
   bytes inside `alloc_locked`, before publication. The
   test oracle, however, follows B-009's CAL-003 rule: no
   byte beyond the requested product is inspected as an
   oracle, because no usable-size contract exists by which
   a caller could name the larger extent (B-008 S6.1 N-3).
   Usable-extent coverage therefore rests on code inspection
   of `alloc_locked`, not on the recorded run. Recorded so
   no reader mistakes the byte-by-byte requested-extent
   evidence for a usable-extent measurement.

3. INC-C005-03 — PRESERVE-ON-FAILURE EVIDENCE IS PARTIAL
   (B-008 S8 E-4). E-4 asks for fail-the-Nth-mapping
   injection swept over the move sequence's steps. No
   fault-injection hook exists in Phase 3 scope (the same
   limitation C-004 recorded as INC-C004-02 for the A8/A9/
   A10, F-06, F-07 and F-11 branches), so the sweep cannot
   be run. Preservation is evidenced instead through the
   public API at the two failure points a caller can
   actually induce: the L2 checked-arithmetic refusal
   (SIZE_MAX) and the mapping failure of a representable but
   unsatisfiable size (SIZE_MAX / 2), for both calloc and
   realloc, with sentinel/original contents verified
   byte-for-byte and continued usability proved. The
   release-failure rollback branches of realloc's transfer
   step (old-release failure → new block retired again;
   double failure → new block left LIVE and tracked) rest
   on code inspection only, exactly as free's F-06 branch
   did at C-003/C-004.

4. INC-C005-04 — U-03 GAP NOW COVERS THE WHOLE SURFACE
   (INC-B008-2 carried). contracts.md Appendix A lists only
   `mimir_malloc` and `mimir_free` as [UNRESOLVED: U-03]
   namespaced names; `mimir_calloc` and `mimir_realloc`
   have no rows at all. The header and code cite every
   contract ID "by analogy" accordingly. Nothing in this
   slice creates a direct namespaced contract, an ABI, a
   signature-stability promise, or a symbol version
   (B-008 N-10).

5. INC-C005-05 — SHRINK AND THE DESCRIPTOR'S usable_size.
   B-008 S4.4 requires two records after a shrink — the new
   requested size and the original mapping length — and
   never substitutes one for the other. The B-006 S7.3
   descriptor carries a third size, `usable_size`
   (= mapping_length − usable_offset), whose post-shrink
   value no source names. This implementation updates
   `requested_size` only and leaves `usable_size` as the
   physical extent of the retained mapping, consistent with
   its C-003 definition (a mapping-derived quantity, checked
   by test_b007.c against mapping_length on fresh
   allocations). Preservation, copying and caller-visible
   guarantees all use `requested_size`; free uses
   `mapping_length`; no caller-visible guarantee rests on
   `usable_size` (there is no usable-size API, N-3).
   Recorded as an implementation choice in a silent spot,
   not presented as a source rule.

6. INC-C005-06 — THE MOVE COPY AND THE LOCK'S "NO USER
   BYTES" WORDING (INC-B008-5 pattern). B-006 S9.2 says the
   lock guards only allocator metadata, never user bytes;
   B-008 S4.5's move sequence nevertheless requires the
   allocator to copy the preserved range, and its step 5
   holds the lock across the transfer. This implementation
   copies under the lock, between construction of the new
   block and retirement of the old, because validation,
   publication and retirement must be atomic against other
   threads. The reading is the temporal boundary B-008 S3.3
   established for calloc's zeroing: the allocator touches
   user bytes only while ownership is in construction or in
   transfer, never while a block is simply caller-owned.
   Recorded so a reader of B-006 alone is not left with an
   apparent contradiction; a B-006/B-008 wording amendment
   remains the design slices' to make.

7. Smoke-test change (declared, in the C-003 manner): the
   C-001 smoke test's header comment recorded that the
   absence of `mimir_calloc` / `mimir_realloc` was verified
   with nm rather than at runtime. That absence ended with
   this slice, so the comment now names the facilities that
   remain not-present (reallocarray, usable-size, aligned
   and standard-name entry points), and the smoke test gains
   additive checks only: compile-time prototype checks for
   the two new declarations, one zeroed calloc block, and
   one realloc(NULL) + grow round trip. Every pre-existing
   smoke check is unchanged in label, order and expectation.
   The recorded run's nm section changes accordingly: the
   API translation unit now exports four symbols
   (mimir_malloc, mimir_free, mimir_calloc, mimir_realloc),
   exactly the B-008 S7.2 amended surface, and nothing else.

8. Scope kept: no startup/init work (C-006), no
   unsupported-behaviour enforcement beyond what B-008
   requires of calloc/realloc themselves (C-008). The
   not-present facilities of B-008 S6.1 remain absent; the
   UB cases of S6.2 remain UB (the baseline's non-destructive
   return on an invalid realloc pointer is the free path's
   existing posture, not a designed refusal, and no test
   asserts it); the unresolved cases of S6.3 remain
   unresolved per item 1.
