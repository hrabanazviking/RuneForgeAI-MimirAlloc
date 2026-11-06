# Phase 4 - Foundation Plan and Target Evidence Matrix

Slice: **D-001 (team-01)**. Wave 4, Phase 4 foundation plan.
Deliverable: `docs/PHASE_4_FOUNDATION_PLAN.md` (this file only).
Base: `45b0f5b3872db8dba4c03dad51ff4a1fdecbdd1b` on branch
`yrsa/mimiralloc-01`, worktree `/home/pi/mimiralloc-team-01`
on Raspberry Pi `purpleviking1`. Governing text: Roadmap
Section 8 Phase 4, read with Roadmap Section 5 (supported-
target matrix), Section 2 (evidence levels), Section 12
(Android and Samsung tablet checklist) and Section 7.5
(address-space safety). Wave 4 slice queue: WAVE_PLAN.md in
the coordinator checkout `~/mimiralloc-forge` (D-002..D-009),
read by this team on 2026-11-06. `README.md` was not touched
by this slice.

> **PHASE 4 IS NOT EXITED, AND IS NOT CLOSE TO EXITED.**
> The Roadmap Phase 4 exit sentence (Section 8, Phase 4) is,
> in substance: physical-device allocate/fill/free and
> multithread tests pass with logs, system page size and ABI
> recorded - and, verbatim, *"A cross-compile alone does not
> pass the gate."* At this base **no** Phase 4 exit item is
> met: there is no Android NDK on the Pi, no Android build of
> MimirAlloc anywhere in the repository, no MimirAlloc run on
> any Android device, no CI, and no Phase 4 recorded run of
> any kind. What exists is a Phase 3 namespaced allocator
> that is built and test-green on one target only - native
> ARM64 Linux + glibc on this Pi - plus a tablet environment
> inventory from the field notes that never ran MimirAlloc.
> This slice is a plan and an evidence map. It builds
> nothing, runs no allocator test, and promotes no target.

## 1. What this plan does

1. Decomposes Roadmap Section 8 Phase 4 into every task item
   (Section 5, T1-T13) and every exit item (Section 6,
   E1-E5), with no item silently dropped or merged away.
2. Maps each item to exactly one primary evidence source:
   **Pi-native**, **Cross-compile**, **Physical Android
   device**, or **OWED** (the missing thing is named, never
   left as a bare TBD). Where an item needs two sources in
   sequence (build cross, run on device), the run source is
   primary and the build source is named as its prerequisite.
3. Restates the supported-target status per Roadmap
   Section 5 honestly at this base (Section 7): **no target
   is supported**, and says what evidence each target still
   lacks, in the Section 2 evidence-level vocabulary.
4. Adopts Roadmap Section 12 as the governing checklist for
   every Android slice in Wave 4, item by item, with the
   current status of each item (Section 8).
5. Records every inconsistency this team found while
   mapping, as INC-D001-* (Section 10). None is silently
   resolved; two of them (INC-D001-02, INC-D001-06) change
   how later Wave 4 slices must word their claims.

## 2. Evidence actually checked on the Pi in this slice

Only the facts in this section were checked by this team in
this slice. Everything else in this document is cited to a
banked document and labelled as such. No build was run and
no allocator test was run by this slice (the plan needs
inventory facts, not a re-run; the Phase 3 recorded runs
stand as banked and are cited, not restated as new).

Commands run on `purpleviking1` on 2026-11-06, in
`/home/pi/mimiralloc-team-01` unless noted:

- `free -g` - 15 GiB total, 13 GiB available at check time
  (device health via the device service agreed: 14,181 MiB
  available of 16,209 MiB, load about 1.15, 57.9 C). Light
  load confirmed before any work; nothing heavy was run.
- `uname -a` - `Linux purpleviking1 6.12.112-v8-16k-hailo+
  #1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64
  GNU/Linux`. `uname -m` - `aarch64`.
- `getconf PAGESIZE` - `16384`. `getconf PAGE_SIZE` -
  `16384`. `getconf LONG_BIT` - `64`. `nproc` - `4`.
  `cat /proc/meminfo` (head) - MemTotal 16,598,576 kB.
- `cat /etc/os-release` - Debian GNU/Linux 12 (bookworm).
- `cc --version` / `gcc --version` - GCC 12.2.0 (Debian
  12.2.0-14+deb12u1). `clang --version` - Debian Clang
  14.0.6, target `aarch64-unknown-linux-gnu`.
  `ldd --version` - glibc 2.36 (Debian GLIBC
  2.36-9+rpt2+deb12u14). `ld --version` - GNU Binutils
  2.40. `make --version` - GNU Make 4.3.
