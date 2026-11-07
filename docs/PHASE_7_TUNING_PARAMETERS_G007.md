# Phase 7 — Tuning Parameters: Configurable, Validated, Visible (Slice G-007)

Status: IMPLEMENTED and MEASURED on this host (Raspberry Pi,
16 KiB pages) in `tests/core/recorded_run_g007.txt` (real run,
base `5e3987b`, branch `yrsa/mimiralloc-07`). Plain verdict:
**the G-005 RP-2 per-class empty-span retention bound — the
one retention knob the Phase 7 design has — is now a runtime
tuning parameter on the internal/test surface: it can be set,
invalid values are refused with the previous value kept, and
the effective value reads back exactly. With nothing
configured, behaviour is bit-for-bit the banked G-003/G-005
behaviour (the G-005 evidence test still passes unchanged,
97/0, against the tuned code). The public ABI did not move:
`nm -D` on the shared library shows exactly the four public
symbols, and the public header and README are byte-identical
to base.** Phase 7 is NOT exited by this slice; the exit
assembly remains G-009's.

Companions: `docs/PHASE_7_FOUNDATION_PLAN.md` (G-001, whose
INC-G001-05 named the configuration channel an open design
question and whose exit matrix row X7 this slice discharges),
`docs/PHASE_7_SIZE_CLASSES_G002.md` (G-002),
`docs/PHASE_7_SMALL_OBJECT_PATH_G003.md` (G-003, the
implementation), `docs/PHASE_7_RECLAIM_POLICY_G005.md`
(G-005, policy RP-1..RP-5; RP-2/RP-5 are what this slice
makes tunable).

## 1. The mechanism decision

**Chosen: ONE explicit setter/getter pair on the
internal/test-only surface** (INC-E001-07 shape (b), the same
pattern as the E-005 audit counters and the G-003 span
counters):

```c
/* tests/core/mimir_tune_g007.h -- test-only, never installed */
int    mimir_tune_set_retention_bound(size_t value);
size_t mimir_tune_get_retention_bound(void);
```

defined in `src/core/mimir_core.c` beside the state it
governs. The pair is reachable only by linking the library
OBJECTS directly (as `make test-g007` does). In the shared
library the core object is compiled `-fvisibility=hidden`
and the version script binds every non-public symbol local,
so neither function appears in `libmimiralloc.so`'s dynamic
symbol table and no `.so` consumer can link against them —
proven in the recorded run (`nm -D` exactly 4;
`NO_TUNE_SYMBOLS_IN_SO`). The public header
`include/mimir_alloc.h` is **byte-identical** to base
(sha256 verified in the recorded run): no public surface was
added, silently or otherwise.

Why this mechanism, in one paragraph: the parameter is
allocator-internal policy state guarded by the allocator's
one global lock; an explicit function pair stores and reads
that one scalar under that same lock, so validation,
application and visibility are three checkable code paths in
the layer that owns the state (L4/L5), with no parsing, no
stringly-typed input, no once-only read timing, and no new
failure channel leaking into the four public entry points.
It is callable at any time (not only before first use),
which is what makes the changed-value evidence below
executable in a single test process at all.

**Alternatives rejected, with reasons:**

- **Environment variable read once at first use**
  (e.g. `MIMIR_RETENTION_BOUND`). Rejected: (1) the value is
  invisible — there is no exact read-back road short of
  inferring it from behaviour, failing this slice's
  "visible" requirement in kind, not degree; (2) string
  parsing adds a malformed-input surface (empty, trailing
  junk, overflow) whose refusal semantics would be posture,
  not proof; (3) "once at first use" makes the effective
  value depend on which call happens first and makes the
  default-equivalence and changed-value legs of one test
  process mutually exclusive; (4) environment state leaks
  across `fork`/`exec` boundaries the design has made no
  claims about (U-17..U-21 open). Environment tuning may
  still be a legitimate *future* front-end over this same
  scalar; it cannot be the mechanism of record.
- **Configuration file.** Rejected: adds file I/O, path
  policy, and partial-parse states to a layer whose banked
  discipline is "no allocation, no constructor, no init
  step" (B-007 C2); every failure mode it introduces is
  outside the allocator's one defined failure channel.
- **Compile-time constant (the status quo ante).** That is
  what G-003 shipped (`MIMIR_MAX_EMPTY_SPANS_PER_CLASS 2`);
  it is configuration by rebuild, which the Roadmap Phase 7
  element "tuning parameters" — read with G-001's X7 —
  asks to supersede. The constant survives as the *default*
  (Section 2), so nothing banked is lost.
- **New public entry points in `include/mimir_alloc.h`.**
  Rejected for this slice by the hard constraint: the
  namespaced surface is [UNRESOLVED: U-03] with no contract
  rows; expanding the public ABI to carry a tuning setter
  is a contract decision, not an implementation detail.
  Recorded as the proposal INC-G007-01 for a future
  contract slice.

