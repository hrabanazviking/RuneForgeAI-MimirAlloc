# Z13 Gap Closure for Real Programs (Z-083)

## 0. Position and honesty notes

This slice is Wave 10's second slice. Its predecessor,
Z-082 (the interposing variant: an opt-in build whose
library exports the STANDARD allocation names), is being
built in parallel by team-02 and is NOT on this slice's
base: the base here is the banked seat main at Z-078
(78 of 117), whose public surface is exactly the ten
namespaced entry points. Nothing in this slice claims,
duplicates, or pre-empts Z-082's work:

- This slice adds NO standard-name symbol. The recorded
  run proves the negative mechanically (S3: no `malloc`,
  `memalign`, `strdup`, ... export exists in the shipped
  .so). Standard names remain Z-082's, exclusively.
- What this slice closes is the NAMESPACED half of the
  gap: the memalign-family shapes real programs call,
  served as `mimir_memalign` / `mimir_valloc` /
  `mimir_pvalloc` over the banked Z-074 construction, in
  the exact pattern Z-074 / Z-075 / Z-077 set for surface
  growth. When Z-082's variant banks, its standard-name
  `memalign` / `valloc` / `pvalloc` are one-line
  compositions over these entries (or over the Z-074
  core directly); the composition is Z-082's to file.
- The gap list in S1 is built from MEASUREMENT on this
  host, not from memory: the allocation-symbol inventory
  of this host's glibc 2.44 (recorded run S10) and a
  behaviour probe of its memalign family (S3). Statuses
  distinguish what is served, what is shimmed here, what
  a named sibling slice owns, what was assessed and
  honestly not shimmed, and what is provably unreachable.

## 1. The gap list (host glibc 2.44, measured)

Inventory source: `nm -D` over this host's
/lib/x86_64-linux-gnu/libc.so.6, filed verbatim in
tests/abi/recorded_run_z13_z083.txt S10. Statuses:
SERVED = a namespaced entry exists on the banked base;
SHIMMED = added by this slice; SIBLING = owned by a
named in-flight slice; ASSESSED = examined here, not
shimmed, reason named; UNREACHABLE = provably cannot be
reached by any interposition or linking route.

| glibc symbol class | Status | Where / why |
|---|---|---|
| malloc, calloc, realloc, free | SERVED | mimir_malloc / mimir_calloc / mimir_realloc / mimir_free (C-001/C-005). Standard names: SIBLING Z-082. |
| reallocarray | SERVED | mimir_reallocarray (Z-077). Standard name: SIBLING Z-082. |
| posix_memalign, aligned_alloc | SERVED | mimir_posix_memalign / mimir_aligned_alloc (Z-074). Standard names: SIBLING Z-082. |
| memalign | SHIMMED | mimir_memalign (this slice, S2). Standard name: SIBLING Z-082. |
| valloc | SHIMMED | mimir_valloc (this slice, S2). Standard name: SIBLING Z-082. |
| pvalloc | SHIMMED | mimir_pvalloc (this slice, S2). Standard name: SIBLING Z-082. |
| malloc_usable_size | SERVED | mimir_usable_size (Z-075, decision D-012; the standard-name shape was deliberately NOT taken there). |
| memalignment | ASSESSED | A query of a block's alignment, not an allocation shape; outside the roadmap-named memalign/valloc/pvalloc family. For this allocator's own blocks the answer is construction-known (16-aligned floor; page-aligned mapping blocks), but a truthful per-block query needs a registry answer for foreign/interior pointers in the D-012 style. Named candidate for a later surface slice; not silently approximated here. |
| malloc_trim | SERVED | mimir_trim (Z-077) is the same shape over the I-003 machinery. Standard name: SIBLING Z-082. |
| free_sized, free_aligned_sized | ASSESSED | C23 sized-free: the size argument is a caller promise the callee may check. This allocator's free deliberately takes no size (its records, not the caller, own the extent). A shim that ignored the argument would accept contract violations silently -- worse than absence (B-008 S6.1: absence at link time is the documented unsupported behaviour). Re-open if a size-checking free is ever designed. |
| cfree | ASSESSED | A legacy alias of free in glibc. Under any interposing composition it IS free; a namespaced duplicate would add a second name for the identical course and no semantics. Not shimmed; the alias is Z-082's one-line composition if its audit wants it. |
| strdup, strndup | ASSESSED | See S4 -- the honest assessment the roadmap orders. Not shimmed. |
| mallopt | ASSESSED | glibc heap-tuning parameters (M_TRIM_THRESHOLD and kin) name glibc-internal machinery with no counterpart here; this allocator's tuning surface is its build flags and mimir_trim. A shim returning success while changing nothing would lie; returning failure for parameters real programs set at startup would break them for no gain. Absence is the honest answer. |
| mallinfo, mallinfo2, malloc_stats, malloc_info | ASSESSED | glibc heap statistics in glibc's own accounting units and layout. This allocator's honest statistics surface already exists (mimir_stats_snapshot, Z-076, versioned). Translating its counters into mallinfo's fields would fabricate equivalences (arena / hblks / uordblks have no referent here). Not shimmed. |
| __libc_malloc, __libc_free, __libc_calloc, __libc_realloc, __libc_memalign, __libc_valloc, __libc_pvalloc, __libc_reallocarray, __libc_mallinfo, __libc_mallopt | UNREACHABLE | S5. |
| libc-internal callers of every symbol above (libc's own strdup / stdio / NSS / locale allocations, and every other DSO's libc-internal paths) | UNREACHABLE | S5. |

