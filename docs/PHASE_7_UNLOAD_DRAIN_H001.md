# Phase 7 — Unload Drain for Retained Spans (H-001)

Slice H-001, Wave 8 (head of the wave, commissioned fix).
Base: `3a93c11` (G-009 banked; Wave 7 complete, Phase 7
exit PARTIAL on INC-G008-02). Host: Raspberry Pi
purpleviking1 (aarch64, page size 16384), Samtal live on
port 8787 throughout (HTTP 200 before, during, and after
every run below); nothing was stopped, configured, or
installed. All workloads in this slice are **synthetic**.
Full evidence: `tests/core/recorded_run_h001.txt`
(1,555 lines; every exit code recorded). Harness:
`tests/core/test_h001.c`; test-only declaration:
`tests/core/mimir_drain_h001.h` (landed with the slice
assignment, INC-E001-07 shape (b)); standalone via
`make test-h001` (not part of `make test`).

**The defect, restated.** G-008 measured that a
`dlclose` orphaned every retained EMPTY span: anonymous
rw bytes grew by exactly 49,152 B = 3 pages per cycle
for the G-008 workload's 3 warmed classes (256, 512 via
the realloc grow, 1024), 98,304 B after cycle 0 →
2,506,752 B after cycle 49, because `src/` registered
no destructor (INC-G008-02). The G-009 exit packet
carried Phase 7 X4/X6 PARTIAL on this finding alone and
commissioned the fix as H-001 (packet Section 9, item
H-001). This slice is that fix, with its required
evidence.

## 1. Design decision

**Chosen: an image destructor calling one internal
drain entry point.** `src/core/mimir_core.c` now
registers exactly one `__attribute__((destructor))`
function, `mimir_core_unload_drain`, whose whole body
is a call to the new internal entry point
`mimir_core_drain_retained()`. The destructor runs when
the image is finalised — at `dlclose`, and at process
exit for a statically linked or never-unloaded image —
in each case before the image unmaps, while the code
and the lock that own the spans are still mapped.

The drain itself, under the existing one global lock
(B-006 S9 discipline, held across the release calls per
S9.4 as everywhere in this file): walk the span
registry once, capture each span's `list_next` before
any release, and for every span whose `live_count == 0`
release it whole through the banked `span_release_locked`
(the L5 path over L6, with its F-06 rollback), counting
the spans actually released. Nothing else is touched:
no descriptor-registry walk (the mapping path retains
nothing after free — the Phase 3 baseline — so there is
nothing retained for it), no tuning state, no counters
beyond the span counters `span_release_locked` itself
keeps exact.

**Why not an explicit public shutdown entry point.**
The H-001 register entry offered the alternative "if a
contract decision prefers it". No contract decision
exists that could prefer it: the namespaced surface is
[UNRESOLVED: U-03], a public symbol is an ABI/contract
act, and the established pattern for exactly this
situation is INC-G007-01 (the proven G-007 tuning pair
stays internal; promotion is a future contract slice's
business). A public shutdown call would also fail the
actual defect: the orphan happens at `dlclose` whether
or not the consumer remembers to call anything, so the
fix has to run at unload, not on request. Accordingly:

- `mimir_core_drain_retained` is INC-E001-07 shape (b)
  internal state, like the G-007 tuning pair and the
  E-005/G-003 counters: declared in the internal
  `src/core/mimir_core.h`, re-declared for tests only
  in `tests/core/mimir_drain_h001.h`, bound local in
  the shared library (hidden visibility + version
  script). `nm -D` stays at exactly 4 and contains no
  drain symbol (recorded run, Section 5).
- The public header `include/mimir_alloc.h` and
  `README.md` are byte-identical to the base (sha256
  verified in the recorded run, before and after).
- No constructor is registered: initialisation stays
  clean by construction (B-007 C2), exactly as before.
  This slice adds the project's first destructor and
  nothing else of the kind — no atexit, atfork, TLS, or
  pthread-key registration exists in `src/` after this
  slice either (the recorded grep shows the only
  destructor hits are this slice's own comment and
  registration).

**Derived worst case, now on the release side.** The
arithmetic G-008 derived for the leak applies unchanged
to the drain: a consumer that warms all 20 classes to
the RP-2 bound in one load now has the full RP-5 worst
case — 40 spans = 640 KiB — *released* at its `dlclose`
instead of orphaned. Derived from the bound, as G-008's
figure was; the measured per-cycle figure for the
harness workload is in Section 5.

