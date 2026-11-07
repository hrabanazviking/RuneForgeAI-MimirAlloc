# Phase 9 - Telemetry Separation Proof (Slice I-007)

Status: **MEASURED PROOF.** This slice proves, in one process at
quiescent points on the Pi, that the I-002 telemetry separation
holds: allocator-managed quantities and process-wide figures are
different accounts, each moving exactly when its own owner acts
and not when the other does. Evidence test:
`tests/core/test_i007.c`, standalone via `make test-i007` (NOT in
`make test`); recorded run: `tests/core/recorded_run_i007.txt`
(**63 checks, 0 failures, RESULT: PASS**, plus three consecutive
unrecorded repeat runs PASS 63/63). Base: `d6a093d` (I-005 banked,
Wave 9 = 5 of 9). No `src/`, header, or README change; the only
build change is the additive Makefile `test-i007` block.

## 1. Definitions (written BEFORE the run, per I-002)

Allocator accounts (this allocator instance only):

- **Q-MAPPED** = live mapping-path bytes + live span bytes, read
  as ONE locked snapshot via the I-003 test-only surface
  (`mimir_core_mapped_bytes()`, tests/core/mimir_trim_i003.h).
- **Q-RETAINED** = EMPTY spans x page size, one locked snapshot
  (`mimir_core_retained_bytes()`). The only cache; inside Q-MAPPED.
- **Audit counters** (E-005 + G-003, test-linked objects only):
  call_malloc/call_free, alloc_published/mapping_retired,
  span_published/span_retired, small_alloc/small_free. Identities:
  live mappings = published - retired; live spans =
  span_published - span_retired; live small = alloc - free.
- Page arithmetic on this host (page measured 16384):
  `mimir_malloc(4096)` maps page_round_up(16384+4096) = **32768 B**
  (1 reserve page holding the descriptor + 1 usable page; the
  reserve page and the 96 B span headers are INSIDE Q-MAPPED, so
  no header term appears outside it); a new span = **16384 B**.

