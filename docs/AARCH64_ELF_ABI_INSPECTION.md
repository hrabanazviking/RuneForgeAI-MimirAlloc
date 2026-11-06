# AArch64 ELF / ABI inspection — D-002

Slice D-002 (Wave 4, team-02). Native AArch64 build of the banked
Phase 3 code on Volmarr's Raspberry Pi Mimir (paired device
purpleviking1), with an ELF/ABI inspection of the built artifacts.
Every measurement below was run on the Pi in this slice; nothing is
claimed from another machine. Raw tool output is preserved in
`tests/abi/inspection_d002.txt`; the full build-and-test log is
`tests/abi/recorded_run_d002.txt`.

- Base commit: `45b0f5b3872db8dba4c03dad51ff4a1fdecbdd1b`
  (Wave 3 complete, C-009). Branch: `yrsa/mimiralloc-02`.
- Build run: 2026-11-06T15:27:26-05:00 (America/Indiana/Indianapolis).
- Scope note: the public header `include/mimir_alloc.h` declares the
  Phase 3 surface "Proposed" and expressly *not* a stable ABI. This
  document is an inspection snapshot of what the banked code actually
  produces on this target, not an ABI freeze (see INC-D002-07).

## 1. Device manifest (all values read on the Pi)

| Item | Value (as read on device) |
| --- | --- |
| Host | purpleviking1 (Raspberry Pi), user `pi` |
| CPU | ARM Cortex-A76, 4 cores, r4p1, `aarch64`; features include fp, asimd, aes, sha1, sha2, crc32, atomics, lrcpc |
| OS | Debian GNU/Linux 12 (bookworm), VERSION_ID 12 |
| Kernel | `Linux purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64 GNU/Linux` (16K-page kernel build) |
| Compiler | `cc` = gcc (Debian 12.2.0-14+deb12u1) 12.2.0 |
| Linker / binutils | GNU ld 2.40; readelf/nm/ar GNU Binutils 2.40; file-5.44 |
| glibc | 2.36 — `ldd (Debian GLIBC 2.36-9+rpt2+deb12u14) 2.36`; `getconf GNU_LIBC_VERSION` = `glibc 2.36` |
| Runtime page size | `getconf PAGESIZE` = 16384; `getconf PAGE_SIZE` = 16384; `sysconf(_SC_PAGESIZE)` = 16384 (probe, §5) |
| Memory at build | 15 GiB total, 13 GiB available (`free -g` before and after the build); build kept small, Samtal not starved |
| Build flags (banked Makefile) | `-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc` — note: no `-fPIC`/`-fpic` (INC-D002-03) |

## 2. Build and test run

`make clean && make test`, exactly as the banked Makefile defines it,
exit code 0. Full verbatim output: `tests/abi/recorded_run_d002.txt`
(639 lines). All seven test binaries report PASS:

| Binary | Result recorded in the run |
| --- | --- |
| build/test_smoke | SMOKE 18/18 checks passed |
| build/test_core | CHECKS: 29 run, 0 failed — RESULT: PASS |
| build/test_b007 | CHECKS: 70 run, 0 failed — RESULT: PASS |
| build/test_c005 | CHECKS: 95 run, 0 failed — RESULT: PASS |
| build/test_c006 | CHECKS: 23 run, 0 failed — RESULT: PASS |
| build/test_c007 | SUITE cases: 40 passed, 0 failed, 70 skipped, 9 inspected (of 119 registered); CHECKS: 122 run, 0 failed — RESULT: PASS; x86-64 run OWED, Phase 3 exit UNMET (as the suite itself states) |
| build/test_c008 | CHECKS: 42 run, 0 failed, 7 skipped-not-yet — RESULT: PASS |

## 3. ELF identity of the artifacts

From `file` and `readelf -h` (raw output in the inspection file):

- Library objects `build/mimir_alloc.o`, `build/mimir_core.o`,
  `build/mimir_platform.o`: ELF 64-bit LSB relocatable, ARM aarch64,
  version 1 (SYSV), not stripped. Class ELF64, little endian,
  Machine `AArch64`, Type REL.
- Test executables (e.g. `build/test_smoke`): ELF 64-bit LSB pie
  executable, ARM aarch64, dynamically linked, interpreter
  `/lib/ld-linux-aarch64.so.1`, Type DYN (PIE), for GNU/Linux 3.7.0
  (`.note.ABI-tag`: OS Linux, ABI 3.7.0), with GNU Build IDs (listed
  in the raw inspection file).
