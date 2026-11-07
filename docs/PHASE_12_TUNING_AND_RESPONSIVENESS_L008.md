# Phase 12 - Tuning Evaluation + Responsiveness Record (L-008)

Status: **MEASURED. NO-CHANGE VERDICT - no tuning change
ships.** The one runtime tuning knob the allocator exposes
(retention_bound, G-007) was A/B-measured on the exact
L-006 phase shape and on a small-object churn sensitivity
leg, in both build flavours. The banked default (2) is
CONFIRMED, not merely left alone: in the production flavour
turning retention OFF is ~47% slower on the churn leg,
while raising it to 16 gains nothing over 2 and would raise
the retained-after-free ceiling 8x. Every other candidate
the profiles suggest is either structural (not a knob, per
the banked G-007 catalogue) or has no timing evidence
behind it - and the L-004/L-005/L-006 profiles contain no
timing at all, which this slice states plainly: the A/B
timing below is THIS slice's first measurement, run under
the Phase 12 discipline (tune only where a profile shows a
cost; a no-change verdict with measured evidence is a
complete slice - K-005 / H-004 precedents). Phase 11's
honest headline is untouched by anything here: this slice
measured no glibc comparison and claims none.

Base: 03436f3 (L-006 bank). Workload shapes, sizes and
snapshot discipline are L-006's; the internal tune/stats
surfaces are reached by object-link only (L-004 pattern).
No src/, header, Makefile or README change. Public ABI:
exactly 4 dynamic symbols, verified.

## 1. What the banked profiles actually implicate

From L-004 (load/metadata), L-005 (tokenizer/KV), L-006
(prefill/decode/teardown) - every figure banked there:

- **Small objects are nearly absent from the phase
  shapes.** L-006 live small objects at every snapshot:
  exactly 1 (the 128 B prompt-id buffer, inside one
  16,384 B span). L-004: 23 live small (64 B metadata
  records + key copies). L-005 tokenizer leg: 4 large
  allocations, no small churn. No profile shows a
  small-object hot loop.
- **The large (mapping) path carries everything else.**
  KV: 32 slices x 45,056 B requested, 49,152 B granted
  per slice (16 KiB page rounding on this host, page =
  16,384 B), live_mappings 0 -> 32 -> 69 across phases;
  decode churn frees 5,767,168 B of scratch+logits
  publications inside 32 steps; teardown releases
  11,223,040 B total. Slack per KV slice is page
  rounding, not fragmentation evidence.
- **Retained span at teardown is one page.** L-006:
  bytes_mapped=16,384 retained (1 empty span,
  bytes_cached=0), the G-005 posture. L-004: 49,152 B
  (3 spans, different small-object mix). Both reported
  there as posture, not defect.
- **No timing exists in L-004/L-005/L-006.** All three
  slices profile allocation counts/bytes only and say
  so. Any tuning claim needing time evidence had to
  measure it here, on the profile shapes - which is
  exactly what the A/B below does, and no more.

## 2. Candidates, evidence, A/B, decisions

Method (both flavours): probe_l008, one fresh process per
leg; retention set via the internal G-007 setter and read
back (gates); 1 warm-up + 7 timed iterations per workload,
per-iteration median printed; 5 reps (debug) / 3 reps
(production) per bound with the bound order ROTATED per
rep so no leg always runs first or cold. W1 = the exact
L-006 shape (KV 32x45,056 held; prefill ids 128 + embed
131,072 + workspace 4x262,144; decode 32 steps each with
a held KV slice + 8,192 scratch + 128,000 logits churned;
full teardown). W2 = small churn sensitivity leg shaped
on the L-004/L-005 small sizes (200 batches of 64x64 B +
32x128 B alloc/touch/free per iteration): W2 is NOT a
Phase 12 profile shape - it is the retention knob's best
case, included and labelled so the knob is tested where
it could possibly matter, not only where it cannot.
Medians below are medians of the per-rep medians, from
tests/perf/recorded_run_l008.txt.

### C1 - retention_bound (the only runtime knob, G-007)

Profile evidence FOR testing it: the retained-span
figures above (16,384 B / 49,152 B at teardown). Profile
evidence AGAINST expecting a phase-shape effect: the
phase shapes hold 1-23 small objects (Section 1).

A/B (W1, L-006 shape, ms per iteration):
| flavour | bound 2 | bound 0 | bound 16 |
|---|---|---|---|
| debug | 2.112 | 2.060 | 2.249 |
| production | 2.353 | 2.219 | 2.403 |
Per-rep ranges overlap completely in both flavours
(e.g. production bound-0 reps span 2.162-2.949, wider
than any between-bound gap). No winner on the profile
shape, in either flavour.

A/B (W2, churn sensitivity leg, ms per iteration):
| flavour | bound 2 | bound 0 | bound 16 |
|---|---|---|---|
| debug | 7.689 | 7.694 | 7.878 |
| production | 4.024 | 5.923 | 3.968 |
Debug is a dead heat (within 2.5%): the J-005 debug
quarantine pins freed blocks before reuse and holds
their spans, masking the retention path - in the debug
flavour every leg's after-teardown footprint is the
identical 32,768 B / 1 counted empty span regardless of
bound (INC-L008-02). Production separates cleanly:
bound 0 is +47% slower than bound 2 (5.923 vs 4.024 ms)
and +49% vs bound 16, in every rep; bound 2 vs bound 16
differ by 1.4%, inside noise. Production teardown
footprint: bound 0 retains 0 B / 0 spans in all reps;
bounds 2 and 16 retain the identical 32,768 B / 2 spans
(one per class W2 touches) - on this shape 16 retains
no more than 2, it only raises the CEILING (G-007:
20 classes x bound x 16,384 B = 655,360 B at 2 vs
5,242,880 B at 16) with no measured gain behind it.

