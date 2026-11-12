# Z13 Pressure Cycles on This Host

Slice: Z-060 — Pressure cycles on this host (Wave 7).
Status: measured on this host, 2026-11-12, team-05
worktree, on top of merge 321ba7f (seat main c2ed635,
banked Z-001..Z-056, merged into yrsa/mimiralloc-team-05).
Artifacts: tests/perf/probe_z060.c (modes cycles / caps /
madv / ceiling / hog / hogfree), tests/perf/run_z060.sh,
tests/perf/summarize_z060.py, Makefile probe-z060 /
test-z060 targets (standalone, NOT in make test), and
tests/perf/recorded_run_z13_z060.txt (the evidence).

## 1. What this slice is

The roadmap text: "Phase-9 pressure-cycle heritage
re-run: cgroup ceilings, trim response, recovery after
pressure release (with Z-050's policy). Gate: cycle
report; no unbounded retention observed or the bound
that failed is named."

The heritage is docs/PHASE_9_PRESSURE_CYCLES.md (I-008,
on the Pi): repeated cycles in ONE process under a
per-process ceiling, retained/live figures returning to
the same bound every cycle, stable failure behaviour,
responsiveness reported with no bound claimed. Z-060
re-runs that experiment on this host with two things
I-008 did not have: the Z-048 mapping pool (live since
Z-048 banked) and Z-050's trim policy as the trim under
test. Z-050's mapping-pool clauses CP-1..CP-5 were
CONDITION-PENDING there and their pressure-cycle leg
was OWED to this slice; section 4 discharges or names
each one.

Safety on a shared host: every ceiling in this slice is
scoped to this slice's own test processes only — a
systemd user scope (systemd-run --user --scope,
MemoryMax + MemorySwapMax=0) created per leg and gone
when the leg ends, plus the probe's own RLIMIT_AS in
the I-008 form. No host, system, or other-campaign
limit was set or touched; the hog legs' OOM kills were
contained inside their scopes (exit 137, host load
unchanged, witness lines in the recorded run).

## 2. The legs and their results

All numbers are from tests/perf/recorded_run_z13_z060.txt
(run_z060.sh, load 0.90/0.82/1.04 at start and end —
a quiet window on this shared host; RSS legs are
resident-memory measurements, process-local).

- cycles (5 cycles in one process, x3 attempts, all
  PASS, tuples identical in all 15 cycles): build the
  Z-050 span shape (6 classes x 3000, retention bound
  16 -> 96 retained spans = 393,216 B) plus a mapping
  pool of 8 dense-touched larges (6,979,584 B pooled).
  After free, RSS 13,624 KiB vs baseline 2,076 KiB —
  both retentions visible in RSS. CALM (policy apply,
  sample 0/0): decision NONE, 0 released, nothing moved
  (Z-050 P-1). PARTIAL (RAISED, idle 0): decision
  PARTIAL, exactly 48 spans released, and the mapping
  pool delta is 0 bytes — PARTIAL is span-blind to the
  pool (section 4, CP-3). Rebuild restores 96 spans.
  FULL (injected HIGH sample through the apply form,
  Z-050's input contract): decision FULL, 96 spans
  released, pool 8 -> 0 and retained -> 0, and RSS fell
  13,624 -> 6,424 KiB: a drop of exactly 7,200 KiB =
  (393,216 + 6,979,584) / 1024, the two retentions to
  the KiB (CP-1). Recovery allocations pattern-verified
  after every FULL; quiescent reconciliation after every
  cycle: live mappings 0, live small objects 0,
  Q-MAPPED 0, Q-RETAINED 0. No ratchet: the 16-field
  per-cycle counter tuple equals cycle 1's in every
  cycle of every attempt.
- caps (CP-4 proof, section 4): 65 distinct mapping
  lengths freed -> pool count stops at exactly 64,
  pooled bytes exactly 8,781,824 (the first 64
  lengths), and the 65th retire's 270,336 B appear in
  Q-RELEASED (the munmap path). A single 20 MiB mapping
  pools whole (20,975,616 B) — there is NO per-mapping
  eligibility line in the implementation; a second
  20 MiB retire would cross the byte cap and munmaps
  (Q-RELEASED += 20,975,616). A single 40 MiB mapping
  (41,947,136 B) can never pool and munmaps at free.
  The pool never exceeds 64 units / 33,554,432 B.
- ceiling (I-008 form inside a cgroup scope): the probe
  ran inside a systemd user scope (MemoryMax=512M,
  MemorySwapMax=0; its own cgroup path is in the run)
  and set its own RLIMIT_AS = measured VmSize baseline
  7,328 KiB + 40 MiB headroom (soft limit 49,446,912 B,
  hard limit untouched). With both retentions built, the
  1 MiB fill succeeded exactly 34 times, then returned
  NULL, and NULL again (stable failure, a real OS
  failure — the I-005/I-008 class). At the pressure
  point a 64 B allocation served from a retained span
  completed in 2,505 ns and a /proc/self/status health
  read in 24,236 ns (reported, no bound claimed, the
  heritage posture). Release: fill freed, FULL trim
  under the ceiling left both pools at 0, soft limit
  restored, and a fresh 1 MiB allocation succeeded and
  pattern-verified (recovery after pressure release).
- hog (pure cgroup containment): raw 32 MiB dense-touch
  chunks inside a scope at MemoryMax=256M,
  MemorySwapMax=0: 7 chunks completed (RSS 231,548 KiB),
  the 8th fault was met by the cgroup OOM killer —
  exit 137, the kill contained in the scope, host
  loadavg unchanged after (witness in the run).
- madv + hogfree (CP-5 pricing, section 5): on raw
  16 MiB mappings, MADV_DONTNEED returned 16,276 KiB of
  RSS at once and re-touch re-faulted exactly 4,096
  pages; MADV_FREE returned 0 KiB and re-touch faulted
  0 pages (pages still resident — lazy). Under the
  256M scope ceiling, a hog that MADV_FREEd 128 of its
  192 MiB survived to 11 completed chunks with RSS
  pinned at the ceiling (~262.7 MiB) — the FREE pages
  were reclaimed first — where the plain hog died in
  its 8th chunk. FREE under pressure is a reclaim
  source; FREE without pressure returns nothing.

Standing gate on the final tree
(scripts/z13/run_gate.sh, hidden_files/z060_gate.log):
4988 checks / 0 failed / 77 by-design skips, GREEN —
identical to the pre-slice baseline gate on the merged
tree (hidden_files/z060_baseline_gate.log) and to the
Z-001 baseline. Neighbouring standalone suites on the
final tree: test-z048 57/0 PASS, test-z050 60/0 PASS,
test-z052 35/0 PASS. nm -D on libmimiralloc.so stays
exactly 4 (mimir_malloc/calloc/realloc/free).

## 3. Gate verdict

Cycle report: filed (the recorded run). NO UNBOUNDED
RETENTION OBSERVED. Every retention on this tree is
hard-bounded and the bounds were measured, not read:
span retention 393,216 B at the raised bound of 16
(bound x 20 classes x 4 KiB; 160 KiB at the default 2),
mapping pool 64 units and 33,554,432 B (32 MiB), both
returning to exactly 0 under one FULL policy evaluation
or one trim/drain, in every one of 15 cycles, with RSS
following to the KiB.

The bound that FAILED is a policy bound, not a
retention bound, and it is named plainly: Z-050's CP-3
promised PARTIAL a two-step DONTNEED-then-evict course
over the mapping pool; on the banked implementation a
PARTIAL evaluation releases ZERO pooled bytes (measured
partial_pool_delta=0 in all 15 cycles). Under sustained
RAISED pressure the mapping pool therefore stays at its
cap until a HIGH evaluation (or idle >= 10 s escalation
to FULL) — bounded, but not graduated. CP-3 remains
owed (OWED-Z050-01 stands); it does not fail this
slice's gate, whose terms are the cycle report and the
retention bound, but no future text may cite PARTIAL as
shedding pooled mappings.

## 4. CP-1..CP-5: the Z-050 discharge table

Verdicts against the BANKED implementation
(seat c2ed635; src/core/mimir_core.c), each grounded in
this slice's measurements:

- CP-1 (FULL reaches the pool): DISCHARGED-PROVEN.
  mimir_core_trim_retained and drain both call
  pool_release_all_locked in the same locked section;
  the cycles leg measures it: one HIGH apply took the
  pool 8 -> 0 and RSS down the pooled bytes to the KiB,
  15 cycles out of 15.
- CP-2 (eviction order, LRU per bucket): NOT
  DISCHARGED — STILL OWED. There is nothing in the
  implementation for the clause to describe: no
  per-length buckets exist (Z-047's MC-16 design was
  not implemented in that form); the pool is one list,
  inserted at the head, exact-length take scanning from
  the head — LIFO reuse. Whole-pool release makes order
  moot for FULL, and PARTIAL never evicts, so no
  eviction order is observable at all. Closing CP-2
  needs an explicit policy decision — adopt single-list
  LIFO as the design of record (superseding CP-2) or
  commission buckets — not a measurement. This slice
  records the facts and takes neither decision.
- CP-3 (PARTIAL = DONTNEED-then-evict): NOT
  DISCHARGED — STILL OWED. Neither step exists: PARTIAL
  routes to mimir_core_trim_retained_some only (spans),
  there is no madvise anywhere in src/platform or the
  pool code, and no per-unit DONTNEED entry point
  exists (OWED-Z050-01). Measured consequence in
  section 3. The platform layer has no madvise surface
  at all today, so CP-3 is a platform + pool
  implementation slice, not a policy edit.
- CP-4 (caps are the spend): DISCHARGED-WITH-
  CORRECTION. The caps are real, measured in the caps
  leg, and are the operative spend bound — but the
  Z-050 S6 / Z-047 text is wrong in two places against
  the banked code, and this doc is the correction of
  record: total byte cap is 32 MiB (33,554,432 B), NOT
  64 MiB (Z-048's recorded run already stated 32 MiB;
  the policy doc repeated the design figure); and
  there is NO <= 16 MiB per-mapping eligibility line —
  a single mapping up to the full remaining byte cap
  pools (measured: 20,975,616 B pooled whole). Count
  cap 64 stands as written. Z13_TRIM_POLICY.md S6 CP-4
  should be amended to 64 units / 32 MiB, eligibility
  struck, at the keeper's banking hand (this slice does
  not rewrite the banked policy doc).
- CP-5 (MADV_FREE candidate): DISCHARGED BY DECISION —
  priced beside CP-3 in section 5 and REJECTED as a
  trim mechanism for this policy. Re-open condition
  named in section 5.

## 5. CP-5 priced: MADV_FREE on this host

Question left open by Z-047/Z-050: should a trim course
use MADV_FREE (lazy return) beside or instead of a
DONTNEED-style (immediate return) step? Priced on raw
mappings here (no allocator surface exists for either
call; these are host/kernel prices, stated as such):

- Without pressure: FREE returns 0 KiB of RSS and the
  pages stay resident (re-touch: 0 faults) — a FREE trim
  would leave RSS exactly where it was until someone
  else's pressure reclaims the pages. DONTNEED returns
  everything at once (16,276 KiB of a 16 MiB mapping;
  the residue is the probe's own footprint) and costs
  the full re-fault on reuse (4,096 faults / 16 MiB).
- Under cgroup pressure: FREE pages are the first
  reclaim source (hogfree leg: +4 chunks of survival at
  the same ceiling, RSS pinned at the cap while the
  FREE pool drained). FREE is a real pressure valve —
  for the process being squeezed.

Decision: REJECTED as a trim mechanism for the Z-050
policy. The policy's promise is RSS honesty (Z-006's
leg, policy S5): a trim must return what Q-RETAINED /
the pool gauges say it returned, when it says so. A
FREE-based step would make the gauges lie by exactly
the un-reclaimed residue, for an unbounded time, on a
host whose normal state is no pressure (118 GiB
available at run time). Re-open condition: a future
caller whose contract is explicitly lazy (it wants the
pressure valve, not the RSS) may adopt FREE for ITS
path with the residue named in its own gauges; the
policy of record does not.

## 6. Findings and limitations, honestly filed

- Policy input is HOST PSI; scope-local pressure is
  invisible to it. The ceiling legs generate real
  pressure inside a user scope; /proc/pressure/memory
  (the policy tick's input) stayed at avg10 0.00 through
  the run — the pressure lives in the scope's own
  memory.pressure, which the policy does not read. The
  cycle legs therefore drive the policy through its
  documented apply form with injected samples (the
  Z-050 contract, whose live-file path the Z-050 suite
  covers), and the ceiling leg proves allocator
  behaviour under real confinement separately. A
  per-cgroup pressure input is a future policy
  candidate, named here, not built.
- Evidence limit: a transient scope's memory.events
  (oom_kill counter) dies with the scope; containment
  evidence for the hog legs is the exit code 137 plus
  the chunk/RSS trail plus the host witness lines.
- Responsiveness numbers (2,505 ns probe alloc,
  24,236 ns health read at the pressure point) are
  single observations on a shared host, reported in
  the I-008 posture: no bound is claimed from them.
- No allocator defect was found by this slice: every
  predicted count was met exactly in every leg. The
  two defects found were documentary (CP-4's figures,
  section 4) and in this slice's own probe drafts
  (section 7).

## 7. Bugs found and fixed in scope

- Probe header collision (this slice's own): the first
  probe draft included both the Z-050 test header and
  the J-002 stats header, which pulls in
  src/core/mimir_core.h — the policy structs are
  defined in both. Fixed by taking the policy surface
  from mimir_core.h alone (which declares it) and
  dropping the stats header; the pool/trim/tune test
  headers redeclare functions only and coexist legally.
- Probe madvise visibility (this slice's own): under
  -std=c11 with _POSIX_C_SOURCE, madvise/MADV_FREE are
  not declared; the probe now defines _GNU_SOURCE.
- Probe minflt parser (this slice's own, caught by the
  madv leg itself): the /proc/self/stat reader fed the
  state character to strtol and read -1 everywhere,
  reporting 0 re-faults for DONTNEED — a measurement
  bug, not a kernel result. Fixed (skip the state
  field); the leg then read exactly 4,096 / 0.
- No pre-existing allocator or suite bug was found;
  the standing gate and the z048/z050/z052 neighbours
  are green on the final tree with identical counts.