## 2. Parameter catalogue

Exactly one parameter is implemented. Candidates that were
evaluated and deliberately **not** made tunable are listed
after it, with reasons — a catalogue that names what is not
tunable is part of the design record, so no later reader
mistakes absence for oversight.

| # | Parameter | Default | Valid range | Effect | Visibility road |
|---|-----------|---------|-------------|--------|-----------------|
| TP-1 | `retention_bound` — empty spans retained per size class (G-005 RP-2) | **2** (the banked G-003/G-005 value) | **0..16** inclusive; anything else refused (-1), previous value kept | Read at free time in `small_free_locked`: when a span empties and its class already holds more empty spans than the bound, the span that emptied is released whole to L6 in the same locked free (RP-3, unchanged). 0 = retain nothing (every empty span released at free time). The retained-after-free ceiling becomes 20 classes × bound × one system page (RP-5 generalised): default 655,360 B on this host; maximum 5,242,880 B at bound 16 | `mimir_tune_get_retention_bound()` reads the exact scalar under the global lock (test-only shape (b)); the G-003 span counters make every effect countable exactly |

Application semantics (normative for this slice): the
bound applies **at free time**, exactly where RP-3 already
places reclaim. Lowering the bound does **not** eagerly
unmap spans already retained, and raising it does not
re-warm anything: each class converges to the new bound at
its next emptying frees. This is deliberate — an eager
sweep would unmap an unbounded number of spans inside one
setter call, under the lock, which is precisely the
deferred/storm behaviour RP-3 forbids at free time — and
it is the semantics the evidence test pins (P3/P4/P5).

Candidates evaluated and NOT made tunable:

