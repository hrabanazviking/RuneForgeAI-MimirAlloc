# Phase 9 Exit Evidence Packet (Slice I-009)

Status: **PHASE 9 EXIT = PARTIAL.** Graded honestly from banked
evidence on base `4a2a307` (I-007 banked; Wave 9 = 7 of 9 at this
base, I-008 banking-parallel). No Android, x86-64, musl or proot
result is claimed anywhere in this packet.

Governing texts: `docs/PHASE_9_FOUNDATION_PLAN.md` (I-001, exit
items X1-X10 + XC, Section 4) and the roadmap Phase 9 exit sentence:
*fault injection and constrained-memory runs show documented,
bounded behavior; no claim to prevent Android low-memory kills.*

## 1. Fresh verification run (this base, this slice)

Recorded in `tests/core/recorded_run_i009.txt`, run on the Pi
2026-11-07: `make clean && make test` exit 0 (4590 checks, 0
failed); `make test-i003` 72/0; `test-i004` 248/0; `test-i005`
PASS; `test-i006` 120/0; `test-i007` 63/0 — all exit 0.
`nm -D --defined-only` = exactly 4 (mimir_malloc, mimir_free,
mimir_calloc, mimir_realloc). README.md and
`include/mimir_alloc.h` byte-identical to base (sha256-verified).
Samtal HTTP 200 before/after; free 13 GiB available.
`test-i008` is NOT in this base and was NOT run here (see X9).

## 2. Exit-item grades

