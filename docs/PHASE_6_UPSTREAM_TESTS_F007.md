# Phase 6 - Relevant Upstream Test Execution Record

Slice: **F-007 (team-07)**. Wave 6, Phase 6 task **T5**
(execute relevant upstream tests), per Roadmap Section 8
Phase 6 and the F-001 exit-evidence matrix (Section 7,
row T5). Base: `38b27b06754730504f1033f1c8d6dcd4fa9fe204`
(F-006 banked; Wave 6 = 6 of 9) on branch
`yrsa/mimiralloc-07`, worktree `/home/pi/mimiralloc-team-07`
on Raspberry Pi `purpleviking1`. Date of the recorded run:
2026-11-06 (~22:4x EST). Toolchain: Debian GNU/Linux 12
(bookworm); cc 12.2.0; Mojo **1.1.0 (8189361e)** via the
`/usr/local/bin/mojo` pixi wrapper, used exactly as it
stands; kernel 6.12.112-v8-16k-hailo+; `getconf PAGESIZE`
= 16384. Pi load stayed light (sequential small runs only;
`free -g` showed 13 GiB available before and after; Samtal
HTTP 200 on port 8787 at S0). Nothing was installed.
`README.md` is byte-untouched (sha256
`bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`
for both the worktree file and `git show HEAD:README.md`);
the public header is byte-untouched (sha256
`55107f05b3eb8c120230f76b2193fdc953af800af57a91caa405c84971af8772`)
and no C source was touched. The dynamic ABI stays exactly
the four `mimir_*` symbols (no src/ change in this slice).