- **Small-object cutoff (1024) and the class table
  (20 classes, G-002).** Not tunable: live span headers
  record their class index and carve arithmetic derives
  from the table; changing either under a live registry
  would invalidate every published span. These are
  geometry, not policy; a different table is a design
  revision (G-002's road), not a tuning act.
- **Span size (one system page) and span header (96 B).**
  Not tunable: both are layout constants static-asserted
  against the B-006 S7.3 discipline.
- **Large-path behaviour.** Nothing to tune: the mapping
  path retains 0 after free by construction (G-001
  baseline) and has no cache.

## 3. Measured evidence (recorded run, exact counts)

Method: span/object counts are exact audit-counter deltas
(G-003 counters via `tests/core/mimir_small_g003.h`);
retained bytes are live spans × the runtime page size
(16,384 here). The evidence test is
`tests/core/test_g007.c`, standalone `make test-g007`
(additive Makefile wiring only; `make test` composition
unchanged). **132 checks, 0 failures, RESULT: PASS, exit
0.** Phases, exactly as run:

- **A (visibility + default).** Before any setter call the
  getter reads exactly **2**. No span state after the
  large-path warm-up.
- **P1 (default equivalence).** Nothing configured; the
  G-005 P1 shape for all 20 classes (hold 2·cap+1, free
  all): every class published exactly 3 spans, retired
  exactly 1, retained delta exactly **+2**; cumulative
  **40 spans** — the G-005 end state and the RP-5 worst
  case, reproduced exactly.
- **P2 (invalid refusal).** `set(17)`, `set(100)` and
  `set(SIZE_MAX)` each return **-1**; after each refusal
  the getter still reads **2**; a 3·cap churn cycle of
  class 128 afterwards leaves live spans unchanged —
  no partial application, no state damage.
- **P3 (bound 1).** `set(1)` returns 0, getter reads 1.
  Classes 512/640/768 (each holding 2 retained spans from
  P1): a 2·cap+1 cycle publishes exactly 1 span, retires
  exactly 2, converges each class to exactly 1 retained
  span. Total 40 → **37**.
- **P4 (bound 0).** `set(0)` returns 0, getter reads 0.
  Classes 1024/320: a 2·cap+1 cycle publishes exactly 1,
  retires exactly 3, converges each class to exactly 0
  retained spans. Total 37 → **33**.
- **P5 (restore).** `set(2)` returns 0, getter reads 2.
  Re-running the P1 shape on classes 1024/320 restores
  exactly +2 retained spans each. Total 33 → **37** (the
  three bound-1 classes keep their converged 1 each —
  free-time application, Section 2, observed not assumed).
- **P6 (final accounting).** Live small objects **0**;
  live spans **37**; retained mapped bytes **606,208**
  (= 37 × 16,384).

Regression evidence in the same recorded run:
`make clean && make test` exit **0** (final accounting
4590 checks, 0 failed — identical totals to the banked
base); `make test-g005` exit **0** (**97 checks, 0
failures**) — the banked G-005 evidence test passing
unchanged against the tunable code is the strongest form
of the default-equivalence claim: unconfigured behaviour
is not merely re-measured here, it is re-proven by the
previous slice's own instrument. `make lib` exit 0;
`nm -D` exactly **4** (`mimir_malloc`, `mimir_free`,
`mimir_calloc`, `mimir_realloc`); no tune symbol in the
`.so`. README sha256 `bd7f6614…` and public-header sha256
`55107f05…` both identical to base `5e3987b`. Samtal
light check during the run: HTTP 200; Pi memory 13 GiB
available before and after — loads stayed light.

Files of this slice (one commit on `yrsa/mimiralloc-07`):
`src/core/mimir_core.c` (+60/−4: the constant becomes the
locked scalar + setter/getter; no other logic touched),
`tests/core/mimir_tune_g007.h` (new, test-only header),
`tests/core/test_g007.c` (new), `tests/core/recorded_run_g007.txt`
(new), `docs/PHASE_7_TUNING_PARAMETERS_G007.md` (this file),
`Makefile` (+25, additive `test-g007` target only).

## 4. Gaps, surprises, and owed work (INC-G007-NN)

- **INC-G007-01 (public tuning ABI — proposal, owed to a
  contract slice).** The tuning surface is test-only
  shape (b) by design (Section 1). If a production caller
  ever needs to set the bound, the road is a contract
  slice that first settles the namespaced surface's
  status ([UNRESOLVED: U-03]) and then promotes this
  exact setter/getter pair — semantics already proven
  here — to shape (c). This slice proposes; it does not
  promote.
- **INC-G007-02 (the range cap 16 is this slice's
  decision).** Nothing banked mandates a maximum. 16 was
  chosen so the worst-case retained-after-free ceiling
  stays a small, statable number (20 × 16 × page =
  5,242,880 B on this host; 1,310,720 B at 4 KiB pages)
  while leaving 8× headroom over the default for
  latency-seeking tuning. G-009 should carry the cap and
  its arithmetic into the exit packet as a decided
  parameter, or re-decide it there with reasons.
- **INC-G007-03 (no eager convergence — by design).**
  After lowering the bound, a class that never allocates
  again keeps its previously retained spans for the life
  of the process (bounded by the *old* bound, at most
  the RP-5 ceiling at default). Recorded as a stated
  property for G-008/G-009, in the same spirit as
  INC-G005-03: it is the price of storm-free, free-time
  application, and it is bounded — but a reader must not
  claim the new bound is the instantaneous retained
  total; it is the bound the next frees enforce.
- **INC-G007-04 (one parameter only — deliberate).**
  Section 2 lists the evaluated non-parameters. If a
  future workload needs a second knob, the honest
  candidates are policy-shaped (e.g. a per-class bound
  vector) and each needs its own validation/visibility
  proof in this test's shape; none was invented here to
  pad the catalogue.
- **INC-G007-05 (lock-failure paths unexercised).** The
  setter's -2 return and the getter's (size_t)-1
  sentinel on global-lock failure were not exercised by
  execution: no lock-failure injection hook exists at L6
  in this base (same posture as INC-G005-06). Both paths
  are covered by code inspection only; a fault-injection
  slice could prove them by execution.
- **INC-G007-06 (multithreaded tuning unmeasured).**
  The setter/getter take the same global lock as every
  allocator operation, so concurrent tuning is race-free
  by construction and the stored value is the value
  free-time decisions read — but no phase of this test
  sets the bound *while* other threads allocate (P3 of
  G-005 covers idle threads for retention, not live
  tuning races). The claim made here is exactness under
  the lock discipline, not a measured concurrent-tuning
  result; G-008's responsiveness record is the natural
  home if that measurement is ever wanted.
- **INC-G007-07 (keeper reconciliation note).** G-006
  (team-06, thread exit handling) builds in parallel on
  its own branch from the same base and is not in this
  slice's base. This slice touches `src/core/mimir_core.c`
  in two regions (the retention-bound definition block
  and the setter/getter insertion immediately before the
  construction dispatcher) and appends to the Makefile;
  if G-006 also edits the core or the Makefile, the
  keeper's banking merge must union both, as it did for
  the G-004/G-005 Makefile. This slice's test also
  depends on no G-006 behaviour.
- **INC-G007-08 (stale macro name in banked comments).**
  `tests/core/test_g005.c` (line 18) and the G-005 slice
  doc still name the old compile-time constant
  `MIMIR_MAX_EMPTY_SPANS_PER_CLASS`, which this slice
  replaces with the tuned scalar whose default is the
  same 2. Cosmetic only — no behaviour, count or verdict
  in G-005 changes — and deliberately not edited here to
  keep this slice's files disjoint from banked evidence;
  owed as a one-line tidy to the keeper or G-009.
