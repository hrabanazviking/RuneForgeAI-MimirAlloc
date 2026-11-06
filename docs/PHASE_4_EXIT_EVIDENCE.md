# Phase 4 - Exit Evidence Packet

Slice: **D-009 (team-09)**. Wave 4, Phase 4 exit evidence
packet - the last Wave 4 slice. Synthesis only: this slice
adds this packet and its recorded fresh run
(`tests/recorded_run_d009.txt`); it changes no
implementation, no test, and no Makefile line.
Packet base: **8f878afe9ee45efb3ed549219db45d61f9aa7881**
(banked main: D-001..D-008; Wave 4 = 8 of 9 at this base)
on branch `yrsa/mimiralloc-09`, worktree
`/home/pi/mimiralloc-team-09` on Raspberry Pi
`purpleviking1`. Host for every run cited: ARM64 Linux +
glibc - `Linux purpleviking1 6.12.112-v8-16k-hailo+
#1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64
GNU/Linux`, runtime-discovered system page size **16384**
(`getconf PAGESIZE`), 4 cores, compiler
`cc (Debian 12.2.0-14+deb12u1) 12.2.0`, glibc 2.36.
Governing text: Roadmap Section 8, Phase 4, decomposed
in `docs/PHASE_4_FOUNDATION_PLAN.md` (D-001, banked
`8037dd9`) into tasks T1-T13 and exit items E1-E5, read
with Roadmap Sections 2, 5 and 12. Modelled on the banked
Phase 3 packet `docs/PHASE_3_EXIT_EVIDENCE.md` (C-009).
`README.md` was not touched by this slice (verified:
`git diff 8f878af..HEAD -- README.md` is empty).

