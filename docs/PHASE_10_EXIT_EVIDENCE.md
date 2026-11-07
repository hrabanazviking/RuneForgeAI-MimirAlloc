# Phase 10 Exit Evidence Packet (Slice J-009)

Status: **PHASE 10 EXIT = PARTIAL.** Graded honestly from
banked and banking-parallel evidence on base `1aab762`
(J-002 banked; Wave 10 = 2 of 9 banked at this base,
J-003..J-008 banking-parallel on sibling branches, held
for serialized banking). No Android, x86-64, musl or
proot result is claimed anywhere in this packet.

Governing texts: `docs/PHASE_10_FOUNDATION_PLAN.md`
(J-001, exit items X1-X10 + XC, Section 6) and the
roadmap Phase 10 exit sentence: *statistics balance
under tests, error reports are useful and do not
recursively allocate, and production/debug behavior is
clearly distinguished.* Precedent followed exactly:
H-009 / I-009 (honest MET/PARTIAL/UNMET grading;
banking-parallel evidence named as such, never as a
run on this base).

Sibling evidence (all on base `1aab762`, read-only from
sibling branches, keeper-verified): J-003 `df9b084`,
J-004 `43be05d`, J-005 `86ce5eb`, J-006 `9920686`,
J-007 `386be3b`, J-008 `083189c`. J-003 banking was in
flight at this packet's tick; its figures below are
cited from its branch commit, not re-run here.

## 1. Fresh verification run (this base, this slice)

Recorded in `tests/core/recorded_run_j009.txt`
(960 lines, sha256
4ab7e573d242450278068209794b60831ec9d74118b13c75cfae50222891f859),
run on the Pi 2026-11-07: `make clean && make test`
exit 0 (4590 checks, 0 failed); `make test-j002`
550/0; standalone targets i003, i004, i005, i006,
i007, i008, h001-h008, g001, g003-g007, f003, f005,
f006, f007, e002-e008, d003, d005-d008, c007 and
f004-mojo all exit 0 (40 targets at exit 0 in total).
The one non-zero target is recorded, not hidden:
`make test-g008` exit 2 (214 checks, 1 failure) -
solely the stale growth PASS-check of INC-H001-01,
banked and superseded by test-h001 (299/0, in this
run at exit 0); test_g008.c is banked evidence and
was not edited. `nm -D --defined-only` = exactly 4
(mimir_malloc, mimir_free, mimir_calloc,
mimir_realloc). README.md and
`include/mimir_alloc.h` byte-identical to base
(sha256-verified in the recorded run; diff 0).
Samtal HTTP 200 before/after on port 8787;
free 13 GiB available before/after.

## 2. Exit-item grades

