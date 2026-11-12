# Z13 Trim Policy Under Pressure

Slice: Z-050 — Trim policy under pressure (Wave 6).
Status: policy designed, implemented, and measured on this
host, 2026-11-12, team-04 worktree, on top of seat main
517bbb1 (banked Z-001..Z-046). The mapping-pool clauses of
section 6 are CONDITION-PENDING on Z-048 (see there).
Artifacts: this doc (the policy of record);
src/core/mimir_core.c + src/core/mimir_core.h (the policy
layer over the I-003 trim surface);
tests/core/mimir_trim_policy_z050.h (test-only surface);
tests/core/test_z050_trim_policy.c (parser / decision /
apply suites); tests/perf/probe_z050.c (the RSS
pressure-cycle probe); tests/perf/run_z050.sh +
tests/perf/summarize_z050.py (the run of record);
tests/perf/recorded_run_z13_z050.txt (the evidence).

## 0. Position and the dependency, stated plainly

The roadmap gate reads: "Conservative return-to-OS policy
driven by pool idle time and /proc/pressure/memory; RSS
honesty proven by Z-006's leg. Gate: pressure-cycle run
shows RSS follows the policy's promise."

Z-050 builds on the Z-048 mapping cache — and Z-048 is
BUILT BUT NOT BANKED at this slice's commit (team-02
b528228, on top of the same seat main; Z-047's design is
team-01 936d7b4, Z-049 team-03 9dc6d05; banking order is
Z-047, Z-048, Z-049, then this slice). The seat tree this
slice merges therefore contains NO mapping pool: on the
banked tree the large path still munmaps on every free and
retains nothing, and the only retained pool in existence
is the small-span pool (per-class empty-span retention,
bound default 2, max 16; the I-003 trim surface over it).

This slice therefore does two honest things:

- It implements and MEASURES the full policy on the pool
  that exists on this tree (the span pool): PSI parser,
  decision table, idle clock, execution through the one
  I-003 release path, and a pressure-cycle RSS run in
  which RSS follows the policy's promise to the KiB.
- For the mapping pool it writes the policy clauses
  against Z-048's banked-to-be surface (section 6),
  marked CONDITION-PENDING in the Z-033/Z-039 style: the
  clauses activate on Z-048's banking with no policy-code
  change (Z-048 already extends the full trim/drain entry
  points to release the whole pool), and the mapping-pool
  pressure-cycle leg is OWED to Z-060, whose roadmap text
  already runs the pressure cycles "with Z-050's policy".
  No mapping-pool number is claimed anywhere in this
  slice, because the code that could produce one is not
  on this tree.

## 1. What the policy drives (this tree)

- Q-RETAINED (I-002 S3.2): retained EMPTY small spans and
  their bytes, read exactly under the lock by
  mimir_core_retained_spans / _retained_bytes.
- The release path is the existing one and only one:
  mimir_core_trim_retained (all) and
  mimir_core_trim_retained_some (class-fair prefix), both
  over trim_some_locked / span_release_locked (I-002
  S5.1). Z-050 adds NO second release path.
- The internal surface is INC-E001-07 shape (b), exactly
  like drain/trim: declared in src/core/mimir_core.h,
  re-declared for tests in
  tests/core/mimir_trim_policy_z050.h, bound local in the
  shared library — nm -D on libmimiralloc.so stays exactly
  4 (verified on the final tree), and
  include/mimir_alloc.h is untouched.

## 2. The policy

Inputs, and only these two:

- A PSI sample from /proc/pressure/memory: some avg10 and
  full avg10 (percent), parsed by mimir_pressure_parse
  (pure, fixture-tested) or read live by
  mimir_core_pressure_read. The parser anchors each
  line's trailing total= at its own line start — the
  kernel format is "some avg10=.. avg60=.. avg300=..
  total=..", and there is no "some total=" substring in
  it (this slice's first parser draft searched for one;
  its own suite caught it before any run was filed —
  section 7).
