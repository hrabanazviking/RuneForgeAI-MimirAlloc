# Z13 Wave 9 — Contracts Appendix and ABI Audit (Z-081, D-10)

Slice: Z-081 (Wave 9, Phase: public surface and contracts)
Team: team-01, branch yrsa/mimiralloc-team-01
Base audited: merge 135ca29 of seat main 2fd40db (banked
Z-001..Z-078) into the team branch; the merged tree is
byte-identical to the seat tree.
Host: purpleraider (ASUS ROG Flow Z13, Strix Halo), x86_64,
kernel 7.3.0-10-generic, cc (Ubuntu 15.3.0-4ubuntu1) 15.3.0,
glibc 2.44, page size 4096.
Method: M-002-style audit re-run over the widened surface —
fresh `make lib` + `nm -D` on this host this run, full read
of the public header, the version script and the L1 API
file, a per-symbol documentation census across
docs/contracts.md Appendix A, the slice docs of record and
the decision ledger, and a new standalone suite
(`make test-z081`) that exercises every public symbol and
reconciles all ten E-005 audit counters. The recorded ABI
run is tests/abi/recorded_run_z13_z081.txt. No src/ file,
no header and no README is changed by this slice; the only
documentation changed is docs/contracts.md Appendix A/B,
which is this slice's named work.

## 1. Files reviewed (line counts from this host)

| File | Lines | Role |
|---|---|---|
| include/mimir_alloc.h | 424 | public header, 10 declarations + stats struct |
| src/api/mimir_alloc.c | 317 | L1 boundary forwarders + E-005 call counters |
| src/api/mimir_alloc.map | 45 | version script (export contract) |
| src/core/mimir_core.h | 785 | internal core surface |
| src/core/mimir_core.c | 8023 | L2/L4/L5 core |
| src/platform/mimir_platform.h | 197 | L6 published interface |
| src/platform/mimir_platform.c | 329 | L6 Linux/POSIX form |
| Total | 10120 | |

Build gate: the Makefile compiles everything with
-std=c11 -Wall -Wextra -Werror -pthread; the fresh
`make lib` and `make test-z081` builds this run produced
0 warnings.

## 2. Exported dynamic symbols — exactly the 10

Fresh `make lib` on this host this run, then
`nm -D --defined-only` (full output in the recorded run):
the defined dynamic symbols of build/libmimiralloc.so are
exactly

  mimir_aligned_alloc, mimir_calloc, mimir_free,
  mimir_malloc, mimir_posix_memalign, mimir_realloc,
  mimir_reallocarray, mimir_stats_snapshot, mimir_trim,
  mimir_usable_size

counted: 10, all type T. Every other name in .dynsym is an
undefined import (libc/pthread) or a weak loader symbol.

Three-way agreement, verified symbol by symbol this run
(recorded run S2/S4): each of the 10 is (a) declared in
include/mimir_alloc.h (the header declares exactly 10
functions — grep count 10), (b) listed under `global:` in
src/api/mimir_alloc.map, and (c) exported T in .dynsym.
No fourth list disagrees. The internal surface —
mimir_core_*, mimir_platform_* and the ten E-005 audit
counters (mimir_audit_*) — is bound local by the anonymous
version script plus hidden-visibility PIC objects; the
recorded run proves no mimir_core_/mimir_platform_/
mimir_audit_ name appears anywhere in .dynsym.

Each public symbol also has exactly one L1 audit counter
(tests/abi/mimir_audit_e005.h): the ten counters and the
ten symbols are in 1:1 correspondence, and test_z081
reconciles all ten (S7 of the static test: every counting
wrapper's calls equal its counter's delta, exactly).

## 3. Documentation audit — the finding and its closure

Status frame, stated plainly first: contracts.md assigns
NO ABI. Its Section 5 is PROPOSED in full — no ABI version
is assigned, the final exported symbol set is
[UNRESOLVED: U-01], and the namespaced mimir_* API is
outside Section 6 scope [UNRESOLVED: U-03]. The public
header says exactly the same. This audit therefore grades
CONSISTENCY and INVENTORY COMPLETENESS — whether every
exported symbol is documented somewhere of record and
inventoried in Appendix A — not stability, which nobody
claims.

### 3.1 Per-symbol census (before this slice)

| Symbol | Origin | Header | Slice doc / ledger of record | In Appendix A before Z-081 |
|---|---|---|---|---|
| mimir_malloc | C-001 | L194 | header contract text | YES |
| mimir_free | C-001 | L199 | header contract text | YES |
| mimir_calloc | C-005 | L210 | header contract text | NO — INC-M002-03 |
| mimir_realloc | C-005 | L225 | header contract text | NO — INC-M002-03 |
| mimir_aligned_alloc | Z-074 | L300 | docs/Z13_ALIGNED_ALLOCATION_CONTRACT.md (Z-073) | NO |
| mimir_posix_memalign | Z-074 | L317 | docs/Z13_ALIGNED_ALLOCATION_CONTRACT.md (Z-073) | NO |
| mimir_usable_size | Z-075 | L350 | decision D-012 | NO |
| mimir_stats_snapshot | Z-076 | L418 | docs/Z13_PUBLIC_STATISTICS.md | NO (unnamed "statistics" row only) |
| mimir_trim | Z-077 | L274 | docs/Z13_Z077_TRIM_REALLOCARRAY.md | NO (unnamed "trim" row only) |
| mimir_reallocarray | Z-077 | L247 | docs/Z13_Z077_TRIM_REALLOCARRAY.md | NO |

