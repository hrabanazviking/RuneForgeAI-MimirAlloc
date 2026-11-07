# Shared Library Build — Slice E-002 (Wave 5)

Base: `8619a4b` (Wave 4 COMPLETE), branch `yrsa/mimiralloc-02`.
Host where every measurement below was made:
`Linux purpleviking1 6.12.112-v8-16k-hailo+ aarch64`,
`getconf PAGESIZE` = **16384**, compiler
`cc (Debian 12.2.0-14+deb12u1) 12.2.0` (measured this run,
2026-11-06; full record in `tests/abi/recorded_run_e002.txt`).
Governing text: banked finding **INC-D002-02** (D-002: a shared
library linked from the banked objects would export 13 dynamic
symbols, not 4 — no library target, visibility control, or
version script existed), roadmap Section 6 (contract-first
API/ABI specification), and the public header
`include/mimir_alloc.h` ([UNRESOLVED: U-03]; B-008 N-10: no
symbol-version promise is made).

## Verdict (plain first)

**INC-D002-02 is remedied, proven on this Pi.**
`build/libmimiralloc.so` now builds via `make lib`, and its
defined dynamic exports are **exactly the four public symbols**
— `mimir_malloc`, `mimir_free`, `mimir_calloc`,
`mimir_realloc` — verified by `nm -D --defined-only` (4 lines)
and by `readelf --dyn-syms` (the only DEFINED `.dynsym`
entries are those four GLOBAL FUNC symbols). Every internal
`mimir_core_*` / `mimir_platform_*` definition is local in the
library (lowercase `t` in the full symbol table) and absent
from `.dynsym`. A consumer program linked only against the
`.so` (`tests/abi/test_e002.c`, `make test-e002`) runs green:
**16 checks, 0 failed, exit 0**. Native `make test` still
exits **0** with its composition unchanged.

## What this slice adds

| File | Role |
|---|---|
| `Makefile` (+51 lines, appended only) | `lib` and `test-e002` targets; PIC object rules under `build/pic/` |
| `src/api/mimir_alloc.map` (29 lines) | Version script: the machine-readable export contract |
| `tests/abi/test_e002.c` (139 lines) | Consumer evidence test, linked against the `.so` only |
| `tests/abi/recorded_run_e002.txt` | Recorded inspection + consumer run (commands, exit codes, counts) |
| `docs/SHARED_LIBRARY_BUILD.md` | This record |

No banked source file is modified. `README.md` is untouched.
`lib` and `test-e002` are standalone targets, deliberately
**not** part of `make test`; the `test` target's prerequisites
and recipe are byte-identical to the base (verified against
`git show HEAD:Makefile`).

## Build

```sh
make lib        # builds build/libmimiralloc.so
make test-e002  # builds and runs the consumer test
```

The banked objects are compiled without `-fPIC` (INC-D002-03),
so the library is linked from **dedicated PIC objects** under
`build/pic/`, compiled from the same sources:

- `build/pic/mimir_alloc_pic.o` ← `src/api/mimir_alloc.c`,
  `-fPIC -fvisibility=default` (its only definitions are the
  four public entry points);
- `build/pic/mimir_core_pic.o` ← `src/core/mimir_core.c`,
  `-fPIC -fvisibility=hidden`;
- `build/pic/mimir_platform_pic.o` ←
  `src/platform/mimir_platform.c`, `-fPIC -fvisibility=hidden`;

linked with
`cc -shared -Wl,--version-script=src/api/mimir_alloc.map -o build/libmimiralloc.so … -pthread`.
Result this run: ELF 64-bit LSB shared object, ARM aarch64,
71,048 bytes, BuildID[sha1]=a25ec2c467958479ea71f4d57473076a2fb9abf2.

## Visibility mechanism (two independent layers)

1. **Compile-time visibility.** The core and platform objects
   are compiled `-fvisibility=hidden`, so their definitions
   are never candidates for dynamic export; the API object is
   `-fvisibility=default` because it defines exactly the four
   public entry points and nothing else.
2. **Link-time version script.** `src/api/mimir_alloc.map`
   lists the four public symbols under `global:` and binds
   `*` under `local:`, so even a future object added to the
   link with default visibility cannot leak a symbol: the
   script, not per-file discipline alone, is the contract.