- The retained pool's idle time: pool-level, derived at
  tick time from the J-002 activity counters. While bytes
  are retained and bytes_requested + bytes_released has
  not moved since the previous tick, the pool is idle;
  the idle age is CLOCK_MONOTONIC seconds since the last
  movement. The policy's own releases move
  bytes_released, so a trim restarts the idle clock — a
  pool that just paid a trim is never immediately
  re-trimmed by the calm band.

Decision table (mimir_trim_decision_for, pure; thresholds
are the policy of record, named once in the code):

| Band   | Condition                                    | Idle         | Decision |
|--------|----------------------------------------------|--------------|----------|
| HIGH   | some avg10 >= 5.00 OR full avg10 >= 2.50     | any          | FULL     |
| RAISED | some avg10 >= 1.00                           | >= 10 s      | FULL     |
| RAISED | some avg10 >= 1.00                           | < 10 s       | PARTIAL  |
| CALM   | some avg10 < 1.00 AND full avg10 < 2.50      | >= 60 s      | PARTIAL  |
| CALM   | some avg10 < 1.00 AND full avg10 < 2.50      | < 60 s       | NONE     |

- FULL = mimir_core_trim_retained(): every retained span
  returns to the OS in this evaluation.
- PARTIAL = mimir_core_trim_retained_some(ceil(retained
  spans / 2)): half the pool, class-fair round-robin (the
  I-003 walk), oldest-acquired first within a class.
- NONE = nothing is released.
- full avg10 below the HIGH line never raises the band by
  itself: full pressure means ALL tasks stalled, which on
  a shared desktop host is usually someone else's storm;
  the conservative policy keys escalation on `some`, and
  treats `full` as a HIGH-only corroborator.
- Fail-calm: a PSI read/parse failure, a NULL sample, or
  a failed stats snapshot makes the tick return -1 and
  trim NOTHING. A broken pressure input must never cause
  a trim. (It also never blocks an explicit caller-driven
  mimir_core_trim_retained.)
- Evaluation is caller-driven: mimir_core_trim_policy_
  tick(report) performs one read + decision + apply and
  files sample, idle, decision, released spans, and
  Q-RETAINED before/after. This allocator has no
  background thread and Z-050 adds none; a host that wants
  periodic trimming calls the tick from its own cadence.
  mimir_core_trim_policy_apply(sample, idle, &decision)
  is the same decision executed against a caller-supplied
  sample — the form the pressure-cycle probe drives.

## 3. The promises

- P-1 (calm conservatism): with CALM pressure and a pool
  idle less than 60 s, the policy releases nothing. RSS
  does not move.
- P-2 (pressure response): with HIGH pressure, ONE
  evaluation releases the entire retained pool, whatever
  its idle age. Q-RETAINED falls to 0 and RSS falls by
  the retained resident bytes.
- P-3 (graduated middle): RAISED pressure, or a CALM pool
  idle a full minute, releases exactly half the retained
  spans (rounded up) per evaluation — the pool halves
  toward zero under sustained pressure/idleness and never
  oscillates, because each release restarts the idle
  clock.
- P-4 (no surprises): live spans, live objects, and live
  mappings are never touched (the I-003 walk selects only
  EMPTY, non-quarantine, non-bin-held spans); the
  allocator is fully usable after any policy trim; a
  FULL on an empty pool is a no-op.
- P-5 (honest input): the policy acts only on a real PSI
  sample or a caller-supplied one through the documented
  apply form; it invents no pressure of its own.

## 4. Evidence on this tree (the run of record)

tests/perf/recorded_run_z13_z050.txt, produced by
tests/perf/run_z050.sh on this host; machine load state
is in the run header (shared host, Aesir + Tensile
co-tenants; RSS is a resident-memory measurement, not a
timing, and the legs are process-local).

