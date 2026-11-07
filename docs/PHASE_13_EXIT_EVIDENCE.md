# Phase 13 Exit Evidence Packet - M-009 (Wave 13, final slice)

Slice: M-009, team-09, branch yrsa/mimiralloc-09.
Base graded: 83125d680004b449370e5619bf5287141375395b (M-008 bank).
Host: Raspberry Pi Mimir (purpleviking1), aarch64. Date: 2026-11-07.
Recorded run: tests/abi/recorded_run_m009.txt (fresh this slice).
Banked evidence graded: M-001 62298e2, M-002 721e518, M-003 4e3bc36,
M-004 6b312e3, M-005 c6e57a3, M-006 11b36d6, M-007 6e066ce, M-008 83125d6.

## 1. Headline verdict

Phase 13 exit: **NOT MET as a stable-release exit; PARTIAL overall.**
Per M-001 Section 3, all SRC criteria must be MET for a stable claim;
they are not (tally in Section 3). What stands is a **research release
0.1.0** (M-007), characterised exactly as M-001's criteria grade it:
an experimental allocator, verified on one target only, losing to
glibc on all measured workloads, with every limitation public.
**No default global interposition is claimed** by this packet, by
any Wave 13 slice, or by any artifact in the tree.

## 2. Fresh run at 83125d6 (recorded_run_m009.txt)

- `make clean && make test` exit 0: `CHECKS: 4590 run, 0 failed`.
- `make lib` exit 0; `nm -D --defined-only` = exactly 4 symbols:
  mimir_malloc, mimir_free, mimir_calloc, mimir_realloc.
- `make test-fuzz-m004` exit 0: seeds 4001/4002/4003 + MT 4004 all
  SEED-RESULT PASS, RESULT: PASS, 0 fails.
- `make test-g008` standalone: 214 checks / 1 failure, exit 2 - the
  known stale check (INC-H001-01), reproduced, never hidden (SRC-08).
- Samtal HTTP 200 at start and at end; free -g available 13 G at both
  ends. The Pi was not starved; Samtal was not disturbed.

## 3. SRC-01..SRC-20 grading (M-001 Section 3 criteria)

| SRC | Grade | Evidence |
|---|---|---|
| 01 Suite | MET | Fresh run above: 4590/0, exit 0 |
| 02 ABI | MET | Fresh nm: exactly 4 symbols |
| 03 Code review | MET | M-002 covers all src/ + include/; findings fixed or public (INC-M002 register) |
| 04 API contract | PARTIAL | M-002 audit consistent overall; INC-M002-03 contract-doc inconsistency + Invariant 9 GAP divergences stand public |
| 05 Warnings | PARTIAL | Shipped flavours clean -Wall -Wextra -Werror (M-003); extended hardening gate failed on 2 flags, INC-M003-01 |
| 06 Fuzzing | PARTIAL | Corpus runs 0 violations (fresh re-run PASS); seed-set/model deviation, Section 4 |
| 07 Soak | MET | M-005 PASS: 10 cycles, 159,476 ops, RSS flat 1392 kB, dlopen 6/6, Samtal 200 throughout; depth caveat, Section 4 |
| 08 Stale g008 | PARTIAL | Fresh 214/1 reproduced + carried in M-008 public register; re-point decision still OWED |
| 09 Targets | MET | M-006 refreshed SUPPORTED_TARGETS matches runs only; INC-I009-01 discharged |
| 10 x86-64 | OWED | No hosted suite run; publicly listed unmet; no x86-64 stable claim. Phase 3 exit UNMET |
| 11 Android | OWED | No physical device run; publicly listed owed; no Android claim. Phase 4 exit UNMET |
| 12 Aesir comparison | OWED | Qwen3 absent + no aarch64 Aesir engine; Phase 12 NOT MET in register; no Aesir claim |
| 13 Versioning | MET | M-007: SemVer 0.1.0, SONAME libmimiralloc.so.0, versioned file + install target DECIDED-IMPLEMENTED |
| 14 Artifacts | MET | M-007: release/SHA256SUMS verifies; rebuild byte-identical on Pi |
| 15 SBOM | MET | M-008 SBOM + sbom.json enumerate components; third-party runtime NONE beyond libc, verified vs readelf/ldd |
| 16 Platforms+reporting | PARTIAL | SECURITY.md channel named but owner-enable owed (INC-M008-04); SUPPORTED_PLATFORMS grades stale vs SRC-09, Section 4 |
| 17 Public register | PARTIAL | M-008 register: 44 entries, full sweep; but register predates M-006/M-007 banks (INC-M008-01/02), Section 4 |
| 18 Interposition | MET | No artifact, doc or default enables global interposition; opt-in only |
| 19 Performance honesty | MET | Register states plainly: loses to glibc on all measured workloads (Phase 11, ~5x-65x) |
| 20 Pi safety | MET | Fresh run: Samtal 200 both ends, 13 G available both ends |

