# libc / Alignment Compatibility — Slice D-008 (Wave 4)

Slice: **D-008 (team-08)**. Wave 4, Phase 4 ARM64 and Android
foundation. Deliverables: this document,
`tests/compat/test_d008.c` (alignment / compatibility probe),
the recorded run `tests/compat/recorded_run_d008.txt`, and an
additive `test-d008` Makefile target (standalone; `make test`
composition unchanged). Base: `4f2fbaa1fa89dead6cff81ffb976c26bc078cbf0`
(D-007 banked) on branch `yrsa/mimiralloc-08`, worktree
`/home/pi/mimiralloc-team-08` on Raspberry Pi `purpleviking1`.
Governing text: Roadmap Sections 5 (supported-target matrix),
8 Phase 4 ("Inspect ELF properties, Android API level, libc
usage and alignment"), and 12 (Android checklist); the A-008
contract in `docs/contracts.md` as carried by
`include/mimir_alloc.h`; banked evidence D-002 (ELF/ABI
inspection), D-003 (page-size proof), D-005 (cross-compilation),
D-006 (Android / bionic plan). `README.md` was not touched.

**Claim discipline.** Every claim below carries one label:
**PROVEN on the Pi** (run or inspected on this host in a
banked slice, with the evidence file named), **by-analogy**
(the position the banked contracts take for the namespaced
symbols, or the documented behaviour of a libc that is not
installed here), or **OWED** (no evidence exists yet; the
smallest next step is named). Nothing about musl or bionic
behaviour is stated as measured: neither libc exists on
this Pi (Section 5, re-verified in this slice).

> **VERDICT (plain first).** On the one libc actually
> present — glibc 2.36, AArch64, 16 KiB pages — the banked
> allocator is alignment-compatible with room to spare:
> every pointer it returned in this slice's probe, across
> 33 sizes and the calloc / realloc paths, was aligned to
> `_Alignof(max_align_t)` = 16, and in fact every one was
> page-aligned (Section 2, PROVEN). The real compatibility
> differences are behavioural, not geometric: zero-size
> calls return NULL where glibc `malloc(0)` returns a
> pointer (INC-D008-01), and allocation failure leaves
> `errno` untouched where glibc sets `ENOMEM`
> (INC-D008-02) — both deliberate, documented postures
> under the still-open contract items U-09 and U-10, not
> defects, but exactly the differences a caller ported
> from any of the three libcs would observe. The musl leg
> is **BLOCKED** (no musl toolchain on the Pi; install is
> Volmarr's word, not this slice's) and the bionic leg is
> **OWED** (no NDK / sysroot, D-006; no device run,
> D-007). **Phase 4 exit: UNMET** — the physical Android
> run is still owed in full.

## 1. Checks performed on the Pi in this slice

Host first, load kept light: `free -g` showed 13 GiB
available of 15 GiB before and after all work; Samtal
(127.0.0.1:8787) answered HTTP 200 after the runs (recorded
run Section 3). Host: Linux 6.12.112-v8-16k-hailo+, aarch64,
16,384-byte pages, 4 cores. Nothing was installed; no
Android device was contacted.

- HEAD verified `4f2fbaa...` on `yrsa/mimiralloc-08` before
  any work; base matches the D-007 bank.
- Source audit (Section 4): every file under `src/` and
  `include/` read or pattern-searched for libc-specific
  facilities, `errno` use, feature-test macros, alignment
  and page assumptions.
- Probe: `make test-d008`, exit 0 — **CHECKS: 163 run,
  0 failed; RESULT: PASS** (recorded run Section 1). The
  probe's ALLOCATIONS 43 / FREES 41 differ by exactly the
  2 realloc grows, whose old blocks are retired inside
  `mimir_realloc` rather than through `mimir_free`; the
  probe counts only its own explicit calls.
- Regression: `make clean && make test`, exit 0 — all
  eight suites RESULT: PASS (smoke 18/18; core 29; B-007
  70; C-005 95; C-006 23; C-007 suite 40 passed / 0 failed
  / 70 skipped / 9 inspected of 119, checks 122; C-008 42
  with 7 skipped-not-yet; D-004 4,590). `make test`
  composition was not changed by this slice (recorded run
  Section 2).
- Toolchain re-verification (Section 5): `musl-gcc`,
  `musl-clang`, `aarch64-linux-musl-gcc`, musl header /
  library paths, `adb`, `ndk-build`,
  `aarch64-linux-android*-clang`, NDK directories — all
  absent, commands and exit codes in recorded run
  Section 0.

## 2. AArch64 alignment verification (measured on the Pi)

The contract expectation, from `include/mimir_alloc.h` by
analogy to MA-6.1-MALLOC-01: a successful allocation is
"suitably aligned for any object type with a fundamental
alignment requirement". No numeric value is claimed
(U-15 / V-07 open). The operational check, as in D-002 and
D-007, is `_Alignof(max_align_t)` from `<stddef.h>`.

Measured host facts (probe ENV lines, recorded run
Section 1; consistent with D-002 Section 5):

| Fact | Value | Label |
| --- | --- | --- |
| `sysconf(_SC_PAGESIZE)` | 16384 | PROVEN on the Pi |
| `_Alignof(max_align_t)` | 16 | PROVEN on the Pi |
| `sizeof(max_align_t)` | 32 | PROVEN on the Pi |
| Pointer width | 64 bits | PROVEN on the Pi |
| Host libc | glibc 2.36 (`__GLIBC__=2`, `__GLIBC_MINOR__=36`) | PROVEN on the Pi |

Probe results (all PROVEN on the Pi, recorded run
Section 1):

| Path | Cases | Aligned to 16 (max_align_t) | Notes |
| --- | --- | --- | --- |
| `mimir_malloc` ladder | 33 sizes: 1, 2, 3, 5, 7, 8, 15, 16, 17, 24, 31, 32, 33, 63, 64, 65, 100, 127, 128, 255, 256, 1000, 1024, 4095, 4096, 4097, 8192, 16383, 16384, 16385, 65536, 100000, 262144, all live at once | 33 / 33 | Also pairwise distinct; **33 / 33 were 0 mod 16384** (page modulus, observation) |
| `mimir_calloc` | (1,1), (3,7), (16,32), (100,100), (1,65537) | 5 / 5 | Full requested extent verified zero at return, 5 / 5 |
| `mimir_realloc` | grow 37 -> 1000, grow -> 70000, shrink -> 13, `realloc(NULL, 41)` | 4 / 4 steps | Prefix preserved byte-for-byte at every step (37, 1000 and 13 bytes checked respectively) |
| Host `malloc` / `calloc` / `realloc` (glibc, contrast only) | same 33-size ladder + calloc (3,7) + realloc 37 -> 1000 | 35 / 35 | Host-libc fact for this host only; the comparison baseline, not a MimirAlloc claim |

Why the page modulus appears: `alloc_locked` in
`src/core/mimir_core.c` returns `mapping_base +
usable_offset` with `usable_offset` exactly one system
page (B-006 S7.1), and the mapping base is page-aligned
by the OS. Page alignment is therefore a construction
fact of this mapping-per-allocation baseline on any
page size — and an accident a future redesign could
silently weaken (INC-D008-08). The contract floor the
probe enforces is 16, and 16 is what a caller may rely
on; nothing stronger is claimed. D-002's smaller probe
(6 malloc sizes + 1 calloc, all 0 mod 16384) is
consistent with and subsumed by this run.

Alignment expectations for the other libcs (no local
evidence; labelled accordingly): on AArch64 the PCS gives
`long double` a 16-byte alignment, so
`_Alignof(max_align_t)` = 16 is expected under musl and
under bionic as it is under glibc — **by-analogy** for
both, becoming PROVEN only when the probe is built and
run under each libc (Sections 5, 6). The system
allocators of all three libcs return 16-byte-aligned
storage on 64-bit targets as their documented baseline
(glibc leg PROVEN by Phase D above; musl and bionic legs
**by-analogy**).

## 3. Per-symbol contract differences: glibc vs musl vs bionic

The four public symbols are `mimir_malloc`, `mimir_free`,
`mimir_calloc`, `mimir_realloc` (`include/mimir_alloc.h`).
The namespaced API has no direct contract rows (U-03
open); every MimirAlloc behaviour below is the banked
position **by analogy** to the core-symbol rows, and its
Pi evidence is the glibc-host run of this slice (plus
D-002 / D-004 where named). The libc columns describe the
*corresponding system allocator entry points* a caller
would be porting from — the differences that matter when
the same calling code meets MimirAlloc instead.

### 3.1 Alignment guarantees

| | Position |
| --- | --- |
| MimirAlloc (all targets) | Non-numeric "suitably aligned" only (U-15 open). Measured on glibc/Pi: 16-modulus always, page-modulus in this construction (Section 2, PROVEN). No explicit-alignment entry point exists (U-11 open): no `aligned_alloc` / `posix_memalign` analogue is declared, so code needing over-alignment has no MimirAlloc route on any libc — absence at link time, the B-008 S6.1 unsupported posture. |
| glibc | `malloc` family returns 16-byte-aligned storage on AArch64 (PROVEN, probe Phase D). Explicit-alignment entry points exist with glibc-specific validity rules (V-03). |
| musl | Same 16-byte AArch64 baseline expected **by-analogy**; musl's own allocator alignment is an internal detail no banked document records. No musl build exists to check MimirAlloc against it (BLOCKED, Section 5). |
| bionic | Same 16-byte baseline expected **by-analogy** for 64-bit; bionic documents 16-byte alignment for `malloc` on 64-bit. Verification is **OWED** to the NDK build + device run (D-006 / D-007). |

Difference that matters: none for ordinary allocations —
all four agree at the fundamental-alignment floor. The
gap is the missing explicit-alignment surface, identical
on every libc because it is a MimirAlloc scope decision,
not a libc difference.

### 3.2 Zero-size behaviour

| Call | MimirAlloc (posture; U-09 / U-12 unresolved) | glibc | musl | bionic |
| --- | --- | --- | --- | --- |
| `malloc(0)` analogue | `mimir_malloc(0)` -> NULL (OBSERVED on Pi, probe Phase E) | `malloc(0)` -> non-null distinct pointer (PROVEN on Pi, probe Phase E) | Returns a distinct non-null pointer **by-analogy** (musl documented behaviour) | Returns a distinct non-null pointer **by-analogy** | 
| `calloc(0, n)` / `calloc(n, 0)` | NULL (OBSERVED on Pi, both factor orders) | Non-null distinct pointer (PROVEN on Pi: return values measured in this slice — `calloc(0,8)` and `calloc(8,0)` both non-null; the measuring snippet was run ad hoc and is quoted in Section 6, INC-D008-01) | Non-null **by-analogy** | Non-null **by-analogy** |
| `realloc(p, 0)`, live p | NULL, original block left LIVE and unchanged (OBSERVED on Pi; byte check in probe Phase E) | Returns NULL (PROVEN on Pi: return value measured in this slice); the accompanying free of p is glibc's documented semantic, **by-analogy** for the side effect | Frees p, returns NULL **by-analogy** | Frees p, returns NULL **by-analogy** |
| `realloc(NULL, 0)` | NULL (OBSERVED on Pi) | Non-null (PROVEN on Pi: return value measured in this slice), as `malloc(0)` | Non-null **by-analogy** | Non-null **by-analogy** |

This is the sharpest porting difference in the slice
(INC-D008-01): code written against any of the three
libcs may legally hold and later free a `malloc(0)`
result, and may treat `realloc(p, 0)` as a free. Against
MimirAlloc today the first idiom receives NULL (which
such code often reads as failure) and the second leaves
the block live (which such code reads as already-freed).
Neither MimirAlloc posture settles U-09 / U-12; both are
recorded here so the eventual contract decision is made
with the divergence in view. C23's move to make
`realloc(p, 0)` undefined behaviour narrows the second
gap for future code but changes nothing for existing
callers.

### 3.3 errno / error posture

| | Position |
| --- | --- |
| MimirAlloc | Failure is exactly a NULL return; `errno` is NOT promised (U-10 / V-02). The implementation never writes `errno`: the name appears in `src/` only in comments recording that posture (PROVEN by inspection, Section 4). Probe: `errno` set to 0 before a failing `mimir_malloc(SIZE_MAX)` was still 0 after (OBSERVED, recorded run Section 1) — consistent with the design, and itself not a promise. |
| glibc | `malloc` failure sets `errno = ENOMEM` (PROVEN on Pi: probe Phase E read back 12 = `ENOMEM` after host `malloc(SIZE_MAX)`). |
| musl | `malloc` failure sets `ENOMEM` **by-analogy** (musl documented behaviour). |
| bionic | `malloc` failure sets `ENOMEM` **by-analogy** (bionic follows the POSIX posture). |

Difference that matters (INC-D008-02): a caller that
distinguishes failure causes through `errno` — a common
glibc idiom — gets no signal from MimirAlloc on any
target, by design. The NULL return is the whole channel
(B-007 D.0). This is uniform across libcs precisely
because MimirAlloc, not the libc, owns the posture.

### 3.4 Thread-safety assumptions

| | Position |
| --- | --- |
| MimirAlloc | One global `pthread` mutex (L6, statically initialised, `PTHREAD_MUTEX_INITIALIZER`, normal type) serialises all four entry points; metadata only, never user bytes (B-006 S9). PROVEN on glibc/Pi by D-004 (4 threads, 4,590 checks, cross-thread free) and re-exercised by every `make test` since. No async-signal-safety and no fork-safety is claimed (header; U-17..U-21 open). |
| glibc | System `malloc` is thread-safe (MT-Safe); internal locks are fork-registered, so a child of a fork in a multithreaded parent can allocate. PROVEN only in the thread leg (this host runs the suite); the fork leg is glibc documented behaviour, **by-analogy** here. |
| musl | System `malloc` is thread-safe **by-analogy**; musl also maintains its malloc lock across fork **by-analogy**. MimirAlloc under musl would consume musl's `pthread_mutex` primitives — POSIX-identical surface, unverified locally (BLOCKED). |
| bionic | System `malloc` is thread-safe **by-analogy**; bionic registers fork handlers for its allocator locks **by-analogy**. MimirAlloc's pthread use under bionic is **OWED** to the NDK build (D-006 table: every shared dependency, `pthread_mutex_lock` included, binds per API level). |

Difference that matters (INC-D008-05): the fork leg.
All three libcs give their own allocator a defined
post-fork posture; MimirAlloc gives none on any libc —
if a second thread holds the global mutex at `fork()`,
the child's first MimirAlloc call can block on a mutex
that will never be released in the child. That is a
real compatibility gap against all three baselines,
recorded as OWED to a design decision (register
`pthread_atfork` handlers, or document fork as
unsupported in a multithreaded parent), not as a defect
this slice may silently fix: B-008 claims no fork
behaviour, so inventing one here would falsify the
contract record.

## 4. libc-specific assumptions in the banked sources (audit)

Every file under `src/` and `include/` was inspected in
this slice (headers and sources: `include/mimir_alloc.h`,
`src/api/mimir_alloc.c`, `src/core/mimir_core.{c,h}`,
`src/platform/mimir_platform.{c,h}`; pattern searches for
`errno`, feature-test macros, GNU extensions and OS
facilities). Findings, all PROVEN by inspection on the Pi:

1. **The dependency surface is minimal and portable.**
   The only undefined libc symbols in the library objects
   are `memcpy`, `memset`, `mmap`, `munmap`, `sysconf`,
   `pthread_mutex_lock`, `pthread_mutex_unlock` (D-002
   Section 4, PROVEN), all versioned `@GLIBC_2.17` in the
   ad-hoc shared object — the AArch64 glibc baseline.
   Headers included anywhere in `src/` + `include/` are
   only `<stddef.h>`, `<stdint.h>`, `<string.h>`,
   `<sys/mman.h>`, `<unistd.h>`, `<pthread.h>` (this
   slice's grep, recorded in the run log). No GNU-only
   facility is used: no `mremap`, `malloc_usable_size`,
   `mallopt`, `__libc_*`, `sbrk`, `getpagesize`, or
   `pthread_atfork` appears in any production source
   (grep-verified in this slice; the only `fork` hits are
   the header comments disclaiming fork-safety).
2. **`errno` is never written or read by the code.** The
   identifier occurs in production sources only inside
   comments stating the U-10 posture (Section 3.3).
   Porting consequence: nothing in the allocator depends
   on a libc `errno` model (per-thread or otherwise), so
   this is one axis on which glibc, musl and bionic
   cannot differ for MimirAlloc.
3. **The one glibc-flavoured assumption: feature-test
   macros in `mimir_platform.c`.** That file — the only
   TU naming OS facilities — opens with
   `#define _POSIX_C_SOURCE 200809L` and
   `#define _DEFAULT_SOURCE 1`, with a comment noting
   `MAP_ANONYMOUS` is POSIX.1-2024 and that glibc exposes
   it under the default feature set. `_DEFAULT_SOURCE`
   is a glibc (and BSD-lineage) macro name; whether musl
   headers expose `MAP_ANONYMOUS` under exactly this
   macro set, and what bionic headers require, is
   unverified — no musl or bionic header exists on this
   Pi to check (INC-D008-04). The facility set itself
   (`MAP_PRIVATE | MAP_ANONYMOUS` anonymous mappings,
   `sysconf(_SC_PAGESIZE)`, default-type pthread mutex)
   is POSIX surface that all three libcs provide in some
   feature configuration **by-analogy**; the risk is
   confined to macro spelling at compile time, never to
   run-time behaviour, and it will surface as a build
   failure, not a silent divergence, the first time a
   musl or bionic compile is attempted.
4. **No alignment assumption beyond construction.** No
   source literal names an alignment value; the header
   claims none (U-15). The returned alignment follows
   from `usable_offset = page_size` with the page size
   discovered at runtime (D-003, PROVEN) — a construction
   that is libc-independent, since it rests on `mmap`
   page alignment, a kernel property all three libcs
   expose identically **by-analogy** (glibc leg PROVEN).
5. **No initialisation-order or constructor assumption.**
   The global lock is statically initialised
   (`PTHREAD_MUTEX_INITIALIZER`); allocator state is
   zero-initialised statics; no `.init` / constructor /
   `__attribute__((constructor))` use exists. This
   matters for bionic in particular, where library
   initialisation order under the Android linker differs
   from glibc's — MimirAlloc asks nothing of either
   (PROVEN by inspection; the bionic consequence is
   **by-analogy** until an NDK build exists).
6. **Test-side, not allocator-side: glibc header
   attributes leak into test builds.** Probe Phase E
   originally called host `malloc(SIZE_MAX)` directly;
   glibc's `<stdlib.h>` declares `malloc` with an
   alloc-size attribute and GCC's
   `-Walloc-size-larger-than` under `-Werror` refused
   the constant call, while the identical
   `mimir_malloc(SIZE_MAX)` call compiled (MimirAlloc's
   header carries no such attribute). The probe now
   routes the size through a `volatile` variable; the
   run-time call is identical (INC-D008-03). Recorded
   because it proves the *test suite*, unlike the
   allocator, is not automatically libc-neutral.

## 5. musl and bionic availability on the Pi (re-verified)

Re-verified in this slice, 2026-11-06 (commands and exit
codes: recorded run Section 0). This confirms D-005's and
D-006's findings unchanged; nothing was installed.

| Component | Status | Evidence |
| --- | --- | --- |
| `musl-gcc` / `musl-clang` / `aarch64-linux-musl-gcc` | **ABSENT** | `command -v` empty, exit 1 |
| musl headers / libs (`/usr/lib/musl`, `/usr/include/aarch64-linux-musl`, `/usr/aarch64-linux-musl`) | **ABSENT** | `ls` errors, recorded run Section 0 |
| Any musl sysroot | **ABSENT** | path checks + `find` sweep, this slice |
| Android NDK (any path) | **ABSENT** | no `~/android-ndk*`, no `/opt/android*` (also D-006) |
| `adb`, `ndk-build`, `aarch64-linux-android*-clang` | **ABSENT** | `command -v` empty, exit 1 (D-007 recorded exit 127 for `adb version`; `command -v` form here exits 1 — same absence, different shell probe) |
| bionic sysroot / headers | **ABSENT** | no NDK, no sysroot path exists |
| Host libc | glibc 2.36 only | `ldd --version`, `getconf GNU_LIBC_VERSION` (PROVEN) |

**musl leg: BLOCKED.** Smallest next step, reserved for
Volmarr's word (installs are his decision, per D-005):
`apt install musl-tools musl-dev` (Debian bookworm
candidate 1.2.3-1, host-architecture aarch64 musl —
`apt-cache policy` output in recorded run Section 0).
That package serves exactly this row (D-005 INC-D005-06:
it would NOT buy an x86-64 musl road). After install:
build the full suite + this probe with `musl-gcc`, run
both, and Sections 2–3 of this document gain a PROVEN
musl column; the `_DEFAULT_SOURCE` question
(INC-D008-04) is answered by that same compile.

**bionic leg: OWED.** Smallest next step unchanged from
D-006: provision an Android NDK on a host that can run
it (the Pi's ability to run the NDK is itself
INC-D006-02), build via `scripts/android/build_android.sh`,
then the physical SM-X400 run per the D-007 packet.
Until then every bionic cell in Section 3 stays
by-analogy and the Phase 4 exit stays UNMET.

## 6. Inconsistencies recorded (INC-D008-*)

- **INC-D008-01 — Zero-size posture diverges from all
  three libcs.** MimirAlloc returns NULL for
  `mimir_malloc(0)`, either zero `calloc` factor, and
  `mimir_realloc(NULL, 0)`, and leaves the original
  block live for `mimir_realloc(p, 0)` (all OBSERVED on
  the Pi). glibc returns non-null pointers for the
  first three (PROVEN on the Pi: probe Phase E for
  `malloc(0)`; an ad-hoc measuring snippet run in this
  slice printed `calloc(0,8)=non-null
  calloc(8,0)=non-null realloc(live,0)=NULL
  realloc(NULL,0)=non-null` for the host libc) and NULL
  with a free for `realloc(p, 0)` (return PROVEN, free
  by-analogy). musl / bionic legs by-analogy
  (Section 3.2). Not a defect: U-09 / U-12 are open
  contract items and the header documents the posture.
  Recorded so the divergence — and its two caller
  idioms, holding a `malloc(0)` result and treating
  `realloc(p, 0)` as free — is decided knowingly when
  U-09 / U-12 are settled.
- **INC-D008-02 — errno posture diverges from all three
  libcs, by design.** MimirAlloc never touches `errno`
  (inspection, Section 4 item 2; OBSERVED still-0 after
  failure) where glibc sets `ENOMEM` (PROVEN) and musl
  / bionic do so by-analogy. U-10 stays open; callers
  must use the NULL return. Recorded as a standing
  porting note, not a bug.
- **INC-D008-03 — The test suite is less libc-neutral
  than the allocator.** glibc's alloc-size attribute on
  `malloc` makes a constant oversize host call a
  `-Werror` build failure where the MimirAlloc call
  compiles (Section 4 item 6). Fixed in this slice's
  probe (volatile size). Future differential tests
  against a host allocator must expect header-attribute
  friction of this kind on glibc, and unknown
  equivalents under musl / bionic headers (OWED).
- **INC-D008-04 — `_DEFAULT_SOURCE` / `MAP_ANONYMOUS`
  exposure unverified for musl and bionic.** The
  platform TU's feature-macro set is glibc-spelled and
  requests a POSIX.1-2024 facility under a 200809L
  POSIX level (Section 4 item 3). On glibc it compiles
  and runs (PROVEN). For musl and bionic the exposure
  is **OWED** — answerable in one compile each once a
  toolchain exists (Section 5); until then the musl /
  bionic *build* (not behaviour) carries this risk.
- **INC-D008-05 — No fork posture, against all three
  baselines.** Section 3.4: a fork while another thread
  holds the global mutex leaves the child's allocator
  blocked, where glibc / musl / bionic system
  allocators all maintain fork handling for their own
  locks (by-analogy for their internals; the MimirAlloc
  side is PROVEN by inspection — no `pthread_atfork`
  anywhere). OWED to a design decision; U-17..U-21
  remain open. Phase 5+ interposition work must not
  begin before this is decided.
- **INC-D008-06 — The musl target row still has zero
  evidence of any kind.** A-003 recorded "Unevidenced /
  TBD" for Linux AArch64 + musl; D-005 and this slice
  re-verify the toolchain absence, but no MimirAlloc
  musl compile, run, or even header check exists.
  BLOCKED on the Section 5 install step (Volmarr's
  word). Until then the row must not be described as
  anything stronger than Planned.
- **INC-D008-07 — Every bionic cell is by-analogy.**
  Alignment, zero-size, errno and thread rows for
  bionic (Section 3) rest on documentation-level
  analogy and D-006's dependency table, not on a
  bionic build or run; the minimum-API decision that
  would pin the dependency binding is itself OWED
  (INC-D006-04), and the physical run is OWED
  (D-007). Phase 4 exit UNMET.
- **INC-D008-08 — Observed alignment strength is a
  construction accident, not a claim.** Section 2's
  page modulus follows from mapping-per-allocation
  with a one-page reserve; the same construction on a
  4096-page host yields a 4096 modulus, and any future
  size-class, splitting or caching redesign (Phase 7)
  can reduce returned alignment to the 16-byte floor
  without violating any written claim (U-15 open).
  Recorded so no caller, test, or later slice cites
  page alignment as a MimirAlloc property.

## 7. Evidence files for this slice

- `tests/compat/recorded_run_d008.txt` — Section 0:
  host libc facts + musl / bionic re-verification with
  exit codes; Section 1: full `make test-d008` output
  (163 checks, exit 0); Section 2: full `make test`
  output (exit 0); Section 3: Samtal HTTP 200 + memory
  after the runs.
- `tests/compat/test_d008.c` — the probe source (346
  lines; phases ENV / A–E as in its
  header).
- This document — the compatibility record and the
  INC-D008-* register.
- Prior banked evidence relied on: `tests/abi/`
  (D-002), `tests/platform/recorded_run_d003.txt`
  (D-003), `tests/concurrency/recorded_run_d004.txt`
  (D-004), `tests/cross/recorded_run_d005.txt` +
  `recorded_run_d006.txt` (D-005 / D-006),
  `tests/android/recorded_run_d007.txt` (D-007).

README.md was not touched by this slice.

## 8. Phase 4 exit status

**UNMET.** This slice closes the libc-usage and
alignment-inspection part of Roadmap Phase 4 for the
glibc host only. Still owed for the exit: a musl build
and run (BLOCKED, INC-D008-06), an NDK bionic build
(OWED, D-006), and the physical-device allocate / fill /
free and multithread runs on the Samsung SM-X400 with
logs, page size and ABI recorded (OWED, D-007). A
cross-compile alone does not pass the gate, and no
result in this document is an Android or musl result.
