# C-003 notes — inconsistencies and observations recorded during the slice

1. INC-C003-01 (B-006 total-mapping formula, INC-B007-A1 carried):
   B-007 A.5 records that B-006 states the total mapping size two
   ways — S6: `page_round_up(metadata_reserve + requested_size)`,
   with `metadata_reserve` never valued; S11.1 step 5 readable as
   either `page_round_up(usable_offset + requested_size)` or
   `usable_offset + page_round_up(requested_size)` — and does not
   choose between them. C-003's implementation is unaffected in
   result: S7.1 fixes `usable_offset` at exactly one whole system
   page, and rounding up distributes over a whole-page term, so
   both readings give the identical mapping length. The core
   computes the checked sum `usable_offset + requested_size`
   (B-007 A3/A4) and lets L6 apply its checked page round-up
   (A5/A6, inside C-002's acquire). This code takes no position
   on the in-doc reconciliation, which remains owed by a future
   revision of the B-006 design document, exactly as B-007 A.5
   requires. No test in this slice cites either unreconciled
   text as settling `mapping_length`.

2. Free-path validation order (implementation choice, recorded):
   B-006 S8.2 step 2 / S11.2 step 4 derive a candidate descriptor
   address from the caller's pointer (p minus one system page)
   and validate its fields. Dereferencing an address derived
   from an unvalidated pointer reads memory the allocator may
   not own (for a foreign pointer it may not even be mapped).
   C-003 therefore performs owner determination by walking the
   registry — only allocator-owned links — to find the LIVE
   descriptor whose recorded user pointer equals p, then applies
   the same validations to that trusted descriptor (magic,
   state == LIVE, mapping_base + usable_offset == p, and
   mapping_base == the descriptor's own address per S7.1). The
   set of accepted pointers is identical to the design's; only
   the order of reads differs, and no byte of an unvalidated
   pointer's neighbourhood is ever read. Invalid frees remain
   undefined behaviour with no promised outcome (S8.3); this
   baseline's non-destructive return on a non-matching pointer
   is not a defined behaviour and must not be cited as one.

3. Free-path release ordering (B-006 S11.2 steps 5–7 vs F-06):
   S11.2 orders mark-RETIRED → munmap → unlink/decrement on
   success, while F-06 requires that on munmap failure the
   descriptor stays LIVE, stays linked, and counters stay
   unchanged (and explicitly permits an implementation that
   unlinks first to restore the LIVE entry on failure). C-003
   marks RETIRED, unlinks, and decrements under the lock, then
   releases via L6 with the lock held (S9.4 decision); on a
   release failure it restores state LIVE, re-links the
   descriptor, and restores both counters in the same locked
   section — the F-06 outcome. On success no descriptor field
   is touched after the release (Invariant 2).

4. Zero-size posture (U-09 still UNRESOLVED): neither B-007 nor
   B-008 prescribes a mimir_malloc(0) behaviour (B-006 F-05 gives
   it none; B-008 §3.6 covers only the calloc zero-factor, also
   unresolved). Following the tasking rule for this slice, the
   implementation returns NULL for size == 0, continuous with
   the C-001 stub's posture, recorded in code comments and in
   both tests as a posture that does NOT settle U-09.

5. Smoke-test change (declared per the slice tasking): the
   C-001 smoke test expected mimir_malloc(1) and
   mimir_malloc(1024) to return NULL because the stub
   implemented nothing. Those two expectations genuinely
   changed with the real allocator and were updated to assert
   success plus owner write/read and free. All other smoke
   expectations are unchanged (see the test's header comment).

6. Layering: B-006's collapsed Phase 3 form puts admission (L2),
   ownership (L4) and mapping layout (L5) in one core below the
   L1 boundary; C-003 expresses that as src/core/mimir_core.c
   behind the thin L1 forwarder src/api/mimir_alloc.c, consuming
   L6 only through src/platform/mimir_platform.h. L3 is
   pass-through (nothing to implement) and L7 is compiled out,
   per B-006 S4. No inconsistency.