> **EXIT VERDICT - Phase 4 exit is UNMET.**
> The roadmap Phase 4 exit sentence (Section 8, Phase 4)
> is, verbatim: *"physical-device allocate/fill/free and
> multithread tests pass with logs, system page size and
> ABI recorded. A cross-compile alone does not pass the
> gate."* Under the conservative reading D-001 adopted
> and recorded (INC-D001-06), "physical-device" means the
> physical Android device the phase is about - the
> inventoried Samsung SM-X400 - with the Pi runs as the
> Linux leg. **No MimirAlloc run on any Android device
> exists.** There is no Android NDK on the Pi (D-006,
> live-verified again in this packet's fresh run), no
> Android build of MimirAlloc anywhere in the
> repository, and no device road from this campaign to
> the SM-X400 (no `adb`, D-007). Phase 4 exit therefore
> stays honestly **UNMET** until the physical Android
> allocate/fill/free and multithread logs exist.
>
> The other open items, named plainly: **(a)** the
> 4096-page-host run is OWED (D-003, INC-D003-02 - the
> 4 KiB case is proven arithmetically only; the only
> 4 KiB device is the Android tablet, so this item and
> the Android run are the same missing run seen from two
> sides); **(b)** the x86-64 suite run is OWED (D-005 -
> the RUN leg is proven, the BUILD leg is blocked on a
> missing x86-64 libc sysroot; this is also the Phase 3
> exit debt, so Phase 3 exit remains UNMET too);
> **(c)** musl evidence is BLOCKED (D-008 - no musl
> toolchain on the Pi; the smallest step, a
> `musl-tools`/`musl-dev` install, is reserved for
> Volmarr's word, as are the x86-64 sysroot package and
> any NDK install). Nothing in this packet promotes any
> target to supported (Roadmap Section 5 sense).

## 1. Verdict vocabulary and reading rules

- **MET** - the evidence the item specifies exists, in
  the form it specifies, in the banked repo (or, for the
  fresh-run items, in this packet's recorded run).
- **UNMET** - some evidence exists, but the item as
  written is not satisfied (a required target, leg, or
  half of the item is missing).
- **OWED** - the evidence is wholly absent at this base;
  the row names exactly what is missing and who or what
  it waits on.
- An item whose Pi/Linux leg is held but whose Android
  leg is owed is **UNMET**, never MET; the held leg is
  stated exactly so the partial state is not hidden.
- A cross-compile, a freestanding object, a native Pi
  run of an Android test program, and an emulator result
  are never Android evidence (D-001 E5, in force).
- Counts are copied from the recorded-run files named in
  each row, never restated from memory or from another
  slice's summary. Contract mappings for the namespaced
  `mimir_*` surface are by analogy only, under U-03,
  exactly as in every Wave 4 slice. Nothing here is an
  ABI freeze: the public header declares the surface
  Proposed, not stable (INC-D002-07).

## 2. Fresh verification run for this packet (D-009)

Run once on the Pi in `/home/pi/mimiralloc-team-09` at
base `8f878af` on 2026-11-06 (17:51 EST). Raw combined
log: `tests/recorded_run_d009.txt` (1,097 lines,
75,129 bytes, sha256
`c80ba2869c451028b3ec274540f7be7ab2b791f49dfda62fce513b550b27ada8`).
`free -g` before and after: 15 GiB total, 13 GiB
available - load kept light, Samtal not starved.

Commands and exit codes, exactly as run:

| Command | Exit | Headline counts as logged |
|---|---|---|
| `make clean && make test` | **0** | smoke `SMOKE 18/18 checks passed`; core `CHECKS: 29 run, 0 failed`; B-007 `CHECKS: 70 run, 0 failed`; C-005 `CHECKS: 95 run, 0 failed`; C-006 `CHECKS: 23 run, 0 failed`; C-007 `SUITE cases: 40 passed, 0 failed, 70 skipped, 9 inspected (of 119 registered)`, `CHECKS: 122 run, 0 failed`; C-008 `CHECKS: 42 run, 0 failed, 7 skipped-not-yet`; D-004 `CHECKS: 4590 run, 0 failed`, `ALLOCATIONS: 2088 successful, 2088 frees` - every suite `RESULT: PASS` |
| `make test-d003` | **0** | `CHECKS: 141 run, 0 failed` - `RESULT: PASS` (Part A measured at host page size 16384; Part B arithmetic-only against the `__int128` oracle at 4096 and 16384) |
| `make test-d005` | **0** | `FREESTANDING PROBE: PASS (compile + link + run, exit 42 verified twice)` - x86-64 freestanding probe compiled with `clang --target=x86_64-linux-gnu`, linked `ld.gold -m elf_x86_64`, run via binfmt and via explicit Box64, output `D005-X64-PROBE-OK` both times. This is the RUN leg only; it proves nothing about running the allocator on x86-64 |
| `make test-d006` | **0** | Probe complete, exit 0: `ndk-build`, `sdkmanager`, every `aarch64-linux-android*-clang`, `adb`, and every NDK/SDK path **ABSENT**; native `cc`/`gcc`/`clang` PRESENT. **There is no Android NDK on this host - re-verified live in this run, not copied from D-006** |
| `make test-d007` | **0** | `CHECKS 59 run, 0 failed`; `ALLOCATIONS 58 FREES 58`; `RESULT: PASS`; in-run `ENV PAGESIZE=16384 (sysconf, measured in this run)`. **Native logic check on this Pi only - NOT an Android result** (Section 5, item D-007) |
| `make test-d008` | **0** | `CHECKS: 163 run, 0 failed` - `RESULT: PASS` (alignment probe, Section 5, item D-008) |

Every fresh count is identical to the corresponding
banked recorded run for D-003, D-004, D-007 and D-008,
and to the banked `make test` composition counts. The
fresh run adds no new claim; it confirms the banked
evidence still reproduces at this packet's base.

## 3. Banked evidence inventory (D-001..D-008)

All commits are banked main commits; counts are as
logged in the named file.

| Slice | Banked commit | Evidence files | Recorded result (as logged) |
|---|---|---|---|
| D-001 foundation plan + target evidence matrix | `8037dd9` | `docs/PHASE_4_FOUNDATION_PLAN.md` (418 lines) | T1-T13 and E1-E5 decomposed and mapped to Pi-native / cross-compile / physical Android / OWED; no target supported per Section 5; Section 12 checklist adopted as governing; Phase 4 exit UNMET at its base. INC-D001-01..08 |
| D-002 native AArch64 build + ELF/ABI inspection | `ae08290` | `docs/AARCH64_ELF_ABI_INSPECTION.md` (201 lines), `tests/abi/recorded_run_d002.txt`, `tests/abi/inspection_d002.txt` | `make test` exit 0, all 7 binaries PASS at its base; API object exports exactly the 4 public symbols; ELF64 AArch64, `NEEDED` exactly `libc.so.6`, no TEXTREL, LOAD `p_align` 0x10000; undefined libc symbols all `@GLIBC_2.17`; ad-hoc shared library leaks 13 symbols (INC-D002-02). INC-D002-01..07 |
| D-003 page-size portability audit + proof | `f5965c1` | `docs/PAGE_SIZE_PORTABILITY.md` (182 lines), `tests/platform/test_d003.c`, `tests/platform/recorded_run_d003.txt` | Audit of all 17 code files (6,362 lines): runtime `sysconf(_SC_PAGESIZE)` discovery proven, no fixed page-size constant in any production source/header; one test defect found and fixed (INC-D003-01); proof test 141 checks PASS at host 16384, Part B arithmetic-only at 4096 + 16384. INC-D003-02 (4096 host run OWED), INC-D003-03 |
| D-004 physical multithread run, Pi leg | `749dd04` | `docs/PHASE_4_MULTITHREAD_RUN.md` (184 lines), `tests/concurrency/test_d004.c`, `tests/concurrency/recorded_run_d004.txt` | 4 threads; phases A private churn / B cross-verified live set (64 distinct) / C cross-thread free / D shared hammer with realloc grows; `CHECKS: 4590 run, 0 failed`, `ALLOCATIONS: 2088/2088`, `RESULT: PASS` standalone and in full `make test`. INC-D004-01..05 (Android half OWED) |
| D-005 cross-compilation + CI scripts | `18028d2` | `docs/PHASE_4_CROSS_COMPILATION_AND_CI.md` (396 lines), `tests/cross/recorded_run_d005.txt`, `scripts/cross/`, `.github/workflows/cross-x86_64.yml` (PROPOSED) | x86-64 road HALF BUILT: RUN leg proven (Box64 0.4.5 + binfmt registered; freestanding probe run exit 42 twice; Wine loader runs); BUILD leg blocked - no x86-64 libc sysroot, 2 of 3 library TUs fail at missing libc headers; smallest next step `gcc-x86-64-linux-gnu` install reserved for Volmarr's word; workflow proposed, manual-only, UNEXECUTED. INC-D005-01..06 |
| D-006 Android NDK readiness + build plan | `761739b` | `docs/PHASE_4_ANDROID_BUILD_PLAN.md` (411 lines), `tests/cross/recorded_run_d006.txt`, `scripts/android/probe_android_toolchain.sh` (EXECUTED), `scripts/android/build_android.sh` (PROPOSED) | NDK ABSENT live-verified (probe executed + recorded, exit 0); E1: API TU compiles freestanding for `aarch64-linux-android24`, object defines exactly the 4 public symbols; E2: hosted TUs fail for want of a bionic sysroot; E3: `adb` absent; build script preflight BLOCKED exit 3; API-level/bionic contract by analogy only (U-03 open); minimum-API decision OWED. INC-D006-01..06 |
| D-007 Android physical-device test packet | `4f2fbaa` | `docs/PHASE_4_ANDROID_DEVICE_PACKET.md` (477 lines), `tests/android/test_d007.c`, `tests/android/recorded_run_d007.txt` | Section 12 checklist applied item-by-item to the recorded SM-X400 inventory: **0 of 10 satisfied by run evidence**, every run-time record OWED; on-device program ready (4 public symbols, in-run ENV page-size record, phases A-E bounded: 2 threads, sizes at most 8192, 58 allocations); native logic check on Pi PASS (59 checks, 58/58) - NOT an Android result; Android-target compiles fail honestly (D-006 E2 mode); `adb` absent exit 127. INC-D007-01..07 |
| D-008 libc/alignment compatibility | `8f878af` | `docs/LIBC_ALIGNMENT_COMPATIBILITY.md` (464 lines), `tests/compat/test_d008.c`, `tests/compat/recorded_run_d008.txt` | Probe 163 checks 0 failed: 33-size `mimir_malloc` ladder + calloc/realloc paths all aligned to `_Alignof(max_align_t)` = 16 and in fact page-aligned (construction fact, INC-D008-08); host glibc ladder 35/35 for contrast; source audit: only `memcpy`/`memset`/`mmap`/`munmap`/`sysconf`/`pthread` deps, `errno` never touched, one glibc-spelled assumption (INC-D008-04); zero-size posture diverges from all three libcs (INC-D008-01), errno posture by design (INC-D008-02), no fork posture (INC-D008-05); musl ABSENT - BLOCKED (INC-D008-06); bionic/NDK/`adb` ABSENT - OWED (INC-D008-07). INC-D008-01..08 |
| D-009 this packet | (this slice) | `docs/PHASE_4_EXIT_EVIDENCE.md`, `tests/recorded_run_d009.txt` | Fresh run in Section 2 |

## 4. Exit gate, item by item (D-001 E1-E5)

| # | Exit item (roadmap wording, in substance) | Evidence | Verdict |
|---|---|---|---|
| E1 | Physical-device allocate/fill/free tests pass, with logs | Pi leg HELD: D-004 recorded run (4,590 checks, 2,088/2,088, PASS) and every Phase 3/4 Pi suite in Section 2. Android leg: **no run exists** - no NDK build (D-006), no device road (D-007), the ready program in `tests/android/test_d007.c` has never executed on any Android device | **UNMET** - Android leg OWED |
| E2 | Physical-device multithread tests pass, with logs | Pi leg HELD: D-004 phases A-D including cross-thread free, in `make test` and standalone, re-run identically in Section 2. Android leg: **no run exists** - D-007's bounded Phase E (2 threads, cross-thread free) is designed, native-checked, and unrun on Android (INC-D007-03) | **UNMET** - Android leg OWED |
| E3 | System page size recorded (for the exit runs) | Pi exit runs: 16384, measured in-run (`sysconf`, failure-checked) in D-003, D-004, D-007's native check, D-008 and Section 2 of this packet. Android exit run: **no run, no record** - the tablet's 4,096 rests on one field-notes probe whose command is not named (INC-D001-08, carried as INC-D007-05); D-007's program self-records page size in-run precisely so the future run produces this record | **UNMET** - Android run record OWED |
| E4 | ABI recorded (for the exit runs) | Linux/glibc half HELD: D-002 ELF/ABI inspection (ELF64 AArch64, 4 public symbols in the API object, `libc.so.6` only, GLIBC_2.17 floor for library symbols, `p_align` 0x10000, header surface Proposed not stable). Android half: **cannot exist yet** - arm64-v8a / bionic / target-API ELF, interpreter and dependency record all wait on the D-006 build | **UNMET** - Android ABI record OWED |
| E5 | The negative gate: a cross-compile alone does not pass | Rule IN FORCE and honoured by every Wave 4 slice: D-005's proven RUN leg and D-006's freestanding E1 object were each recorded as explicitly not passing any gate; no slice in Wave 4 claims an exit on build evidence | **MET** (as a rule: in force, and breached nowhere in the banked record) |

**Exit tally: MET 1 / UNMET 4 / OWED 0, of 5** - and the
four UNMET items each carry an OWED Android leg, which
is the substance of the verdict above. **Phase 4 exit:
UNMET.**

## 5. Task map, item by item (D-001 T1-T13)

| # | Task | Evidence | Verdict |
|---|---|---|---|
| T1 | Create native AArch64 builds | D-002 recorded clean build + full device manifest at its base; reproduced at this base in Section 2 (`make test` exit 0, all 8 suites PASS) | **MET** (Pi-native, recorded) |
| T2 | Create Android NDK builds | D-006: NDK absent live-verified at D-006 and again in Section 2 (`make test-d006` exit 0, every component ABSENT); build plan + proposed script banked, preflight BLOCKED exit 3; E1 freestanding object is not a build (INC-D001-05 discipline) | **OWED** - missing: an Android NDK; install reserved for Volmarr's word |
| T3 | CI cross-compilation | D-005: road HALF BUILT (RUN leg proven, BUILD leg blocked on the missing x86-64 libc sysroot); `.github/workflows/cross-x86_64.yml` PROPOSED and UNEXECUTED, manual-only by standing rule; no CI run of any kind exists (INC-D001-04 stands: a local `make test` is not CI) | **UNMET** |
| T4 | Physical tests (umbrella for the exit runs) | Pi leg produced (D-004; E1/E2 Pi halves). Android leg wholly absent (D-007 packet: program ready, run OWED; no `adb`, exit 127) | **UNMET** - Android leg OWED |
| T5 | Inspect ELF properties | D-002: recorded `file`/`readelf`/`nm`/`objdump`/`ldd` inspection of objects, executables and inspection libraries (`tests/abi/inspection_d002.txt`), including the 13-symbol shared-library leak (INC-D002-02) | **MET** (for the native build; Android/cross ELF records wait on T2/T3 and are graded there) |
| T6 | Inspect Android API level | Inventory holds the tablet at Android 16 / API 36 (field notes snapshot; re-confirmation owed at run time). The API level MimirAlloc will target was **not chosen**: D-006 records the minimum-API decision OWED (INC-D006-04), its contract table by analogy only under U-03 | **OWED** - the target-API decision and any run-time confirmation |
| T7 | Inspect libc usage | glibc leg MET: D-002 undefined-symbol record + D-008 full source audit (only `memcpy`/`memset`/`mmap`/`munmap`/`sysconf`/`pthread`; `errno` never touched). musl leg BLOCKED (no sysroot, D-008 INC-D008-06). bionic leg absent (no NDK build to inspect, D-006) | **UNMET** - musl BLOCKED, bionic OWED |
| T8 | Inspect alignment | D-002 measured alignment notes (probe sizes 1..65536 all 0 mod 16, in fact page-aligned); D-003 ELF `p_align` record (INC-D003-03); D-008 probe 163/0 across the 33-size ladder and calloc/realloc paths against `_Alignof(max_align_t)` = 16. The header's claim stays the non-numeric "suitably aligned" (U-15 open) - inspection is complete, no numeric contract is claimed | **MET** (inspection, native target) |
| T9 | Measure 4 KiB / 16 KiB configurations | 16 KiB leg HELD: this Pi, measured in-run by D-003, D-004, D-008 and Section 2. 4 KiB leg: arithmetic proof only (D-003 Part B vs the `__int128` oracle at 4096); **no allocator run in any 4096-page environment exists** (INC-D003-02) | **UNMET** - 4096-page-host run OWED |
| T10 | Avoid fixed page assumptions | D-003: whole-tree audit (17 files, 6,362 lines) - runtime `sysconf` discovery, failure-checked, no fallback constant, no fixed page constant in any production source/header; the one fixed-assumption defect found was in a test and was fixed in-slice (INC-D003-01); proof test 141/0 re-run in Section 2 | **MET** |
| T11 | Run a small native Android program in the actual target environment | D-007: the program exists, is bounded and native-checked (59/0) - and has never run in any Android environment. Missing both prerequisites: the NDK build (T2) and the SM-X400 device road (T4) | **OWED** |
| T12 | Verify Termux behaviour separately | No MimirAlloc compilation or execution in Termux exists, in either its bionic or glibc userspace; field-notes Termux evidence never ran MimirAlloc; D-007 designates Termux a separate, separately labelled future run | **OWED** |
| T13 | Verify Android app behaviour separately | No packaged Android artifact of any kind exists in the repository; no app run exists; D-007's designated primary environment is the NDK command-line binary, with the app run separate and later | **OWED** |

**Task tally: MET 4 / UNMET 4 / OWED 5, of 13**
(MET: T1, T5, T8, T10 - all Pi-native inspection/build
items; UNMET: T3, T4, T7, T9 - each with one leg held
and one leg missing; OWED: T2, T6, T11, T12, T13 - the
entire Android-facing set).

## 6. Roadmap Section 12 checklist (governing for Android)

Applied by D-007 to the recorded SM-X400 inventory
(Samsung Galaxy Tab S10 Lite SM-X400, `gts10litewifi`,
Android 16 / API 36, build `X400XXS8CZI2`, patch
2026-09-05, no serial recorded; field notes 01-14).
Section 12 is a run checklist: no item is satisfied by
snapshot, rule, or design evidence.

| # | Section 12 item | Status for the exit (per D-007, re-verified against the banked record by this packet) |
|---|---|---|
| 1 | Confirm model, Android version, ABI, security patch, no serials | Held as field-notes snapshot only; run-time re-confirmation **OWED** (snapshot may be stale, INC-D007-07) |
| 2 | Record `getconf PAGESIZE`, `/proc/meminfo`, CPU architecture at run time | Snapshot in part (4,096 in one unnamed-command probe, INC-D001-08); run-time record **OWED** - the D-007 program's ENV lines are built to produce the page-size half in-run |
| 3 | Confirm the test environment (Termux / proot / NDK binary / packaged app) | Rule held; primary environment designated (NDK command-line binary); **no instance exists** |
| 4 | Avoid enormous on-tablet builds; cross-build, but still test on device | Rule held for the build leg (D-006 road is entirely off-tablet); the device-test leg is **OWED** |
| 5 | Differentiate RAM, swap, page cache, available memory | Snapshot separates MemTotal 5,841,464 KiB / MemAvailable 1,525,480 KiB / SwapTotal 8,388,604 KiB with its moving-values caveat; page cache never separately recorded; re-record **OWED** (INC-D007-04) |
| 6 | Account for background limits, lifecycle, low-memory kills, access restrictions | Documented as a constraint (Termux-context access denials in the field notes); untested for MimirAlloc - **OWED**; no pressure-survival claim made (Phase 9 territory) |
| 7 | Low concurrency, bounded allocations; UI/thermal impact | Design held (D-007 program: at most 2 threads, sizes at most 8192, 58 allocations); UI/thermal half **OWED with no held input** - Field 02 obtained no battery, thermal, or frame-timing signal |
| 8 | CPU, GPU, NPU are separate systems; MimirAlloc is host-side only | Boundary held; **no claim made** - the D-007 program allocates host memory only |
| 9 | ENN/NNAPI presence is not proof of third-party NPU inference | Negative rule held - **not claimed, not tested** |
| 10 | Support 4 KiB and 16 KiB page environments; measure, do not assume | Partially held: runtime discovery proven (D-003), 16 KiB run under test on the Pi; **the 4 KiB allocator run is OWED** (INC-D003-02) |

**Section 12 tally: 0 of 10 satisfied by run evidence**
(D-007's tally, unchanged at this base). Four items are
held as rules, boundaries, or design (3, 4, 8, 9); item
7 is held in its design half only; items 1, 2, 5 and 10
are held as snapshots or in part; item 6 is a
documented, untested constraint - and every run-time
record is OWED.

## 7. Libc and alignment position (D-008, summarised)

The three-libc contract table is D-008's deliverable
(`docs/LIBC_ALIGNMENT_COMPATIBILITY.md`); this packet
records only the exit-relevant grades:

| Leg | Grade | Basis |
|---|---|---|
| glibc (AArch64, this Pi) | **PROVEN** | D-008 probe 163/0 measured on the Pi (re-run in Section 2): every returned pointer aligned to `_Alignof(max_align_t)` = 16, in fact page-aligned; dependency audit closed (Section 5, T7) |
| musl (AArch64) | **BLOCKED** | No musl toolchain or sysroot on the Pi, re-verified by D-008; smallest step is a `musl-tools`/`musl-dev` install, **reserved for Volmarr's word** (INC-D008-06; see also INC-D005-06 - the cross package, not the host one, is what the x86-64 row needs) |
| bionic (Android) | **OWED, by-analogy only** | No NDK, no sysroot, no device run (D-006, D-007); every bionic cell in D-008's table is labelled by-analogy under U-03 (INC-D008-07) |

Behavioural differences a caller would observe, all
deliberate documented postures under open contract
items, not defects: zero-size calls return NULL where
all three libcs return a pointer (INC-D008-01, U-09);
allocation failure leaves `errno` untouched where the
libcs set `ENOMEM` (INC-D008-02, U-10); no fork posture
exists (INC-D008-05, U-17..U-21).

## 8. Supported-target position (Roadmap Section 5)

Unchanged from D-001's honest update, and re-verified
against the banked Wave 4 record by this packet: **no
target is supported** in the Section 5 sense. The Linux
AArch64 + glibc row is the strongest - implemented,
host-tested and device-tested on this Pi for the
four-symbol namespaced surface, now with its Phase 4
ABI/linker record (D-002), multithread recorded run
(D-004), page-size proof (D-003) and libc/alignment
record (D-008) - and still lacks native CI (T3). Every
other row lacks more: x86-64 lacks a build; musl lacks
a toolchain; Android/bionic, Termux, proot and the
SM-X400 device row lack any allocator run at all; the
Pi row additionally lacks any performance data, and no
performance claim exists anywhere in the repository.

## 9. What closes Phase 4 (the owed set, in order)

1. **Volmarr's word on installs** (each reserved, none
   taken by any slice): an Android NDK on a chosen build
   host (noting INC-D006-02 - current NDK prebuilts are
   x86-64; the Pi is AArch64, so the build host may move,
   the device leg does not); `gcc-x86-64-linux-gnu` for
   the x86-64 sysroot (D-005 Section 5); a musl package
   for the musl leg (D-008 Section 5).
2. **The Android build** per D-006's banked plan
   (`scripts/android/build_android.sh`, API matrix,
   four-symbol + ELF + dependency records), including
   building the D-007 program itself (INC-D007-01).
3. **A device road to the SM-X400** and the physical
   run per D-007 Section 5: allocate/fill/free (phases
   A-D) and multithread (phase E) logs, with the in-run
   ENV page-size record, the Section 12 run-time
   records, and the Section 6 evidence list D-007 names.
   That single run also discharges the 4096-page-host
   run (INC-D003-02) if it records page size 4096 in-run.
4. **The x86-64 suite run** (the Phase 3 debt) once the
   sysroot exists: the portable suite on x86-64 with its
   own recorded run - which also closes Phase 3's exit
   gate together with the already-banked C-008.
5. **Termux and packaged-app runs** (T12, T13),
   separately labelled, after the NDK-binary run.
6. A reconciliation addendum to this packet recording
   the new evidence item by item. Until then this
   packet's verdict stands as written.

## 10. Inconsistencies recorded by this slice (INC-D009-*)

1. **INC-D009-01 - THE MAKEFILE HEADER IS STALE.**
   The Makefile's top comment still reads "Phase 3
   build (slices C-001..C-007)" and its per-slice
   comment block stops at D-004, while the file in fact
   wires D-003, D-004, D-005, D-006, D-007 and D-008
   targets (each with its own comment further down).
   Cosmetic only - the fresh run in Section 2 exercised
   every target successfully - but a reader of the
   header alone would under-scope the build. Fixing it
   is a Makefile comment change, outside this slice's
   synthesis-only scope; recorded for the next slice
   that legitimately touches the Makefile.
2. **INC-D009-02 - INC-D001-02 IS STILL OPEN:
   SUPPORTED_TARGETS.md WAS NEVER REFRESHED.** D-001
   recorded that A-003's status reasoning is stale at
   the Phase 4 base and that "a future slice should
   refresh it or mark it superseded in place". No Wave 4
   slice did so: at this base the file still stands
   unmarked, and its conclusion (no target supported)
   is correct only for the newer Section 5 reasons in
   D-001 Section 7 and Section 8 of this packet.
   Recorded again so the debt is not lost at wave end.
3. **INC-D009-03 - THE D-006 RECORDED RUN LIVES UNDER
   tests/cross/, NOT tests/android/.** D-006's raw
   record is `tests/cross/recorded_run_d006.txt` - the
   D-005 area - while D-007's is
   `tests/android/recorded_run_d007.txt`. Both files are
   banked and cited correctly by their own documents,
   so no evidence is lost; the placement seam is
   recorded so future readers look in both areas for
   Android evidence.
4. **INC-D009-04 - THE PHASE 3 PACKET'S C-008 SENTENCE
   IS STILL BASE-RELATIVE (INC-D001-03, CARRIED).**
   `docs/PHASE_3_EXIT_EVIDENCE.md` records C-008 as OWED
   because its packet base predates C-008's bank
   (`37c5b3b`); at this base C-008 is banked, runs in
   `make test` (42/0, 7 skipped-not-yet, re-run in
   Section 2), and the Phase 3 gate is blocked on the
   x86-64 leg alone. No Wave 4 slice was chartered to
   amend the Phase 3 packet; recorded here so this
   packet - the later document - carries the correction
   in the banked record.
5. **INC-D009-05 - D-007'S RUN PROCEDURE STILL CONTAINS
   ITS BUILD GAP (INC-D007-01, CARRIED).** The banked
   Android build script builds `test_smoke.c`, not the
   D-007 on-device program; D-007 Section 5 bridges the
   gap with an explicit same-session build command, but
   the script itself is unchanged at this base. The
   first Android session must not discover this by
   surprise; it is named again in Section 9, item 2.
6. **INC-D009-06 - WAVE 4's SLICE NUMBERING AND THE
   ROADMAP'S PHASE NUMBERING SHARE NAMES WITH PHASE
   3's.** The banked record now contains both a
   "Phase 3 exit packet" whose fresh run predates D-004
   wiring and this packet, whose `make test` composition
   (8 suites) differs from every earlier packet's (6,
   then 7). All counts in this packet are tied to their
   base and recorded-run file (Sections 2-3) so the
   compositions cannot be silently conflated; recorded
   as a standing reading caution, not a defect.

## 11. Bottom line

Wave 4 built the whole Pi-native half of the Phase 4
foundation and proved it repeatedly: a native AArch64
build with a full ELF/ABI inspection (D-002), a
whole-tree page-size portability proof with its one
defect fixed (D-003), a physical multithread
allocate/fill/free run at 4,590 checks with a balanced
2,088/2,088 alloc/free count (D-004), and a
libc/alignment record that is measured, not asserted
(D-008) - all of it re-run green at this packet's base
in Section 2. The cross roads were mapped with equal
honesty: the x86-64 RUN leg exists and its BUILD leg is
blocked on one named missing package (D-005); the
Android build road is fully planned and blocked on one
named missing toolchain (D-006); the Android device
packet is ready to execute and has never executed
(D-007).

**Phase 4 exit is UNMET.** Combined tally across the
three governing lists in this packet: **MET 5 /
UNMET 8 / OWED 15, of 28** (exit E1-E5: 1/4/0; tasks
T1-T13: 4/4/5; Section 12: 0 satisfied of 10, counted
here as OWED 10 run records). It becomes claimable only
when the physical Android run exists - allocate/fill/
free and multithread logs from the SM-X400, with page
size and ABI recorded in that run - together with the
x86-64 suite run and the musl evidence named in
Section 9. Until then no subset of this packet, and no
cross-compile any slice may add, may be presented as a
Phase 4 exit.