## 2. What the drain guarantees

- **EMPTY spans only.** A span is released if and only
  if its `live_count` is 0 at the walk. Live spans,
  live small objects, and live large mappings are never
  touched (Section 3).
- **Non-terminal.** The drain changes no registry,
  counter, or tuning state besides releasing the empty
  spans themselves; the lock is released before return
  and the allocator is fully usable afterwards — a
  later allocation simply acquires a fresh span. This
  is the property that makes the destructor safe to run
  at process exit, where other finalisation code may
  still legitimately call the allocator before the
  image unmaps.
- **Idempotent.** A second call with nothing retained
  releases nothing and returns 0; an unload that
  follows an explicit drain (or a process exit after a
  `dlclose`-time drain of the same state) is a no-op.
- **Failure posture.** On lock failure nothing is
  released and 0 is returned — no unlocked progress
  (B-006 S9.5 / F-07), the same posture as every other
  entry point. A span whose L6 release fails is rolled
  back LIVE and retained by `span_release_locked`
  itself (F-06) and is simply not counted; the walk
  continues with the next span.

## 3. INC-G008-03 posture — blocks still live at unload

**Decision: consumer duty; the drain deliberately does
not touch live blocks, and no shutdown semantics for
live blocks are defined.** A block that is still live
at `dlclose` keeps its span mapped and its contents
intact after the close (proved by execution, Section
5: the live-at-unload probe's page stays mapped, its
0xA5 pattern reads back byte-intact, and the anonymous
total grows by exactly that one page — the precise
residue the posture intends). But the registry that
owned the block is destroyed with the unmap, so:

- freeing or otherwise using the block through a later
  load remains undefined behaviour, exactly as the
  public header already states for invalid pointers;
- the consumer duty is unchanged and now fully
  sufficient: **free every block before `dlclose`** —
  after H-001, a consumer that does so leaves nothing
  behind at all (0 B per cycle, Section 5), where
  before it unavoidably leaked its retained spans.

Defining forced-release semantics for live blocks
(unmapping memory a consumer may still hold pointers
into, from inside a destructor, with no error channel)
was considered and rejected: it would convert a
documented, bounded leak-until-exit of exactly the
live spans into a use-after-unmap hazard, for a case
the contract already places on the consumer. Carried
owed item O-01 of the G-009 register is therefore
discharged as DECIDED here: live allocations at
`dlclose` remain the consumer's responsibility, by the
same reasoning the packet recorded, now with the
empty-span half of the problem removed.

## 4. INC-G006-03 re-examination — the destructor-order surface

G-006 proved by grep that the allocator registered no
destructors, atexit handlers, or TLS destructors, so no
allocator-side destructor-order interaction existed.
H-001 creates exactly one destructor; the surface it
opens is re-examined here, by case:

1. **This image's own finalisation.** The image has
   exactly one destructor, so there is no intra-image
   ordering question. It allocates nothing, takes only
   the allocator's own global lock, and reports nothing
   (no reporting channel exists at unload; the released
   count is deliberately discarded).
2. **A consumer destructor that runs before ours**
   (e.g. one in the same image finalised earlier, or a
   consumer that frees its blocks from its own
   destructor and then calls `dlclose`): those frees are
   ordinary frees under the lock (G-006 T1/T2 substance);
   the spans they empty are then drained by our
   destructor with the rest. No new behaviour.
3. **Code that runs after our destructor but before
   unmap** (another object's finalisation at process
   exit): because the drain is non-terminal (Section
   2), the allocator remains fully valid and usable in
   that window; a late `mimir_free` or `mimir_malloc`
   behaves exactly as it would have before, and any
   spans it empties simply stay mapped until the unmap
   — the pre-H-001 behaviour for that span, no worse.
4. **Code that calls the allocator after the image has
   unmapped** (cached function pointers across a
   `dlclose`): already undefined behaviour and already
   forbidden consumer discipline (INC-G008-04); H-001
   neither creates nor widens it.
