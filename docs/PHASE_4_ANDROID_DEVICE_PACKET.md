# Phase 4 - Android Physical-Device Test Packet (Slice D-007)

Slice: **D-007 (team-07)**. Wave 4, Phase 4 ARM64 and Android
foundation. Deliverables: this packet,
`tests/android/test_d007.c` (on-device test program SOURCE,
ready to run, not yet run on any Android device), the
recorded run `tests/android/recorded_run_d007.txt`, and an
additive `test-d007` Makefile target (native logic check
only). Base: `761739b5599a98ceb2ffdb4fb33fb15436c4b87a`
(D-006 banked) on branch `yrsa/mimiralloc-07`, worktree
`/home/pi/mimiralloc-team-07` on Raspberry Pi
`purpleviking1`. Governing text: Roadmap Section 12
(Android and Samsung tablet checklist), applied to the
Samsung SM-X400 inventory recorded in the field notes and
carried by `docs/SUPPORTED_TARGETS.md` (A-003) and
`docs/PHASE_4_FOUNDATION_PLAN.md` (D-001); build road per
`docs/PHASE_4_ANDROID_BUILD_PLAN.md` (D-006). `README.md`
was not touched by this slice. Every Pi fact below was
checked or run on the Pi in this slice on 2026-11-06; the
raw record is the recorded run.

> **VERDICT. The physical Android run is OWED in full.**
> This slice delivers the ready-to-run test program and
> the Section 12 packet D-001 (Section 9, item 5) foresaw
> as D-007's honest deliverable while no device road
> exists: there is no Android NDK on the Pi (D-006,
> live-verified), no `adb` (re-verified in this slice,
> exit 127), and no other road from this campaign to the
> Samsung SM-X400. The test program was compile-checked
> and run NATIVELY on this Pi as a logic check only
> (59 checks, 0 failed) - a Linux/glibc result that is
> evidence for this host only and promotes no Android
> claim. No Android build, Android run, emulator run, or
> simulated result exists or is claimed. **Phase 4 exit:
> UNMET** (Section 7 below).

## 1. Checks performed on the Pi in this slice

Host first, load kept light throughout: `free -g` showed
13 GiB available of 15 GiB before and after all work;
Samtal (127.0.0.1:8787) answered HTTP 200 before and
after (recorded run Sections 0 and 4). Host: Linux
6.12.112-v8-16k-hailo+, aarch64, 16,384-byte pages,
4 cores. Nothing was installed; no Android device was
contacted or attempted.

- HEAD verified `761739b...` on `yrsa/mimiralloc-07`
  before any work; base matches the D-006 bank.
- Native syntax check of the new test source:
  `cc -std=c11 -Wall -Wextra -Werror -pthread`
  `-fsyntax-only`, exit 0.
- Native build + run via the additive target:
  `make test-d007`, exit 0 - CHECKS 59 run, 0 failed;
  ALLOCATIONS 58, FREES 58; RESULT: PASS; the in-run ENV
  line recorded PAGESIZE=16384 on this host (recorded
  run Section 1). **Logic check only** (Section 4).
- Android-target compile attempts with the Pi's clang 14
  (recorded run Section 2 and its addendum): hosted
  `--target=aarch64-linux-android24` syntax check fails
  exit 1 inside the host glibc headers at
  `bits/libc-header-start.h` - the exact failure mode
  D-006 recorded as E2; freestanding variants fail at
  the first hosted header (`stddef.h` under `-nostdinc`;
  `stdio.h` under `-nostdlibinc`). See INC-D007-06.
- `adb version`: command not found, exit 127 (recorded
  run Section 2c). No device road from the Pi.
- Native regression: `make test`, exit 0 - all eight
  suites RESULT: PASS, including test_d004's 4,590
  checks (recorded run Section 3).
- Document reads: the Roadmap Section 12 checklist
  (verbatim, 10 items); A-003 Sections 2, 4, 5 and 7;
  D-001 Sections 5 (T4/T6/T11/T12/T13), 6 (E1-E5), 7
  and 8; D-006 Sections 2-9; field notes 01, 02, 03,
  11 and 12; Research 03 (device acceptance gate);
  `include/mimir_alloc.h`.