- `git rev-parse HEAD`, `git branch --show-current`,
  `git status --porcelain`, `git log --oneline -5` - HEAD
  is the base above, branch `yrsa/mimiralloc-01`, tree
  clean except the untracked `hidden_files/` assignment
  directory. `git show --stat HEAD` - HEAD is the C-009
  Phase 3 exit packet commit.
- Toolchain presence probes: `which aarch64-linux-gnu-gcc
  x86_64-linux-gnu-gcc aarch64-linux-android-clang adb
  sdkmanager` returned **only** `/usr/bin/aarch64-linux-
  gnu-gcc`. `which musl-gcc musl-clang x86_64-linux-gnu-
  gcc qemu-x86_64 qemu-aarch64` returned nothing.
  `ls /home/pi/Android` - no such directory. `ANDROID_
  NDK_HOME`, `ANDROID_HOME`, `ANDROID_SDK_ROOT` - all
  empty. `ls /usr/lib/android-sdk` - no such directory.
  `dpkg -l` filtered for musl/cross/android/ndk - no musl,
  no Android SDK/NDK package (only unrelated name matches).
- `clang --print-targets` - the backend registers many
  targets including aarch64, arm64, x86 and riscv64. This is
  a compiler-backend capability list only; see INC-D001-05.
- `ls .github/workflows` - no such directory: **no CI
  configuration exists in this repository**.
- Source inspection: `cat Makefile`, `cat include/mimir_
  alloc.h`, directory listings of `src/`, `include/`,
  `tests/`, `scripts/`, `evidence/runs/`, and `grep -rn
  sysconf src/` - the platform layer queries `sysconf(_SC_
  PAGESIZE)` and checks it for failure (`src/platform/
  mimir_platform.c`, from line 34); no fixed page size is
  used there. `ls build` - no build directory exists in
  this worktree (nothing was built by this slice).
- Document reads: the Roadmap (Sections 2, 5, 8, 12 in
  full), `docs/SUPPORTED_TARGETS.md`, `docs/PHASE_3_EXIT_
  EVIDENCE.md`, `docs/field-notes/android-arm64-2026-11/
  02_TABLET_INVENTORY.md` and `04_ANDROID_AND_LINUX_TOOL-
  CHAINS.md`, and the Wave 4 queue in WAVE_PLAN.md.

## 3. Evidence-source taxonomy (the only four sources)

- **Pi-native** - built and run on this Raspberry Pi
  (ARM64 Linux + glibc, 16 KiB pages) with a recorded run:
  command, exit code, counts and the environment manifest
  of Section 2. Pi-native evidence is real evidence for
  the Linux AArch64 + glibc row and for the Pi device row.
  It is **not** Android evidence of any kind: different
  libc (glibc, not bionic), different loader, different
  page size from the inventoried tablet, no Android sandbox.
- **Cross-compile** - a build produced on one machine for a
  different target (different ISA, libc, or OS ABI) than
  the build host. A cross-compile proves only that sources
  compile and link for that target. Per the Roadmap's own
  gate sentence it never proves runtime behaviour, and
  alone it **does not pass Phase 4**. On this Pi today the
  only cross-shaped tool present is misleading - see
  INC-D001-05: `aarch64-linux-gnu-gcc` is the *native*
  compiler under its Debian triplet name, and no x86-64,
  musl, or Android cross toolchain exists here.
- **Physical Android device** - a run on a real Android
  device (the flagship is the Samsung SM-X400 of the field
  notes), in a named environment (NDK command-line binary,
  Termux, proot, or packaged app - each labelled, never
  merged), with on-device logs recording page size, ABI,
  API level and the exact commands. Emulator results are
  not this source and must never be promoted to it
  (Roadmap Section 2).
- **OWED** - the evidence does not exist and the row names
  exactly what is missing: a tool (Android NDK), a device
  road (a way to put a test binary on the SM-X400 and get
  logs back), a host (x86-64), a sysroot (musl), or a
  recorded run that no slice has produced yet. OWED is a
  status, not a euphemism for passed.

## 4. Starting state at base 45b0f5b (banked, cited)

What Phase 4 starts from, all of it banked evidence, none
of it re-run by this slice:

- The Phase 3 namespaced surface exists and is test-green
  on this Pi: `mimir_malloc`, `mimir_free`, `mimir_calloc`,
  `mimir_realloc` in `include/mimir_alloc.h`, built clean
  under `-std=c11 -Wall -Wextra -Werror -pthread` by the
  plain Makefile; the C-009 packet (`docs/PHASE_3_EXIT_
  EVIDENCE.md`) records a fresh six-suite `make test` at
  exit 0 (smoke 18/18; core 29/0; B-007 70/0; C-005 95/0;
  C-006 23/0; C-007 122 checks 0 failed) and C-008 has
  since banked at `37c5b3b` (see INC-D001-03).
- **Phase 3 exit is UNMET** (C-009 verdict, unchanged by
  anything in this slice): the portable suite has never
  run on x86-64. That debt is a Phase 4 input, not a
  Phase 4 discovery - D-005 owns the cross/host road for
  it, and no Phase 4 claim may paper over it.
- The platform layer already discovers page size at
  runtime (`sysconf(_SC_PAGESIZE)`, failure-checked) and
  this Pi runs 16 KiB pages, so the 16 KiB half of the
  Phase 4 page-size work has a running start on Linux. The
  4 KiB half has never run MimirAlloc anywhere: the only
  4 KiB environment in the record is the SM-X400 tablet
  (field notes), which has never run MimirAlloc.
- Cross-thread behaviour has Phase 3 evidence on the Pi
  (C-007 cases ERR-211/ERR-212 and the C-002 lock counter,
  cited in the C-009 packet). That is Phase 3 contract
  evidence on Linux/glibc; it is **not** the Phase 4 exit
  multithread test, which must be produced as a Phase 4
  recorded run (D-004 on the Pi, D-007 on Android).
- The tablet environment is inventoried (field notes 01-
  14): Samsung Galaxy Tab S10 Lite SM-X400, Android 16 /
  API 36, Exynos 1380 class SoC, 4,096-byte page size in
  one probe, Termux toolchains exercised, one bionic-linked
  NNAPI diagnostic built. **None of it ran MimirAlloc.**

## 5. Phase 4 task map (Roadmap Section 8, Phase 4)

The Phase 4 paragraph, decomposed in its own order. Owner
is the Wave 4 slice that produces the evidence; the source
column is the primary evidence source of Section 3.