Process accounts (NOT the allocator's, per I-002 S4.2): VmSize and
VmRSS from `/proc/self/status`; Rss / Anonymous / Pss_File from
`/proc/self/smaps_rollup`; mapping count and anonymous VM total
(sum of Size over pathname-less or bracket-annotated entries)
from `/proc/self/maps`. File-backed and driver memory are out of
the allocator's accounts by construction (I-002 S4.2): the
allocator maps nothing file-backed and holds no driver memory.

Method: single thread, snapshots only at quiescent points
(I-002 S5.3), snapshot reads via raw open/read into static
buffers (no stdio, no heap) so measuring moves neither account.
Two sentinel blocks (large 4096 B, small 256 B; constant
Q-MAPPED contribution 49152 B) held live across every leg and
verified byte-identical after each leg (7 verifications).

## 2. Measured legs (recorded run values)

Allocator moves, process follows in VM bytes exactly:

- Sentinels: Q-MAPPED 0 -> 49152; VmSize +49152; anonymous VM
  +49152 -- all exactly equal.
- A1 two large blocks: Q-MAPPED +65536, VmSize +65536,
  anonymous VM +65536, live mappings +2, rollup Anonymous
  +65536 and VmRSS +65536 (all usable+reserve pages touched).
- A2 one small 1024 B object: Q-MAPPED +16384 (one new span),
  VmSize +16384 exactly -- a 1024 B request moves the process
  by a full page: page-rounding belongs to the process figure's
  granularity, not to any allocator over-count.
- A3a large frees: Q-MAPPED and VmSize both drop exactly 65536.

Allocator-internal move, process does not move at all:

- A3b freeing the span's only object RETAINS the span:
  Q-MAPPED unchanged (65536), VmSize unchanged, VmRSS unchanged
  -- the ONLY figure that moves is Q-RETAINED, +16384 exactly.
  Retained bytes are allocator-internal state invisible to every
  process figure except as still-mapped VM (already counted).
- A4 trim releases it: Q-MAPPED -16384, VmSize -16384,
  Q-RETAINED back to 0, Q-MAPPED back to sentinel-only 49152.

Process moves, allocator does not move at all:

- B1 raw anonymous mmap 65536 B, all pages touched: VmSize
  +65536, anonymous VM +65536, rollup Anonymous +65536, VmRSS
  +65536 -- and EVERY allocator quantity and all ten audit
  counters exactly unchanged (alloc_same check over Q-MAPPED,
  Q-RETAINED, retained spans and 8 counters). After munmap,
  VmSize and mapping count return exactly.
- B2 file-backed mmap 65536 B (temp file, read-touched): VmSize
  +65536, VmRSS +65536, Pss_File +65536 (50176 -> 115712),
  mapping count +1 -- while anonymous VM and rollup Anonymous
  do NOT move at all, and every allocator quantity/counter is
  exactly unchanged. File-backed residency is a process figure
  with no anonymous or allocator component: the I-002 S4.2
  exclusion, measured (for this harness-made file; see S4).

## 3. Quiescent reconciliation (recorded run, exact arithmetic)

Baseline S0 (no mimir allocation yet):

- VmSize 2916352 = Q-MAPPED 0 + non-allocator VM 2916352
  (executable, libraries, stack, libc heap arena, loader).
- Anonymous VM 933888 = allocator 0 + non-allocator anonymous
  933888 (libc heap + stack + loader, itemised as one measured
  baseline term; /proc cannot split anonymous VM by origin,
  I-002 S4.2 limit 1 -- the split is proven by the DELTAS in S2,
  which are exact, not by inspecting the baseline).
- Resident baseline: VmRSS 1441792, rollup Anonymous 180224,
  Pss_File 50176 -- all non-allocator at baseline.

Peak (A2): Q-MAPPED 131072; anonymous VM 1064960 =
933888 baseline + 131072 allocator -- the allocator's bytes are
a strict subset of process anonymous VM with the gap exactly
the measured non-allocator baseline. Resident series (rollup Anonymous): baseline 180224; S1
245760 (+65536 for 49152 B of touched allocator pages -- one
further page, 16384 B, became resident in the same interval
and is NOT attributable by origin in /proc, I-002 S4.2 limit 1;
it is carried as part of the non-allocator resident term);
A1 311296 (+65536, exactly the touched allocator pages);
A2 327680 (+16384, exactly the touched span page). The VM
accounts separate exactly; the resident account separates
exactly for every leg after the first, with the single
unattributable page at S1 reported here and echoed in the
final residual (INC-I007-02).

Final C (all freed + trim): Q-MAPPED 0, Q-RETAINED 0, mapping
counters published == retired, span counters published ==
retired, small alloc == free, VmSize returns EXACTLY to the
2916352 baseline, anonymous VM returns EXACTLY to 933888.
**Residual (reported, not rounded away):** final VmRSS exceeds
baseline by exactly 16384 B (one page) and rollup Anonymous by
the same 16384 B -- a non-allocator resident page retained by
the process (kernel/glibc residency is not unmapped-accounted;
VM accounts reconcile exactly, resident accounts to within
this one measured page). Across repeat runs the resident
baseline itself varied by one page (RollAnon 180224 / 196608),
confirming residency, unlike VM, is not an exact account.

## 4. What was NOT proven (honest limits)

- **Driver/accelerator memory: excluded by construction only,**
  not measured -- it is absent from /proc/self entirely
  (I-002 S4.2, INC-I002-07) and this slice took no system-level
  reading; no claim about it is made.
- **Model-weight residency:** the B2 file is a 64 KiB harness
  temp file, honestly labelled file-backed residency of that
  kind and size only -- not a model file, not GGUF, no
  page-cache sharing measurement (PSS apportionment untested).
- **Mapping COUNT is not separable evidence** (INC-I007-01):
  kernel VMA merging makes count move by 0 where bytes move by
  exactly 65536. Only byte totals separate; count is reported
  as observed, never reconciled.
- Single process, single thread, quiescent points only;
  nothing is claimed about torn snapshots under concurrency
  (I-002 S5.3 already forbids relying on them), Android
  accounting, or PSS beyond the Pss_File delta in B2.
- Q-LIVEREQ (requested bytes) remains underivable for small
  objects (INC-I002-02, inherited): separation here is proven
  for MAPPED/RETAINED quantities, not requested bytes.

## 5. INC-I007 register

- INC-I007-01 (measured, test corrected in-slice): mapping
  count from /proc/self/maps does NOT track allocations --
  adjacent anonymous mappings merge into one VMA (A1: count
  delta 0 for +65536 B in two mappings; A3a frees then SPLIT a
  VMA, count 18 -> 19 while bytes fell). The proof therefore
  asserts byte totals exactly and count only as an upper bound.
  Any future telemetry reading that reconciles counts instead
  of bytes would be wrong on this kernel.
- INC-I007-02 (measured residual): final resident figures sit
  exactly one page (16384 B) above baseline after complete VM
  reconciliation. Attributed to non-allocator process residency
  (page touched during the run and retained in the libc heap /
  stack footprint); not allocator memory -- Q-MAPPED, VmSize
  and anonymous VM all return exactly. Reported as the
  reconciliation residual, per I-001's residual rule.
- INC-I007-03 (inherited, unchanged): Q-LIVEREQ small-object
  half still underivable (INC-I002-02); driver memory still
  construction-only (INC-I002-07); Android fields unclaimed
  (INC-I002-08).

## 6. Verification

- `make test-i007`: exit 0, 63 checks / 0 failures, PASS
  (recorded run + 3 consecutive repeats).
- Full `make test`: exit 0 (all suites PASS; final suite
  4590 checks / 0 failed) run in this slice on the Pi.
- `nm -D --defined-only build/libmimiralloc.so` (make lib,
  exit 0): exactly 4 public symbols -- mimir_malloc,
  mimir_free, mimir_calloc, mimir_realloc.
- README.md (sha256 bd7f6614...) and include/mimir_alloc.h
  (sha256 55107f05...) byte-identical to base d6a093d,
  verified against `git show HEAD:` hashes in this slice;
  sentinel patterns intact
  after every leg (7 in-test verifications).