- Slice-built inspection libraries (§4): `build/libmimiralloc.so` is
  ELF64 LSB shared object, AArch64, Type DYN; `build/libmimiralloc.a`
  is an `ar` archive of the same three objects. Both live only in
  `build/` (untracked, removed by `make clean`); they are inspection
  artifacts, not deliverables (INC-D002-01).

Dynamic section facts (test_smoke and the inspection .so alike):
`NEEDED` is exactly one library, `libc.so.6`; `ldd` resolves only
`linux-vdso.so.1`, `libc.so.6` (`/lib/aarch64-linux-gnu/libc.so.6`)
and `/lib/ld-linux-aarch64.so.1`. No TEXTREL in either artifact.
A GNU_RELRO segment and a non-executable GNU_STACK (RW) are present.
No FLAGS / BIND_NOW entries were emitted, and no `.note.gnu.property`
exists in any artifact (INC-D002-04).

## 4. Symbol surface — the four public symbols, and what else shows

Public surface declared in `include/mimir_alloc.h`:
`mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc`.

- **API translation unit: exactly the 4 public symbols.**
  `nm -g --defined-only build/mimir_alloc.o` lists exactly
  `mimir_malloc` (28 bytes), `mimir_free` (32), `mimir_calloc` (36),
  `mimir_realloc` (36), all type `T`, and nothing else. The C-007
  suite's UNS-031 audit case records the same fact from its run.
- **Test executables export nothing dynamically.**
  `nm -D --defined-only build/test_smoke` is empty: the statically
  linked executables define no dynamic symbols.
- **A shared library built naively from the library objects exports
  13 functions, not 4** (INC-D002-02). The core object defines
  `mimir_core_alloc`, `mimir_core_calloc`, `mimir_core_free`,
  `mimir_core_realloc`, and the platform object defines
  `mimir_platform_page_size`, `mimir_platform_map_acquire`,
  `mimir_platform_map_release`, `mimir_platform_global_lock`,
  `mimir_platform_global_unlock` — all with default visibility.
  `nm -D --defined-only build/libmimiralloc.so` therefore shows
  13 `T` symbols: the 4 public names plus these 9 internal names.
  Nothing in the banked build hides them (no `-fvisibility=hidden`,
  no version script, no library target at all).

Undefined (libc) symbols used by the library objects (`nm -u`):

- `mimir_alloc.o`: only the four `mimir_core_*` functions.
- `mimir_core.o`: `memcpy`, `memset`, plus the `mimir_platform_*` set.
- `mimir_platform.o`: `mmap`, `munmap`, `sysconf`,
  `pthread_mutex_lock`, `pthread_mutex_unlock`.

In the shared object every undefined libc symbol is versioned
`@GLIBC_2.17` (memcpy, memset, mmap, munmap, sysconf,
pthread_mutex_lock, pthread_mutex_unlock, plus weak
`__cxa_finalize@GLIBC_2.17`) — the AArch64 glibc baseline. The test
executables additionally reference `__libc_start_main@GLIBC_2.34`,
`abort@GLIBC_2.17` and `printf@GLIBC_2.17` from the harness/CRT code
(INC-D002-06). Exported symbols carry no symbol-version nodes of
their own (unversioned exports).

## 5. Alignment notes (measured)

- AArch64 instruction alignment: `.text` in `mimir_alloc.o` has
  section alignment 4; all four public functions sit at 4-byte
  aligned offsets (0x0, 0x1c, 0x3c, 0x60).
- Segment alignment: both LOAD segments of the executables and of
  the inspection .so carry `p_align` 0x10000 (64 KiB) — the linker's
  default maximum page size, not this host's page size
  (INC-D002-05). 64 KiB is a multiple of the 16 KiB runtime page, so
  the artifacts load correctly on this 16K-page kernel.
- Data alignment contract: `alignof(max_align_t)` = 16 on this
  target (`sizeof(max_align_t)` = 32), measured by the slice probe.
- Allocation alignment, measured by the probe
  (`tests/abi/recorded_run_d002.txt`): pointers returned by
  `mimir_malloc` for sizes 1, 16, 100, 4096, 16384 and 65536, and by
  `mimir_calloc(8, 8)`, were all 0 mod 16 and in fact 0 mod 16384 —
  this implementation returns page-aligned storage (its mappings are
  whole 16384-byte pages), which is strictly stronger than the
  header's non-numeric "suitably aligned" claim (U-15 stays open;
  the probe is an observation of this build, not a contract change).