| Item | Grade | Evidence |
|---|---|---|
| X1 counters implemented per S4 contract | **PARTIAL** | J-002 (banked, this base): ONE locked snapshot `mimir_core_stats_snapshot` (shape (b), `tests/core/mimir_stats_j002.h`, nm -D exactly 4); bytes_requested / bytes_allocated cumulative at the two publication points, bytes_cached = Q-RETAINED and bytes_mapped = Q-MAPPED from the snapshot walk, bytes_released = Q-RELEASED at retire / span-release successes, peak_mapped = Q-PEAKMAPPED at publications only, alloc_failures = Q-FAILED-OS with posture NULLs counted separately, per-class span occupancy; test_j002 550/0 incl. I-004-shim failure leg. Recorded exceptions cap the grade: cross_thread_frees PARTIAL (mapping path only, INC-J002-01; small path contributes exactly 0 by recorded choice), lock_contention ABSENT with reason (INC-J002-02, roadmap-optional, deferral recorded not hidden), D-REQ-LIVE not provided (INC-J002-05 / INC-I002-02: small request sizes are not retained, a named impossibility), D-FAIL-BUDGET 0-by-absence (INC-I001-14, no budget exists). |
| X2 statistics balance R1-R12 | **PARTIAL** (banking-parallel) | J-003 `df9b084`: test_j003 2155/0, make test 4590/0, test-j002 550/0, test-i008 PASS, nm -D exactly 4. R1-R9 and R12 PROVEN exactly at quiescent points across churn / calloc / realloc, R9 call accounting, I-004 fault leg incl. rolled-back release, trim cycles, exact peak high-water, and an RLIMIT_AS pressure child. Caps: R10 free-slots clause NOT-PROVABLE (INC-J003-02), R11 Tier 2 (small path) NOT-PROVABLE and the realloc transfer-retire narrowing proven only as documented narrowing, D-LOCK / D-FAIL-BUDGET NOT-PROVABLE as recorded absences (INC-J003-01..04). Balance is proven wherever the surface makes it provable; the unprovable clauses are named, not skipped. |
| X3 debug red zones + canaries | **MET** (banking-parallel, stated scope) | J-004 `43be05d`: MIMIR_DEBUG flavour via `make DEBUG=1` into `build-debug/` (S5.1; production build untouched, diff purely additive, all src changes `#ifdef MIMIR_DEBUG`); small slots gain 16 B pre-zone + 16 B post-zone (pattern 0xA5, stride class+32), verified at free / realloc / reuse and on demand via `mimir_core_debug_verify_all`; detection = fixed-buffer write(2) to fd 2 + abort, no allocation / recursion; test_j004 23/0 under DEBUG=1 incl. disposable-child SIGABRT proofs (post-zone +8 B free, pre-zone -4 B free, post-zone +1 B realloc); nm -D exactly 4 in BOTH flavours. Overhead exact: 32 B/object, capacity table for all 20 classes; bench 719 prod vs ~1068-1156 debug ns/op (timing-never-an-oracle). Stated scope limit carried, not hidden: large-path red zones scoped out (INC-J004-01); overwrites inside class slack are not detectable by any guard outside the class extent. |
| X4 debug poisoning + quarantine | **MET** (banking-parallel, stated scope) | J-005 `86ce5eb`: debug-only poisoning (0xDD over the whole class extent at free) + bounded global FIFO quarantine (capacity exactly 32 blocks, 512 B static table, no allocation), small path only; test_j005 81/0. S5.3 discharged both readings: quarantine lives inside spans and a span holding quarantined blocks is never treated as EMPTY (retention, trim/drain, Q-RETAINED and the J-002 statistics walk all skip it); bytes_quarantined is its own quantity (1024 B proven in Leg A with bytes_cached and Q-RETAINED unmoved), never folded into D-CACHED. Detection at eviction and on demand; corrupted eviction fails clearly (fd 2 report + abort) in disposable children only. Overhead measured: 0 B/block layout, held bytes at bound 7,168 B in Leg C (absolute ceiling 32,768 B), one debug-side timing 687 ns/op; production column zero/absent by construction. Large path excluded with reason (INC-J005-01: a freed large mapping is unmapped whole; no freed bytes exist to poison). |
| X5 metadata integrity | **PARTIAL** (banking-parallel) | J-006 `9920686`: strengthened `descriptor_is_live` / `span_header_is_live` with the exact layout relations publication writes, counter-bounded registry walks, range-checked count-bounded free-list walk, free-list shape validation, and the test-only integrity walk `mimir_core_metadata_check` (shape (b), nm -D exactly 4); make test 4590/0 unchanged, test-j002 550/0. 25 disposable-child probes only: 1 baseline, 16 DETECTED, 6 SILENT, 2 expected SIGSEGV (wild registry `list_next`, INC-J006-01 - no in-process check can validate a pointer without reading it). Caps: the 6 SILENT classes remain misbehaving (incl. consistent length corruption INC-J006-02, free_head naming a live object INC-J006-04, live_count trusted by release paths INC-J006-06), the 2 CRASH classes remain, and detected classes refuse quietly - loud reporting via the J-007 channel is not wired at those sites (INC-J007-03). No run claims repair of arbitrary corruption. |
| X6 error reports useful + allocation-free | **PARTIAL** (banking-parallel) | J-007 `386be3b`: production channel `mimir_core_diag_report` (shape (b)) formats ONE fixed-format line (`mimiralloc: error op=<op> cause=<cause> req=<bytes> failures=<n> posture=<m>`) into a 224-byte stack buffer from fixed tables and writes it with write(2) to fd 2 - no malloc-family call, no L6 call, no stdio, no lock, proven by a counting interposer while backing allocation was armed to fail; re-entry suppressed and counted, maximum depth 1; every genuine failure NULL of the three allocating entry points emits exactly one line with its true cause class, posture NULLs / successes / invalid-pointer realloc emit nothing; test_j007 59/0. Cap (headline, INC-J007-01): emission is gated on `MIMIRALLOC_DIAG=1` because the S5.4 default-on reading collides with the banked C-006 silence battery (MA-6.3-020..023 graded at zero bytes). The channel is wired in production and proven; whether it is on by default is a ruling owed to banking reconciliation / Volmarr - no ruling is invented here. Hence at most PARTIAL. Also carried: J-006 detection sites not wired to the channel (INC-J007-03). |
| X7 hardened build + overflow audit | **MET** (banking-parallel) | J-008 `083189c`: hardened flavour in its own `build-hardened/` directory (probed flags; artefact-verified `-z now` + stack protector via readelf/nm; nm -D exactly 4; test-j008 25/0; make test exit 0, production build unchanged). Integer-overflow audit over every size path at base: 10 CHECKED / 8 SAFE-BY-CONSTRUCTION groups / 0 UNCHECKED, incl. the S4 counters' own arithmetic (modulo-2^64 by the J-001 S4 contract) and re-verification of the S2.10 banked checks. INC-J004-02 production half graded here; its debug half belongs to X8 and is carried there (INC-J008-02). Flag honesty preserved: FORTIFY and stack-clash have no artefact check for these sources and are claimed as armed/accepted only. |
| X8 debug vs production overhead table | **PARTIAL** (banking-parallel) | J-008 `083189c`, S2a measured on this base: production small64 median 121 ns/op, large4096 6,192 ns/op; production + J-002 snapshot per 1,000 ops identical medians (snapshot cost under ~1 ns/op amortised); hardened 73 / 5,644 ns/op with an -O2 control (71 / 5,717) isolating the hardening flags' cost at ~+2 ns/op small, not measurable large. Caps: the debug rows are only provable on the merged tree (INC-J008-01) - J-004 / J-005 figures are cited from their branches (J-004 exact 32 B/object + bench in its own shape; J-005 512 B static + 687 ns/op debug-side), never presented as merged-tree measurements, and J-008 records the exact filler commands for banking. INC-J004-02 debug half likewise owed: three banked checks diverge under debug by design and await flavour grading on the merged tree. Hence at most PARTIAL. |
| X9 production/debug distinguished as a whole | **PARTIAL** (banking-parallel) | For distinction: one `MIMIR_DEBUG` flag (S5.1), separate `build-debug/` directory in both J-004 and J-005, nm -D exactly 4 in both flavours in both slices, production suite 4590/0 unchanged in every slice. Against a whole-wave MET: the two debug slices were built on parallel branches and their union is not yet proven on one tree - J-004's red-zone layout changes the debug class stride/extent that J-005's poison extent and verify walk were written against (INC-J005-03 reconciliation owed: Makefile debug blocks, core `#ifdef` blocks, and the poison extent re-statement), and three banked checks diverge under debug by design awaiting flavour grading (INC-J004-02). The distinction is real per slice; as a merged whole it is owed to banking reconciliation. |
| X10 this packet | **MET** | This packet grades X1-X9 + XC from banked evidence (J-002, fresh run) and named banking-parallel evidence (J-003..J-008, commits cited), carries every unclosed INC by name in Section 4, and claims no Android / x86-64 / musl / proot result. |
| XC standing caveat | **HELD** | No Wave 10 surface expands the public ABI: every slice held nm -D at exactly 4 and the shape-(c) promotion decision remains untaken. Statistics drive no allocator decision (Invariant 8 held in J-002's construction and J-003's proof). No document in the wave claims heap-corruption repair (S9.2): J-006's posture is detect-and-refuse, J-004/J-005 detect in debug only. |