Governing texts: Roadmap Section 8 Phase 6 (task T5);
banked F-001 `docs/PHASE_6_FOUNDATION_PLAN.md` (Sections
6-7; the T5 row and INC-F001-08, which expected this slice
to land as a BLOCKED record); banked F-002
`docs/PHASE_6_MOJO_TOOLCHAIN_READINESS.md` (Section 2.5:
the source checkout's identity and failed build). The full
execution record is `tests/mojo/recorded_run_f007.txt`
(sections S0-S8); section references below are to it.

> **THIS SLICE CLOSES NO PART OF THE PHASE 6 EXIT.**
> Exit element **X1 stays BLOCKED** exactly as F-001
> recorded it (the original failing command was a tablet
> event, never recorded exactly, on versions this Pi does
> not hold); F-008 owns the X1 blocker record and F-009
> owns the exit packet. This slice's T5 result is
> **PARTIALLY EXECUTED** (Section 0), and its contribution
> to X3 is bounded in Section 6 in exact words. Every
> upstream result below is a **WITHOUT-replacement**
> result: the tests ran against the stock Mojo runtime
> allocator, because no banked boundary can reach
> stdlib-internal allocation (F-001 Section 6). No result
> here says anything about MimirAlloc servicing Mojo's
> own allocations.

## 0. Bottom line

- **The suite was ratified before running** (S1), as the
  F-001 matrix requires: the candidates for "relevant
  upstream tests" are the modular/mojo test trees - C1
  stdlib tests (`Mojo/stdlib/test/**`), C2 compiler/tool
  lit suites (`Mojo/test/**`), C3 C++ unit tests
  (`Mojo/unittests/**`), C4 MAX tests, C5 any tests
  shipped in the installed package. Of these, **C1 is
  the relevant suite for an allocator-integration
  phase** (it exercises the stdlib allocation paths a
  generated Mojo program allocates through); the others
  are inventoried and their runnability decided on
  evidence, not skipped silently.
- **C5 and C4 are absent, not merely unrunnable** (S2):
  the installed published package ships exactly one
  stdlib artifact, the compiled `std.mojoc` - zero test
  files, zero lit configs; MAX is not installed on this
  Pi at all, so no MAX test tree exists here.
- **C1 sources exist on this Pi** - contrary to the
  expectation F-001 recorded - inside the operator-local
  checkout `/home/pi/mojo-build/mojo-src` at
  `24f4ceb2ff1701aaca81dc974b7b4794d7d2a0a5`
  (Mojo 1.2.0.dev2026100406): 300 stdlib `test_*.mojo`
  files, plus the C2 lit trees (1,032 `.mojo` files) and
  C3. But the checkout is a **different revision** from
  the installed toolchain (1.2.0.dev vs 1.1.0), it is
  **dirty** (2 entries: `bazel/common.MODULE.bazel`
  modified, the untracked tablet-experiment
  `bazel/public-patches/tcmalloc-39bit-va.patch`), and
  its own Bazel build **failed** (S3: `Target
  //Mojo/tools/mojo:mojo failed to build`, 14 errors,
  elapsed 13776.237s), producing no built `mojo` binary.
- **The execution road nobody had recorded:** Mojo
  1.1.0 has **no `mojo test` subcommand** (S4 attempt A1:
  exit 1, `error: no such command 'test'`), but
  **`mojo run` on a test file discovers and runs its
  `test_` units** (S4 attempt A2), and the documented
  `-I` flag supplies the `test_utils` package the Bazel
  harness otherwise provides as `@mojo//:test_utils`
  (S5b). T5 is therefore executable in part with the
  toolchain exactly as installed - no install, no new
  checkout, nothing fetched.
- **The relevant C1 subset, executed** (12 files named
  before running, S5 preamble: List/Dict/Array/Deque/
  LinkedList/String storage, string memory functions,
  and the memory module's alloc/Arc/MaybeUninit units -
  the allocation paths the F-004 baseline and F-005
  programs actually use):
  - **Ran and passed: 4 files, 112 tests run, 112
    passed, 0 failed** - `bit/test_bit.mojo` 22/22 (S4),
    `collections/string/test_mem_functions.mojo` 22/22
    (S5), `memory/test_arc.mojo` 26/26 (S5b, with `-I`),
    `memory/test_memory.mojo` 42/42 (S5c, with `-I`).
  - **Compile-blocked, 0 tests run: 8 files** -
    `collections/test_list.mojo`, `test_dict.mojo`,
    `test_array.mojo`, `test_deque.mojo`,
    `test_linked_list.mojo`,
    `collections/string/test_string.mojo`,
    `memory/test_alloc.mojo`,
    `memory/test_maybe_uninit.mojo`. With the `-I`
    package path supplied, every one fails **only** on
    1.2-era language or stdlib surface under the 1.1.0
    toolchain (Section 4). That is version skew, not a
    test failure: **no executed upstream test failed**.
  - **C2 and C3 cannot run at all** on this Pi: no lit
    runner exists (S4 attempt A3: `python3 -m lit`,
    exit 1, no module) and the suites are Bazel targets
    against a same-source built toolchain that does not
    exist here (S3).
- **The project's own Mojo-path tests also re-ran** at
  this base (S6, recorded separately and never
  relabelled upstream): `make test-f003` exit 0 (4/4
  steps PASS), `make test-f005` exit 0 (audit leg 24/24
  checks PASS in the recorded tail).
- **Runnable check wired:** because a real, cheap
  re-verification now exists, this slice appends the
  additive Makefile target `test-f007`, running
  `tests/mojo/run_f007_upstream_probe.sh` (S7 shows it
  standalone, exit 0, all verdicts as recorded).
  `make test` composition is unchanged.

## 1. Ratification: what "relevant upstream" means here

Roadmap Phase 6 names no suite, revision, or location
for T5 (INC-F001-08). The F-001 matrix forbids inventing
*a substitute suite and calling it upstream*; it does
not forbid naming the actual upstream trees and testing
their runnability. S1 names the five candidates before
any execution. C1 (the modular/mojo stdlib tests) is
the relevant suite for Phase 6: Phase 6 is about a Mojo
program's allocations, and the stdlib tests are the
upstream correctness record for exactly the allocation-
bearing stdlib surface (collections, strings, memory
primitives) that such programs use. C2/C3 test the
compiler itself - the component whose internal
allocator Phase 6 cannot reach (F-001 Section 6, B6) -
so they are inventoried for completeness and decided in
Section 5, not executed even where a road might be
forced. The executed subset inside C1 is the
allocator-relevant slice of it, named file by file in
the recorded run before execution (S5 preamble);
whole-tree execution (300 files) was not attempted: it
is neither bounded nor relevant, and most of the tree
(gpu/, python/, subprocess/, benchmark/, the
compile_fail and abort-expectation trees) is outside
this phase's question by construction (reasons per
tree in the S5 preamble).

## 2. Presence inventory (S2, S3)

Installed package (`/home/pi/mojo-dev` pixi
environment, Mojo 1.1.0): `lib/mojo/` holds `std.mojoc`
only; `find` for `test_*.mojo` returns 0 and for
`lit.cfg.py` returns 0 across the whole environment.
MAX and the `modular` CLI do not resolve on PATH.

Source checkout (`/home/pi/mojo-build/mojo-src`,
origin `https://github.com/modular/mojo.git`, HEAD
`24f4ceb2ff1701aaca81dc974b7b4794d7d2a0a5`): complete
test trees (300 stdlib test files; 1,032 lit-tree
`.mojo` files, 1 `lit.cfg.py` at the `Mojo/test` root;
`Mojo/unittests/` C++ units), a dirty working tree (2
entries, S3), `bazel-bin/` intermediates but **no
built `mojo` binary** (`test -x` = ABSENT), the failed
build on record in `/home/pi/mojo-build/build.log`,
and no `bazel`/`bazelisk`/`lit` on PATH. F-002 Section
2.5 already recorded this checkout as operator-local
state, **not banked evidence**; this slice uses its
test files as execution inputs and records that
provenance wherever a result depends on them
(INC-F007-03).

