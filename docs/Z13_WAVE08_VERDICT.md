# Z13 Wave 8 Verdict

Slice: Z-072 - Wave 8 verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-12 by team-06 (fresh finishing lead; the six Mythic
Engineering roles run SEQUENTIALLY by the lead as reviewers -
see the team note in section 10; no parallel sub-agents were
spawned this run, and that is recorded plainly here and in
CAMPAIGN_STATE.md).
Base: merge 4661c11 of seat main ea05c0a (banked Z-001..Z-071,
ls-remote verified ea05c0a84cb627d0391702cb8db50f85077fe198)
into yrsa/mimiralloc-team-06 - clean merge, no conflicts; the
merge added exactly the banked Z-070 and Z-071 artifacts on top
of this branch's earlier merge 0330fcb of seat e506749 (banked
Z-001..Z-069). The composed stats surface was verified whole
after the merge: every Wave 8 append stands in slice order
(unlock_failures Z-064; drain/trim/free lock failures Z-065;
metadata_detections Z-066; live_release_detections Z-067;
double_free_detections + poison_detections Z-070), both
free-exit report consumers (z066_free_report,
z067_release_report) are present, and nm -D discipline is
unchanged (exactly 4 public symbols, per the source runs).

STATUS: FINAL for the whole wave. Every Wave 8 slice
(Z-064..Z-071) is banked at the seat and is FINAL below, each
traced to its banked recorded run. NO section is OWED: unlike
the Z-063 build (which carried Z-061/Z-062 as OWED sections
and folded them in later, the Z-036 / Z-045 pattern), this
verdict was written after the last Wave 8 slice banked, so
there is nothing to fold in and no number below rests on an
unbanked source.
The headline the banked evidence supports: Wave 8 made the
allocator's failures VISIBLE and priced its hardening. The
two lock-failure blind spots are counted (Z-064, Z-065);
three of the six J-006 silent corruption classes now DETECT
operationally (Z-066, Z-067) and the other three are closed
as undetectable-by-design with named compensating controls
(Z-067, Z-068) - the J-006 detection standing moves from the
banked 16/25 to 19 DETECTED / 3 by-design silent / 2
crash-expected / 1 valid, inside Z-068's own predicted
18-20 band. The economics slices then spent that visibility
carefully: canaries are priced and adopted as a verdict for
exactly one class group (Z-069, G3 only), double-free
membership + capped poison are ADOPTED and implemented
because the capped form passes all nine pre-registered bars
(Z-070), and the diagnostics gate is ruled to STAND as an
opt-in because its success-path price is a measured TIE and
the banked silence contract decides the rest (Z-071,
DECISION D-011). What still passes silently is listed at
full size in section 8 - nothing in it is hidden.

## Scope and honesty - read first