- Relocations in `mimir_alloc.o` are `R_AARCH64_CALL26` (to the core
  functions) and `R_AARCH64_PREL32` (.eh_frame); the platform object
  also uses PC-relative `R_AARCH64_ADR_PREL_PG_HI21` +
  `R_AARCH64_ADD_ABS_LO12_NC` pairs for its `.bss` lock. No absolute
  text relocations were observed, and the ad-hoc .so has no TEXTREL
  (INC-D002-03).

## 6. Inconsistencies and observations (INC-D002-*)

- **INC-D002-01 — No library target exists.** The banked Makefile
  builds only test executables from the three library objects; there
  is no `.a`/`.so` rule, no SONAME, no install story. The `.a`/`.so`
  inspected here were built by this slice inside `build/` for
  inspection only. A future library slice must add the target.
- **INC-D002-02 — Shared-library surface would leak 9 internal
  symbols.** "Exactly the 4 public symbols" holds for the API object
  and for the (non-exporting) executables, but a shared library
  linked from the banked objects exposes 13 dynamic symbols
  (§4). Remedy for the future library slice: hidden visibility for
  `mimir_core_*` / `mimir_platform_*` or a version script exporting
  only the 4 public names.
- **INC-D002-03 — PIC is incidental, not specified.** `CFLAGS`
  carries no `-fPIC`/`-fpic`. On this toolchain the emitted code is
  PC-relative in practice (CALL26 / ADR_PREL / PREL32 only) and the
  ad-hoc .so linked with no TEXTREL, but the build does not
  guarantee position-independent code for a library target.
- **INC-D002-04 — No BTI/PAC property note.** No artifact carries
  `.note.gnu.property`; the toolchain was not asked for branch
  protection (`-mbranch-protection` absent). Observation only on
  this Cortex-A76 host; no functional claim is affected.
- **INC-D002-05 — Segment alignment 64 KiB vs 16 KiB host pages.**
  Linker default `p_align` 0x10000 against a 16384-byte runtime
  page. Compatible (multiple), not host-matched; recorded so a
  future packaging slice can set `-z max-page-size` deliberately.
- **INC-D002-06 — glibc floor split.** Library code needs only
  GLIBC_2.17 symbols, but executables produced by this toolchain
  require GLIBC_2.34 via `__libc_start_main` (Debian 12 CRT). An
  executable built here will not run on a glibc 2.17–2.33 system
  even though the allocator's own floor is 2.17.
- **INC-D002-07 — Unversioned exports; ABI expressly not stable.**
  The inspection .so's exports carry no version nodes, matching the
  header's position that this surface is Proposed and not a stable
  ABI. Recorded so this snapshot is never cited as an ABI freeze.
  Minor companion fact: the repo has no `.gitignore`, so `build/`
  artifacts are untracked noise in `git status`; the durable
  evidence for this slice is the two files under `tests/abi/`, not
  the binaries.

## 7. Evidence files for this slice

- `tests/abi/recorded_run_d002.txt` — device manifest summary,
  `make clean`, the full `make test` output (exit code 0), and the
  alignment probe source result, all from this Pi.
- `tests/abi/inspection_d002.txt` — raw `file`, `readelf`
  (-h/-l/-S/-d/-V/-n/-r, --dyn-syms), `nm`, `ldd` and `ar` output
  for the objects, executables and inspection libraries, with
  sha256sums of the built artifacts.
- This document — the summary and the INC-D002-* register.

README.md was not touched by this slice.

## 8. Tester verification

Tester pass: **PASS** (independent tester worker, 2026-11-06). Rebuilt from clean on this Pi: `make clean && make test` returned exit code 0 and all counts reproduced (smoke 18/18; core 29; b007 70; c005 95; c006 23; c007 SUITE 40 passed / 70 skipped / 9 inspected plus CHECKS 122; c008 42 run / 7 skipped-not-yet). Independent re-inspection reproduced every ELF/symbol fact in this document (4 API symbols; 13 `.so` dynsym entries; AArch64 ELF64; PAGESIZE 16384; no TEXTREL; no SONAME; LOAD p_align 0x10000) and found the recorded artifact sha256sums in `tests/abi/inspection_d002.txt` byte-identical to a fresh clean rebuild (deterministic build). README.md and all tracked files unmodified. Discrepancies: none.