**Tally: MET 4 (X3, X4, X7, X10), PARTIAL 6
(X1, X2, X5, X6, X8, X9) / UNMET 0. XC HELD.**

## 3. Verdict against the roadmap exit sentence

*Statistics balance under tests:* proven wherever
the banked surface makes balance provable - R1-R9
and R12 exact, R10 / R11 partial by recorded
construction limits (X1, X2 PARTIAL). *Error reports
are useful and do not recursively allocate:* the
channel is built, wired in production and proven
allocation-free under injected backing failure; its
default-on status is unresolved against the banked
silence battery (X6 PARTIAL, INC-J007-01 - a ruling
is owed, not invented). *Production/debug behavior
is clearly distinguished:* per slice yes - one flag,
separate directories, identical ABI, measured costs;
as one merged tree, not yet (X8, X9 PARTIAL).
Phase 10 therefore exits **PARTIAL**, in the H-009 /
I-009 sense: no exit item is UNMET, no claim in this
packet exceeds its evidence, and every remainder is
named below with its owner.

## 4. Carried-owed register (nothing below is discharged)

- INC-J007-01: diagnostics emission gate
  (`MIMIRALLOC_DIAG=1`) vs the C-006 silence battery
  and the S5.4 default-on reading - ruling owed at
  banking reconciliation; X6 caps at PARTIAL until
  then. INC-J007-03: J-006 detection sites not wired
  to the J-007 channel.
