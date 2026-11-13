# Z-109 — API fuzz at scale (Wave 13 reserve slice 1)

Slice: Z-109, Wave 13 (verdict-driven reserve), pool choice recorded in the
seat `WAVE_PLAN.md` before the slice started.
Team: team-01, branch `yrsa/mimiralloc-team-01`, base e4efc01 (clean merge of
seat main 7e76e47, banked Z-001..Z-108).
Host: purpleraider (ASUS ROG Flow Z13, AMD Strix Halo, x86-64, 4 KiB pages,
128 GB UMA). Recorded run: `tests/fuzz/recorded_run_z13_z109.txt`.

## 1. Why this slice, and its success measure

The Wave 13 pool choice for Z-109 is **API fuzz at scale**, chosen because
the banked Z-108 campaign verdict (S8, from S6's filings) names the D-09
coverage gap: the allocator had never been fuzzed *under injected mmap
refusal*. The pre-existing fuzz evidence (M-004, first campaign) covered
only the original four entry points, at 2,000 ops per seed, with the
fault-injection gap recorded as its own INC-M004-01.

Success measure (stated in `WAVE_PLAN.md` before the slice started): a
libFuzzer-style battery over the public surface at scale; every crash /
hang / misbehaviour triaged to a verdict; any defect found fixed in scope
or filed with a named owner; final gate GREEN.

## 2. Deliverables

- `tests/fuzz/fuzz_z109.c` — the harness (v1, trace version 1): one
  byte-stream interpreter feeding three drivers (seeded PRNG streams,
  corpus files, and a real `LLVMFuzzerTestOneInput` entry under
  `-DZ109_LIBFUZZER`), plus a multithread driver and an injection build
  (`-DZ109_INJECT`) linked with the banked I-004 fault shim.
- `tests/fuzz/run_fuzz_z109.sh` — the seven-leg runner (below).
- `tests/fuzz/recorded_run_z13_z109.txt` — the recorded run (verbatim
  runner output plus triage).
- `Makefile` — additive `test-fuzz-z109` block only (standalone; `make
  test` composition and the standing gate counts are untouched). The
  I-004 shim object rule is reused, not duplicated.
- No `src/`, `include/`, or README change. No new public surface: the
  harness links the library objects directly, exactly like M-004.

## 3. Method

The interpreter consumes a byte stream as an operation program: each op
byte selects one of 20 op classes and the following bytes select its
arguments. An independent ownership model (192 slots: pointer, requested
size, usable extent, pattern byte, alignment) is checked after every
batch of 16 ops and at stream end. Observables are **defined behaviour
only** — the M-004 discipline extended to the whole surface:

- pattern integrity of every live block; pairwise disjointness of live
  requested extents;
- the public-stats live-count equality
  `live_small_objects + live_mappings == model live count`, read through
  the **public** `mimir_stats_snapshot` (M-004 used the internal route;
  the Z-076 public surface now carries the same counters);
- alignment of every aligned-family result; calloc zero-fill; realloc /
  reallocarray preserved prefix;
- failure atomicity: a failed realloc-family call leaves the original
  live, byte-identical, with an unchanged usable extent;
- the D-013 zero-size contract shapes (exact NULL; `posix_memalign`
  ENOMEM with `*memptr` unchanged) and the D-012 usable-size law
  (live: > 0 and ≥ requested; NULL: 0; stable under no mutation);
- `posix_memalign` return-code shapes (EINVAL for non-power-of-two or
  non-multiple-of-`sizeof(void*)` alignments and for a NULL `memptr`,
  `*memptr` unchanged on every failure); `mimir_trim` returns only 0/1;
  `mimir_stats_snapshot(NULL) == -1` and the v1 version/size shape.

Undefined behaviour is never invoked: the model frees and reallocs only
live family pointers it owns, exactly once. Invalid alignments and
overflow sizes are used only where the header defines the refusal.

**Surface coverage.** The runner's first leg audits the built shared
library: exactly 13 defined `mimir_*` T symbols — the whole public
surface — and every one is an interpreter op class:
`mimir_malloc`, `mimir_calloc`, `mimir_realloc`, `mimir_reallocarray`,
`mimir_free` (plus `free(NULL)`), `mimir_aligned_alloc`,
`mimir_posix_memalign`, `mimir_memalign`, `mimir_valloc`,
`mimir_pvalloc`, `mimir_usable_size`, `mimir_trim`,
`mimir_stats_snapshot`. Argument tables span the span classes, page
boundaries (4095/4096/4097), the mapping path (to 1 MiB), valid
alignments 1..1 MiB, 13 invalid alignments, and five failure-shaped
sizes up to `SIZE_MAX`.

**Injection (the D-09 leg).** The injection build wraps the four L6
platform entries with the I-004 shim (`--wrap`, the test-i004 link
pattern) and arms the **acquire** channel: before alloc-family ops, a
stream-chosen single-shot mmap refusal with a countdown of 1..24 calls.
A NULL/ENOMEM for a lawful request is then lawful *only* if the same
request succeeds on an immediate disarmed retry — the strictest honest
reading: a genuine defect cannot hide behind the arm, because the retry
runs disarmed. The shim's own counters (attempts / injected /
real-failed, per channel) are printed per seed, and the runner **fails**
any inject seed whose shim counter shows zero injections, so the
coverage cannot silently not happen.

**Multithread leg.** Four threads run independent streams against their
own models (per-thread pattern + overlap checks every batch), exchange
live blocks through a mutex inbox (post every 401 ops, pop every 601),
and spawn per-op cross-thread verify-and-free helpers (op class 18).
After the join, main adopts and frees the inbox leftovers, trims, and
requires the public-stats live count to be exactly 0.

