# E-003 — Static library + static and dynamic linking routes, both recorded

Phase 5 (ABI completeness and interposition), Wave 5, slice 3 of 9.
Base: `e9b6a5a` (E-001 banked). Branch: `yrsa/mimiralloc-03`.
Worktree: `/home/pi/mimiralloc-team-03` on purpleviking1 (Raspberry Pi,
Cortex-A76), user `pi`. Date of the recorded run: 2026-11-06T19:04:51-05:00.
Toolchain: Debian GNU/Linux 12 (bookworm); cc 12.2.0, GNU ar 2.40,
GNU ld 2.40; kernel 6.12.112-v8-16k-hailo+; `getconf PAGESIZE` = 16384;
glibc 2.36. Pi load stayed light (one `make test` at a time; Samtal
unimpeded).

Governing texts: roadmap Phase 5 (Section 8 — exit: "independent test
executable and target tool show the intended symbols binding to
MimirAlloc and pass lifecycle tests. Exact link route recorded");
`docs/PHASE_5_FOUNDATION_PLAN.md` (E-001 — the four-symbol contract,
link-route options R1/R2/R3/R4, and the INC-D002-02 remedy plan);
`docs/contracts.md` (core-symbol rows, by analogy only — the namespaced
`mimir_*` surface is [UNRESOLVED: U-03] and this slice settles nothing
about it); `include/mimir_alloc.h` (the public surface: exactly
`mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc`).

## 0. Bottom line

- **R1 — static archive, direct link: PROVEN.** A new additive Makefile
  target builds `build/libmimiralloc.a` from the three banked objects.
  The new consumer `tests/abi/test_e003.c` links against the archive
  (two link forms, both recorded) and runs **16/16 checks, exit 0**,
  three times (Makefile target, `-L`/`-l` form, final regression).
  `nm` shows the four public symbols defined (`T`) in the executable;
  `readelf -d` shows the only `DT_NEEDED` is `libc.so.6`; `ldd`
  shows no `libmimiralloc` line; the link map shows all three archive
  members pulled in to satisfy the consumer's references.
- **R2 — shared library, dynamic link: OWED at this base.** No
  shared-library target, no built `.so`, and no version script exist
  in this tree, so no dynamic link or run was performed and none is
  claimed. The exact commands that will prove R2 are recorded in
  Section 4 and in the recorded run (Section S7), to be run once
  E-002 is banked into the base. See INC-E003-01 for the precise
  state of E-002's commits in this clone.
- `make test` composition is unchanged and exits 0 (twice in the
  recorded run). README.md untouched. One commit on
  `yrsa/mimiralloc-03`; no push or merge (the keeper banks in roadmap
  order — E-002 banks before E-003).

Evidence file: `tests/abi/recorded_run_e003.txt` (1,494 lines, 110,424
bytes) — every command with its exit code, in sections S0–S8.
This document summarises it; the recorded run is the record.

## 1. The static library (R1 artifact)

### 1.1 Makefile target

Additive only, appended to `Makefile` (lines 198–223; new variable
`STATICLIB = $(BUILD)/libmimiralloc.a`):

```make
$(BUILD)/libmimiralloc.a: $(LIBOBJS)
	ar rcs $@ $(LIBOBJS)

static-lib: $(BUILD)/libmimiralloc.a

$(BUILD)/test_e003_static: tests/abi/test_e003.c $(BUILD)/libmimiralloc.a | $(BUILD)
	$(CC) $(CFLAGS) tests/abi/test_e003.c $(BUILD)/libmimiralloc.a -o $@ -Wl,-Map,$(BUILD)/test_e003_static.map

test-e003: $(BUILD)/test_e003_static
	./$(BUILD)/test_e003_static
```

`make test` is untouched: same eight binaries, same order, same
commands (verified — the recorded run's S1 baseline `make test` output
contains no `libmimiralloc.a` line and no `test_e003` line).
New targets `static-lib` and `test-e003` are standalone, in the
pattern of the earlier `test-d003`/`test-d008` targets.

### 1.2 The archive

Build command (from the recorded run, S2):

```sh
ar rcs build/libmimiralloc.a build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o
```

Result: `build/libmimiralloc.a: current ar archive`, **12,070 bytes**,
members exactly (recorded `ar t` output):

```text
mimir_alloc.o
mimir_core.o
mimir_platform.o
```

`make static-lib` exit 0. No source file changed: the archive packages
the banked objects exactly as built by the banked `CFLAGS`
(`-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc`), with no
`-fPIC` added (see INC-E003-05).

### 1.3 Archive symbol table

Full table is in the recorded run (S3). Summary of defined symbols
(`nm --defined-only build/libmimiralloc.a`):

| Member | Defined symbols |
|---|---|
| `mimir_alloc.o` | exactly the four public symbols, all `T`: `mimir_malloc` 0x0, `mimir_free` 0x1c, `mimir_calloc` 0x3c, `mimir_realloc` 0x60 |
| `mimir_core.o` | 4 `T`: `mimir_core_alloc`, `mimir_core_calloc`, `mimir_core_free`, `mimir_core_realloc`; 6 local `t` helpers (`alloc_locked`, `descriptor_is_live`, `find_candidate_locked`, `registry_insert`, `registry_remove`, `retire_locked`); 3 `b` data (`mimir_live_bytes`, `mimir_live_mappings`, `mimir_registry_head`) |
| `mimir_platform.o` | 5 `T`: `mimir_platform_global_lock`, `mimir_platform_global_unlock`, `mimir_platform_map_acquire`, `mimir_platform_map_release`, `mimir_platform_page_size`; 1 local `t` (`page_round_up_checked`); 1 `b` (`mimir_global_lock`) |

Counting defined global **function** symbols, the archive holds 13
(4 public + 4 core + 5 platform) — the same count as the INC-D002-02
defect for an ad-hoc `.so`. In a static archive this is **not a leak**
and not a defect: archive members are objects, and internal names
existing in a member is normal (E-001 Section 4.2 step 6). The
visibility / version-script remedy controls the **dynamic** surface of
the shared library only. See INC-E003-02.

## 2. The consumer and the static route (R1 proven)

### 2.1 Consumer program

`tests/abi/test_e003.c` (190 lines, 6,480 bytes) is a small consumer
that uses **only** the four public symbols from
`include/mimir_alloc.h`. It calls no internal name (`mimir_core_*` /
`mimir_platform_*`) and no standard-name allocator entry point for its
lifecycle — binding to MimirAlloc is by explicit `mimir_*` calls only
(E-001 Section 5.2, "Explicitly called" finding). Its 16 checks:

- `mimir_malloc(64)` non-null, aligned to `_Alignof(max_align_t)`,
  pattern verifies over the full extent;
- `mimir_calloc(8, 16)` non-null, reads zero at return, then filled
  with a byte pattern;
- `mimir_realloc` grow to 512 preserves the first 128 bytes;
  shrink to 32 preserves the first 32 bytes;
- `mimir_realloc(NULL, 32)` acts as `mimir_malloc` (non-null, writable);
- `mimir_malloc(SIZE_MAX)` returns NULL (oversize refusal);
- `mimir_free(NULL)` returns normally (defined no-op);
- `mimir_free` of the malloc block returns normally; a fresh
  `mimir_malloc(48)` after the frees succeeds and is writable over
  its full extent.

Not checked, by design: zero-size behaviour ([UNRESOLVED: U-09]) and
errno (not promised, U-10). This consumer must not be cited as
settling either.

### 2.2 Static link, form A — explicit archive path (Makefile target)

```sh
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc tests/abi/test_e003.c build/libmimiralloc.a -o build/test_e003_static -Wl,-Map,build/test_e003_static.map
./build/test_e003_static
```

Run: **E003 16/16 checks passed, RESULT: PASS, exit 0** (recorded run
S4, and again in S8 as the final regression).

### 2.3 Static link, form B — E-001 plan command form (`-L` / `-l`)

```sh
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc tests/abi/test_e003.c -Lbuild -lmimiralloc -o build/test_e003_static_lflag
./build/test_e003_static_lflag
```

Run: **E003 16/16 checks passed, RESULT: PASS, exit 0** (recorded run
S5). Form B is valid as a static proof **only at this base**, because
no `.so` sits beside the `.a` in `build/` for the linker to prefer.
After E-002 banks, the same command would resolve to the shared
library instead — see INC-E003-03. Form A (explicit archive path) is
the unambiguous static proof and is the one `make test-e003` uses.

### 2.4 Binding proof on `build/test_e003_static`

Recorded run S6, verbatim results:

- Consumer object before link (contrast): the four public symbols are
  **undefined (`U`)** in the consumer object — the consumer brings no
  allocator of its own.
- In the linked executable, the four public symbols are **defined
  (`T`)**, with addresses:

```text
0000000000000fd4 T mimir_malloc
0000000000000ff0 T mimir_free
0000000000001010 T mimir_calloc
0000000000001034 T mimir_realloc
```

- Every `mimir_*` symbol in the executable is the same set the archive
  carries (4 public `T` + 4 `mimir_core_*` `T` + 5
  `mimir_platform_*` `T` + 4 `b` data symbols) — the allocator code is
  inside the consumer binary, as a static link means.
- `readelf -d build/test_e003_static`: the only `DT_NEEDED` entry is
  `libc.so.6` — **no `DT_NEEDED` for `libmimiralloc`**.
- `ldd build/test_e003_static`: `linux-vdso.so.1`, `libc.so.6`,
  `/lib/ld-linux-aarch64.so.1` — **no `libmimiralloc` line**.
- Link map (`build/test_e003_static.map`, `-Wl,-Map`): all three
  archive members were pulled in to satisfy references —
  `mimir_alloc.o` (for `mimir_malloc`, from the consumer object),
  `mimir_core.o` (for `mimir_core_alloc`), `mimir_platform.o` (for
  `mimir_platform_page_size`); `.text` contributions recorded per
  member (alloc 0x84, core 0x804, platform 0x2d4 bytes).

### 2.5 What R1 does NOT prove

Presence and binding are not use (E-001 Section 5.2): this slice
proves the consumer's calls are linked into MimirAlloc code, not that
every call was serviced by it at runtime — that is E-005's
counter/audit-hook evidence, still owed. R1 also proves nothing about
cross-module ownership (E-008), startup ordering (E-008), or C++
variants. R1 is not a drop-in replacement: it services only explicit
`mimir_*` calls and replaces nothing implicitly.

## 3. `make test` regression

`make test` composition unchanged (same eight binaries:
`test_smoke`, `test_core`, `test_b007`, `test_c005`, `test_c006`,
`test_c007`, `test_c008`, `test_d004`). Recorded run S1 (baseline) and
S8 (final) both exit 0, with identical per-binary counts:

| Binary | Checks |
|---|---|
| `test_smoke` | SMOKE 18/18 |
| `test_core` | CHECKS: 29 run, 0 failed |
| `test_b007` | CHECKS: 70 run, 0 failed |
| `test_c005` | CHECKS: 95 run, 0 failed |
| `test_c006` | CHECKS: 23 run, 0 failed |
| `test_c007` | CHECKS: 122 run, 0 failed |
| `test_c008` | CHECKS: 42 run, 0 failed, 7 skipped-not-yet |
| `test_d004` | CHECKS: 4590 run, 0 failed |

Total per full run: 4,989 checks, 0 failed (plus the 7 documented
skips in `test_c008`). `FINAL_MAKE_TEST_EXIT=0`,
`FINAL_TEST_E003_EXIT=0`.

## 4. The dynamic route (R2 — OWED at this base)

### 4.1 What was checked, and what it showed

Recorded run S0 (before this slice's builds, after `make clean`) and
S7 (after the builds) establish, in this tree:

- `ls build/libmimiralloc.so` — **exit 2, absent** (both before and
  after this slice's builds).
- `grep -n "libmimiralloc\.so" Makefile` — **exit 1, no match**: no
  `.so` target and no `.so` reference in the Makefile.
- `ls src/api/mimir_alloc.map` — **exit 2, absent**: E-002's version
  script file does not exist in this tree.
- `git merge-base --is-ancestor <E-002 commit> HEAD` — **exit 1 for
  both E-002 commits found in this clone** (see INC-E003-01): no
  E-002 commit is in this branch's history. The only E-002 mention
  in HEAD history is the E-001 plan commit `e9b6a5a` itself, whose
  message names E-002 as the next slice.

No dynamic link was attempted, no dynamic consumer was run, and no
`ldd` / `LD_DEBUG` output is claimed for R2 anywhere in this slice.

### 4.2 Exact commands that will prove R2

Run from the worktree root, once E-002 is banked into the base so
that `build/libmimiralloc.so` exists; `<ABSDIR>` is the absolute path
of the worktree's `build/` directory. These commands are also printed
verbatim in the recorded run, S7:

```sh
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -c tests/abi/test_e003.c -o build/test_e003.o
cc -std=c11 -pthread build/test_e003.o -Lbuild -lmimiralloc -Wl,-rpath,<ABSDIR> -o build/test_e003_dynamic
./build/test_e003_dynamic ; echo DYNAMIC_RUN_EXIT=$?
nm -D --defined-only build/libmimiralloc.so
readelf -d build/test_e003_dynamic
ldd build/test_e003_dynamic
nm -D build/test_e003_dynamic
LD_DEBUG=bindings ./build/test_e003_dynamic 2>&1 | grep mimir
```

Expected evidence (E-001 Section 5.2, R2 row): `nm -D --defined-only`
on the `.so` lists exactly the four public symbols; `readelf -d` on
the consumer shows a `DT_NEEDED` naming the library (with the SONAME
E-002 decided); `ldd` shows the `.so` resolved; `nm -D` /
`readelf --dyn-syms` on the consumer shows the four symbols undefined
(`U`) there and defined in the `.so`; an `LD_DEBUG=bindings` excerpt
names the `.so` as the provider of each of the four symbols; the
lifecycle run (this same consumer) exits 0. Whoever runs these —
E-004, or a verification pass after the keeper banks E-002 — records
the results with this same consumer source, so R1 and R2 are proven
on identical code.

### 4.3 R3 and R4 (carried from E-001, unchanged)

- **R3 (`dlopen` / `dlsym`)**: not queued in Wave 5 — **OWED by
  decision, not by failure**. E-009's packet must list it as
  untested-by-decision.
- **R4 (LD_PRELOAD interposition of the standard names)**: **rejected
  route** — the library defines no standard-name symbols, so
  preloading it interposes nothing; demonstrating interposition would
  require adding standard-name definitions, a silent ABI expansion
  into the undecided U-01 territory. Roadmap Section 13's caution
  ("No default global interposition until well validated") stands.

## 5. The exact link route (Phase 5 exit)

This slice does **not** name THE exact link route for Phase 5 exit
purposes. Per E-001 Section 5.4, that naming belongs to E-004, on its
recorded evidence from running the same independent lifecycle
executable through **both** routes. This slice's contribution to that
decision: R1 is proven with the evidence in Sections 1–2; R2 is owed
with the exact proving commands in Section 4.2. E-009's exit packet
may claim "exact link route recorded" only by citing E-004's naming.

## 6. Inconsistencies recorded by this slice (INC-E003-*)

- **INC-E003-01 — Dynamic route OWED at this base, with E-002 commits
  present but unbanked in the clone.** At the slice base `e9b6a5a`,
  this tree has no `.so` target, file, or version script, so R2 cannot
  be run here. Two E-002 commits exist in this clone's refs —
  `73f7458` (on branch `yrsa/mimiralloc-02`) and `71ce730` (contained
  in `main`, i.e. banked on main after this slice's base was cut) —
  but neither is an ancestor of this branch's HEAD
  (`merge-base --is-ancestor` exit 1 for both), so neither is part of
  this slice's tree and neither was used. This slice does not
  cherry-pick E-002: banking order (E-002 banks before E-003) is the
  keeper's, and one slice works from one base. After the keeper banks
  E-002 into the base, the Section 4.2 commands close R2 — most
  naturally in E-004's dual-route run. Status: OWED, with the exact
  closure step named.
- **INC-E003-02 — The static archive exposes the nine internal names
  as global `T` symbols.** `nm` on `build/libmimiralloc.a` shows the
  4 `mimir_core_*` + 5 `mimir_platform_*` names with default
  visibility, the same 13-function-symbol count as INC-D002-02's
  ad-hoc `.so`. This is expected archive behaviour, not a leak:
  visibility and version-script controls govern the `.so` dynamic
  surface only. Consequence: a static consumer that declares an
  internal name could link it; the only protection is the documented
  contract (the public header declares exactly four symbols) plus
  discipline. E-009's packet must carry this distinction so no reader
  cites E-002's export fix as a static-archive fix.
- **INC-E003-03 — The `-L` / `-l` link form stops proving the static
  route once a `.so` exists.** The linker prefers a shared library
  over an archive of the same name in the same directory, so after
  E-002 banks, `cc ... -Lbuild -lmimiralloc` will bind the `.so`,
  not the `.a`. The S5 run is a valid static proof only because no
  `.so` existed at this base. The unambiguous static link is the
  explicit archive path used by `make test-e003` (Section 2.2).
  E-004's exact-route evidence must use the explicit path for R1.
- **INC-E003-04 — First evidence draft mis-annotated two S0 checks;
  caught and corrected in-slice before commit.** The first recorded
  run claimed `git log --all --grep=E-002` was empty (it was not —
  two E-002 commits exist on other refs) and claimed a Makefile
  shared-library grep had no match (its loose `grep "so"` pattern
  matched a comment line, so the exit code did not mean what the
  annotation said). Both errors were caught by reading the recorded
  run before writing this document. The evidence script was corrected
  (precise `libmimiralloc\.so` grep; per-commit
  `merge-base --is-ancestor` checks) and the recorded run regenerated
  whole; the committed `recorded_run_e003.txt` is the corrected run
  (2026-11-06T19:04:51-05:00). No other slice content depended on the
  wrong annotations. Lesson carried: an annotation is a claim, and
  claims are checked against the output above them.
- **INC-E003-05 — Library objects are built without `-fPIC`
  (INC-D002-03 carried).** The static archive works without it, and
  this slice adds no `-fPIC` flag. PIC discipline belongs to E-002's
  dedicated `.so` objects; static consumers on this target are
  unaffected. Recorded so the absence is a known carried item, not an
  oversight.
- **INC-E003-06 — No install story, no SONAME for the static route.**
  `libmimiralloc.a` lives only under `build/` in this worktree; there
  is no install prefix, no installed copy, and no SONAME decision in
  this slice (INC-D002-01 carried, unchanged). The static route needs
  no SONAME; the SONAME decision belongs to E-002 and must be known
  before the Section 4.2 commands are run.

## 7. Files changed by this slice

| File | Change |
|---|---|
| `Makefile` | Additive E-003 block (lines 198–223): `libmimiralloc.a` target, `static-lib`, `test_e003_static` build (explicit archive path + `-Wl,-Map`), `test-e003`. `make test` composition unchanged. |
| `tests/abi/test_e003.c` | New consumer: 190 lines, 6,480 bytes; 16 checks, 4 public symbols only. |
| `tests/abi/recorded_run_e003.txt` | New recorded evidence: 1,494 lines, 110,424 bytes, sections S0–S8. |
| `docs/PHASE_5_LINK_ROUTES_E003.md` | This document. |

No source file (`src/`, `include/`) changed. README.md untouched.
`hidden_files/ASSIGNMENT.md` updated in the worktree (untracked working
note, not part of the commit).

## 8. Bottom line, restated

R1 (static): built, linked twice, run green three times, binding
proven by `nm` / `readelf -d` / `ldd` / link map. R2 (dynamic): owed
at this base, with the exact proving commands recorded and the precise
state of E-002's unbanked commits stated. Phase 5 exit item "exact
link route recorded" remains E-004's to name, on dual-route evidence.