| # | Task (roadmap wording, in substance) | Evidence source | Owner | Status / what counts, what does not |
|---|---|---|---|---|
| T1 | Create native AArch64 builds | Pi-native | D-002 | Partially held: the Phase 3 Makefile builds natively today, but no Phase 4 recorded build with a full device manifest and ELF record exists. Counts: a D-002 recorded run (clean build, compiler/flags logged, manifest of Section 2 refreshed). Does not count: any past `make test` log - those are Phase 3 evidence. |
| T2 | Create Android NDK builds | OWED - missing: Android NDK | D-006 | Wholly owed. Checked in this slice: no NDK, no SDK, no `aarch64-linux-android-clang`, NDK environment variables empty (Section 2). Counts: an NDK build of the four public symbols for a named API level, target triple and linker invocation recorded (field note 04's build-record list). Does not count: a glibc build, a Termux glibc build, or `clang --print-targets` listing arm64. |
| T3 | CI cross-compilation | OWED - missing: CI system and cross toolchains | D-005 | Wholly owed. Checked in this slice: no `.github/workflows`, no CI script, no x86-64 or musl cross compiler, no qemu. Note the standing project rule that GitHub Actions are manual-only (workflow_dispatch) if CI is ever added - the CI design is D-005's to record, not this slice's to invent. Counts: a repeatable, scripted cross build whose outputs are inspected (file format, ABI) and whose limits are stated. Does not count: a local native build, or a cross build presented as a runtime result. |
| T4 | Physical tests (the paragraph's umbrella for the exit runs) | Physical Android device (primary); Pi-native for the Pi leg | D-004 (Pi), D-007 (Android), D-009 (packet) | Owed as Phase 4 evidence on both legs. The Pi leg is producible now (D-004). The Android leg is OWED - missing: a device road to the SM-X400 (no way from this campaign, today, to install/run a test binary on the tablet and retrieve logs). |
| T5 | Inspect ELF properties | Pi-native | D-002 | Owed as a Phase 4 record; producible now with the installed binutils 2.40 (`readelf`, `nm`, `objdump` versions checked in Section 2). Counts: recorded ELF header, program headers / alignment, dynamic section, exported and undefined symbols for the built objects and test binaries. Does not count: the Phase 3 `nm` export lists alone - necessary, already held, but they are not an ELF/alignment inspection. |
| T6 | Inspect Android API level | Physical Android device (record); OWED until an Android build exists | D-006, D-007 | The tablet's API level is on record from the field notes (Android 16 / API 36, build X400XXS8CZI2, patch 2026-09-05 - a snapshot, see INC-D001-07). Owed: the API level MimirAlloc will target (min/target), chosen and recorded by D-006 against the bionic contract, and re-confirmed on device at run time by D-007. The field note that an NNAPI probe needed an explicit API 29 target shows API level changes symbol availability - it is not a formality. |
| T7 | Inspect libc usage | Pi-native (glibc leg); OWED for bionic and musl legs | D-002 (glibc), D-008 (three-libc contract) | Glibc leg producible now: undefined-symbol and `ldd` record for the built binaries against glibc 2.36. Bionic leg OWED (no NDK build to inspect). Musl leg OWED - missing: any musl toolchain or sysroot on the Pi (checked, Section 2). Counts per leg: the exact libc symbols the allocator and its tests depend on, per target, in D-008's contract table. |
| T8 | Inspect alignment | Pi-native | D-002, D-003, D-008 | Owed as a Phase 4 record. Held inputs: the public header claims only non-numeric suitable alignment (U-15 open - no numeric value may be claimed by any Wave 4 slice), and B-003's Phase 2 probe already showed ELF `max-page-size` alignment changes real behaviour on this Pi (a 4096 setting segfaulted, reproduced). Counts: ELF segment alignment recorded for native (and, when it exists, cross/NDK) builds, plus the runtime alignment behaviour D-008 verifies. |
| T9 | Measure 4 KiB / 16 KiB configurations | Pi-native (16 KiB leg, held environment); OWED for the 4 KiB leg - missing: a 4 KiB-page run | D-003 | Split status, stated plainly: the 16 KiB environment is this Pi (16,384, checked) and the allocator already runs there under Phase 3 tests, but D-003 owes the Phase 4 portability proof as its own recorded run. The 4 KiB leg is OWED: no 4 KiB Linux host exists in this campaign and the only 4 KiB device is the Android tablet (device road owed, T4). A 4 KiB *assumption* tested nowhere is not a measurement. |
| T10 | Avoid fixed page assumptions | Pi-native | D-003 | Held in the platform layer as inspected by this slice: page size comes from `sysconf(_SC_PAGESIZE)`, failure-checked, in `src/platform/mimir_platform.c`. Owed: D-003's audit of the whole tree (core, API, tests, scripts) for any fixed 4096/16384 assumption, with the audit record and any fix as its own slice evidence. This slice inspected the platform layer only and claims nothing about the rest of the tree. |
| T11 | Run a small native Android program in the actual target environment | Physical Android device | D-007 | Wholly OWED - missing: Android NDK build (T2) **and** the SM-X400 device road (T4). This is the centre of the phase: the program must allocate, fill, verify and free through MimirAlloc on the device, with logs. Counts: nothing short of that on-device run. Does not count: Termux glibc runs, proot runs, emulator runs, cross-compiles, or the field notes' NNAPI diagnostic (not MimirAlloc). |
| T12 | Verify Termux behaviour separately | Physical Android device (Termux environment) | D-007 | Wholly OWED for MimirAlloc. Held environment evidence only: Termux Clang 21.1.8 (default target aarch64-unknown-linux-android24) and a separate Termux glibc userspace were exercised in the field notes - with neither MimirAlloc program. Counts: a labelled Termux run (bionic-Termux and Termux-glibc recorded as different environments), permission limits documented per Roadmap Section 5. A Termux result is never native-app proof. |
| T13 | Verify Android app behaviour separately | Physical Android device (packaged app) | D-007 | Wholly OWED - missing: NDK build, an app packaging step, and the device road. No packaged Android artifact of any kind exists in the repository. Counts: a packaged app (or the closest NDK native-activity/binary route D-006/D-007 define and label honestly) running the allocator on device, sandbox and lifecycle limits recorded. Nothing else counts, however the earlier items fare. |

## 6. Phase 4 exit map (the gate, item by item)

| # | Exit item (roadmap wording, in substance) | Evidence source | Status at this base |
|---|---|---|---|
| E1 | Physical-device allocate/fill/free tests pass, with logs | Physical Android device (D-007); Pi leg Pi-native (D-004) | **UNMET.** No Phase 4 recorded run exists on any device. The Pi leg is producible in Wave 4; the Android leg is OWED (T4/T11). Phase 3 Pi contract runs are prior-phase evidence and do not discharge this item. |
| E2 | Physical-device multithread tests pass, with logs | Physical Android device (D-007); Pi leg Pi-native (D-004) | **UNMET.** Same position as E1. Phase 3 cross-thread cases (ERR-211/ERR-212, lock counter) are real but are Phase 3 evidence on Linux/glibc only; the Phase 4 multithread runs, at the bounded thread counts Section 12 requires on Android, do not exist. |
| E3 | System page size recorded (for the exit runs) | Physical Android device + Pi-native | **UNMET as an exit record; values held in part.** Pi: 16,384, checked by this slice and runtime-discovered by the code. Tablet: 4,096 in one field-notes probe whose exact command Field 02 does not name (INC-D001-08). The exit packet (D-009) must record the page size *of each exit run, measured in that run* - a value copied from an inventory is not that record. |
| E4 | ABI recorded (for the exit runs) | Physical Android device + Pi-native | **UNMET.** Held in part: the Linux/glibc ABI facts of the Phase 3 surface (four exported namespaced symbols, C ABI, C11) are recorded. Owed: the Android ABI record - arm64-v8a, bionic, target API level, ELF interpreter/dependencies of the Android build - which cannot exist until T2/T6 land. An exit packet that records only the glibc ABI has not met this item as the phase intends it. |
| E5 | The negative gate: a cross-compile alone does not pass | (rule, not evidence) | **IN FORCE.** Recorded here so no later slice can quietly breach it: if Wave 4 ends with NDK and x86-64 cross-compiles but no physical Android run, Phase 4 is UNMET and D-009 must say exactly that. |

**Reading rule for D-009 (binding on this plan's own
terms):** the roadmap names the exit device only as
"physical-device" while the phase's tasks centre on
Android (see INC-D001-06). This plan adopts the
conservative reading - E1/E2 require the Android physical
run, with the Pi runs as the Linux leg - because the
looser reading (Pi runs alone pass Phase 4) would make
T2, T11, T12 and T13 pointless and would contradict the
phase title, *ARM64 and Android foundation*. If Volmarr or
the roadmap's owner rules otherwise, that ruling, not this
plan, changes the gate.

**Tally: E1 UNMET, E2 UNMET, E3 UNMET (values held in
part), E4 UNMET (glibc half held), E5 rule in force.
Phase 4 exit: UNMET.**

## 7. Supported-target status per Roadmap Section 5

Honest update at base 45b0f5b, replacing the *status*
position of `docs/SUPPORTED_TARGETS.md` (A-003) for Phase 4
purposes (that file's citations stand; its "no code exists"
premise does not - INC-D001-02). Vocabulary: Roadmap
Section 2 evidence levels. **Supported** is used only in
the Section 5 sense: the row's *required evidence* exists.
**No row meets it.**

| Target (Section 5) | Required evidence (Section 5) | Evidence level today | Supported? What is still missing |
|---|---|---|---|
| Linux AArch64 + glibc | Native CI, multithread tests, ABI and linker proof | Implemented; Host-tested and Device-tested **on this Pi only**, for the four-symbol namespaced Phase 3 surface (C-009 packet) | **No.** Missing: native CI (no CI exists, T3); the Phase 4 ABI/linker record (T5/T7, D-002); the Phase 4 multithread recorded run (D-004). The strongest row in the matrix, and still not supported. |
| Linux x86-64 + glibc | Same correctness tests; sanitizer / differential value | Proposed (portable code exists; never built or run for x86-64 in this campaign) | **No.** Missing: everything - an x86-64 host or a working cross + run road (D-005). This is also the Phase 3 exit debt. No ARM64 result covers it. |
| Linux AArch64 + musl | Correct compile and runtime; interposition assessed separately | Proposed | **No.** Missing: a musl toolchain/sysroot (checked absent on the Pi), a musl build, a musl run, and the separate interposition assessment (a Phase 5 matter that Section 5 already flags as its own question). |
| Android ARM64 + bionic | NDK build, Android application/native test, device ABI and page-size validation | Proposed (environment inventoried; allocator absent) | **No.** Missing: the NDK build (T2), any native test, the device ABI record (E4), and a MimirAlloc page-size validation on device (E3). The field notes' bionic NNAPI diagnostic is environment evidence, not allocator evidence. |
| Termux on Android | Native bionic compilation and execution; permission limits documented | Proposed for MimirAlloc; the Termux environment itself is field-investigated | **No.** Missing: any MimirAlloc compilation or execution in Termux, in either its bionic or glibc userspace, and the permission-limit record for such a run (T12). |
| Linux userland in Android proot | Independently labelled proot results; not native app proof | Proposed | **No.** Missing: any proot result at all (the field notes record `proot-distro list` refusing in a traced session; no allocator result exists). Even when produced, a proot result can never promote the Android row. |
| Samsung SM-X400 physical tablet | Measured page size, memory conditions, stress and device responsiveness | Device inventoried (field notes); no allocator evidence | **No.** Held: one page-size probe (4,096) and one memory snapshot - inventory, not the required run-time records. Missing: page size measured in a MimirAlloc run, memory conditions during such a run, any stress run, any responsiveness measurement (field note 02 lists thermal/battery/UI signals as never obtained). |
| Raspberry Pi ARM64 Linux | Real-device test, OS/compiler/kernel manifest, performance data | Device-tested for the Phase 3 namespaced surface; manifest held (A-003 and refreshed in Section 2 of this plan) | **No** (as a completed Section 5 row). Missing: performance data - no performance measurement of MimirAlloc exists anywhere in the repository, and none may be claimed from test wall-times; and the Phase 4-specific real-device tests (D-004). |
| macOS ARM64 / Windows ARM64 (later, Section 5 closing paragraph) | Their own ABI and OS-interface work; not claimed until tested | Proposed (named only) | **No.** Nothing exists; out of Wave 4 scope; recorded so the matrix stays complete. |

Research-only rows (RISC-V Linux, microcontroller/RTOS)
remain research subjects, not targets, exactly as A-003
recorded them; Wave 4 changes nothing there.

## 8. Roadmap Section 12 checklist - governing for Android slices

Adopted verbatim in substance as the governing checklist
for D-006 and D-007 (and for the Android half of D-009).
No Android slice may report outside this list. Status is
the status today, at this base.

| # | Section 12 item | Current status |
|---|---|---|
| 1 | Confirm model, Android version, ABI and current security patch, without collecting serial numbers | **Held as a field-notes snapshot; re-confirmation OWED.** Field 02: SM-X400 (`gts10litewifi`), Android 16 / API 36, build X400XXS8CZI2, patch 2026-09-05, no serial recorded. It is a snapshot from the field-work period, not a run-time confirmation (INC-D001-07). D-007 re-confirms every field at run time. |
| 2 | Record `getconf PAGESIZE`, `/proc/meminfo` where readable, and CPU architecture; values can change or be restricted | **Partially held; run-time record OWED.** Tablet: page size 4,096 in one probe (command not named in Field 02 - INC-D001-08), MemTotal/MemAvailable/SwapTotal snapshot in Field 02, eight cores (4x Cortex-A55, 4x Cortex-A78) in `/proc/cpuinfo`. Pi values in Section 2 of this plan are Pi values and must never be quoted as tablet values. |
| 3 | Confirm whether testing is native Termux, proot Linux, an NDK command-line binary, or a packaged Android app | **Rule adopted; no instance exists.** No MimirAlloc Android run exists to label. Binding rule from this plan: every future Android record names exactly one of the four environments, and results are never merged across them (T11/T12/T13 are separate items for exactly this reason). |
| 4 | Avoid building enormous compiler dependencies on the tablet before measuring storage, RAM and time; use ARM64 cross-builds or a separate machine where practical, but still test on device | **Rule adopted; its cross-build leg is OWED.** No Android build exists. The cross-build road the item prefers does not exist on the Pi today (no NDK - T2), and tablet storage/RAM/time for a build have not been measured as a build decision (only the general inventory snapshot exists). D-006 records the build-location decision against measured numbers. |
| 5 | Differentiate physical RAM, swap, page cache and current available memory | **Held as a snapshot, with its caveat.** Field 02 separates MemTotal (5,841,464 KiB), MemAvailable (1,525,480 KiB at that moment) and SwapTotal (8,388,604 KiB) and warns the values move. See INC-D001-07 for the Roadmap Section 2 summary figure that does not quite match. Every D-007 run re-records all four quantities; no snapshot may be quoted as a run condition. |
| 6 | Account for Android background limitations, process lifecycle, low-memory kills and access restrictions; an allocator cannot force the OS to keep an app alive | **Documented as a constraint; untested for MimirAlloc.** Field notes record access denials (for example `/dev/vertex10`) in the Termux context. No MimirAlloc Android process has lived, been backgrounded, or been killed; D-007's packet must record lifecycle behaviour as observed, and no slice may claim pressure survival (that is Phase 9 in any case). |
| 7 | Start with low concurrency and bounded allocations; test impact on foreground UI and thermal state | **Wholly OWED.** No device run exists. Field 02 states explicitly that no battery, thermal, frame-timing or idle-baseline signal was obtained - so the UI/thermal half has no held input at all. D-007's program is specified small and bounded for this reason; unbounded stress on the flagship tablet is out of Wave 4 scope. |
| 8 | Treat CPU, GPU and NPU as separate systems; MimirAlloc manages host-side allocations; GPU/NPU allocations need their own APIs and synchronisation | **Boundary recorded; no claim made.** Field notes: `/dev/mali0` openable, system Vulkan loadable, vendor OpenCL rejected by the linker namespace from that context. None of that is MimirAlloc evidence, and Wave 4 makes no GPU/NPU allocation claim of any kind. |
| 9 | Samsung ENN/NNAPI libraries being present is not proof a supported third-party app can run Qwen3 on the NPU | **Negative rule held - not claimed, not tested.** The system declared a Samsung `enn` service and an NNAPI diagnostic ran (field notes); this plan records, per the roadmap, that this proves nothing about third-party NPU inference, and no Wave 4 slice may present it otherwise. |
| 10 | Support 4 KiB and 16 KiB system-page environments; do not assume this tablet uses one without measuring it | **Partially held; the 4 KiB allocator run is OWED.** Held: the code queries page size at runtime (T10), the 16 KiB environment (this Pi) runs the allocator under test, and the tablet's size was measured (4,096, one probe). Owed: the allocator running at all in a 4 KiB environment (T9), and D-003's portability proof. Support is a run claim, not a code-inspection claim. |

## 9. Wave 4 execution order under this plan

In roadmap order, as queued in WAVE_PLAN.md; this plan
adds only the dependency edges its evidence map forces:

1. **D-002** (Pi-native build hardening, ELF/ABI inspection)
   and **D-003** (page-size portability) are producible now
   and unblock everything else: they turn T1/T5/T7/T8/T9-
   (16 KiB)/T10 into recorded evidence.
2. **D-004** (Pi physical multithread allocate/fill/free)
   produces the Linux leg of E1/E2.
3. **D-005** (cross-compilation + CI scripts) attacks T3
   and the x86-64 debt; whatever it cannot produce must
   land as an honest blocker record naming the missing
   host/toolchain - a blocker record is a valid D-005
   outcome, a silent gap is not.
4. **D-006** (NDK readiness) attacks T2/T6: either the NDK
   build exists with its API-level/bionic contract, or the
   blocker (what must be installed, from where, at what
   size) is recorded exactly.
5. **D-007** (Android physical-device packet) depends on
   D-006 and on a device road to the SM-X400. Until both
   exist, D-007's honest deliverable is the ready-to-run
   test program plus the Section 12 packet with the run
   marked OWED - never a simulated or emulator result.
6. **D-008** (libc/alignment compatibility) completes the
   T7/T8 contract across glibc (producible) and records
   bionic/musl legs as far as D-005/D-006 roads allow.
7. **D-009** (exit packet) assembles E1-E5 under the
   reading rule of Section 6. On today's trajectory - no
   NDK, no device road - the packet's verdict will be
   UNMET with the Android leg OWED, and it must say so in
   those words.

## 10. Inconsistencies recorded by this slice (INC-D001-*)

1. **INC-D001-01 - THE KEEPER BODY'S BASE TEXT IS STALE.**
   The campaign text handed to this campaign's keeper
   describes base `b0b8374` as documentation-only with no
   code. The actual base, verified from git in this slice,
   is `45b0f5b` with Waves 1-3 banked: code, tests and
   recorded runs exist. This slice followed the assignment
   file on the Pi (base 45b0f5b) over the stale text.
   Recorded so no reader reconciles the two by guessing.
2. **INC-D001-02 - SUPPORTED_TARGETS.md IS STALE AT THIS
   BASE.** A-003's honest headline ("no target is
   supported" because no MimirAlloc code exists, and the
   README describes a documentation-only repository) was
   true when written and is half-false now: code exists and
   is Pi-tested, yet still no target is supported, for the
   different, Section 5 reasons in Section 7 of this plan.
   Anyone quoting A-003's *reasons* today would be wrong
   while quoting its *conclusion* correctly. This plan
   supersedes A-003's status reasoning for Phase 4; A-003
   itself is not edited by this slice, and a future slice
   should refresh it or mark it superseded in place.
3. **INC-D001-03 - THE C-009 PACKET'S C-008 SENTENCE IS
   BASE-RELATIVE AND NOW STALE.** C-009 (HEAD) records
   C-008 as OWED because its packet base (94e1a8d) predates
   C-008; but C-008 is in fact banked at `37c5b3b`, this
   base's parent commit, and the Makefile at this base
   already wires `test_c008` into `make test`. Read at
   this base without the git history, the packet
   misleads. Phase 3 exit remains UNMET regardless - on
   the x86-64 leg alone.
4. **INC-D001-04 - "CI" IS REQUIRED TWICE AND EXISTS
   NOWHERE.** Roadmap Section 5 makes native CI part of the
   first target's required evidence and Phase 4 tasks CI
   cross-compilation, yet the repository has no CI
   configuration at all (checked: no `.github/workflows`,
   no CI script) and the project's standing rule makes any
   future GitHub Actions manual-only. Local `make test`
   runs, however green, are not CI and must not be worded
   as CI in any Wave 4 record.
5. **INC-D001-05 - THE PI'S TOOLCHAIN NAMES INVITE TWO
   FALSE READINGS.** (a) `/usr/bin/aarch64-linux-gnu-gcc`
   looks like a cross-compiler and is the native compiler
   on this aarch64 host. (b) `clang --print-targets` lists
   x86, arm64 and more, which is backend capability, not a
   usable cross toolchain: there is no x86-64 gcc, no musl
   tool, no Android clang driver and no sysroot for any of
   them (all checked, Section 2). Any Wave 4 slice citing
   either fact as cross-compilation readiness is in error.
6. **INC-D001-06 - THE PHASE 4 EXIT SENTENCE DOES NOT NAME
   ITS DEVICE.** "Physical-device" in the exit sentence
   could be read as the Pi (a Section 5 physical-device
   row) or as the Android tablet the phase is about. The
   two readings differ enormously in what Wave 4 must
   produce. This plan adopts the conservative (Android)
   reading and states it openly in Section 6 rather than
   letting the ambiguity pass a gate by default; the
   roadmap itself is not amended by this slice.
7. **INC-D001-07 - THE TABLET NUMBERS ARE SNAPSHOTS WITH A
   ROUNDING SEAM.** Roadmap Section 2 summarises the tablet
   as about 5.57 GiB usable RAM and 1.52 GiB available "at
   one point"; Field 02's kernel fields are MemTotal
   5,841,464 KiB (about 5.57 GiB) and MemAvailable
   1,525,480 KiB (about 1.45 GiB) - the available figure
   does not reconcile exactly between the two documents,
   and both are momentary readings in any case. Neither
   figure may be quoted as a device property; run-time
   re-measurement (Section 12 items 2 and 5) is the only
   cure, and it is owed.
8. **INC-D001-08 - THE TABLET'S 4,096 PAGE SIZE HAS NO
   NAMED COMMAND IN ITS INVENTORY RECORD.** Field 02
   reports the value "in one probe" without naming the
   command; Field 11 lists `getconf PAGESIZE` as a
   reproduction-guard command, which is a plan for a
   future run, not a record of the probe that produced
   4,096. The value is plausible and unchallenged, but its
   provenance is thinner than Section 12 item 2 requires
   of an exit record - one more reason E3 stands UNMET
   until a run records it properly.

## 11. Bottom line

Phase 4 starts from a real but narrow foundation: one
allocator, four namespaced symbols, green tests on one
16 KiB-page ARM64 Linux target, an inventoried Android
tablet that has never seen the code, and no NDK, no CI, no
cross toolchain and no x86-64 run anywhere in the record.
This plan maps all thirteen Phase 4 tasks and all five
exit items to the only sources that can honestly evidence
them, keeps every target's status at *not supported* until
its Section 5 evidence exists, and puts the Section 12
checklist in charge of the Android slices. **Phase 4 exit
is UNMET**, and on the evidence available today its Android
half is owed in full - missing, by name: an Android NDK, a
device road to the Samsung SM-X400, and every on-device
run. Wave 4's Pi-native slices (D-002, D-003, D-004) can
land real evidence immediately; nothing they produce, and
no cross-compile D-005 or D-006 may add, passes the Phase 4
gate without the physical Android run the roadmap demands.