## 3. The runner question (S4, S5b)

Three roads were tried, in order:

1. **`mojo test`** - the runner current upstream
   documentation implies. On 1.1.0: `error: no such
   command 'test'`, exit 1. The 1.1.0 command list is
   run/build/repl/debug/precompile/format/doc/demangle.
2. **`mojo run FILE`** - on a program it builds and
   runs `main`; on a **test file it discovers the
   `test_` units and runs them under the testing
   harness**, printing per-test PASS lines and a
   `Summary ... N tests run: N passed` line, exit 0 on
   success. This is the road all executions used.
3. **`mojo run -I DIR FILE`** - the documented include-
   path flag supplies the `test_utils` package (the
   checkout's `Mojo/stdlib/test/test_utils/`, a normal
   package with `__init__.mojo`) that the upstream
   Bazel rules inject as a dependency. S5b separates
   the causes: with `-I`, the `unable to locate module
   'test_utils'` error disappears wherever it appeared;
   files then either run (`test_arc`, `test_memory`)
   or fail on version skew alone.

The lit road (C2's native runner) was also attempted
in its cheapest form: `python3 -m lit --version`
exits 1 (`No module named lit`); there is no lit
binary on PATH. No road to C2/C3 exists without
installing tooling or completing the source build -
both outside this slice's authority (installs are
Volmarr's word; a 3.8-hour Bazel build that already
failed once is not a bounded step).

## 4. Executions in detail (S4, S5, S5b, S5c)

Every execution: `timeout 120 mojo run [-I $SRC] FILE`,
sequential, on the Pi, outputs captured to files and
the tallies transcribed into S8. `$SRC` =
`/home/pi/mojo-build/mojo-src/Mojo/stdlib/test`.

| File | Road | Exit | Tests | Verdict |
|---|---|---|---|---|
| bit/test_bit.mojo | bare (S4 A2) | 0 | 22 run, 22 passed | PASS |
| collections/string/test_mem_functions.mojo | bare (S5) | 0 | 22 run, 22 passed | PASS |
| memory/test_arc.mojo | `-I` (S5b) | 0 | 26 run, 26 passed | PASS |
| memory/test_memory.mojo | `-I` (S5c) | 0 | 42 run, 42 passed | PASS |
| collections/test_list.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `conforms_to` param-list operand; `AnyTrait __bool__` (in test_utils/types.mojo and the file) |
| collections/test_dict.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: same two skew classes |
| collections/test_array.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `AnyTrait __bool__` |
| collections/test_deque.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `AnyTrait __bool__`; `conforms_to` |
| collections/test_linked_list.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `conforms_to` |
| collections/string/test_string.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `String.is_ascii_whitespace` absent from the 1.1.0 stdlib |
| memory/test_alloc.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `Layout` alignment API differs in 1.1.0 |
| memory/test_maybe_uninit.mojo | `-I` (S5c) | 1 | 0 run | compile-blocked: `conforms_to`; `AnyTrait __bool__` |

Reading discipline (INC-F007-02): each run compiles a
**1.2.0.dev test file** against the **installed 1.1.0
compiled stdlib**. A PASS therefore means: the 1.1.0
stdlib satisfies that (newer) upstream test as written.
A compile-block means: the pair is unmeasurable at
these revisions - it is evidence about neither the
test's correctness nor the 1.1.0 stdlib's quality, and
this record never reports it as a failure of either.
No executed test failed, so there is no failure to
attribute anywhere.

## 5. What is blocked / owed, and the smallest next steps

- **T5, remainder of the C1 subset (8 files): BLOCKED
  by version skew.** The test sources pin to
  1.2.0.dev2026100406; the installed toolchain is
  1.1.0. Smallest next step: a ratified matching pair -
  either Volmarr's word to install the published
  1.2.0-line package (F-002 recorded the road and that
  installs are his word) and re-run this exact subset,
  or a checkout of the test tree at the 1.1.0 tag
  obtained as a recorded acquisition. Neither is done
  here: installs are reserved, and fetching a second
  checkout to make a record greener is exactly the
  suite-shopping INC-F001-08 forbids.
- **T5, C2 lit suites and C3 C++ units: BLOCKED by
  absent runner + absent same-source toolchain.**
  Smallest next step: completion of the source build
  (the recorded failure is a compiler error in
  `KGENCompiler.cpp` under the jammy sysroot - a
  source-build repair, not an allocator question) or
  Volmarr's word for a lit installation plus a built
  toolchain. Owed, not attempted.
- **T5, C4 MAX tests: BLOCKED by absence.** MAX is
  not installed; there is nothing to inventory beyond
  that fact. Owed to any future MAX presence.