- Wave 8 (Z-064..Z-072) is hardening and integrity: the
  D-01 / D-02 observability debts, the D-12 silent-corruption
  classes, the D-09 fuzz refresh, and the L-07 diagnostics
  ruling. It is not a performance wave; with one exception
  (Z-071) its graded quantities are exact counters and
  refused/accepted outcomes, never wall times, and this
  verdict grades them in those terms. Where a source slice
  used the roadmap's WIN / TIE / LOSS grades, the grade is
  restated in its own terms (section 10's table).
- The roadmap's Z-072 text asks for: integrity standings -
  detection coverage, hardening costs paid, and the honest
  list of what still passes silently. All three are
discharged in full: coverage in sections 2-3, costs in
  section 6, the silent remainder in section 8.
- Every number below traces to a banked recorded run
  (section 9's index), re-read first-hand from disk on the
  merged seat tree by this lead. The one fresh execution
  beyond the standing gate is the composed battery of
  section 3.4, run by the previous lead on the Z-069 base
  and RE-RUN by this lead on the final merged tree with
  identical results.
- The 25-probe suite (test-j006) is NOT part of the standing
  gate. Its composed standing (15 ok / 10 FAIL) is reported
  exactly as measured, with every FAIL named and assigned
  to its proven family - never absorbed into the gate's
  GREEN and never relabelled.

## Purpose and gate

Roadmap gate for Z-072: "verdict doc traceable to recorded
runs." This document is that verdict for Wave 8, complete
at writing: every number cites its recorded run in
section 9.
Gate status: MET for every Wave 8 slice, Z-064..Z-071,
all banked at the seat before this verdict was written.
Standing correctness gate: scripts/z13/run_gate.sh GREEN
4988 checks / 0 failed / 77 by-design skips on this slice's
final tree (log hidden_files/z072_gate_final.log, counts
read from the log) - identical to the Z-001 baseline; this
slice touches no code, so the delta is zero by
construction.

## Host of record

purpleraider (ASUS ROG Flow Z13, GZ302EA), AMD Ryzen AI MAX+
395 (Strix Halo): 16 cores / 32 threads, 2 CCDs, single NUMA
node (UMA), L3 64 MiB (2 x 32 MiB), 128 GB LPDDR5X, 4 KiB
base pages, THP madvise. Ubuntu 26.10, kernel
7.3.0-10-generic, glibc 2.44, governor performance. Full
header: scripts/z13/host_truth.sh (Z-002), annexed verbatim
in tests/perf/recorded_run_z13_z072.txt (captured
2026-11-12T20:32:41Z, at this build; loadavg 0.07 / 0.16 /
0.30 - a quiet window on the shared host).

## 1. Evidence base - landed

LANDED (banked at the seat; every recorded run read
first-hand from disk by this lead on the merged tree):
| Slice | State | Seat commit | Recorded run |
|---|---|---|---|
| Z-064 lock-failure observability | BANKED | 6d81f1d | tests/perf/recorded_run_z13_z064.txt |
| Z-065 silent courses distinguishable | BANKED | 6e2e1f6 | tests/perf/recorded_run_z13_z065.txt + DECISION_LEDGER D-010 |
| Z-066 silent corruption round 1 | BANKED | a950f96 | tests/perf/recorded_run_z13_z066.txt |
| Z-067 silent corruption round 2 | BANKED | ee9ad17 | tests/perf/recorded_run_z13_z067.txt |
| Z-068 integrity standings | BANKED | 5b53309 | tests/perf/recorded_run_z13_z068.txt |
| Z-069 canary economics | BANKED | e506749 | tests/perf/recorded_run_z13_z069.txt |
| Z-070 double-free detection economics | BANKED | a9553e3 | tests/perf/recorded_run_z13_z070.txt |
| Z-071 diagnostics default-on packet | BANKED | ea05c0a | tests/perf/recorded_run_z13_z071.txt + DECISION_LEDGER D-011 |

OWED: none. No Wave 8 slice remains unbanked; sections 2-8
rest on banked sources only.

## 2. Observability standings - the silent courses are counted

### 2.1 Z-064 - lock-failure observability (D-01): GATE MET

Before Z-064 every unlock call discarded its status: a
failed unlock would wedge every later call with no counter,
no report, no termination (INC-M002-01). The slice wired a
post-unlock failure tally to the internal stats surface -
one _Alignas(64) pre-lock atomic, incremented at a single
counted wrapper through which every central unlock now
routes (41 former (void) call sites for the global lock, 7
for per-shard unlocks), appended to struct mimir_stats as
unlock_failures, and readable lock-free via
mimir_core_unlock_failures() - the getter exists because a
locked snapshot cannot be taken in precisely the wedged
state D-01 feared. The recording path takes no lock, by
design.
Injection proof (I-004 shim heritage, --wrap on the L6
unlock entry, -fno-lto leaf platform object because the
-flto production build inlines the wrapper): control tally
0 over 42 real unlock calls; REPORT mode 0 -> 1 -> 3 with
the workload unharmed; TRUE mode (mutex genuinely left
locked) 3 -> 4 read back through the lock-free getter, the
child exiting 0 within 10 ms of fork against a 10 s bound.
Standalone suite: make test-z064, 19 checks / 0 failed,
reproduced identically in 4 consecutive executions, and
again in this slice's composed battery (section 3.4).
Standing: D-01 is DISCHARGED. A failed unlock is now
counted and reported in both injection modes; in neither
mode does the process hang silently.

### 2.2 Z-065 - silent courses made distinguishable (D-02): GATE MET

Drain and trim returned 0 both for "nothing retained" and
for "the lock failed"; free-on-lock-failure kept its safe
posture invisibly. The ledger decision the roadmap names
was taken and recorded as DECISION_LEDGER D-010: COUNTER,
not sentinel - the I-003 return contract is unchanged (no
caller can misread a sentinel as a span count), a counter
reports to every later observer, and the shape matches
Z-064's neighbouring tally. Three pre-lock atomics
(drain / trim / free lock failures) were appended to the
stats surface with three lock-free getters; every F-07
free course counts (legacy central, owner-routed remote,
shard remote, TLS full-bin batch-drain), and behaviour at
every site is byte-for-byte the old posture - only the
tally moves.
Contract proof (tests/core/test_z065_silent.c, I-004
shim): the armed nothing-retained calls return the SAME 0
as control while trim moves to 2 and drain to 1 -
return-identical, tally-distinct; retained legs release
nothing armed and exactly the retained count unarmed; both
free courses leave the block LIVE and writable armed and
free it clean unarmed; the snapshot equals the getters at
drain 2 / trim 3 / free 2 with exactly 8 injected failures
accounted for. Suite: 561 checks / 0 failed, reproduced
identically in 3 consecutive executions, and again in the
composed battery. Allocation-path lock failures stay on
their pre-existing diagnostics channel and move none of
the three tallies (leg 6).
Standing: D-02 is DISCHARGED. The silent courses are
distinguishable from nothing-retained by counter, at every
course, with no contract change.

## 3. Corruption detection standings - the D-12 classes

### 3.1 Z-066 - round 1 (INC-J006-02): BOTH PROBES FLIP

The consistent-length classes - descriptor
mapping_length + usable_size corrupted consistently
(probe 05) and the span form (probe 19) - flipped from
SILENT to DETECTED. The mechanism is a byte-sum audit
promoted from the J-006 integrity walk to the operational
paths: descriptor_lengths_audit_locked() at the head of
retire_locked (the single choke point every descriptor
release passes through) walks the live registry and
requires walked count == live mappings and walked sum ==
live bytes; on mismatch the retirement is refused BEFORE
any state moves (registry-wide, fail-closed, because a
byte sum cannot localise), the episode tallies, and the
diag channel emits. The span form is judged by the span
length relation at the central touches. Standalone proof:
test_z066_length, 5 legs / 0 failed (control, descriptor,
span, compensating pair, gate-off). In the 25-probe suite
the two rewritten probes assert tally +1 and emitted +1
each with restore+free clean - a detection with a report,
not a relabelled expectation.
Scope of the claim, verbatim from the source run: single
non-compensating corruption on a live block only.
Compensating multi-descriptor corruption is NOT DETECTED
(the +1/-1 page pair with requested_size cover keeps the
walked sum equal - proven as the compensating leg);
pooled-descriptor corruption is NOT COVERED (off the live
registry); TLS-bin stock is served without re-validation
(detection fires at the next central touch). Cost: section
6. INC-J006 remains OPEN as a family after this slice.

### 3.2 Z-067 - round 2 (INC-J006-06 wired; INC-J006-04 by design)

Class 4 (span-live-zero, live_count trusted by release
paths) FLIPS to DETECTED: span_live_audit_locked() - the
per-span half of the walk's span invariant (free-list
nodes + live_count + TLS-binned count == carve_count;
OOL spans judged by bitmap recount) - is consulted at
exactly the whole-span release decisions: trim victim
selection behind drain / trim / trim_some, the
retention-overflow release in small_free_locked, the TLS
retention sweep, and the tally free course. A candidate
that fails the audit is skipped, never released.
Standalone proof: test_z067_livecount, 7 legs / 0 failed
(control, drain, trim, free/retention, gate-off,
freehead-live, sweep).
Class 3 (span-freehead-live, free_head naming a live
object, INC-J006-04) is UNDETECTABLE BY DESIGN in the
in-band free list: a free node's only mark is its position
in the list, a carved slot start is shape-valid as a node
whether live or free, every count the allocator keeps is
preserved by the corruption, and there is no liveness
record outside the 96-byte header for a pop-time check to
consult. Detection requires new state, not new wiring.
Compensating controls stand instead: shape validation
refuses wild / misaligned / out-of-carve heads outright;
the blast radius is one object of one class in one span
with the allocator's own accounting consistent through
the handout; the probe runs host-true in the suite and
passes AS SILENT with the zero-operational-detection
facts asserted; the designated fix road (a per-span
liveness record) was costed by Z-069 / Z-070 - see
section 4.2 for what their adoption does and does not
change here; MIMIR_DEBUG's J-005 quarantine narrows the
window in that flavour only.

### 3.3 Z-068 - integrity standings: the last two classes closed by record

The two remaining silent classes were closed by proven
undetectable-by-design records, NOT wiring - the same
honesty standard as Z-067's gate form. Standalone proof
tests/core/test_z068.c, fails=0 (re-verified by this lead
on the final merged tree, section 3.4):
- desc-owner-scratch (INC-J006-03): STAYS SILENT. The
  scratch is a diagnostics-grade record no allocator
  decision reads; with it flipped the walk reports 0,
  the free succeeds, the mapping retires normally, and
  the SOLE effect is cross_thread_frees falsely +1.
  Compensating control: the counter is recorded as
  diagnostics-grade trust - never evidence about memory
  safety. Closed BY-DESIGN SILENT.
- span-class-consist (INC-J006-05): STAYS operationally
  SILENT. A consistent class pair (index 1->3, size
  32->64) passes every header self-check by
  construction; the free is ACCEPTED under the false
  class (live 2 -> 1). The walk DOES flag it via the
  class_active cross-check - walk-only detection.
  Compensating controls: that cross-check, plus the
  named successor design (a static side record of each
  span's true class outside the 96-byte header - a
  design slice; walk-promotion into the free path was
  already rejected on cost in J-006 S6). Closed
  BY-DESIGN SILENT (operational) / WALK-DETECTED.
The D-09 fuzz refresh at M-004's harness, longer budget:
100,000 single-thread ops across 5 seeds (15,922
failure-shaped realloc, 10,114 overflow-shaped calloc),
3,155,737 checks, fails=0, every seed live_end=0;
multithread seed 4004 x3 PASS byte-identical; seed 4001
repeat REPRODUCIBLE. One harness defect was found and
fixed in scope (the maybe-uninitialized stats struct
that blocked the build on this host's gcc; 1 line).
D-09's named limit stands: mmap-refusal injection is
still not covered (section 8).

### 3.4 The composed standing - the full battery on the banked composition

Each Wave 8 slice proved itself on its own tree and
pre-declared its banking resolution (keep BOTH appends
in slice order; both free-exit report lines kept). The
composition itself was then executed, twice, with
identical results:
- Previous lead, 2026-11-12 14:56 EST, on merge 0330fcb
  (seat e506749, banked Z-001..Z-069): test-z064 19/0,
  test-z065 561/0, test-z066 5/5 legs, test-z067 7/7
  legs, test-j006 15 ok / 10 FAIL
  (hidden_files/z072_composed.log as first written).
- This lead, 2026-11-12 15:32 EST, on merge 4661c11
  (seat ea05c0a, banked Z-001..Z-071 - the final tree):
  the same battery, byte-for-byte the same standings
  (log hidden_files/z072_composed.log as re-run), plus
  make test-z070 PASS (5 legs / 0 failed) and the
  test_z068 treatment proof re-run standalone, fails=0.
  Load quiet (0.07 -> 0.30); every quantity is an exact
  counter or outcome.
The composed test-j006 standing, probe by probe: the 15
ok are 00 valid; 01, 02, 03, 04, 06, 07, 08 DETECTED;
05 desc-len-consist DETECTED (the Z-066 flip, tally +
emission asserted); 09 crash-expected (SIGSEGV, as
expected); 10 walk-detected; 11 desc-owner-scratch ok
AS SILENT (the Z-068 by-design record, asserted); 15
span-live-zero DETECTED (the Z-067 flip); 19
span-len-consist DETECTED (the Z-066 flip); 20
span-freehead-live ok AS SILENT (the Z-067 by-design
record, asserted). The 10 FAIL are 12 span-magic, 13
span-class-mismatch, 14 span-class-consist, 16
span-carve-reduced, 17 span-carve-inflated, 18
span-len-incons, 21 span-freehead-wild, 22
span-freelist-cycle, 23 span-next-cycle - the TLS-era
accounting family: J-006-era span expectations that
predate the Z-029 TLS cache (same-thread frees bin
without central validation; snapshot accounting settles
at flush) and the Z-048 mapping pool, failing at
snapshot / walk assertions, NOT at their classes'
detection substance (probe 14's silent leg itself
passed; it fails at its second snapshot assertion, and
its operational standing is proven directly by
test_z068 leg B) - plus 24 span-next-wild, whose child
SURVIVED because TLS binning serves the free before the
walk dereferences the wild link (recorded as observed
by Z-068, not relabelled).
Detection count verdict vs the banked 16/25: the banked
J-006 accounting was 16 DETECTED / 6 SILENT / 2 CRASH /
1 valid. Wave 8 moves it to 19 DETECTED / 3 SILENT /
2 CRASH / 1 valid: probes 05 and 19 DETECTED by Z-066,
probe 15 DETECTED by Z-067, and the three remaining
silences (11, 14, 20) are the by-design records of
sections 3.2-3.3, each with its compensating control -
19 lands inside the 18-20 band Z-068 predicted before
the composition existed. The suite's PASS count (15/25)
and the detection count (19/25) are different numbers
measuring different things - the 10 FAILs are stale
suite expectations, itemised as the suite-modernisation
debt in section 8 - and this verdict keeps them separate
on purpose.

## 4. Hardening economics - what was bought, what was refused

### 4.1 Z-069 - canary economics: one group adopts, three refuse

Per-block canaries costed at this machine's scale
(Z-004 protocol, pinned core 5, medians of 5 processes
per cell, two full sweeps with identical group verdicts;
quiet window, loadavg 0.26 -> 0.32). Time alone would
call canaries noise on every span class (fill+verify
+0.01%..+2.66%); the byte axis decides. Verdicts per
class group (stride forms tail16 / full32):
- G1 tiny (16-64): REJECT both. Capacity loss
  19.35%-50.00% (tail16) and 33.87%-66.80% (full32) -
  up to two-thirds of the arena for guard bytes.
- G2 small (80-256): REJECT both. Governed by class 128
  (loss 12.90% / 19.35% > the 10% bar); class 256 alone
  would have adopted tail16 - a group cannot adopt on
  its best class and bill its worst.
- G3 medium (320-1024): ADOPT-CANDIDATE both forms.
  Capacity loss 0.00% (page slack absorbs the stride),
  time +0.01%..+0.55% - the one group where the price
  is noise on BOTH axes. Adopted AS A VERDICT; the
  implementation is a named follow-on (section 8), per
  the campaign's paper-then-implementation separation.
- G4 large (>1024): REJECT both. The delta (+329 ns /
  +14.46% at 8192; +669 ns / +29.40% at 65536) is a
  page fault per fresh large block - the tail canary
  must be written at handout into a page the caller may
  never touch - not instruction cost at any size.
The J-004 debug red zones (MIMIR_DEBUG only) are
untouched; these verdicts govern production always-on
adoption only. One probe defect was found and fixed in
slice (the first draft's can32 leg overflowed the 16 B
class; the segfault was the probe's, not the
allocator's).

### 4.2 Z-070 - double-free detection: membership and capped poison ADOPTED

The cheap always-on subset costed the same way (medians
of 7 processes per leg, pinned core 2, quiet host).
Membership (freelist membership scan: the TLS bin scan
that closes the same-thread course, plus the J-006
central walk now labelling its meeting) is cheap
everywhere: +0.25..+2.27 ns/pair across both tables.
Full-extent poison is NOT: its cost grows linearly with
class size (+8.0 ns/pair at class 64, +67.6 at 256,
+222.0 at 1024 - 8.8x the whole pair) while its
detection value does not, and it FAILED its
pre-registered adoption bars at classes 64 / 256 / 1024
and at churnN / churnF class 64. REJECTED in that form.
The adopted form caps the poisoned range at bytes
[8, 40) of every class (MIMIR_DFREE_POISON_SPAN = 32
bytes past the link word): re-measured on the as-shipped
build, ALL NINE pre-registered bars PASS, full cost flat
at +0.9..+4.6 ns/pair from class 16 through class 1024.
Implementation landed in the slice: a second free of a
small block is REFUSED at every residence exactly once
(TLS bin membership, central walk label), tallied and
reported (double_free_detections); poison verify at
every reuse handout tallies and reports and the handout
PROCEEDS (telemetry posture). Injection proof:
test_z070_dfree, 5 legs / 0 failed, including the cap
boundary pinned as tested scope (a use-after-free write
at offset 39 is detected; at offset 40 it is NOT, by
design). Escape hatch documented and proven:
-DMIMIR_DFREE_GUARD=0 compiles every per-op check out
(core_off 50632 B vs core_full 52488 B); each half can
be disabled separately. Out of scope by design: large
mapping-path blocks, MIMIR_DEBUG (J-005 owns
detection), MIMIR_OOL_META spans.
Bearing on INC-J006-04 (section 3.2): the adopted
membership is double-free membership - it refuses a
SECOND free of a block already on a free list. It is
not a span liveness bitmap and does not give the
free-list pop a liveness record to consult, so
free_head naming a live object remains by-design
silent; Z-067's re-open condition (adoption of
membership STATE usable at pop time) is narrowed but
not met, and this verdict does not claim otherwise.

## 5. Diagnostics posture - Z-071 and the D-011 ruling

Diag-on cost measured across the four K-002 oracle
workloads (Z-004 protocol, pinned core 5, shared host
loadavg 0.74 at start; checksums equal to bench_k002's
on all four, identical across legs):
  workload          closed    open     dup   gate%
  small_churn        28.39   28.34   28.70   -0.18
  mixed_batch        26.58   26.61   26.52   +0.11
  large_batch        93.89   94.16   93.96   +0.29
  mixed_lifecycle   410.67  409.60  413.96   -0.26
Verdict: TIE on all four workloads - every gate delta
sits inside its identical-leg agreement (0.07-1.09%)
and far under the pre-registered 5% bound; emitted
delta is exactly 0 on every success leg in both
postures (success paths make no report calls, so an
open gate has nothing to pay for). On failure paths
emission is exact (1.00 report per genuine failure
over 10,000 attempts per shape) at ~97 ns per fixed
93.5 / 88.5-byte line to /dev/null - a floor price,
paid only by failing calls.
The ruling, DECISION_LEDGER D-011: the
MIMIRALLOC_DIAG=1 opt-in gate STANDS as the permanent
production posture; the default does NOT flip to on;
INC-J007-01 is DISCHARGED in the keep direction.
Grounds: cost decides neither way (above); the banked
C-006 silence contract decides - four banked suites
assert unset-means-silent, and a flip would amend four
evidence suites to change a default whose measured
price is zero; and nothing counted is lost by the gate
- detection and counting are always on (the z066 / z067
gate-off legs prove tally=1 with emitted=0), the stats
surface is the always-on record, and the stderr line is
a courtesy copy for an operator who opts in. No gate
bypass for corruption detections: the gate is uniform.
The INC-J007-03 site inventory is discharged by listing:
14 wired mimir_core_diag_report call sites (9
entry-point genuine failures, 3 locked-construction /
lock courses, 2 detection consumer helpers consumed at
13 exits); the Z-064 / Z-065 tallies are counted-not-
emitted by their own slices' design; the unwired
remainder is listed with reasons in the source run
(S6 + the S9 fold-in: the walk-only remainder, the
J-004 red-zone reporter, posture NULLs deliberately
unwired, and the detection limits restated so they are
not mistaken for wiring gaps). One pre-existing defect
was recorded, not fixed in slice: make test-j007 fails
59 checks / 10 failures identically on both parent
trees (fault-injection legs vs the Z-048 pool) -
section 8 carries it as named campaign work.

## 6. Hardening costs paid - the wave's price list

Every price below was measured on this host by its own
slice; where a slice graded only exact counters, no
timing price is claimed for it.
- Z-064 tally: one atomic fetch-add at the unlock site
  on a pre-lock cache line; no lock taken. No per-op
  timing price was graded (counters slice); the control
  leg's 42 unlocks over two workloads is the scale.
- Z-065 tallies: one atomic increment per FAILED lock
  at the silent courses - the success path pays
  nothing; no timing price graded (counters slice).
- Z-066 descriptor audit: ~2 ns per live registry entry
  per retirement (large alloc/free pair 31 -> 32 ns/op
  at 1 live mapping, 31 -> 166 ns/op at 65; large
  realloc transfer 161 -> 161 at 1, 170 -> 479 ns/op
  at 65, the transfer paying the audit twice). Linear
  in live mappings, immeasurable at desktop counts;
  the span relation and free-path label cost nothing
  measurable on the small path (20 ns/op both). The
  named successors if a many-mappings workload ever
  makes this bite are in the source run (side record
  of true lengths, or a checksum).
- Z-067 release audit: the invariant is recomputed only
  at whole-span release decisions (trim victim
  selection, retention-overflow release, the TLS
  retention sweep, the tally course) - cold paths by
  construction; no per-op price was recorded and none
  is claimed.
- Z-069 canaries: NOT PAID in production. The prices
  that bought the rejections: capacity loss up to
  66.80% (G1 full32) and a page fault per fresh large
  block (G4). The G3 price (0.00% capacity, +0.01%..
  +0.55% time) is approved but not yet paid - its
  implementation slice will pay it (section 8).
- Z-070 guard, as shipped: membership + capped poison
  together +0.9..+4.6 ns/pair, flat class 16 through
  1024, all nine bars passed. The full-extent price
  (+224.031 ns/pair at class 1024) was measured,
  judged, and NOT paid.
- Z-071 gate: 0 on every success path (TIE); ~97 ns
  per emitted line on failure paths only, and only
  when an operator opts in.
Net: the wave's always-on production price is the
Z-070 guard's few ns/pair plus two audit costs on cold
or large-path operations. Everything else is a counter
on a failure path, a rejected price, or an opt-in.

## 7. Wave 8 change log, Z-064 to Z-071

### Z-064 - Lock-failure observability - FINAL (banked 6d81f1d)
Unlock tally wired (stats append + lock-free getter);
injection proof moves the tally in REPORT and TRUE
modes; test-z064 19/0; gate GREEN 4988/0/77. D-01
discharged. Pre-existing test-j002 / test-i004 staleness
recorded by name (section 8), not absorbed.

### Z-065 - Silent courses made distinguishable - FINAL (banked 6e2e1f6)
DECISION D-010: COUNTER over sentinel. Drain / trim /
free lock-failure tallies at every course, behaviour
unchanged; test-z065 561/0; gate GREEN 4988/0/77. D-02
discharged.

### Z-066 - Silent corruption classes, round 1 - FINAL (banked a950f96)
INC-J006-02 descriptor + span consistent-length classes
wired DETECTED (byte-sum audit at retire; span relation
at central touches); probes 05 and 19 flip; test-z066
5/5 legs; A/B cost table filed; gate GREEN 4988/0/77.
Scope exclusions proven, not hidden (compensating,
pooled, TLS-bin stock).

### Z-067 - Silent corruption classes, round 2 - FINAL (banked ee9ad17)
INC-J006-06 live_count audit wired at every whole-span
release decision; probe 15 flips DETECTED. INC-J006-04
freehead-live recorded undetectable by design with five
compensating controls; probe 20 host-true, passes AS
SILENT. test-z067 7/7 legs; gate GREEN 4988/0/77.

### Z-068 - Integrity standings - FINAL (banked 5b53309)
INC-J006-03 and INC-J006-05 closed BY-DESIGN SILENT by
proof (test_z068 fails=0); full 25-probe re-run on its
base recorded probe-for-probe; D-09 fuzz refresh
100,000 ops / 3,155,737 checks / fails=0 at 10x budget;
one harness build defect fixed in scope; gate GREEN
4988/0/77. Detection-count prediction (18-20) confirmed
by section 3.4's composed 19.

### Z-069 - Canary economics - FINAL (banked e506749)
Cost table (time + bytes) over 9 classes, two sweeps,
identical verdicts: G1 REJECT, G2 REJECT, G3
ADOPT-CANDIDATE (both forms), G4 REJECT (page-fault
mechanism). No production code; G3 implementation is
the named follow-on. Gate GREEN 4988/0/77.

### Z-070 - Double-free detection economics - FINAL (banked a9553e3)
Full-extent poison measured and REJECTED (bars failed
at 64 / 256 / 1024); capped poison [8, 40) + freelist
membership ADOPTED and IMPLEMENTED - all nine bars pass
on the shipped build (+0.9..+4.6 ns/pair); second frees
refused at every residence; escape hatch proven;
test-z070 5 legs / 0 failed; gate GREEN 4988/0/77.

### Z-071 - The diagnostics default-on packet - FINAL (banked ea05c0a)
Diag-on cost TIE on all four oracle workloads; failure
emission exact at ~97 ns/line; DECISION D-011: the
opt-in gate STANDS, INC-J007-01 discharged (keep),
INC-J007-03 discharged by the site inventory (14 wired
sites; the rest listed with reasons). Gate GREEN
4988/0/77. Pre-existing test-j007 failure recorded
identically on both parents (section 8).

## 8. Remaining gap, itemised - what still passes silently, and the named debts

The honest list the roadmap names for this verdict.
Nothing here was discovered by this synthesis; every
item is a named gap of record from its source slice,
carried with its owner.
1. By-design silences (closed, with compensating
   controls - not defects): INC-J006-03 owner-scratch
   (statistics-only lie, Z-068); INC-J006-05
   class-consist (walk-detected only; successor = the
   side-record design slice, Z-068); INC-J006-04
   freehead-live (no liveness record exists to consult;
   re-open condition narrowed, not met, by Z-070,
   section 4.2). A same-span overlap handout, a free
   accepted under a false consistent class, and a +1
   cross-thread counter lie still pass operationally
   silently, by proven design limits.
2. Detection limits of the wired audits (Z-066 S6,
   Z-067 S2, restated by Z-071 S9): compensating
   multi-descriptor corruption NOT DETECTED;
   pooled-descriptor corruption off the live registry;
   relation-preserving live_count corruption outside
   the release audit; TLS-bin stock served without
   re-validation for every header class (detection at
   the next central touch). INC-J006 remains OPEN as
   a family.
3. Walk-only detection (Z-071 S6 + S9 fold-in): the
   integrity walk itself is wired to no production
   caller; 16 J-006 probes' detection exists only as
   mimir_core_metadata_check()'s return value. Z-066 /
   Z-067 promoted two of its invariants to operational
   paths; the rest stay walk-only, listed.
4. Suite-modernisation debt (the standing gate is
   GREEN; these standalone suites are not in it):
   test-j006 10 FAIL on the composed tree (section
   3.4's TLS-era family + probe 24's survival);
   test-j002 550/113 and test-i004 248/116 (Z-048 pool
   era, recorded at Z-064); test-j007 59/10
   (fault-injection vs pool/trim, recorded at Z-071
   identically on both parent trees). Each needs its
   own reconciliation slice; none is silently left -
   each is named here and in its source run as the
   keeper's queue.
5. Approved but unimplemented: the G3 canary verdict
   (Z-069 S6) - a flag-gated MIMIR_CANARY stride
   variant for classes 320-1024 with its own gate run
   and detection proofs. Until that slice lands, no
   production canary exists in any group.
6. Coverage limits kept: D-09's mmap-refusal injection
   still not covered by the fuzz harness (Z-068 S4);
   Z-070's guard does not cover the large mapping
   path, offset >= 40 poison writes, MIMIR_DEBUG or
   MIMIR_OOL_META postures (each by named design);
   Z-064 / Z-065 tallies are counted, never emitted
   (their slices' own design; the stats surface is
   their record).
7. Build hygiene (Z-071 S8): after a merge that grows
   struct mimir_stats, standalone make test-zNNN needs
   make clean first - stale objects once stack-smashed
   a battery; run_gate.sh cleans first and is immune.

## 9. Traceability index

Every number above traces to exactly one landed file:
- Unlock tally wiring, shim proof (42 calls; 0 -> 1 ->
  3 -> 4), test-z064 19/0, variant compiles, j002 /
  i004 standings: tests/perf/recorded_run_z13_z064.txt
  (6d81f1d).
- D-010 decision, three tallies, contract legs, final
  snapshot drain 2 / trim 3 / free 2, test-z065 561/0:
  tests/perf/recorded_run_z13_z065.txt (6e2e1f6) +
  docs/DECISION_LEDGER.md entry D-010.
- Class map, audit mechanism, probe flips, cost table
  (31 -> 166 / 170 -> 479 ns/op at 65 mappings),
  verbatim scope block: tests/perf/
  recorded_run_z13_z066.txt (a950f96).
- Release-path audit sites, probe 15 flip, probe 20
  by-design record + controls, composed expectation
  15/10: tests/perf/recorded_run_z13_z067.txt
  (ee9ad17).
- By-design proofs for probes 11 / 14, fuzz refresh
  (100,000 ops, 3,155,737 checks, fails=0), the 16/25
  banked accounting and the 18-20 prediction:
  tests/perf/recorded_run_z13_z068.txt (5b53309).
- Canary time + byte tables, grading rule, G1-G4
  verdicts, G4 page-fault mechanism:
  tests/perf/recorded_run_z13_z069.txt (e506749).
- Membership / poison Tables A + B, bars, cap ruling,
  escape-hatch object sizes, boundary-pinned proof:
  tests/perf/recorded_run_z13_z070.txt (a9553e3).
- Oracle TIE table, failure emission price, D-011
  ruling grounds, site inventory + S9 fold-in,
  test-j007 standing: tests/perf/
  recorded_run_z13_z071.txt (ea05c0a) +
  docs/DECISION_LEDGER.md entry D-011.
- Composed battery (both runs), test-z070 PASS and
  test_z068 fails=0 on the final tree, standing gate
  on this slice's final tree: tests/perf/
  recorded_run_z13_z072.txt (this slice) +
  hidden_files/z072_composed.log,
  hidden_files/z072_gate_final.log (team-local).

## 10. Verdict and what remains owed

Wave 8's landed verdict: the allocator no longer fails
quietly where the campaign could afford to hear it, and
the campaign now knows the price of every silence it
keeps. The observability debts are discharged by
counters that cost nothing on success paths (Z-064,
Z-065). The corruption standings are complete: 19 of
25 probes DETECTED, the 3 remaining silences proven
undetectable in this design and closed with controls,
the 2 wild-pointer classes crash-expected - and the
suite that carries those standings is itself honestly
red in 10 named places, a debt of the suite, not of
the standings. The economics were run before the
spending: canaries bought for one group as a verdict
and refused for three, poison adopted only after its
price was capped to flat, and the one implemented
guard (Z-070) pays +0.9..+4.6 ns/pair for refusing
every small double-free. The diagnostics question is
settled by ruling, not drift: emission is free on
success paths, priced on failure paths, and OFF by
default because the banked silence contract outranks
a zero-price default flip (D-011).
Per-slice grades, in each slice's own terms:
  Z-064 GATE MET (tally moves in both injection modes)
  Z-065 GATE MET (D-010 COUNTER; courses distinguishable)
  Z-066 DETECTED x2 (probes 05, 19 flip)
  Z-067 DETECTED x1 (probe 15) + BY-DESIGN x1 (probe 20)
  Z-068 BY-DESIGN x2 (probes 11, 14) + fuzz CLEAN
  Z-069 G3 ADOPT-CANDIDATE; G1 / G2 / G4 REJECT
  Z-070 ADOPTED as capped (full-extent REJECTED)
  Z-071 TIE on all four oracle workloads; gate STANDS
Nothing remains owed for Wave 8: no slice is unbanked,
no section of this verdict is OWED, and no number rests
on an unbanked source. The gaps that remain are the
non-slice gaps of section 8 - suite modernisation, the
G3 implementation follow-on, the INC-J006-05 side-record
design, and D-09's injection coverage - each named
with its owner for the keeper's queue, none of them
inside a verdict slice's scope.

Team note: this slice was executed by a fresh team-06
finishing lead (the previous lead composed the battery
of section 3.4 on the Z-069 base and went quiet before
writing the verdict). The finishing lead merged seat
ea05c0a, re-ran the composed battery plus test-z070 and
the test_z068 proof on the final tree, verified every
number above first-hand against the banked recorded
runs in the merged tree, and ran the six Mythic
Engineering roles SEQUENTIALLY itself as reviewers -
evidence audit, observability review, detection review,
economics review, gap / honesty review, and gate /
commit-scope review - because no parallel sub-agents
were spawned for this completion; recorded plainly here
and in CAMPAIGN_STATE.md, per the muster instruction.
All device-side work was executed by the lead alone, in
this worktree only. Bugs discovered in the allocator:
none. No new benchmark was run for this verdict; the
fresh executions on the final tree are the composed
battery and the standing gate.