After this slice, no row is left merely "missing": every
row is served, shimmed, owned by the named sibling
Z-082, assessed with its reason, or proven unreachable.
That is the roadmap gate: the gap list shrunk to the
provably-unreachable remainder (plus the assessed rows,
whose reasons are the shrinkage -- each was a candidate
only until the reason was written).

## 2. The shims (decision D-016)

All three entries are L1 forwarders (src/api) over core
entries (src/core) that build entirely on the Z-074
aligned construction. No new allocation machinery, no
new release path: every published block is an ordinary
family block and is retired by mimir_free exactly once.

- mimir_memalign(alignment, size): the historical
  memalign shape with EXACTLY mimir_aligned_alloc's
  contract -- validity is a non-zero power of two;
  invalid alignment is a defined failure (exactly NULL,
  counted in alloc_failures); zero size is the D-013
  contract null (counted in alloc_posture_nulls); the
  size need not be a multiple of the alignment.
- mimir_valloc(size): aligned_alloc at this host's page
  size (4096, Z-002). The size is NOT rounded; only the
  pointer is page-aligned. Zero size is the D-013 null
  through the aligned path.
- mimir_pvalloc(size): the size is rounded UP to a whole
  page multiple first (checked: a size whose rounding
  overflows size_t is exactly NULL, a genuine failure,
  never a wrapped allocation), then served as valloc.
  Zero size is the D-013 null BEFORE rounding -- never
  rounded into a one-page allocation. A successful
  result's usable extent is at least the rounded size.

errno is not promised at any of the three (U-10,
settled by D-013); the checked refusals leave errno
exactly as on entry, sentinel-pinned by the tests.

## 3. Measured contrast: this host's glibc 2.44

Probe run on purpleraider at build time (quiet window);
MimirAlloc answers are the D-013 / Z-074 family
contract, diverged from by recorded decision:

| call | glibc 2.44 (measured) | MimirAlloc (contract) |
|---|---|---|
| memalign(64, 100) | pointer, mod 64 == 0, usable 104 | aligned pointer (Z-074 routing) |
| memalign(24, 100) | SUCCEEDS (rounds up internally), errno untouched | NULL, a genuine failure |
| memalign(0, 100) | SUCCEEDS | NULL, a genuine failure |
| memalign(64, 0) | distinct freeable pointer, usable 24 | NULL, posture (D-013) |
| valloc(100) | page-aligned pointer, usable 104 (size unrounded) | same shape: page-aligned, size unrounded |
| valloc(0) | distinct pointer | NULL, posture (D-013) |
| pvalloc(100) | page-aligned, usable 4104 (size rounded to one page) | usable >= 4096 |
| pvalloc(4097) | usable 8200 (rounded to two pages) | usable >= 8192 |
| pvalloc(0) | minimum block, usable 24 | NULL, posture (D-013) |

## 4. The strdup class, assessed honestly