## 2. The SM-X400 inventory, exactly as recorded

Nothing in this section is new measurement. It is the
inventory the repository already holds, cited to its
source; D-007 invents no device facts. Sources: Field 02
(`docs/field-notes/android-arm64-2026-11/02_TABLET_
INVENTORY.md`) unless noted; A-003 and D-001 carry the
same facts and are cited where they add a caveat.

- Identity: Samsung Galaxy Tab S10 Lite Wi-Fi, model
  `SM-X400`, device `gts10litewifi`, SoC identifier
  `s5e8835` (associated with Exynos 1380 in Field 02,
  citing Samsung's processor page as background).
- Software snapshot: Android 16 / API 36, build
  `X400XXS8CZI2`, security patch 2026-09-05. No serial
  number was collected or recorded.
- CPU: `/proc/cpuinfo` exposed eight ARM64 cores - four
  Cortex-A55 (`0xd05`) and four Cortex-A78 (`0xd41`);
  kernel maximum-frequency files reported 2,002,000 kHz
  (CPU 0) and 2,400,000 kHz (CPU 4) - reported maximums,
  not measured sustained clocks.
- Page size: 4,096 bytes in one probe; Field 02 does not
  name the probe's command (INC-D001-08, carried as
  INC-D007-05 below). Other Android devices can differ.
- Memory snapshot (kernel fields, one moment): MemTotal
  5,841,464 KiB (about 5.57 GiB), MemAvailable
  1,525,480 KiB (about 1.45 GiB), SwapTotal 8,388,604 KiB.
  A later guarded build observed a minimum 847,196 KiB
  available before it stopped; that reading includes
  other activity. Field 02 warns the values move and
  MemAvailable is an estimate, not a reservation or a
  per-app limit.
- Address space (Field 03, via A-003 Section 4): a 4 KiB
  fixed-no-replace mmap probe succeeded at 2^38 and
  failed ENOMEM at 2^39 and 2^47, consistent with a
  39-bit user virtual-address boundary on that device -
  not a complete kernel audit or a cross-device
  guarantee.
- Accessible interfaces (scoped to an untrusted-app
  Termux context and that OS build): `/dev/mali0` and
  `/dev/dri/renderD128` openable; `/dev/vertex10` and an
  NPU throughput node denied; system Vulkan library and
  Android neural-network library loadable; vendor OpenCL
  library present but rejected by the Android linker
  namespace from that context; a Samsung `enn`
  neural-network service declared.
- Missing signals (Field 02, explicit): no battery state,
  measured thermal level or headroom, ActivityManager
  low-memory threshold, foreground classification, frame
  timing, power, or repeatable idle baseline was
  obtained.

## 3. Roadmap Section 12 checklist, applied item by item

Status vocabulary: **Held (snapshot)** = the inventory
above records it, as a snapshot only; **Held (rule)** =
the governing rule is adopted and nothing yet tests it;
**OWED** = the run-time evidence does not exist and is
named. No item is marked satisfied on snapshot evidence:
Section 12 is a run checklist, and D-001 already held
this line (its Section 8); this slice re-applies it to
the D-007 run specifically.