Finding: **8 of the 10 exported symbols were missing from
contracts.md Appendix A** — the M-002 finding INC-M002-03
(calloc/realloc omitted) widened by the whole Wave 9
surface. Every one of the 8 was fully documented elsewhere
of record (the public header carries complete contract
text for all ten, and each Wave 9 symbol has its slice doc
or ledger decision, per the census above), so the gap was
an Appendix A inventory gap, never an undocumented
implementation: no symbol exists that is not documented,
and after this slice's amendment no symbol exists that is
not also inventoried in Appendix A.

### 3.2 Closure (this slice)

docs/contracts.md Appendix A is amended: the two
namespaced rows become ten, one per exported symbol, each
naming its §2 analogue and its doc/ledger of record;
`mimir_calloc` and `mimir_realloc` rows discharge
INC-M002-03; the unnamed "Statistics / trim functions
(names not specified in the roadmap)" row is replaced by
the named mimir_stats_snapshot and mimir_trim rows. A
Z-081 amendment note after the table and an Appendix B
change-log row record the change. The amendment changes
no status, scope or stability: U-01/U-03 remain open and
every entry's stability class stays Unassigned (§5.5).

The closure is mechanical, not asserted: run_z081.sh S9
extracts Appendix A and greps every exported symbol's
name in it, and the recorded run files the result.

## 4. Absence proofs (recorded run S3)

- No standard-name interposition symbol is exported:
  malloc, free, calloc, realloc, aligned_alloc,
  posix_memalign, malloc_usable_size, reallocarray,
  memalign, valloc, pvalloc, strdup — all absent from the
  defined dynamic list. Standard-name interposition stays
  the Wave 10 opt-in variant's business (Z-082), exactly
  as D-012 records for malloc_usable_size.
- No C++ mangled symbol is exported: the operator
  new/delete family (§2.10 conditional, U-06) is absent.
- No internal symbol is exported (Section 2 above).

## 5. Versioning and SONAME state of record

- The version script's node remains deliberately
  ANONYMOUS on this tree: a named node would add an ABS
  version-name symbol to .dynsym and imply a stability
  promise the header withholds.
- readelf -d on the fresh .so this run shows NO SONAME,
  and this tree has no install target. Both are the
  INC-M002-05 owed items, and both are discharged by
  slice Z-080 (SONAME, versioning, install) — committed
  on team-06 (0a48239) and queued IMMEDIATELY AHEAD of
  Z-081 in banking order, so the banked tree gains its
  SONAME before this audit banks. This slice records the
  pre-Z-080 state of its own base honestly and claims
  nothing about it (recorded run S10).

## 6. Behavioural leg (make test-z081, this run)

- Static audit test (tests/abi/test_z081.c): 300 checks,
  0 failed — every public symbol exercised per its header
  contract (core four incl. calloc zeroing and realloc
  prefix preservation; aligned pair incl. invalid
  alignment refusals and the posix EINVAL/*memptr rule;
  usable-size exact class extent 90 -> 96 per D-012;
  stats snapshot version/size/page fields and the
  NULL -> -1 refusal; trim return domain {0,1} and
  idempotent second full trim; reallocarray product and
  checked-overflow preservation), plus the ten-counter
  reconciliation of Section 2.
- Dynamic C consumer and C++ consumer (linked ONLY
  against the .so): both PASS; the C++ consumer and the
  header-only C/C++ units take all ten prototypes by
  address, proving exact signatures inside the extern "C"
  guards (recorded run S5/S6).

## 7. Test evidence (this run, on this host)

- Standing gate scripts/z13/run_gate.sh on the merged
  base before building: GREEN, 4988 checks / 0 failed /
  77 by-design skips (hidden_files/z081_baseline_gate.log)
  — identical to the campaign baseline.
- make test-z081: static 300 / 0, dyn PASS, cpp PASS.
- Recorded ABI run tests/abi/recorded_run_z13_z081.txt:
  PASS (S1–S11, incl. the S9 documentation gate and the
  neighbour suites test-e005 and test-z078).
- Standing gate on the final tree: GREEN, counts read
  back from hidden_files/z081_gate.log at commit time and
  recorded in the team's CAMPAIGN_STATE.md.

## 8. INC register

- INC-M002-03 (Appendix A omits mimir_calloc /
  mimir_realloc): DISCHARGED by this slice (Section 3.2).
- INC-M002-05 (no SONAME / versioned filename / install
  target): NOT this slice's to close; discharged by
  Z-080, queued immediately ahead in banking order
  (Section 5). No residual after Z-080 banks.
- New findings: none. No code defect was found in the
  widened surface; header, version script, implementation
  and dynamic symbol table agree exactly.

## 9. Verdict

The widened ten-symbol surface is internally consistent
and fully documented: header, version script and .dynsym
agree exactly on the same ten names; every exported
symbol carries complete contract text in the public
header and a slice doc or ledger decision of record; and
with this slice's amendment, contracts.md Appendix A
inventories all ten by name — no undocumented public
symbol remains. The ABI remains Proposed by design;
U-01, U-03 and U-07 stay open as the project owner's
decisions, and this audit promotes nothing.