strdup / strndup are string functions that allocate:
they copy a string into a fresh allocation the caller
frees with the process allocator. Three facts decide
their disposition:

1. A NAMESPACED mimir_strdup would not close any
   allocator gap. It would be a new string API wearing
   the allocator's prefix; no program's strdup call is
   redirected by it, and the repository's own discipline
   (B-008: the surface is the allocation family) does
   not admit it.
2. Under Z-082's interposing variant, a dynamically
   linked program's OWN strdup calls CAN be interposed
   in principle (strdup is an ordinary PLT call in the
   program and its DSOs). Composing it there is one line
   over the variant's malloc plus strlen/memcpy, and it
   is Z-082's composition to file -- with one coherence
   duty: the returned block must be freeable by the
   variant's free, which it is by construction.
3. LIBC-INTERNAL users cannot be interposed at all.
   glibc's own code -- stdio, NSS, locale, resolver and
   the rest -- calls its internal allocation entry
   points (the __libc_* aliases and its private strdup
   paths) through bindings resolved INSIDE libc at its
   own build time. No LD_PRELOAD object, however
   complete its symbol table, is ever consulted for
   those calls. Any claim that an interposing allocator
   "covers strdup" is therefore false for exactly the
   callers that use it most; this doc says so plainly,
   as the roadmap orders, and the Z-084/Z-085 battery
   slices must grade preload coverage with this boundary
   in view: a preloaded process's libc-internal
   allocations stay with glibc, and cross-allocator
   frees at that boundary are the known hazard the
   battery exists to catch.

Disposition: NOT SHIMMED here; the standard-name
composition is Z-082's; the libc-internal remainder is
unreachable (S5).

## 5. The provably-unreachable remainder

Two classes, one proof shape: a call is reachable by an
external allocator only if it is resolved through the
dynamic symbol table at load or call time (a linked
consumer's undefined symbol, or a PLT entry the loader
can redirect to a preloaded definition). Neither class
below ever is:

- The __libc_* allocation aliases are libc's INTERNAL
  entry points. Calls to them originate inside libc.so
  itself and are bound locally when libc is built (they
  exist so libc's own code never depends on interposable
  symbols for its correctness). A program that names
  __libc_malloc directly is using a private,
  non-contract interface; no supported program does, and
  an allocator that exported the aliases would change
  nothing -- the internal callers would still not be
  redirected.
- libc-internal callers of the public names (including
  its strdup paths): same binding, same conclusion.
  This is a property of how glibc is linked, verifiable
  on this host by inspection of libc's own relocations;
  it is not a limitation a later slice of this campaign
  can engineer away, and Z-090's verdict must carry it
  as a permanent boundary, not a backlog item.

## 6. Evidence

- make test-z083: static test_z083 301 checks / 0
  failed, PASS (three consecutive runs, bit-identical
  counts); test_z083_dyn PASS; test_z083_cpp PASS.
- tests/abi/recorded_run_z13_z083.txt: ABI RUN PASS --
  exports exactly 13 (the ten banked plus the three new),
  no internal and no standard-name exports, DT_NEEDED +
  undefined-symbol + unmangled-C++ proofs, header-only
  C/C++ objects, neighbour make test-e005 PASS, and the
  S10 glibc inventory the S1 table is built from.
- Neighbours on the final tree: make test-z074 PASS,
  make test-z077 PASS, make test-z078 PASS. (The earlier
  slices' run_z0NN.sh recorded runs are point-in-time
  records of their own trees -- their total export-count
  assertions describe their own base, exactly as
  run_z076.sh's count of 7 described the pre-Z-077 tree;
  they are not re-run after later surface growth, and
  the standing gate is the count authority.)
- Variant compiles of the patched core, -Werror clean:
  MIMIR_LOCK_SHARD, MIMIR_DEBUG, MIMIR_ARENA_CCD,
  MIMIR_LOCK_HIST, MIMIR_OOL_META.
- Standing gate: scripts/z13/run_gate.sh on the final
  tree -- counts filed in the slice's CAMPAIGN_STATE
  entry (expected GREEN 4988 / 0 / 77, identical to the
  pre-slice baseline on this base; the slice suite is
  standalone and does not move the gate total).