Tally: **MET 11 / PARTIAL 6 / OWED 3 / UNMET 0 of 20.**

## 4. Roadmap Phase 13 body tasks

Code review MET (M-002). API/ABI audit PARTIAL (M-002, GAP below).
Compiler-warning gates PARTIAL (M-003). Fuzzing PARTIAL (M-004).
Long soak MET (M-005). Target-matrix verification MET (M-006).
Checksum-verified artifacts MET (M-007; checksum route, not signing).
Versioning MET (M-007). SBOM MET (M-008). Reproducible-build
instructions MET (M-007). Documented supported platforms PARTIAL
(M-008 vs M-006, Section 5). Security-reporting channel PARTIAL
(M-008; owner enable owed). No default global interposition MET.
Exit clause 1 (stable criteria met): NOT MET. Exit clause 2 (all
limitations and unresolved defects public): PARTIAL - register is
public and near-complete; the Section 5 staleness is the gap.

## 5. Reconciliations (explicit, nothing hidden)

- **M-004 seed-set/model deviation.** M-004 was built to its original
  brief before the keeper's assignment rewrite landed. As run and
  re-run fresh here: seeds 4001/4002/4003 single-thread + MT 4004,
  168,384 checks / 0 failed in its recorded run, PASS again in the
  M-009 fresh run. The rewritten assignment's Section 6 seed set was
  NOT the set run, and the model field set is reduced vs the rewrite.
  Weighed: the corpus actually run is real, seeded, saved in tree and
  reproducible, and covers the roadmap Section 10 sequence classes;
  but SRC-06 cannot grade MET against a spec the corpus does not
  match. Grade PARTIAL stands; the deviation is a spec-conformance
  gap, not an evidence gap.
- **M-005 depth vs rewritten Sections 7-8.** M-005 was likewise built
  to its original brief plus its own assignment. Its design (10
  cycles, acceptance band derived from cycles 1-2 and never widened,
  <=20 min bound) matches the rewritten spec's bounds, and its PASS
  evidence is complete for SRC-07's terms. Its document and
  recorded-run depth are thinner than the rewritten Sections 7-8
  ask. Weighed: behaviour graded MET; the thinner record is noted
  here permanently, not silently absorbed.
- **M-008 platform docs vs M-006 SUPPORTED_TARGETS (11b36d6).** YES,
  M-008 statements are now stale in three grades. M-008 was authored
  at base 6b312e3 against the A-003-era matrix (its binding rule),
  before M-006's refresh banked. Against M-006 (authoritative):
  (a) x86-64: M-008 says OWED / nothing run; M-006 grades PARTIAL-RUN
  (freestanding probe re-run PASS + API TU cross-compiled; hosted
  suite OWED). M-008 understates. (b) musl: M-008 says OWED; M-006
  grades BLOCKED (toolchain probed absent). (c) Android: M-008 says
  BLOCKED; M-006 grades OWED (run) / ANALOGY-ONLY (contract cells).
  The AArch64 Linux/glibc VERIFIED-RUN grade agrees in both. This
  packet follows M-006 wherever they differ; a future slice should
  refresh docs/SUPPORTED_PLATFORMS.md to match. INC-M008-01 is
  discharged by this reconciliation; the doc refresh itself is OWED.