5. **Lock state at finalisation.** The destructor
   takes the one global lock. If finalisation were
   somehow entered with the lock already held by the
   finalising thread, the drain would block like any
   allocator call would; no supported path holds the
   lock across a return to the caller (S9.4), and
   `dlclose` from inside an allocator call is not a
   supported case anywhere in the banked posture. A
   lock *failure* (as opposed to contention) is the
   F-07 posture of Section 2: nothing released, return
   0, spans stay mapped until exit — degraded to the
   pre-H-001 behaviour, never to corruption.

Executed vs inspection, stated plainly: the destructor
itself is executed 52 times in the recorded dlopen leg
(50 cycles + live probe + reload) and at every test
binary's exit in `make test` — its effect is the
measured evidence of Section 5. A deliberately
interleaved consumer-destructor ordering test was NOT
executed; cases 2–4 above rest on the G-006 executed
concurrent/cross-thread free evidence plus inspection
of the drain's non-terminal construction, and are
labelled accordingly (INC-H001-02).

## 5. Evidence (recorded run, fresh on the Pi)

- **`make test` exit 0 — 4,590 checks, 0 failures,**
  identical totals to the banked base: smoke 18/18,
  core 29, b007 69, c005 95, c006 23, c007 122, c008 42
  (7 skipped-not-yet, as banked), d004 final accounting
  "CHECKS: 4590 run, 0 failed". `make test` composition
  unchanged.
- **`make test-h001` exit 0 — 299 checks, 0 failures**
  (dlopen leg 267, direct leg 32):
  - *50-cycle G-008 shape:* every cycle dlopen + dlsym
    ×4 succeed, workload verify clean, `dlclose` 0,
    `img_after_close=0`. **Anonymous rw bytes after
    close: 49,152 B at cycle 0, at cycle 49, and at
    every cycle between — growth exactly 0 B per cycle,
    0 B start-to-cycle-0 and 0 B cycle-0-to-cycle-49**
    (the G-008 series for the same harness rose 49,152
    B per cycle to 2,506,752 B). VmRSS flat at 1,392 KiB
    across all 50 cycles (G-008: +48 KiB per cycle);
    end-of-leg RSS 1,408 KiB is after the live probe
    below (+16 KiB = its one resident page).
  - *Live-at-unload probe (INC-G008-03):* one 256 B
    block leaked across a close stays mapped, pattern
    byte-intact, anonymous total exactly +16,384 B =
    1 page over the cycle level — the one live span,
    deliberately not drained. A reload then runs the
    workload clean and its close returns the anonymous
    total to exactly the probe level (65,536 B): the
    reload's own retained spans drained.
  - *Direct leg:* drain on an empty instance returns 0
    twice; 3 warmed classes retain exactly 3 EMPTY
    spans; with a 256 B block and an 8,192 B mapping
    live, the drain releases exactly 2 (the 512 and
    1,024 EMPTY spans), the live patterns verify
    byte-for-byte after it, and the live mapping count
    is unchanged; a further drain returns 0; a
    post-drain allocation works on a fresh span; the
    final drain releases exactly 2 and every counter
    pair reconciles (spans published == retired == 4,
    small alloc == free == 14, mappings published ==
    retired == 1).
  - *Function-pointer observation:* `mimir_malloc`
    landed at the same address in 49 of 49 later cycles
    in this run (G-008 recorded 4 of 49). Recorded as an
    observation only — no stability is promised and
    INC-G008-04's discipline (never cache across a
    close) stands unchanged (INC-H001-05).
- **`make test-g005` exit 0 — 97 checks, 0 failures:**
  the RP-1..RP-5 reclaim policy behaves exactly as
  banked against the drain-bearing core (in-process
  retention is untouched by H-001; the drain runs at
  unload or on explicit test call only).
- **`nm -D` exactly 4** defined dynamic symbols
  (`mimir_malloc`, `mimir_free`, `mimir_calloc`,
  `mimir_realloc`); drain symbols in `nm -D` output: 0.