## 4. Scale and results (run of record)

| Leg | Scale | Result |
|---|---|---|
| Export audit | 13 expected symbols | PASS (13) |
| Plain seeded streams | 10 seeds × 25,000 ops | PASS, 0 fails |
| Determinism | seed 10901 twice at 25,000 ops | PASS, byte-identical |
| Inject seeded streams | 6 seeds × 25,000 ops | PASS; shim injected 734 refusals (112–134/seed) |
| Multithread | 4 threads × 20,000 ops + inbox | PASS, final live = 0 |
| Corpus replay | 16 files × 4,096 B | PASS, 3,770,519 checks |
| libFuzzer (clang, ASan+UBSan) | 300 runs, ≤512 B inputs, 500-op cap | PASS, no crash artifact |

Runner totals: **530,000 stream operations, 72,860,018 checks, 0
failed** (including the determinism pair; 480,000 ops in the primary
legs). Every one of the 20 op classes was exercised 22,101–24,698 times
across the stream legs. The inject legs additionally drove ~6,000
genuine OS refusals (`real_failed` shim counters) from the harness's
representable-unsatisfiable shapes — real mmap failure paths, not only
injected ones. Wall time ~6 minutes for the runner in the window below;
the standing gate ran separately.

## 5. Triage — every crash, hang and misbehaviour to a verdict

- **Crashes: none.** No leg crashed or aborted; the sanitizer leg
  finished its bounded runs with no libFuzzer crash artifact.
- **Hangs: none.** Every invocation ran under `timeout(1)`; no leg
  returned 124 or needed its bound.
- **Misbehaviour: none.** Zero FINDING lines across all legs. There is
  therefore no failing prefix to minimise and no reproducer to record;
  the minimisation road is the harness's own (`seed <seed> <ops>` with a
  reduced op count bisects any future failing stream, and corpus mode
  replays any single stream file).
- **Defects in the allocator: none found.** Nothing to fix in scope.
- **Harness-side defects found and fixed during the build** (all in the
  new test file only): a plain-build unused-function error (`rd64` is
  used only by the injection arming path), an inject-build
  misleading-indentation error introduced by a first patch, and unused
  driver functions in the libFuzzer build (drivers are now guarded by
  `#ifndef Z109_LIBFUZZER`). Each was caught by the tree's own
  `-Werror` gates before any run depended on the file.

## 6. The D-09 disposition

D-09's mmap-refusal fuzz-injection coverage is **discharged for the
acquire channel** by this slice: fuzzing at scale now runs *through*
injected mapping refusals, with the retry discipline above, and the
evidence is the shim's own counters in the recorded run — not the
harness's word. The remaining shim channels are filed, not hidden
(INC-Z109-01 below).

## 7. Filed items (never leave a discovered bug unfixed — or unfiled)

- **INC-Z109-01** (coverage limit, honest): the release, lock and
  page-size shim channels are compiled into the injection build (all
  four `--wrap` flags, counters printed) but never *armed* by this
  slice — only acquire (the D-09 channel) is. Fuzzing under injected
  release/lock/page-size failure remains owed. **Owner: the next fuzz
  slice on this line, or the Z-117 campaign-closure hand-off if none
  runs.**
- **INC-Z109-02** (coverage limit, honest): the MT leg fuzzes concurrent
  streams, cross-thread frees and inbox transfers; it does not fuzz
  arbitrary unsynchronised interleavings over shared blocks beyond
  those shapes. **Owner: a soak-class follow-on (the M-005 territory),
  if the line commissions one; otherwise the Z-117 hand-off.**
- **INC-Z109-03** (scope, honest): the undefined-behaviour surface
  (foreign, interior, double-freed pointers; use after free) is never
  invoked, by design — no verdict is claimed there. Corruption
  *detection* rates are Z-103's measured territory, unchanged.
- The libFuzzer leg is **bounded by design** (300 runs at a 500-op cap
  under ASan+UBSan; ~7 runs/s in this window because every input pays
  full model verification). It is a sanitizer spot-weld on the same
  interpreter, not an open-ended fuzzing campaign; the seeded legs
  carry the scale claim.

## 8. Load honesty

Shared host, contended window: load average 4.34 / 2.97 / 2.03 at
runner start and 2.86 / 2.87 / 2.25 at end, with the Aesir forge's mojo
`run_all` process (~276% CPU) and team-03's Z-111 trace probe (~100%
CPU) live as co-tenants. No verdict in this slice is a timing or a
performance number — every grade rests on exact counters (operations,
checks, shim counters), which contention cannot move. The final gate
ran at loadavg 1.39 at start; gate counts are exact counters too.

## 9. Gates

- Baseline gate on the merged base tree: GREEN 4988 / 0 / 77
  (`hidden_files/z109_baseline_gate.log`).
- Final gate on the slice tree (`scripts/z13/run_gate.sh`): **GREEN
  4988 checks / 0 failed / 77 by-design skips**, make exit 0
  (`hidden_files/z109_gate.log`) — identical to baseline, as
  constructed (the slice adds only standalone test files, a doc, and
  an additive Makefile block outside `make test`).
- Slice gate (`make test-fuzz-z109` runner): PASS, exit 0, totals in
  S4.

**Verdict: PASS — the pool item's success measure is met.** The public
surface survives fuzzing at scale, including under injected mmap
refusal, with no crash, no hang and no misbehaviour to file against the
allocator.