- INC-J008-01 / INC-J008-02 (with INC-J004-02):
  debug overhead rows and the INC-J004-02 debug half
  are provable only on the merged tree; J-008's
  filler commands are the banking recipe. X8 caps
  at PARTIAL until they run.
- INC-J005-03 / INC-J006-08: J-004 + J-005 debug
  union reconciliation (one build-debug/ directory,
  one flag shape, poison extent re-stated against
  the red-zone layout) owed at banking; X9 caps at
  PARTIAL until the merged debug flavour passes.
- INC-J002-01 / INC-J002-02: cross-thread counting
  stays mapping-path PARTIAL (Tier 2 uncounted);
  lock contention stays ABSENT by recorded deferral.
- INC-J006-01..09 remainders: 6 SILENT + 2 CRASH
  corruption classes with their named smallest
  remedies, unrepaired by design in Wave 10.
- INC-J004-01: large-path red zones owed to a
  future slice if "where appropriate" is read to
  include them.
- INC-I001-14: budget enforcement was adopted by
  NO slice, Wave 10 included; D-FAIL-BUDGET stays
  0-by-absence and Phase 9 X1 stays PARTIAL.
- INC-I009-01: `docs/SUPPORTED_TARGETS.md` is
  stale (still claims documentation-only); owed to
  its own docs-freshness slice, not folded here.
- Phase 3 exit UNMET (x86-64 suite run owed);
  Phase 4 exit UNMET (physical Android run owed);
  Phase 9 exit PARTIAL - all carried unchanged.
  Every Wave 10 figure is a Pi-native AArch64
  Linux/glibc figure on 16 KiB pages.
- U-03 / U-09 / U-10 / U-12 remain open exactly as
  the public header states them.

## 5. INC-J009 register (found by this packet)

- INC-J009-01 (banking order): this packet, J-003
  and J-004..J-008 all sit on base `1aab762`; the
  grades marked banking-parallel become banked
  grades only when the keeper's serialized banking
  lands those commits in order and the merged-tree
  runs named in Section 4 (debug rows, union
  reconciliation) are recorded. Until then Wave 10
  stands at 2 of 9 banked plus this packet held.
- INC-J009-02 (fresh-run honesty): the fresh run's
  single non-zero standalone target (test-g008,
  INC-H001-01's stale check) is reproduced exactly
  as banked; it is not a Phase 10 regression and
  no count in Section 1 hides it.
- No Makefile change was needed or made, no src/
  change, no allocator defect was found by this
  slice. README.md and `include/mimir_alloc.h`
  are byte-identical to base (diff 0, Section 1).