| # | Section 12 item | Applied to the SM-X400 inventory | Status for the D-007 run |
|---|---|---|---|
| 1 | Confirm model, Android version, ABI and current security patch, without collecting serial numbers | Inventory holds model `SM-X400` / `gts10litewifi`, Android 16 / API 36, build `X400XXS8CZI2`, patch 2026-09-05, arm64-v8a device class, no serial (Section 2) | **Held (snapshot); re-confirmation OWED.** Every field is re-confirmed at run time by the run procedure (Section 5) - the snapshot may be stale (INC-D007-07) |
| 2 | Record `getconf PAGESIZE`, `/proc/meminfo` where readable, and CPU architecture; values can change or be restricted | Snapshot holds page size 4,096 (one probe, command unnamed), the three memory fields, and the 8-core CPU record | **Partially held; run-time record OWED.** The test program self-records page size and processor count IN the run (its ENV lines are the E3 record); `/proc/meminfo` capture is a run-procedure step. Any value Android denies is recorded as denied, never filled in from the snapshot |
| 3 | Confirm whether testing is native Termux, proot Linux, an NDK command-line binary, or a packaged Android app | No MimirAlloc Android run exists to label (A-003; D-001) | **Held (rule); primary environment designated.** The D-007 run is designated an **NDK command-line binary** run (D-001 T11, D-006 Section 7). Termux (T12) and packaged-app (T13) runs are separate, separately labelled, and never merged with it; a proot result would be convenience evidence and promote nothing |
| 4 | Avoid building enormous compiler dependencies on the tablet before measuring storage, RAM and time; use ARM64 cross-builds or a separate machine where practical, but still test on device | The build road this packet adopts is entirely off-tablet (D-006: NDK build on the Pi or another of Volmarr's machines); tablet storage/RAM/time for an on-tablet build were never measured as a build decision (D-001 item 4) | **Held (rule) for the build leg; the device-test leg is OWED.** No on-tablet build is planned by this packet, so no tablet build measurement is needed for it; the run itself remains owed |
| 5 | Differentiate physical RAM, swap, page cache and current available memory | Snapshot separates MemTotal 5,841,464 KiB, MemAvailable 1,525,480 KiB, SwapTotal 8,388,604 KiB, with Field 02's moving-values caveat; page cache was not separately recorded | **Held (snapshot, with caveat); re-record OWED.** The run procedure records MemTotal, MemAvailable, SwapTotal and the page-cache fields (`Cached`, `Buffers`, `MemFree` where readable) before and after the run; no snapshot figure may be quoted as a run condition (INC-D007-04) |
| 6 | Account for Android background limitations, process lifecycle, low-memory kills and access restrictions; an allocator cannot force the OS to keep an app alive | Field notes record access denials in the Termux context (`/dev/vertex10`); no MimirAlloc Android process has ever lived, been backgrounded, or been killed | **Documented constraint; untested, OWED.** The run procedure runs the binary in the foreground, bounded, and records lifecycle behaviour as observed. No pressure-survival claim is made or implied - that is Phase 9 territory in any case |
| 7 | Start with low concurrency and bounded allocations; test impact on foreground UI and thermal state | The test program is designed to this item: at most 2 threads, sizes at most 8,192 bytes, 58 allocations total, seconds of runtime | **Design held; UI/thermal half OWED with no held input.** Field 02 obtained no battery, thermal, frame-timing or idle-baseline signal, so there is nothing to compare a run against; a future run captures what is observable and marks the rest unknown (Field 11) |
| 8 | Treat CPU, GPU and NPU as separate systems; MimirAlloc manages host-side allocations; GPU/NPU allocations need their own APIs and synchronisation | Inventory records Mali/Vulkan/NNAPI environment facts (Section 2) - none of it is MimirAlloc evidence | **Boundary held; no claim made.** The D-007 program allocates host memory only. Wave 4 makes no GPU/NPU allocation claim of any kind |
| 9 | Samsung ENN/NNAPI libraries being present is not proof a supported third-party app can run Qwen3 on the NPU | The system declared a Samsung `enn` service and an NNAPI diagnostic ran in the field work | **Negative rule held - not claimed, not tested.** Recorded per the roadmap so no later slice presents library presence as NPU inference proof |
| 10 | Support 4 KiB and 16 KiB system-page environments; do not assume this tablet uses one without measuring it | The code queries page size at runtime with failure checks (D-003: no fixed page-size constant in any production source); the allocator runs under test in the 16 KiB environment (this Pi - including this slice's native logic run at PAGESIZE=16384); the tablet measured 4,096 in one probe | **Partially held; the 4 KiB allocator run is OWED.** Support is a run claim, not a code-inspection claim (D-001 item 10). The tablet run at 4 KiB is exactly the owed D-007 run; the Pi's 16 KiB Linux run is not Android evidence for it |

**Checklist tally: 0 of 10 items satisfied by run
 evidence. Four items are held as rules, boundaries or
 design (3, 4, 8, 9); item 7 is held in its design half
 only; items 1, 2, 5 and 10 are held as snapshots or in
 part; item 6 is a documented, untested constraint -
 and every run-time record is OWED.**
That tally is the honest state of the flagship device:
inventoried, never run.

## 4. The test program (tests/android/test_d007.c)

Purpose: the small native Android program of D-001 T11 -
it must allocate, fill, verify and free through
MimirAlloc on the device, with logs. It links only
against `include/mimir_alloc.h` and exercises all four
public symbols (`mimir_malloc`, `mimir_calloc`,
`mimir_realloc`, `mimir_free` - the header's full surface;
contract position by analogy only, U-03 open, exactly as
stated in the source header). Layout follows the repo's
test conventions: source under `tests/<area>/`, recorded
run beside it, standalone Makefile target, deterministic
counts, `RESULT: PASS` only on a clean run.

| Phase | What it does | Gate item it serves |
|---|---|---|
| ENV | Prints `sysconf(_SC_PAGESIZE)` measured in the run (failure printed, never guessed), processor count where readable, pointer width | E3 page-size record; Section 12 item 2 |
| A | 3 rounds over sizes {1, 16, 64, 256, 1024, 4096, 8192}: allocate / fill / verify every byte / free | allocate/fill/verify/free (Research 03 gate) |
| B | 8 blocks live at once: filled, verified while live, pairwise distinct, `max_align_t` alignment (non-numeric, U-15), freed | alignment; distinct storage |
| C | `mimir_calloc(16, 32)` zero over the full extent; realloc grow 128 -> 512 with prefix preserved byte-for-byte; `mimir_realloc(NULL, 64)` as malloc | resize behaviour (Field 12) |
| D | `mimir_malloc(SIZE_MAX)` returns NULL; `mimir_free(NULL)` returns normally. Zero-size is deliberately NOT checked (U-09 unresolved) | overflow / failure |
| E | 2 threads: producer allocates, fills, verifies 25 blocks; main thread verifies again and frees them (cross-thread free); one fresh allocation afterwards | multiple threads; cross-thread free (bounded per Section 12 item 7) |

**Pi-side check results (this slice, recorded run):**
native syntax check exit 0; native run via
`make test-d007` exit 0 - CHECKS 59 run, 0 failed,
ALLOCATIONS 58, FREES 58, RESULT: PASS. Those numbers
are the program's deterministic totals for a passing
run on ANY host; on the SM-X400 they must reappear
identically (except the ENV values, which must be the
tablet's) or the run is reported as the failure it is.

**What the native PASS proves, and what it does not.**
It proves the program's logic is sound against the real
allocator on ARM64 Linux and that the source is clean
C11 under `-Wall -Wextra -Werror`. It proves nothing
about bionic, the Android sandbox, the tablet's 4 KiB
pages, or the device: the Android-target compile
attempts in recorded run Section 2 fail for want of a
bionic sysroot (D-006 E2's failure mode, verbatim), and
no Android binary exists. Any wording that promotes the
native run to an Android result breaches D-001 E5.

## 5. How it will be run when a road exists

Both legs are reserved for Volmarr's word (NDK install:
D-006 Section 3; device access: this slice's standing
instruction). The procedure is written so the first
session with both legs present executes without further
design work.

**Build road (per D-006 Section 8, in order):**

0. Host check: confirm the NDK's prebuilt LLVM toolchain
   executes on the chosen build host (INC-D006-02 - the
   Pi is AArch64, current NDK prebuilts are x86-64; if
   neither native nor D-005's Box64 road runs it, the
   build host moves, the device leg does not).
1. Set `ANDROID_NDK_HOME`; run
   `scripts/android/build_android.sh` for API 24, 29 and
   36 - this produces the allocator + smoke test with
   the ELF, `llvm-nm` four-symbol and `llvm-readelf`
   dependency records (D-006 Section 6). **Gap:** that
   script builds `test_smoke.c`, not this program
   (INC-D007-01). In the same session, build this
   program with the same driver and flags, e.g.:
   `$NDK/toolchains/llvm/prebuilt/<host>/bin/`
   `aarch64-linux-android36-clang -std=c11 -Wall -Wextra`
   `-Werror -pthread -Iinclude -Isrc src/api/mimir_alloc.c`
   `src/core/mimir_core.c src/platform/mimir_platform.c`
   `tests/android/test_d007.c -o build-android/test_d007`
   (command shown as the proposed procedure, never yet
   executed), then record `file`, `llvm-nm` and
   `llvm-readelf -d` output for `test_d007` exactly as
   the script does for the smoke binary.
2. Bionic inspection per D-006 Section 6 / D-008: the
   undefined-symbol list against the bionic sysroot, the
   dependency list, and ELF segment alignment for the
   4 KiB / 16 KiB question.

**Device road options (exactly one environment per run,
labelled in the record; D-006 Section 7):**

- **(a) NDK command-line binary via `adb` - the primary
  D-007 route (T11).** With platform-tools present:
  `adb push build-android/test_d007 /data/local/tmp/`,
  `adb shell chmod 755 /data/local/tmp/test_d007`, run
  it from the device shell capturing full stdout and
  exit status, `adb pull` the log. Today: `adb` is
  ABSENT on the Pi (exit 127, this slice) and no device
  connection of any kind has been attempted.
- **(b) Termux on-device (T12, separate evidence).** Copy
  the source (or an NDK binary) into Termux on the
  tablet and build/run there with Termux's clang (field
  record: Clang 21.1.8, default target android24),
  recording which sub-environment - bionic Termux or the
  separate Termux glibc userspace - and its permission
  limits. A Termux result is developer-workflow evidence
  and is **never** native-app proof.
- **(c) Packaged native app (T13, separate evidence).**
  The allocator inside a packaged application under the
  app sandbox and lifecycle. No packaging step exists in
  the repo; this route is named for completeness and is
  the furthest from runnable.
- **proot** is not a D-007 route: convenience evidence
  only, independently labelled, promoting nothing.

**Run-time re-confirmation (before the binary runs,
Section 12 items 1, 2, 5):** record model, Android
version, build, security patch and API level as the
device reports them THEN (no serial); `getconf
PAGESIZE` or the program's own ENV line; `/proc/meminfo`
head (MemTotal, MemAvailable, MemFree, Buffers, Cached,
SwapTotal where readable) before and after; foreground
state; storage free. Mark anything denied as denied
(Field 11: never bypass access controls to fill a
report). Run in the foreground, once per Section 6's
repetition requirement, at the program's built-in
bounds - no larger stress on the flagship tablet is in
Wave 4 scope.

## 6. Exact evidence a future physical run must produce

The Research 03 device acceptance gate, the Field 11
baseline record, and D-001 E1-E5, made concrete. A run
that lacks any item is a partial run and is reported as
one; D-009 judges the packet, not this slice.

1. **Build identity:** source revision (git hash), the
   test binary's SHA-256, NDK version, clang driver and
   API level used, and the exact build commands
   (Section 5, steps 1-2 records included: ELF header,
   four-symbol `llvm-nm` list, `llvm-readelf -d`
   dependencies, segment alignment).
2. **Device identity at run time:** model, device name,
   Android version, build, security patch, API level -
   re-confirmed in the session, no serial numbers or
   account identifiers (Field 11 privacy rule).
3. **Environment label:** exactly one of NDK
   command-line binary / Termux (naming the
   sub-environment) / packaged app, with the route's
   exact commands (`adb push`/`shell` lines or their
   Termux equivalents) and the binary's on-device path.
4. **In-run page size:** the program's ENV line -
   `sysconf(_SC_PAGESIZE)` measured in that run on the
   device (the E3 record; the inventory's 4,096 does not
   substitute), plus processor count and pointer width
   as printed.
5. **Memory conditions:** `/proc/meminfo` fields of
   Section 5 before and after the run (physical RAM,
   swap, page cache and available memory differentiated
   per Section 12 item 5), foreground state, and free
   storage - or each denied value marked denied.
6. **The full run log:** complete stdout of the binary,
   ending `CHECKS 59 run, 0 failed`, `ALLOCATIONS 58`,
   `FREES 58`, `RESULT: PASS`, and the process exit
   status 0 - the deterministic totals of Section 4.
   Any deviation is reported verbatim, not summarised
   away; failures and partial logs are retained
   (Field 11: retain failures).
7. **Multithread + cross-thread evidence:** the Phase E
   PASS lines of that same log (producer fill/verify
   clean; 25 blocks verified and freed cross-thread;
   fresh allocation afterwards). Whether this bounded
   2-thread phase satisfies exit item E2's "multithread
   tests pass" on Android is D-009's judgement
   (INC-D007-03) - the log must let D-009 judge it.
8. **Repeated startup / exit:** the binary run to
   completion at least 3 separate times (3 process
   startups, 3 logs, 3 exit statuses) - the gate's
   repeated startup/exit item is a property of the
   process record, not of the program's internal
   3-round Phase A loop.
9. **Lifecycle observation:** what happened around the
   run - foreground throughout or any backgrounding,
   any low-memory or lifecycle event observed, UI
   responsiveness and thermal state as far as they are
   observable, each unobservable signal marked unknown
   (Section 12 items 6-7; Field 02's missing signals
   are the standing warning).
10. **What this program does NOT produce, still owed
    after a clean run:** pressure recovery (the gate
    lists it; the program has no pressure phase by
    design - INC-D007-02; Field 12's road is injected
    allocation failure, never exhausting the tablet),
    and any performance measurement. A clean D-007 run
    discharges T11's allocate/fill/verify/free centre;
    it does not by itself exit Phase 4.

## 7. Phase 4 exit position after D-007

| # | Exit item | Status |
|---|---|---|
| E1 | Physical-device allocate/fill/free tests pass, with logs | **UNMET.** The Android program exists, is logic-proven natively, and has never run on the device. The Pi leg stands (D-004). |
| E2 | Physical-device multithread tests pass, with logs | **UNMET.** Same position; the program's bounded Phase E is ready and unrun on Android (INC-D007-03). |
| E3 | System page size recorded for the exit runs | **UNMET as an exit record.** The program self-records it in-run by design; no Android run exists to record it in. |
| E4 | ABI recorded for the exit runs | **UNMET.** The Android ABI record (arm64-v8a, bionic, target API, ELF interpreter/dependencies) awaits the NDK build (D-006 Section 6). |
| E5 | A cross-compile alone does not pass the gate | **IN FORCE**, and this slice adds: a native Pi run alone does not pass it either. |

**Phase 4 exit: UNMET.** Still OWED, by name: an Android
NDK (Volmarr's word), an NDK build with its bionic
inspection record, a device road to the SM-X400
(Volmarr's word), the on-device run with Section 6's
evidence, and pressure recovery beyond this program.

## 8. Inconsistencies recorded (INC-D007-*)

- **INC-D007-01** - The banked Android build road does
  not build the device test program: D-006's
  `scripts/android/build_android.sh` compiles
  `tests/contract/test_smoke.c`, not
  `tests/android/test_d007.c`. Section 5 step 1 records
  the direct-driver build for this program so D-009
  does not assume the script covers D-007; extending
  the script additively is future work for the run
  session, not silently assumed done.
- **INC-D007-02** - The Research 03 acceptance gate
  requires pressure recovery; this program deliberately
  has no pressure phase (Section 12 item 7 bounds the
  flagship tablet; Field 12 directs injected failure
  over exhaustion). The gate item therefore stays OWED
  even after a clean D-007 device run, and needs its
  own bounded design before D-009 can close the gate.
- **INC-D007-03** - E2's Android multithread bar is
  undefined in size: this program's Phase E (2 threads,
  25 blocks, 59 checks total) is far smaller than
  D-004's Pi run (4 threads, 4,590 checks). The packet
  records the program as the bounded device test
  Section 12 item 7 asks for and leaves the E2
  sufficiency judgement to D-009, rather than
  pre-claiming it here.
- **INC-D007-04** - Carried from INC-D001-07: Roadmap
  Section 2 summarises the tablet's available memory as
  about 1.52 GiB "at one point"; Field 02's kernel
  field is MemAvailable 1,525,480 KiB (about 1.45 GiB).
  The figures do not reconcile exactly and both are
  momentary; neither may be quoted as a device property.
- **INC-D007-05** - Carried from INC-D001-08: the
  tablet's 4,096 page size rests on one probe whose
  command Field 02 does not name. This program cures
  the provenance gap prospectively (in-run `sysconf`
  print, Section 6 item 4); the inventory value itself
  remains thinner than an exit record may be.
- **INC-D007-06** - The freestanding compile-check road
  does not extend to the device test program: D-006's
  E1 worked because the API translation unit needs no
  libc headers, but this program is hosted by design
  (`stdio`, `pthread`, `unistd`), so on a host with no
  bionic headers NO Android-target compile check of it
  is possible at all (recorded run Section 2: hosted
  attempt dies in the host glibc headers; freestanding
  attempts die at the first hosted header). Its only
  Pi-side checks are the native syntax check and the
  native logic run. A future reader must not cite E1
  as a compile check of this program.
- **INC-D007-07** - The SM-X400 inventory is a
  field-work snapshot (patch level 2026-09-05) of
  unstated age relative to any future run; the device
  may have been updated since. This packet therefore
  treats every inventory field as a planning input
  only, and Section 5 makes run-time re-confirmation a
  procedure step, not a formality.

## 9. Self-test checklist (solo slice - tester role)

Executed by the team lead in the tester role on the Pi,
2026-11-06, against the finished worktree:

1. HEAD verified `761739b...` on `yrsa/mimiralloc-07`
   before any work; base matches the D-006 bank. PASS
2. Every device fact in Sections 2-3 traced to Field 02,
   Field 03, A-003 or D-001 as cited; no device fact
   appears without a source, and no Pi value is quoted
   as a tablet value. PASS
3. Test program builds clean under `-Wall -Wextra
   -Werror` and its native run output was read back
   from recorded run Section 1: 59 checks, 0 failed,
   58 allocations, 58 frees, RESULT: PASS - and is
   labelled logic-check-only in the source header, the
   Makefile target, this packet and the recorded run.
   PASS
4. Android-target attempts read back from recorded run
   Section 2 verbatim (glibc-header failure, hosted-
   header failures, `adb` exit 127); no text anywhere
   calls any of them a build or a run. PASS
5. `make test` exit 0 after all edits (recorded run
   Section 3); Samtal HTTP 200 before and after;
   `free -g` 13 GiB available before and after. PASS
6. Section 12 applied item by item - all 10 items in
   Section 3 with a per-item status; none marked
   satisfied on snapshot evidence. PASS
7. `README.md` untouched (git status shows no README
   change); Makefile change additive only (one target
   family plus one `.PHONY` word); no package was
   installed and no Android device was contacted. PASS
8. Phase 4 exit stated UNMET in the verdict, Section 7
   and here; the physical run is named OWED, never
   implied. PASS

**Tester verdict: PASS** - the slice claims only what
was run or read on the Pi, the program is ready to run,
and the owed run is specified exactly.

## 10. Exit status

**Phase 4 exit: UNMET.** D-007 delivers the packet and
the ready-to-run program D-001 assigned it: Section 12
applied to the recorded SM-X400 inventory with per-item
status, the four-symbol on-device test source with its
native logic proof and its honestly-failed Android
compile attempts, the run procedure for the day both
roads exist, and the exact evidence list a physical run
must produce. The run itself - and with it E1-E4's
Android halves - remains OWED, blocked on exactly two
of Volmarr's words: an Android NDK, and a device road
to the Samsung SM-X400.