- Unit/policy suite (make test-z050): 60 checks run,
  0 failed. Parser fixtures (the live kernel format,
  boundary values, malformed inputs, *out untouched on
  failure, the host's real /proc/pressure/memory parses);
  every decision-table boundary on both sides; apply
  against a real retained pool (96 spans built across six
  classes): CALM+young released 0 (P-1), RAISED+young
  released exactly ceil(96/2) = 48 (P-3), HIGH released
  the remaining 48 and Q-RETAINED reconciled to 0 bytes
  (P-2), FULL-on-empty no-op (P-4), allocator usable
  after; live tick on this host's real PSI (avg10 0.00 at
  run time) filed a self-consistent report with decision
  NONE.
- Pressure-cycle probe (3 attempts, all PASS): baseline
  RSS ~1.86 MB; build + touch + free across six classes
  with the retention bound at 16 leaves 96 retained spans
  = 393,216 B, and RSS after free ~6.59 MB — the retained
  bytes are VISIBLE in RSS (Z-006 honesty, section 5).
  CALM leg: decision NONE, 0 released, RSS unmoved by the
  policy. PRESSURE leg (injected HIGH sample some 25.00 /
  full 10.00 through the apply form — the policy's input
  contract is a PSI sample; the live-file path is covered
  by the tick test): decision FULL, 96/96 spans released,
  Q-RETAINED 0, and RSS fell by exactly 384 KiB — the
  retained bytes to the KiB — in all three attempts.
  RSS FOLLOWS THE POLICY'S PROMISE (P-2).
- Standing gate on the final tree
  (scripts/z13/run_gate.sh, hidden_files/z050_gate.log):
  4988 checks / 0 failed / 77 by-design skips, GREEN —
  identical to the Z-001 baseline. The Z-050 suites are
  standalone (make test-z050 / probe-z050), NOT wired
  into make test, per the Z-043 precedent.

Measurement notes, filed honestly:

- The probe compares the pressure leg against the RSS
  read immediately before it (after the calm leg), not
  against the after-free read: the calm leg's own stdio
  activity grew RSS ~132 KiB between the two reads in the
  first (failed-baseline) run, masking the drop. The
  first probe run is a FAIL of the probe's baseline
  choice, not of the policy — from the calm peak the drop
  was already exactly 384 KiB — and the correction is
  recorded in the probe source.
- Retained-pool measurement must drain the TLS bins
  first: the retained queries flush the calling thread's
  pending list, but only the trim surface drains TLS
  bins, and a span under bin-held blocks is EMPTY only
  after the drain. Both suites therefore normalise with
  mimir_core_trim_retained_some(0) — releases nothing —
  before measuring, so the measured pool is the pool the
  policy walks. Without this, before/after span counts
  disagree by exactly the bin-held spans.

## 5. RSS honesty (Z-006's leg, restated for this policy)

Z-006 proved the pre-pool allocator's virtue: AFTER_FREE
RSS returned to baseline because nothing was retained.
The span pool spends a bounded part of that virtue on
purpose (bound: retention_bound x 20 classes x 4 KiB =
at most 1.25 MB at the maximum bound of 16, 160 KiB at
the default of 2), and this policy is what gives it back:
after-free RSS = baseline + retained resident bytes, the
retained bytes are exactly auditable through Q-RETAINED
at any instant, and P-2 guarantees one HIGH evaluation
returns the whole spend. The probe's numbers are the
proof on this tree: 393,216 B retained, 393,216 B of RSS
returned, to the KiB, three attempts in a row.

## 6. CONDITION-PENDING: the mapping pool (Z-048)

Everything in this section is policy WRITTEN against
Z-047's design (team-01 936d7b4) and Z-048's
implementation surface (team-02 b528228), both built on
the same seat main but NOT banked at this slice's commit.
It takes effect when Z-048 banks, in this order, with no
policy-code change; the verdict leg is owed to Z-060.

- CP-1 (FULL reaches the pool): Z-048 extends the full
  trim and drain entry points to release the whole
  mapping pool as well as the span pool (its core header
  states this verbatim). The policy's FULL decision —
  the HIGH-pressure response — therefore evicts every
  pooled mapping in the same evaluation once Z-048 is
  banked: one policy, one release path, both pools.