- **INC-M008-02 discharged here.** M-007 has since banked (6e066ce):
  SECURITY.md / SBOM generic release references now tie to SemVer
  0.1.0, SONAME libmimiralloc.so.0, versioned artifacts and
  release/SHA256SUMS. The register's Part iii D-07 entry recording
  those items OPEN is stale in the opposite direction - superseded
  by the M-007 bank, recorded here.
- **INC-M002-01/02 (known defect, GAP).** Roadmap 9.1 Invariant 9
  grades GAP on its reporting leg only: a lock/unlock failure is
  never reported (INC-M002-01) and the observable return is silently
  ambiguous (INC-M002-02). The safety leg holds everywhere - no sync
  failure returns overlapping memory. This is a known, public defect
  carried unresolved; it alone bars any production claim.
- **INC-M006-03 (owed).** BASELINE_INVENTORY.md is stale (claims no
  measured baseline / no allocator yet; both false since Phases
  3/11). Not refreshed by M-006 or by this slice. OWED to a future
  slice: refresh it or mark it superseded in place.
- **M-007 release character.** The 0.1.0 release is a **research
  release**, graded under M-001's SRC criteria as authored: SRC-13/14
  MET, but the release inherits every PARTIAL/OWED above. Nothing in
  M-007 or this packet promotes it to a stable or production release.

## 6. Carried-owed register (all stay OWED / visible beyond Wave 13)

1. Phase 3 exit UNMET - x86-64 hosted suite run owed (SRC-10).
2. Phase 4 exit UNMET - physical Android device run owed (SRC-11).
3. Phase 12 exit NOT MET - Qwen3 comparison run OWED: model absent
   on the Pi + no aarch64 Aesir engine exists (SRC-12).
4. K-007 proposal P-1 (batching) - recorded, NOT implemented.
5. INC-M002-01/02 - silent lock-failure reporting GAP (known defect).
6. INC-M006-03 - BASELINE_INVENTORY.md refresh owed.
7. test-g008 re-point decision (INC-H001-01 / SRC-08) owed.
8. SUPPORTED_PLATFORMS.md refresh to M-006 grades owed (Section 5).
9. SECURITY.md private-reporting enablement - owner action owed
   (INC-M008-04).
10. musl target BLOCKED - musl-tools install reserved, not given.
11. Phase 1 exit unmet; Phase 6 Clause A UNMET / X1 BLOCKED -
    carried unchanged from earlier packets.
12. Phase 7 exit PARTIAL (X5, INC-G001-06); Phase 8 decided postures
    (async cancellation, fork-while-busy unsupported); Phase 9 exit
    PARTIAL incl. INC-I001-14 (budget enforcement not adopted);
    Phase 10 exit PARTIAL items; Phase 11 exit PARTIAL (RSS/PSS,
    energy) - all as last graded, all in the M-008 public register.
13. R3 consumer route (dlopen/dlsym) untested by decision since
    Phase 5 - owed unless deliberately commissioned.
14. Phase 13's own PARTIALs: SRC-04, SRC-05, SRC-06, SRC-08,
    SRC-16, SRC-17 - each owed a discharging slice or decision.

Count: **14 carried-owed entries** (entry 12 bundles the per-phase
PARTIAL item families exactly as the M-008 register lists them).

## 7. Closing statement

Wave 13 produced real, recorded, reproducible evidence on one
target and an honest public register. It did not produce a stable
release, and this packet - the final slice of the final phase -
grades the Phase 13 exit **PARTIAL / NOT MET for any stable claim**,
with the failing and owed list above as the standing public record.
The research release 0.1.0 stands as exactly that, nothing more.

*End of M-009.*
