# Z-092 - Batch API prototype

Roadmap slice Z-092 (Wave 11): bulk same-class alloc/free
API over the Wave-4 cache, shaped by Z-091's dominant
factor; contract tests first. Gate: prototype green on
its contract; not yet a public symbol. MET: the contract
suite is green on the prototype (tests/contract/
recorded_run_z092.txt: CHECKS and RESULT read there),
and the runner's nm proof shows the prototype exports
nothing from the shared library (exactly the 13 public
exports - the Z-078 ten plus Z-083's memalign family -
zero mimir_batch_* names).

Team note: the lead ran the six Mythic Engineering roles
sequentially itself (design, evidence audit, implemen-
tation, test engineering, gate verification, ledger /
risk review); no sub-agents were spawned for this slice
and that is recorded plainly here and in the team
CAMPAIGN_STATE.md.

## S1 What Z-091 actually settled (the shaping verdict)

Z-091 (docs/Z13_Z091_BATCHING_POSTMORTEM.md, commit
105c229 on team-01, read by this lead before finishing
this slice) names the DOMINANT factor in batching's
K-007 loss as F-4: per-op cost is a function of live-set
size, because every alloc and free on the central core
pays linear registry/span walks over the live set
(priced from Z-024). Lock amortisation (F-1) is capped
at ~50 ns/pair and page faults / span release (F-2,
proposal P-1's target) at ~5-11% of the burst penalty
on this machine; neither is dominant. Z-091's brief for
this slice: a batch prototype can only WIN by attacking
the walk - one lock acquisition for the batch, pointers
grouped by owning span, one registry pass per span -
and Z-093's verdict run should grade the prototype
against exactly that expectation.

## S2 What the prototype is - and is not (honest scope)

The prototype (src/core/mimir_core.c, internal surface
declared in src/core/mimir_core.h, contract header
tests/contract/mimir_batch_z092.h) is:

- mimir_batch_alloc(size, out, count): all-or-nothing
  bulk allocation of count same-class blocks. Refusals
  (return 0, out[] byte-identical to entry, nothing
  allocated net): out NULL, count > 4096, size == 0
  (the D-013 refusal, per element), or any element
  failing (this call's blocks are freed in reverse
  order before returning). count == 0 is a defined
  no-op, not a failure.
- mimir_batch_free(ptrs, count): frees the non-NULL
  entries in REVERSE (LIFO) order, returns the number
  actually freed, never writes the array. Refusals
  (return 0, nothing freed): ptrs NULL or count > 4096.
  A duplicate pointer in one batch is a double free
  (UB), exactly as two mimir_core_free calls would be.
- mimir_batch_snapshot(out): process-global monotone
  counters (calls, blocks, failures, the 4096 cap) for
  the contract suite; observability, not a public
  statistics promise. -1 on NULL out; moves nothing.

What it does NOT do, stated plainly so Z-093 grades the
real thing: it does NOT remove the F-4 walk. The entry
points loop the banked per-op core entries; they add
no new allocator state, no span grouping, and no
shared registry pass. For classes the Wave-4 TLS cache
serves, the per-op core entry is the cache path (bin
hit / batched refill-drain of 8), so a burst's cost is
the cache's, not the central walk's; for cache misses
and above-cutoff sizes every element still pays the
full central-core cost Z-091 priced. The one admission
decision the batch does make is contractual, not
mechanical: the whole burst is validated (cap, size,
array) before the first block moves, and it succeeds
whole or not at all. Per Z-091's pricing this prototype
is therefore EXPECTED to lose or tie in Z-093 except
where the TLS cache already absorbs the burst; any win
Z-093 finds must show up as cache absorption, not walk
removal, because no walk was removed. That is the
honest state of the prototype and the question Z-093
exists to answer with numbers.

## S3 Contract-first order of work

The contract header and suite were written against the
roadmap gate and Z-091's brief; the prototype core code
had been landed uncommitted by the previous lead and
was audited line-by-line against the contract before
the suite ran (one defect found and fixed: the core
comment block cited K-007 L-3 locality as the priced
dominant factor and Z-091 as still OWED; Z-091 has
since landed naming F-4, and the comment now cites the
banked verdict and this doc's S2 scope). The suite's
phases: P1 snapshot discipline, P2 alloc refusals with
exact failure accounting, P3 success across classes
(incl. 2048 B, above the cache cutoff) with distinct-
ness, 16-byte alignment, writability and freed-set
reuse, P4 free refusals / NULL skipping / array never
written by the callee's contract, P5 the exact 4096
boundary, P6 four-thread disjoint batches reconciling
exactly, P7 public-surface regression, P8 final
allocated == freed reconciliation. Weak symbols keep
the suite honest on a core without the surface (every
surface phase fails planned, by label).

## S4 Verification

- Contract run: tests/contract/run_z092.sh (also
  `make test-z092`); recorded run filed at
  tests/contract/recorded_run_z092.txt.
- Standing gate: scripts/z13/run_gate.sh GREEN on the
  final tree at the baseline counts (4988 checks /
  0 failed / 77 by-design skips); the slice suite is
  standalone, so the gate total does not move. Logs in
  the team hidden_files/.
- No benchmark numbers are claimed by this slice: the
  gate is contract green, and performance grading is
  Z-093's assignment. Load state at the runs is
  recorded in the recorded run header regardless.

## S5 OWED and next

- Z-093 grades this prototype (prototype vs per-op loop
  vs glibc on batch-shaped workloads) against Z-091's
  expectation named in S2. Nothing in this slice
  pre-judges that verdict.
- If a future slice wants batching to WIN, Z-091's
  brief is the design it must implement: span-grouped
  processing with one registry pass per span. This
  prototype's contract (all-or-nothing, cap, LIFO free,
  internal-only) is written to survive that redesign
  unchanged.