The script's node is deliberately **anonymous** (no version
name). A named node such as `MIMIRALLOC_1.0` would (a) add an
ABS version-name symbol to `.dynsym`, so the defined dynamic
symbol list would no longer be exactly the four public
symbols, and (b) imply a symbol-version / ABI-stability
promise the public header expressly withholds — the
namespaced API is [UNRESOLVED: U-03] and no symbol-version
promise is made (B-008 N-10). A named version node belongs
to the future slice that freezes the ABI, not to this one.

## Inspection results (this run, on the Pi)

Full output: `tests/abi/recorded_run_e002.txt`.

- `nm -D --defined-only build/libmimiralloc.so` → exactly
  4 lines: `mimir_calloc`, `mimir_free`, `mimir_malloc`,
  `mimir_realloc` (all `T`). Grep counts for `mimir_core_`
  and `mimir_platform_` in the defined dynamic list: **0, 0**.
- `readelf --dyn-syms -W` → 18 entries; the only DEFINED
  ones are entries 14–17, the four public FUNC GLOBAL
  symbols. All remaining entries are UND imports
  (`memcpy`, `memset`, `mmap`, `munmap`, `sysconf`,
  `pthread_mutex_lock/unlock`, `__cxa_finalize`, weak
  runtime symbols) or the null/section entries. No ABS
  version-name symbol exists.
- `nm --defined-only` (full table) → every `mimir_core_*`
  and `mimir_platform_*` definition is local (`t`), and the
  internal state (`mimir_global_lock`, `mimir_live_bytes`,
  `mimir_live_mappings`, `mimir_registry_head`) is local
  data (`b`).
- Consumer: `ldd build/test_e002` resolves
  `libmimiralloc.so` to the build tree `.so`; the test
  exercises malloc (non-null, `max_align_t`-aligned, distinct
  live blocks, contents intact), calloc (zeroed extent,
  writable), realloc grow/shrink (prefix preserved),
  `mimir_free(NULL)` as a no-op, and post-free allocator
  health — **16 checks, 0 failed, RESULT PASS, exit 0**,
  both under `make test-e002` and standalone.
- `make test` → exit **0** (per-binary CHECKS lines in the
  recorded run, Section 6; composition unchanged).

Behaviour exercised is the public header's defined-case list,
by analogy under [UNRESOLVED: U-03], exactly as the header
states it; zero-size and realloc-to-zero postures are not
exercised because no behaviour is promised for them
(U-09 / U-12 remain unresolved).

## Inconsistencies and owed items

- **INC-E002-01 (OWED, deliberate):** the library carries no
  SONAME, no versioned file name (`libmimiralloc.so.1`), and
  there is no `install` target or `ldconfig` story. All three
  are ABI-freeze decisions; the ABI is expressly not frozen
  (U-03 unresolved, B-008 N-10). Owed to the future
  ABI-freeze slice, together with the named version node.
- **INC-E002-02 (noted, pre-existing):** the consumer binary
  finds the library through an rpath of `$ORIGIN` into the
  build tree (`-Wl,-rpath,'$$ORIGIN'`). That is evidence
  wiring for this slice, not an installation layout; no
  claim is made that the `.so` is installed or discoverable
  system-wide. Resolves with INC-E002-01.
- **INC-E002-03 (noted, pre-existing):** the tree has no
  `.gitignore`, so `build/` (including the `.so`, PIC objects,
  and test binaries) shows as untracked. This predates E-002
  (the same was true for every earlier slice's build output)
  and no build artifact is committed by this slice; a
  repo-wide ignore policy is outside this slice's scope and
  is recorded here for the coordinator.
- **INC-E002-04 (OWED):** shared-vs-static behavioural parity
  is evidenced only by the consumer test's 16 defined-case
  checks against the `.so`; the full native suite
  (`make test`) still runs against the static objects. A
  full-suite run linked against the `.so` is owed before any
  claim that the shared build is behaviourally equivalent
  across the whole contract surface.

## Banking note

One commit on `yrsa/mimiralloc-02`; not pushed or merged by
this slice — the keeper banks E-002 after E-001 in roadmap
order. README.md untouched, per the standing rule.