| Item | Grade | Banked evidence |
|---|---|---|
| X1 soft budgets | **PARTIAL** | Defined in full by I-002 (`docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md`: counted quantity, boundary, scope, setter). Enforcement was adopted by NO slice: INC-I001-14 stands (I-003 adopted only the partial-trim stop-at-need mechanism, INC-I003-02). Defined-not-enforced; grades no higher by the plan's own rule. |
| X2 telemetry definitions + reconciliation | **MET** | Definitions: I-002 S3.2/S4/S5.3. Proven by recorded reconciliation: I-007 (`docs/PHASE_9_TELEMETRY_SEPARATION.md`, 63/0) and I-003's locked Q-MAPPED/Q-RETAINED snapshots (72/0). |
| X3 trim / cache release on demand | **MET** | I-003 implemented + measured (`docs/PHASE_9_TRIM_IMPLEMENTATION.md`, recorded run 72/0): retained empty spans released on demand via the test-only/internal surface by recorded decision (no public ABI expansion), counters and OS figure move together, live blocks untouched, retention bound re-verified. |
| X4 fault injection (Section 9.3) | **MET** | I-004 (`docs/PHASE_9_FAILURE_HANDLING.md`, recorded run 248/0): Nth-mapping fault-injection shim landed (discharging INC-I001-01), exact NULL, no corruption/overlap/hang, recovery succeeds, realloc failure byte-atomic, both paths, several N. |
| X5 constrained-memory runs | **MET** (Linux track) | I-005 (`docs/PHASE_9_CONSTRAINED_RUNS.md`, 35 checks total across legs, 0 failures): per-child RLIMIT_AS calibrated from measured VmSize + exact page arithmetic, 30 s parent wall bound; large-fill exactly 32 blocks at 64 pages; refusal exactly NULL; healthy afterwards. Android and proot tracks named NOT RUN (owed, Section 4). |
| X6 reclaim overhead measured | **MET** | I-006 F-4 (`docs/PHASE_9_FRAGMENTATION_AND_RECLAIM.md`, 120/0): free-path and trim release costs with exact span/byte deltas; hit median 130 ns vs miss median 2482 ns (timing never an oracle). |
| X7 fragmentation measured | **PARTIAL** | I-006: F-3 free-but-unusable 70,976 B (R-EXT 0.270752), F-1(b) tails 3,360 B (R-INT 0.022786), F-2 mapping overhead 54,688 B — all measured. F-1(a) is NOT measurable from allocator state (INC-I002-02) and was computed workload-side exact (360 B) and labelled as such; F-5 unmeasurable (no budget); F-6 cross-slice leg owed (INC-I006-02). |
| X8 telemetry separation proven | **MET** | I-007 (63/0, +3 repeat PASS): same-instant allocator/process snapshots at 11 quiescent points; separation proven both directions; exact VM reconciliation; +16,384 B resident residual honestly reported as kernel-managed (INC-I007-02). |
| X9 repeated pressure cycles | **MET** (banking-parallel) | I-008, keeper-verified figures (Pi commit 55bb6ab; this doc was NOT in the I-009 base — reconciles at I-008's banking, per H-009/E-009 precedent): test_i008 child 213/0 + parent 10/0; 10 RLIMIT_AS cycles, IDENTICAL count tuple every cycle (zero drift): small 1200/1200, retained after free-all exactly 5 spans / 81,920 B, large successes before clean NULL exactly 61/cycle, trim released exactly 5/cycle; all 20 quiescent snapshots reconcile exactly (cumulative 50 spans / 610 mappings each way); Samtal HTTP 200 before/during/after; INC-I008-01..04; no allocator defect found. No run of test-i008 is claimed by I-009. |
| X10 this packet | **MET** | This packet grades X1-X9 from banked evidence, carries the Android caveat in its verdict (Section 3), and claims no Android/x86-64/musl/proot result. |
| XC Android LMK caveat | **HELD** | No Phase 9 document claims the allocator can prevent, survive-by-guarantee, or gracefully recover from an Android low-memory kill. Phase 9 makes NO claim to prevent Android LMK kills. |

**Tally: MET 8 (X2, X3, X4, X5-Linux, X6, X8, X9, X10),
PARTIAL 2 (X1, X7) / UNMET 0 / OWED items in
Section 4. XC HELD.**

## 3. Verdict against the roadmap exit sentence

The exit sentence's Linux half is satisfied: fault injection
(I-004) and constrained-memory runs (I-005, I-008) show
documented, bounded behavior on Pi-native AArch64 Linux/glibc,
16 KiB pages. The exit as a whole grades **PARTIAL**, because:
(1) X1 budget enforcement was never adopted (INC-I001-14);
(2) X7 is partial (F-1(a) allocator-state form unmeasurable,
F-5 unmeasurable, F-6 cross-slice leg owed); (3) the Android
track is entirely OWED — no Android run exists (Phase 4 exit
UNMET), and per XC this packet claims nothing about Android
low-memory kills.

## 4. Carried-owed register (nothing below is discharged)

- INC-I001-14: budget enforcement NOT adopted by any slice;
  X1 caps at PARTIAL and F-5 stays unmeasurable until a future
  slice adopts enforcement by recorded decision.
- INC-I006-02: F-6 cross-slice leg (Q-MAPPED sampling across
  I-005's constrained runs) still owed.
- Phase 3 exit UNMET: x86-64 suite run still owed; every Wave 9
  figure is a 16 KiB-page, single-target figure.
- Phase 4 exit UNMET: physical Android run still owed; the
  Android pressure track of X5 is OWED with it.
- Phase 8 PARTIAL remainder (S5 postures), Phase 7 X5 remainder,
  Phase 1 unmet, Phase 6 Clause A UNMET/X1 BLOCKED: carried
  unchanged, as restated in the foundation plan Section 5.

## 5. INC-I009 register (found by this packet)

- INC-I009-01 (stale doc): `docs/SUPPORTED_TARGETS.md` still
  states the repository contains documentation only and no
  allocator exists — true at its Phase 0 slice, stale since
  Phase 3. Not edited here (outside this slice's file scope);
  recorded for a future docs-freshness slice.
- INC-I009-02 (count presentation): I-005's doc total (35
  checks) sums child legs, while its recorded run's parent
  RESULT line reads 10 checks; both are correct, but a reader
  can mismatch them. Noted, not a defect in the evidence.
- No Makefile change was needed or made (H-009 precedent held).
  No count mismatch was found between the banked Phase 9 docs'
  headline figures and their recorded runs for I-003..I-007.
