# Page-Size Portability Audit — Slice D-003 (Wave 4)

Base audited: `ae08290` (D-002 banked), branch `yrsa/mimiralloc-03`.
Host where every on-device measurement below was made:
`Linux purpleviking1 6.12.112-v8-16k-hailo+ aarch64`,
`getconf PAGESIZE` = **16384** (measured this run, 2026-11-06).
Governing text: roadmap Sections 5 (supported-target matrix),
7.5 (large allocations and address-space safety: "Discover
actual page size at runtime where appropriate"), and 12
(Android checklist: record `getconf PAGESIZE`, measure, do not
infer). Admitted targets include both 4096-byte and
16384-byte system page sizes.

## Verdict (plain first)

The D-001/D-002 claim is **PROVEN for the allocator itself**:
`src/platform/mimir_platform.c` discovers the system page size
at runtime on every call via `sysconf(_SC_PAGESIZE)`,
failure-checked and admission-checked, and **no production
source or header anywhere in the tree uses a fixed page-size
constant, a `PAGE_SIZE` macro, or `getpagesize()`**. Every
page quantity in the core is derived from that discovered
value (`usable_offset = page_size`, exactly one system page,
B-006 S7.1), and the page round-up in L6 is generic in its
`page_size` parameter with checked arithmetic (B-007 A5/A6).

**One real fixed-assumption defect was found — in a test, not
in the allocator** — and is fixed in this slice:
INC-D003-01 (the `test_c008.c` RLIMIT_AS loop cap was silently
calibrated to large pages; see below). Two further items are
recorded as observations, not defects: INC-D003-02 (the
4096-byte case cannot be *measured* on this Pi — arithmetic
proof only, by necessity) and INC-D003-03 (ELF segment
alignment is a toolchain default, compatible with both
targets).

## Audit method

Every code file in the tree was read or pattern-audited
(17 pre-existing code files, 6362 lines, plus this slice's
new test). Patterns searched across `include/`, `src/`,
`tests/`, `scripts/` and the `Makefile`:

- literals `4096`, `16384`, `8192`, `32768`, `65536`,
  `0x1000`, `0x4000`, `0x10000`;
- `PAGE_SIZE`, `PAGESIZE`, `getpagesize`, `_SC_PAGESIZE`,
  `sysconf`, `SC_PAGE_SIZE`;
- shift/mask alignment arithmetic (`<<`, `>>`, `& ~`) and
  the words `align`/`ALIGN`;
- segment/linker assumptions: `PT_LOAD`, `p_align`,
  linker scripts, `-z max-page-size` / `common-page-size`
  flags (none exist in the tree or the Makefile);
- every remaining numeric literal in production sources was
  classified individually (descriptor layout, sizes, seeds).

Each hit was classified as one of: **(P)** page-size use —
must be runtime-derived; **(R)** ordinary request size in
bytes (never a page claim, MA-6.2-041); **(D)** documentation
mention of a host fact; **(U)** unrelated constant.

## Per-file audit record

| File | Lines | Finding |
|---|---|---|
| `include/mimir_alloc.h` | 171 | Clean. No page constant of any kind. Alignment is promised only as the non-numeric "suitably aligned" (U-15/V-07, lines 101–105, 134); no numeric alignment value is claimed. |
| `src/api/mimir_alloc.c` | 108 | Clean. L1 forwarder; names no OS facility, holds no numeric page quantity. |
| `src/core/mimir_core.c` | 696 | Clean **(P, correct form)**. `alloc_locked` obtains `page_size` from `mimir_platform_page_size()` and sets `usable_offset = page_size` ("exactly one system page, S7.1"); discovery failure is a defined failure with **no fallback constant** (F-11). All other literals classified (U): 96-byte descriptor and its B-006 S7.3 offsets (a fixed *layout*, identical at any page size — the descriptor sits inside the reserve page, it does not size it), descriptor magic, state values. No shift/mask page arithmetic. |
| `src/core/mimir_core.h` | 51 | Clean. No constants. |
| `src/platform/mimir_platform.c` | 156 | Clean **(P, the discovery site)**. Line 34: `queried = sysconf(_SC_PAGESIZE)` on every call, nothing cached; admission: positive, fits `size_t`, power of two, else `ERR_PAGESIZE`. `page_round_up_checked(x, page_size, …)` is fully generic in `page_size` (B-007 A5/A6 checked forms). `mmap`/`munmap` consume only the computed whole-page length. |
| `src/platform/mimir_platform.h` | 104 | Clean. Lines 15–16 mention 16384 and 4096 **(D)** — stated as host facts about admitted targets in the design comment, not used as constants. Interface exposes no page constant. |
| `tests/contract/test_smoke.c` | 191 | Clean. Requests 1, 8, 32, 128, 1024 are **(R)** ordinary byte counts. |
| `tests/contract/test_c007.c` | 1512 | Clean **(P, correct form)**. Header (lines 11–16) states the discipline — no literal system page size; every page quantity from `sysconf(_SC_PAGESIZE)`, failure-checked (line 167, used at 470, 764, 917, 1384). The `8192` at line 1276 is **(R)** a realloc target size in bytes. |
| `tests/core/test_core.c` | 226 | Clean **(P, correct form)**. Page from `sysconf` (line 99); exercised sizes are `page-1 / page / page+1` and multi-page, derived at runtime (comment lines 102–107). `4096` at lines 212–217 is **(R)** a post-free recovery allocation size. |
| `tests/core/test_b007.c` | 483 | Clean **(P, correct form)**. Page from `sysconf` (line 227); expected mapping length computed as `page + page_round_up(n, page)` (line 277); boundary sizes derived from `page` (lines 268–270, 426–429). `4096` at lines 463–469 is **(R)**. |
| `tests/core/test_c005.c` | 558 | Clean **(P, correct form)**. Page from `sysconf` (line 186); grow/boundary cases derived from `page` (lines 210–211, 371, 451). |
| `tests/core/test_c006.c` | 528 | Clean **(P, correct form)**. Page from `sysconf` (line 217); descriptor expectations checked against the runtime `page` (lines 248–251). Descriptor offset literals are **(U)** layout constants as in the core. |
| `tests/core/test_c008.c` | 722 → 732 | **One defect: INC-D003-01 (fixed in this slice).** Otherwise clean: boundary section is runtime-derived (`page-1/page/page+1/2page`, lines 199–285); page from `sysconf` (line 678). `4096`/`8192` request sizes at lines 375–397, 531–574 are **(R)** — *except* the RLIMIT_AS loop cap analysed below. |
| `tests/platform/test_platform.c` | 154 | Clean **(P, correct form)**. Prints and uses the queried page size; `requested = 100` is deliberately not a page multiple **(R)**; mapped-length check is `mapped_length % page_size == 0` against the discovered value. |
| `tests/failure/test_failure_capture.py` | 253 | Clean **(P, correct form)**. Line 248: expected page size is `os.sysconf("SC_PAGE_SIZE")` read from the host at test time; the manifest value must equal it. |
| `scripts/failure_capture.py` | 443 | Clean **(P, correct form)**. Line 162: `os.sysconf("SC_PAGE_SIZE")` recorded as a host fact (failure-tolerant). The `65536` at line 48 is **(U)** a file-read chunk size, unrelated to pages. |
| `scripts/failure_cases/smoke_fail.sh` | 6 | Clean. No numeric or page content. |
| `Makefile` | 107 → 123 | Clean, plus this slice's additive D-003 wiring. No page-size, segment, or linker flags anywhere; plain `cc -std=c11` build. |
| `tests/platform/test_d003.c` | 377 (new) | This slice's proof test. Part A uses only the runtime-discovered page size. Part B uses the literals 4096/16384 **as explicit test inputs**, labelled arithmetic-level only — they are the two admitted target values under examination, not assumptions. |

Recorded-run text files under `tests/` (e.g.
`tests/core/recorded_run_*.txt`, `tests/abi/inspection_d002.txt`)
contain values like `mapping_length == 65536` — those are
**(D)** outputs measured on this 16384-page host during those
runs, not source assumptions; the generating tests compute
their expectations from the runtime page size as shown above.

## The one defect — INC-D003-01 — and its fix

**INC-D003-01 (test portability defect, FIXED in this slice).**
`tests/core/test_c008.c`, S93-2 (RLIMIT_AS controlled-failure
case): the child sets a 96 MiB `RLIMIT_AS` and loops
`mimir_malloc(4096)` expecting an eventual NULL refusal,
capped at 8192 blocks (old lines 349, 374). But the VA cost
of one such allocation is page-size-dependent: reserve page +
usable round-up = **8192 bytes at 4096-byte pages** vs
**32768 bytes at 16384-byte pages**. At the old cap, a
4096-page host could consume at most 8192 × 8192 = 64 MiB of
allocator mappings — *below* the 96 MiB limit — so the
refusal the case exists to observe was not guaranteed there,
and the case could have exited 11 ("no failure observed")
and failed on exactly the target class this audit protects.
On this 16384-page Pi the case passes (32 KiB per block
reaches the limit in ≈3072 blocks), which is why the defect
was invisible here. This is an arithmetic proof, not a
4096-host measurement (see INC-D003-02).

**Fix:** cap and block array raised 8192 → 32768 in
`tests/core/test_c008.c`, with an in-code comment recording
the reasoning. 32768 × 8192 = 256 MiB > 96 MiB guarantees the
refusal is reached at 4096-byte pages, and a fortiori at
16384-byte pages. Proof: `tests/platform/test_d003.c`
section B3 checks this arithmetic against the same checked
round-up formula; the patched `test_c008` was re-run
on-device in the full suite (below) and passes.

## Observations (not defects)

**INC-D003-02 (evidence limit, OWED).** No 4096-byte-page
host exists in this campaign: the Pi runs a 16 KiB-page
kernel (`6.12.112-v8-16k`), and page size is a kernel
property, not settable per process. The 4096-byte case is
therefore proven at the arithmetic/design level only
(test_d003 Part B: checked round-up vs an independent
`unsigned __int128` oracle at both 4096 and 16384, layout
table at both sizes, overflow refusals at both sizes) plus
by this audit's code inspection. A measured 4096-host run of
the suite remains OWED and must not be claimed until a
4096-page target is actually run.

**INC-D003-03 (segment alignment, informational).** The
D-002 ELF evidence (`tests/abi/inspection_d002.txt`) shows
`PT_LOAD` `p_align = 0x10000` (65536) in the built binaries.
That value comes from the aarch64 toolchain's default
maximum page size, not from any MimirAlloc source, header,
Makefile flag, or linker script (none exist). 65536 is a
multiple of both admitted page sizes (4096 and 16384), so
the binaries load correctly on both target classes; no
allocator behaviour depends on it. No action taken; recorded
so a future ABI slice (Phase 5) can revisit it deliberately
rather than discover it accidentally.

## Proof recorded in this slice

`tests/platform/test_d003.c` (new, 377 lines), built and run
on the Pi via the additive Makefile target `make test-d003`;
full output recorded verbatim in
`tests/platform/recorded_run_d003.txt`.

- **Part A [MEASURED ON-DEVICE]** (host page size 16384):
  L6 discovery equals an independent `sysconf(_SC_PAGESIZE)`;
  `map_acquire` returns exactly `round_up(request, runtime
  page)` lengths with runtime-page-aligned bases for requests
  1, page−1, page, page+1, 2page+1, pattern-verified over the
  whole mapping and released with the exact pair; allocator
  level: for requests 1, 100, 4095, 4096, 4097, page−1, page,
  page+1, 32769 the user pointer is runtime-page aligned,
  the descriptor records `usable_offset == runtime page` and
  `mapping_length == round_up(page + request, page)`
  (32768 / 49152 / 65536 observed as appropriate at this
  host's page size), and the requested extent pattern-verifies.
- **Part B [ARITHMETIC-LEVEL ONLY]**: as described under
  INC-D003-02, plus the B3 arithmetic proof of the
  INC-D003-01 fix.
- **Result on-device: 141 checks run, 0 failed — PASS.**
- **Full regression suite on-device (`make test`) after the
  INC-D003-01 fix: PASS** — smoke 18/18; test_core 29;
  test_b007 70; test_c005 95; test_c006 23; test_c007 122
  (0 failed); test_c008 42 run, 0 failed, 7 skipped-not-yet
  (pre-existing skips, unchanged).

## What is NOT claimed

- No run on a 4096-byte-page host (INC-D003-02).
- No Android/bionic page-size behaviour (roadmap Section 12
  measurements remain owed to their own slices).
- No claim about ELF/ABI beyond INC-D003-03's observation.
- README.md was not touched by this slice.