- **Banked `make test-g008` — recorded honestly, exit
  2 on exactly one inverted check.** The G-008 harness
  asserts the defect itself as a PASS check ("anonymous
  rw bytes grow across cycles: retained spans orphaned
  by dlclose (no destructor runs)"). With the drain its
  own series is flat — start 49,152 B, after cycle 0
  49,152 B, after cycle 49 49,152 B, growth 0 B — so
  that single check now FAILs: **214 checks, 1 failure**,
  every other check passing, including
  `img_after_close=0` ×50 and its orphan probe (whose
  expectations are the INC-G008-03 posture and still
  hold). Because the binary exits 1, `make` stops
  before the target's probe leg; the probe script was
  therefore run separately in the same recorded run
  (below). `test_g008.c` is banked evidence and is NOT
  edited by this slice; `test_h001` is the superseding
  harness for this behaviour (INC-H001-01).
- **Responsiveness probe (run separately, exit 0):**
  Samtal 30/30 samples HTTP 200 (15 baseline + 15 under
  churn) plus the post-churn check HTTP 200; Samtal p50
  0.881 ms baseline vs 0.934 ms under churn (means 0.926
  vs 1.398 ms — single-run observations, no bound
  claimed, INC-G008-05 posture); churn 15,679,744 ops,
  0 fails (2 threads × 12 s, synthetic). Samtal also
  returned HTTP 200 in the header and closing checks of
  the recorded run.
- **Integrity:** `README.md` and
  `include/mimir_alloc.h` sha256-identical to HEAD
  before and after (recorded run, header and closing
  sections). Slice diff: `src/core/mimir_core.c`
  (+112 lines: file-header addendum, drain, destructor),
  `src/core/mimir_core.h` (+18: internal declaration),
  `Makefile` (+34: appended `test-h001` target only),
  plus the new files `tests/core/mimir_drain_h001.h`
  (assignment-landed), `tests/core/test_h001.c`,
  `tests/core/recorded_run_h001.txt`, and this doc.

**What this discharges.** The G-009 packet's own words
(Section 10): "When H-001 banks with its zero-growth
harness record, X4 and X6 can be re-graded on that
evidence and the exit declared; until then it is not."
The record the packet named — the G-008 harness shape
re-run to exactly 0 B per cycle with
`img_after_close=0` throughout, plus `make test` and
test-g005 green — is the recorded run above. The
re-grade itself belongs to that future packet, not to
this slice (INC-H001-06).

## 6. Gaps, surprises, and owed work (INC-H001-NN)

- **INC-H001-01 (banked G-008 harness assertion
  inverted — expected obsolescence, recorded not
  hidden).** `make test-g008` now exits 2 on the single
  growth-assertion check (Section 5). The harness was
  written to prove a defect's presence byte-exactly;
  the fix falsifies its premise. It is left byte-
  identical as banked evidence; `make test-h001`
  supersedes it for the unload behaviour. Any future
  re-pointing of the G-008 target at the new
  expectations is an evidence-policy decision for the
  keeper or a future slice, not this one.
- **INC-H001-02 (destructor-order interleave —
  inspection-covered).** Section 4, cases 2–4: no
  executed test interleaves a consumer destructor,
  TLS destructor, or atexit handler with the drain.
  The safety argument rests on the drain's executed
  non-terminal/idempotent semantics (direct leg) plus
  G-006's executed concurrent-free evidence. An
  ordering harness belongs to a future teardown slice
  if one is ever commissioned (roadmap Phase 8 names
  cancellation and teardown; O-04 of the G-009
  register).
- **INC-H001-03 (drain lock-failure path
  unexercised).** No lock-failure injection hook exists
  (same posture as INC-G007-05 / register O-08); the
  return-0-release-nothing branch rests on inspection.
  A fault-injection slice discharges it together with
  the other queued branches.
- **INC-H001-04 (fork/exec posture unchanged, stated
  for completeness).** `exec` replaces the image
  without running destructors — retained spans die with
  the address space, so no orphan crosses an `exec`.
  A `fork` child inherits copies of the same registries
  and mappings it inherited before H-001; the
  single-global-lock fork hazard and the no-fork-claim
  posture (INC-G006-02, U-17..U-21) are untouched by
  this slice.
- **INC-H001-05 (address-stability observation, no
  promise).** Section 5: 49/49 same-address loads in
  this run vs 4/49 in G-008's. Nothing in the contract
  or this slice promises load-address stability;
  INC-G008-04's consumer discipline stands.
- **INC-H001-06 (exit re-grading and a public
  shutdown surface both remain future business).**
  This slice supplies the evidence the G-009 packet
  named for X4/X6; the re-grade itself is the next
  exit packet's. Promotion of the drain to a public
  shutdown entry point remains available only via a
  future contract slice settling U-03 (INC-G007-01
  pattern); H-001 neither needs nor makes that change.