- **T5, WITH-replacement leg: BLOCKED by boundary
  scope (INC-F007-04).** Upstream tests allocate
  through stdlib internals; the banked B2+B1 boundary
  services only a generated program's own explicit
  `mimir_*` calls. A WITH leg becomes possible only
  if a future phase adds a standard-name surface or
  an allocator hook (F-001 Section 6, B3/B5) - a
  design decision for Volmarr, not a test step.
- **X3 wording (Section 6) and the F-009 packet**
  inherit this slice's PARTIAL state; nothing here
  discharges them.

## 6. Contribution to the Phase 6 exit (exact words)

F-001's matrix bounds X3 ("no new correctness
failures") by T5's record. At F-001's writing the
bound was: *"no new failures observed in the
F-004/F-006 runs."* This slice strengthens the
record, and F-009 may state X3's basis in exactly
these words and no stronger: **no failures observed
in the F-004/F-006 runs, in this slice's re-run of
the F-003/F-005 programs, or in the 112 relevant
upstream stdlib tests executed at this base; the
remainder of the relevant upstream subset is
compile-blocked by version skew and the lit/C++
suites are unrunnable here, so X3 is PARTIAL, not
MET.** X1 remains BLOCKED (F-008's record); X2 stands
on F-005; X5's hello-world exclusion is untouched -
no minimal-program success is counted anywhere in
this slice.

## 7. Inconsistencies recorded in this slice - INC-F007-NN

- **INC-F007-01 - INC-F001-08 is only slice-discharged,
  not roadmap-discharged.** F-001 expected T5 to land
  BLOCKED because no suite was ratified and none
  existed on the Pi. Both premises turned out partly
  wrong (a checkout exists; a subset runs). This slice
  ratifies the modular/mojo stdlib tree at the
  checkout's pinned revision as the relevant suite -
  a slice-level ratification. The roadmap text still
  names no suite; a future reader of the roadmap alone
  cannot reproduce this choice. Owner: F-009, to carry
  the ratification into the exit packet explicitly.
- **INC-F007-02 - Cross-version measurement.** All C1
  results pair 1.2.0.dev test files with the 1.1.0
  stdlib/compiler. The pairing is recorded in every
  tally's reading discipline (Section 4), but the
  structural fact remains: upstream's own harness would
  never run this pair, so these results are a
  Pi-environment record, not an upstream CI result,
  and must not be quoted as the latter.
- **INC-F007-03 - The execution inputs are not banked.**
  The test files live in an operator-local, dirty
  checkout that F-002 expressly left unbanked. This
  slice records the checkout HEAD, its dirty entries,
  and the sha256 of the S4 probe file
  (`4503631cc518f16cd1b714457b56c59c519e3c1abaeb87c176c0f4c09f74d57d`
  for `test_bit.mojo`); the other eleven files are
  identified by path at that HEAD only. If the checkout
  changes, the tallies stand as a dated record but are
  not bit-reproducible. Owner: any future slice that
  re-runs the subset must re-record the HEAD first
  (the probe prints it).
- **INC-F007-04 - T5's comparison dimension is one-
  sided here.** The Phase 6 paragraph asks for
  comparison "against the same revision without the
  replacement"; for upstream tests only the WITHOUT
  side exists (Section 5). The with/without comparison
  proper remains F-006's, over the project's own
  programs. Owner: F-009's synthesis; do not merge the
  two records into a single comparison claim.
- **INC-F007-05 - Helper-skew vs unit-skew attribution
  is coarse.** Several files fail inside
  `test_utils/types.mojo` itself (the 1.2-era helper
  uses `AnyTrait __bool__` surface 1.1.0 lacks) before
  reaching the unit under test. Those files are
  blocked by their *harness helper*, not demonstrably
  by their own contents; per-file surgery (pinning an
  older helper) was not attempted, as it would change
  what "upstream test" means mid-record.
- **INC-F007-06 - The probe re-checks the road, not the
  subset.** `make test-f007` re-runs one representative
  file (test_bit, 22 tests) plus the presence verdicts;
  it does not re-execute the twelve-file subset, whose
  tallies stand on this slice's recorded run alone. A
  green probe therefore means "the F-007 road is still
  live", never "the subset still passes 112/112".

## 8. What this slice does not do

- It installs nothing, fetches nothing, and modifies
  nothing outside its own four files (this document,
  `tests/mojo/recorded_run_f007.txt`,
  `tests/mojo/run_f007_upstream_probe.sh`, and the
  appended Makefile block). The checkout, the pixi
  environment, and every banked file are untouched;
  `make test` composition is unchanged.
- It runs no whole-tree suite, no benchmark, no soak:
  twelve named files, one diagnostic pair, one probe
  file - every execution bounded by `timeout 120` and
  recorded with its exit code.
- It claims no allocator servicing on the Mojo path
  beyond what F-003/F-005 already proved for explicit
  calls, and no part of the Phase 6 exit.