**DECISION C1: RETAIN the banked default 2. No change.**
0 is measurably worse where retention can matter at all;
16 is measurably no better and costs ceiling. The
default is the A/B winner by elimination, on evidence.

### C2 - small-object cutoff (1,024 B) / class table

G-007 Section 2, banked: NOT tunable - live span headers
record their class index and carve arithmetic derives
from the table; changing either is a design revision,
not a tuning act. Profile evidence gives no reason to
seek one: small objects are 1 (L-006) to 23 (L-004) live
at once against megabytes of large-path traffic, and W1
timing (C1) shows no small-path signature to chase.
**DECISION C2: no change; no A/B - there is no knob to
A/B without redesigning the span geometry.**

### C3 - large-path behaviour (per-slice mappings, slack)

The largest figures in the profiles live here: 69 live
mappings at L-006 S5, 4,096 B page-rounding slack per
45,056 B KV slice (49,152 granted), 5,767,168 B of
decode churn publications freed in-step. G-007 Section 2,
banked: the mapping path retains 0 after free BY
CONSTRUCTION and has no cache - there is no tuning knob;
a large-allocation cache would be a new design element,
not tuning. And the evidence discipline forbids building
one on this record: no banked profile and no leg of this
slice attributes W1's ~2.1-2.4 ms to mapping syscalls as
against touching, carving or locking - W1 is a whole-
shape time only. **DECISION C3: no change. The per-op
breakdown that could ever justify a cache is owed
(INC-L008-01), not assumed.**

### C4 - span size (one system page) / span header (96 B)

Structural, static-asserted layout constants (G-007
Section 2). The retained-span figures that might look
like their cost (16,384 B at L-006 teardown) are one
page by design under the G-005 posture, and C1's A/B
shows the retention POLICY above them already sits at
its measured-best value. **DECISION C4: no change.**

## 3. Responsiveness record (Samtal, :8787)

Samtal answered HTTP 200 before the sweep (1.0 ms) and
after it (3.7 ms). During the sweep itself - the
heaviest leg of this slice, both flavours, all reps - a
sequential curl loop sampled it continuously: **154
samples, 154 x HTTP 200, mean 1.8 ms, max 11.3 ms.**
Sampling-density honesty: the sampler is ONE sequential
loop (one request in flight, next issued after the
previous returns, plus process-start overhead per
sample), so samples are dense but not continuous - gaps
between samples exist and nothing is claimed about them;
no latency bound is asserted beyond the 154 samples
actually taken. Thermal: 57.6 C / throttled=0x0 before,
60.4 C / throttled=0x0 after. Memory: 13 GiB available
throughout; loads kept to the single bounded probe.

## 4. Owed / incomplete (INC-L008)

- **INC-L008-01 (owed):** per-op timing breakdown of the
  large path (map / touch / carve / lock shares of W1).
  Without it the C3 cache question is undecidable; this
  slice measured whole-shape times only and decided
  accordingly (no change).
- **INC-L008-02 (mechanism, recorded):** in the debug
  flavour the J-005 quarantine pins freed small blocks
  (capacity-bounded) and their spans, so the retention
  bound is not observable at teardown and barely in
  time; the production flavour is the retention
  evidence flavour. Any future retention measurement
  must state its flavour or it measures quarantine.
- **INC-L008-03 (carried blockers):** Aesir's real
  prefill/decode path still does not run on this Pi
  (INC-L003-01/02) and no Qwen3 model is present
  (INC-L003-04 / L-007's owed record). All tuning
  evidence here is over the harness's explicit-adoption
  shapes, exactly as L-004/L-005/L-006 were; nothing
  here is an Aesir-engine tuning result.
- **INC-L008-04 (sampling bound):** the responsiveness
  record covers the samples taken (Section 3); it is
  not a continuous monitor and claims no bound between
  samples.

## 5. Files and verification

Deliverables: this document, tests/perf/probe_l008.c,
tests/perf/run_l008.sh (100755),
tests/perf/recorded_run_l008.txt. No src/, include/,
Makefile or README change (git diff vs base: empty).
Recorded run: debug 15/15 legs PASS, production 9/9
legs PASS (all gates ok, teardown live counters 0 in
every leg), Samtal 154/154 x 200 during the sweep and
200 before/after, make test exit 0 (4590 run / 0
failed), nm -D --defined-only on build/libmimiralloc.so
exactly 4 (mimir_calloc, mimir_free, mimir_malloc,
mimir_realloc), README.md + include/mimir_alloc.h
byte-identical to base 03436f3.

*End of L-008. Verdict: no tuning change ships; the
banked retention default 2 is A/B-confirmed, the other
candidates are structural or evidence-free, and the
record above is the measured basis for leaving every
knob exactly where the banked design put it.*