- CP-2 (eviction order): pool eviction for trim takes
  the least-recently-pooled unit first (Z-047 MC-16:
  buckets are LIFO for reuse, LRU for eviction), per
  bucket, so a PARTIAL-scale eviction sheds the coldest
  shelter first and keeps the warmest units — the ones
  whose resident pages are the dense-reuse prize
  (Z-047 S1: 92–95% of the fresh dense price).
- CP-3 (the DONTNEED middle course): for the mapping
  pool, PARTIAL is a two-step course, not a half-evict.
  Step one, on the first PARTIAL against a unit:
  MADV_DONTNEED over the unit's whole usable extent
  only, recorded in the unit's flags (Z-047 S3.7) — RSS
  is returned, the VMA and the bucket slot are kept, and
  a later reuse repays the faults (Z-047's probe priced
  this exactly: DONTNEED pooling saves 0.2–1.4% of time;
  it is an RSS tool, never a speed tool). Step two, on a
  further PARTIAL (or any FULL) against an
  already-DONTNEED unit: evict whole (POOLED -> RETIRED,
  exact-pair munmap, Z-047 MC-01). A unit therefore costs
  two sustained-pressure evaluations to fully shed, and
  HIGH pressure (FULL) still sheds everything in one.
  Implementing CP-3 needs one Z-048-follow-up: a per-unit
  DONTNEED entry point on the pool surface — named here
  as OWED-Z050-01 to the slice that first banks a
  policy change after Z-048 (expected: Z-060's cycle
  work), because the DONTNEED flag write must happen in
  the pool's own locked section.
- CP-4 (caps are the spend): whatever the policy does,
  the pool's worst-case RSS spend stays Z-047's hard
  caps (64 units, 64 MiB, eligibility <= 16 MiB), exposed
  by mimir_core_pool_count / mimir_core_pool_bytes
  (Z-048's gauges) — the mapping-pool analogue of
  section 5's audit, restated in Z-054's verdict per
  OWED-Z047-06.
- CP-5 (MADV_FREE): not adopted, not rejected. Z-047
  left it unmeasured on this kernel; it stays a
  candidate for Z-060 to price beside CP-3, and this
  policy's resident-or-gone posture keeps that
  measurement clean.
- OWED leg: the pressure-cycle run for pooled mappings
  (with-pool AFTER_FREE RSS beside the banked figure,
  Z-047 OWED-Z047-02, then a HIGH evaluation returning
  RSS toward baseline) is OWED to Z-060, whose gate is
  exactly the cycle report on this host. Z-050 claims
  the span-pool cycle only.

## 7. Bugs found and fixed in scope

- Parser needle bug (this slice's own, caught by this
  slice's own suite before any run was filed): the first
  parser searched for the substrings "some total=" /
  "full total=", which do not exist in the PSI format —
  every valid sample parsed as failure. Fixed by
  line-anchored " total=" searches (section 2). The
  suite's malformed-input rows now also pin the correct
  behaviour against the real kernel text.
- Probe baseline flaw (measurement, not allocator):
  comparing the pressure leg's RSS against the
  after-free read masked the exact 384 KiB drop behind
  the calm leg's own +132 KiB footprint growth. The
  probe now compares against the immediate pre-pressure
  read; the reasoning is recorded in its source.
- No pre-existing allocator bug was found by this
  slice's suites; the Z-043 fixes this branch carries
  survived the seat merge (95841de) and the gate.

## 8. Files

- docs/Z13_TRIM_POLICY.md (this doc)
- src/core/mimir_core.c (policy layer: parser, live
  read, decision, apply, tick — over the I-003 surface)
- src/core/mimir_core.h (shape-(b) declarations)
- Makefile (test-z050 / probe-z050 targets only;
  standing gate untouched)
- tests/core/mimir_trim_policy_z050.h
- tests/core/test_z050_trim_policy.c
- tests/perf/probe_z050.c
- tests/perf/run_z050.sh
- tests/perf/summarize_z050.py
- tests/perf/recorded_run_z13_z050.txt
