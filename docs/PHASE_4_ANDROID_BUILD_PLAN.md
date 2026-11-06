# Phase 4 - Android NDK Readiness and Android Build Plan (Slice D-006)

Slice: **D-006 (team-06)**. Wave 4, Phase 4 ARM64 and Android
foundation. Deliverables: this doc,
`scripts/android/probe_android_toolchain.sh` (EXECUTED),
`scripts/android/build_android.sh` (PROPOSED - build path
unexecuted, preflight executed), the recorded run
`tests/cross/recorded_run_d006.txt`, and an additive
`test-d006` Makefile target.
Base: `18028d218c2fdd5bfd36aa2d9aa338d9b343a3e9` (D-005
banked) on branch `yrsa/mimiralloc-06`, worktree
`/home/pi/mimiralloc-team-06` on Raspberry Pi
`purpleviking1`. Governing text: Roadmap Section 8
Phase 4 tasks T2 and T6 as decomposed in
`docs/PHASE_4_FOUNDATION_PLAN.md` (D-001), read with
Roadmap Sections 2, 5 and 12, the A-008 contract document
`docs/contracts.md`, and the D-002 inspection
`docs/AARCH64_ELF_ABI_INSPECTION.md`. `README.md` was not
touched by this slice. Every Pi fact below was checked or
run on the Pi in this slice on 2026-11-06; the raw record
is `tests/cross/recorded_run_d006.txt`.

> **VERDICT. There is no Android NDK on the Pi - confirmed
> live in this slice, not copied from D-001 - and there is
> no Android build of MimirAlloc.** No `ndk-build`, no NDK
> clang driver, no SDK, no bionic sysroot, no `adb`, and
> no NDK directory at any standard path (recorded run
> Section 1). The one Android-adjacent capability the Pi
> does have is clang 14's AArch64 code-generation backend,
> which this slice exercised directly: the API translation
> unit compiles freestanding for `aarch64-linux-android24`
> and its object defines exactly the four public symbols
> (experiment E1) - and the hosted build then fails for
> want of a bionic sysroot (E2). A clang backend is not a
> toolchain (D-001, INC-D001-05), and a freestanding object
> is not an Android build. **Phase 4 exit: UNMET. The
> Android physical-device run remains OWED in full**
> (D-001 T4/T11, D-004 INC-D004-04). This slice delivers
> the readiness verdict with live evidence, the blocker
> record with the smallest next step, and the build plan
> that executes the moment an NDK exists. Nothing was
> installed; installation is reserved for Volmarr's word.

## 1. Checks performed on the Pi in this slice

Host first, load kept light throughout: `free -g` showed
13 GiB available of 15 GiB before and after all work;
load stayed near 1.0; Samtal (127.0.0.1:8787) answered
HTTP 200 before and after (recorded run Sections 1, 5).
Host: Linux 6.12.112-v8-16k-hailo+, aarch64, Debian 12
(bookworm), 16,384-byte pages, 4 cores.

- The probe script this slice wrote and EXECUTED,
  `scripts/android/probe_android_toolchain.sh`
  (read-only; installs and builds nothing): environment
  variables, NDK/SDK commands, NDK/SDK paths, native
  compilers for contrast, clang target list, Termux/proot
  presence, a read-only `dpkg` scan, and a bounded-depth
  name search (recorded run Section 1, probe exit 0;
  re-run via `make test-d006`, Section 6, exit 0).
- The PROPOSED build script's preflight,
  `scripts/android/build_android.sh`: executed, refused
  with exit 3 and the BLOCKED message naming the missing
  NDK path (Section 2). The build path below the
  preflight has never executed.
- Direct clang experiments E1-E3 in a scratch directory
  (Section 3 of the recorded run; analysed in Section 4
  below). Nothing was installed, no package state was
  changed, and no file outside this worktree and the
  scratch directory was written.
- Source inspection of the three library translation
  units for their libc dependencies (Section 6 below):
  `grep` over `src/` and `include/` in this worktree.
- Native regression: `make clean && make test`, exit 0
  (recorded run Section 4), re-verified after the
  additive Makefile edit, exit 0 (Section 6).
- Document reads: D-001 (T2/T6 rows, Sections 2, 7, 8),
  `docs/contracts.md` (A-008), the public header
  `include/mimir_alloc.h`, D-002, D-005's cross doc and
  scripts (the pattern this slice follows), and the
  field notes as cited through D-001 and
  `docs/SUPPORTED_TARGETS.md` (A-003).

## 2. The Android road, component by component (all verified live)

| Component | Status on the Pi | Evidence |
|---|---|---|
| Android NDK (any version, any path) | **ABSENT** | probe Sections: paths, commands, name search all empty |
| `ndk-build`, `sdkmanager` | **ABSENT** | probe (`command -v` empty) |
| NDK clang drivers (`aarch64-linux-android*-clang`) | **ABSENT** | probe |
| `ANDROID_NDK_HOME` / `ANDROID_HOME` / `ANDROID_SDK_ROOT` | **UNSET** (all empty) | probe |
| Android sysroot (`/usr/aarch64-linux-android`, any NDK sysroot) | **ABSENT** | probe path check; E2 failure mode |
| `adb` (any device road from the Pi) | **ABSENT** - `command not found`, exit 127 | probe; E3 |
| Native compilers (gcc 12.2.0, clang 14.0.6, target `aarch64-unknown-linux-gnu`) | PRESENT - glibc road only, **not** Android evidence | probe |
| clang AArch64 backend (`--print-targets` lists `aarch64`, `arm64`) | PRESENT - code generation only; usable freestanding (E1), not a hosted Android toolchain | probe; E1, E2 |
| Termux on this Pi host | **ABSENT** - no `termux-info`, no `/data/data/com.termux`, no `PREFIX` (Termux lives on the Android tablet, not on this Linux host; see Section 7) | probe |
| `proot` / `proot-distro` on this Pi host | **ABSENT** | probe |
| Debian packages for Android/NDK/adb | **NONE** - the `dpkg` grep hits are false positives (`mariadb` contains "adb"; INC-D006-05) | probe |
| .NET Android workload manifests (`~/.dotnet/.../microsoft.net.sdk.android`) | Name match only - **not an NDK**, no toolchains, no clang drivers (INC-D006-06) | slice's wider `find` sweep |

D-001 recorded the same headline ("no Android NDK on the
Pi") from path and environment checks. This slice
re-verified it live with four independent methods
(commands, paths, package scan, name search) plus two
compile experiments that fail exactly where a missing
sysroot must fail. The verdict is unchanged; the evidence
under it is now D-006's own.

## 3. Blocker record

**Blocked:** every Android build, inspection, and run
item (D-001 T2, T6's build leg, T7's bionic leg, T11,
T12, T13) on exactly one missing component - an Android
NDK - plus one missing road: any way to reach an Android
device from this campaign (`adb` absent; the SM-X400
device road is D-007's OWED item).

**What would be needed, exactly:**

1. An Android NDK unpacked on the Pi (or on another of
   Volmarr's machines used as the build host) with its
   LLVM toolchain (`toolchains/llvm/prebuilt/.../bin/
   aarch64-linux-android<API>-clang`, `llvm-nm`,
   `llvm-readelf`) and the bionic sysroot for
   `arm64-v8a`. The NDK is distributed as a standalone
   archive, not a Debian package; nothing in this slice
   downloaded or installed anything.
2. `ANDROID_NDK_HOME` (or `ANDROID_HOME` with an `ndk/`
   directory) pointing at it - the exact variables the
   proposed build script's preflight checks.
3. For the device leg only: Android platform-tools
   (`adb`) and the SM-X400 road - D-007's territory.

**Smallest next step (reserved for Volmarr's word):**
approve obtaining one Android NDK archive and unpacking
it under `~/Android/Sdk/ndk/` on the Pi, then set
`ANDROID_NDK_HOME`. That single step makes
`scripts/android/build_android.sh` runnable as written
(subject to INC-D006-02) and unblocks the bionic
inspection in Section 6. No other component is missing
for the *build* leg: the sources' libc needs are small
and ordinary (Section 6), the clang-backend question is
settled (E1), and the native suite is green.

**One plan risk, recorded not hidden (INC-D006-02):**
current NDKs ship their prebuilt LLVM toolchain for
x86-64 Linux hosts. This Pi is an AArch64 Linux host.
Whether the NDK clang driver can run on this host at
all - natively, or via the Box64 0.4.5 + binfmt road
D-005 proved for x86-64 binaries - is UNVERIFIED, and
the proposed script's default `linux-x86_64` prebuilt
path may need the `NDK_TOOLCHAIN` override or a
different build host. The build plan therefore names
"NDK host execution on the Pi" as its step 0 check
(Section 8), before any build claim.

## 4. Experiments (recorded run Section 3)

- **E1 - freestanding Android-target compile of the API
  TU.** `clang --target=aarch64-linux-android24
  -ffreestanding -nostdinc` with only clang's resource
  headers, full `-Wall -Wextra -Werror`: exit 0; the
  object is `ELF 64-bit LSB relocatable, ARM aarch64`;
  `nm -g --defined-only` lists exactly `mimir_malloc`,
  `mimir_free`, `mimir_calloc`, `mimir_realloc`, all
  type `T` - the same four-symbol surface D-002 recorded
  natively and D-005's E2 recorded for x86-64. This
  proves the clang AArch64 backend generates Android-triple
  code for the one TU that needs no libc headers. It
  proves nothing hosted: no bionic header, library, or
  linker was involved.
- **E2 - hosted Android-target attempts, core and
  platform TUs.** The same target triple without
  `-nostdinc`: both fail, exit 1. The failure mode is
  itself the evidence (INC-D006-01): with no Android
  sysroot, clang fell back to the HOST glibc headers
  (`/usr/include/stdint.h`) and died inside them at
  `bits/libc-header-start.h` - a header the Android
  target cannot satisfy from a glibc tree. There is no
  bionic `string.h` or `sys/mman.h` anywhere on this Pi
  for the target to find. A hosted Android build is
  impossible here today, and any future compile that
  does not pin the NDK sysroot risks silently building
  against the wrong libc's headers.
- **E3 - adb.** `adb version` and `adb devices`:
  `command not found`, exit 127. There is no device road
  from the Pi, independent of the NDK question.

## 5. API-level plan (D-001 T6: the owed choice, narrowed)

The tablet on record is the Samsung SM-X400: Android 16
/ **API 36** (field-notes snapshot via D-001 Section 8
item 1; re-confirmation at run time is D-007's duty).
Two further API levels are evidenced in the banked
record: Termux's clang on the tablet defaulted to target
`aarch64-unknown-linux-android24` (**API 24**, Field 04
via A-003), and the field notes' one bionic-linked
diagnostic needed an explicit **API 29** target (Field
04 via D-001 T6) - proof in the record that API level
changes bionic symbol availability and is not a
formality.

**Proposed plan (labelled Proposed; the decision itself
stays OWED - INC-D006-04):** build for a matrix of API
24 (the lowest level with any evidence in this repo),
API 29 (the level the record shows changing symbol
availability), and API 36 (the flagship device), with
API 24 as the proposed minimum in the build script's
`ANDROID_API` default. No single minimum is *decided*
by this slice: the deciding evidence - which bionic
symbols the allocator actually binds at each level,
read off an NDK build with `llvm-nm`/`llvm-readelf` -
cannot exist until the NDK lands. Any slice that named
a minimum without that inspection would be inventing it.

## 6. API-level / bionic contract table for the four public symbols

Grounding, stated once for the whole table: the four
symbols are declared in `include/mimir_alloc.h` with a
plain C11/C ABI (Decision D-006 in the decision ledger).
In `docs/contracts.md` (A-008) they have **no direct
contract rows**: the namespaced API is
[UNRESOLVED: U-03], outside Section 6 scope, Appendix A
stability *Unassigned* - every contract statement below
is **by analogy only** to the core-symbol rows, Proposed
only, never an ABI or symbol-version promise (header
U-03 caveat; B-008 N-10). D-002 recorded the native ELF
facts: the API object defines exactly these four symbols
(type `T`, nothing else), and allocations measured
page-aligned on the native build. The libc column is
this slice's own source inspection: beneath all four
symbols the library uses only `mmap`/`munmap`
(`sys/mman.h`), `sysconf(_SC_PAGESIZE)` (`unistd.h`),
a statically-initialised `pthread_mutex_t`
(`pthread.h`), and `memcpy`/`memset` (`string.h`).

| Symbol | Signature | Contract (by analogy, Proposed) | Bionic / API-level status |
|---|---|---|---|
| `mimir_malloc` | `void *mimir_malloc(size_t)` | Success: non-null, distinct storage, suitably aligned for any fundamental-alignment type (MA-6.1-MALLOC-01). Failure - including oversize/unrepresentable - is exactly a null return, never a partial block (MA-6.1-MALLOC-02/-04, MA-6.2-020/-024). Zero-size is [UNRESOLVED: U-09]; errno is NOT promised (U-10); no numeric alignment value is claimed (U-15) | Project symbol - no bionic version gate of its own. Its bionic exposure is entirely through the shared deps below (mmap, sysconf, pthread, memset). Availability at API 24/29/36: **OWED** - to be read off the NDK build, not assumed |
| `mimir_free` | `void mimir_free(void *)` | Live pointer retired exactly once (MA-6.1-FREE-01). `mimir_free(NULL)` is a defined no-op (MA-6.1-FREE-02, MA-6.2-010). Foreign/interior/double free is undefined behaviour with no defined error (MA-6.1-FREE-03/-04) | As above: no direct bionic call in the API TU; deps shared. **OWED** as above |
| `mimir_calloc` | `void *mimir_calloc(size_t, size_t)` | Success: suitably aligned, requested extent zero at return (MA-6.1-CALLOC-01/-05). Product overflow is checked before the multiplication is relied on: exactly a null return, no wrapped allocation (MA-6.1-CALLOC-02). Other failure: null only (MA-6.1-CALLOC-03). Zero factor [UNRESOLVED: U-09]; errno not promised (U-10) | As above, plus `memset` beneath the zeroing. **OWED** as above |
| `mimir_realloc` | `void *mimir_realloc(void *, size_t)` | Success: first min(old, new) bytes preserved byte-for-byte, move or stay; grow tail unspecified, not zeroed (MA-6.1-REALLOC-01/-06). Failure: null, original block valid, unchanged, still caller-owned (MA-6.1-REALLOC-02). `mimir_realloc(NULL, n)` acts as `mimir_malloc(n)` (MA-6.1-REALLOC-03). `mimir_realloc(ptr, 0)` is [UNRESOLVED: U-12] - outside the supported list | As above, plus `memcpy` beneath the preserving move. **OWED** as above |

Shared bionic notes (all OWED until the NDK build
exists; nothing here is a bionic claim):

- **Page size.** The platform layer queries
  `sysconf(_SC_PAGESIZE)` at runtime, failure-checked,
  and D-003 proved no fixed page-size constant exists in
  any production source. That is the design Android's
  4 KiB / 16 KiB split requires (Roadmap Section 12
  item 10); running proof in a 4 KiB environment remains
  OWED (D-003 INC-D003-02, D-001 T9).
- **Mapping.** `mmap`/`munmap` are called with
  `PROT_READ | PROT_WRITE` and `MAP_PRIVATE |
  MAP_ANONYMOUS` from the platform layer as inspected;
  Android's sandbox, linker-namespace, and lifecycle
  behaviour around those calls is device evidence
  (D-007), not a build claim.
- **Locking.** One global `pthread_mutex_t` with
  `PTHREAD_MUTEX_INITIALIZER`; no Android-specific
  initialisation entry point exists or is promised
  (allocation-free startup is C-006's audited ground).
- **What the NDK build must record** to discharge this
  section: per API level (24/29/36), the `llvm-nm`
  undefined-symbol list of the three library objects
  against the bionic sysroot, the `llvm-readelf -d`
  dependency list of the linked test binary (its
  interpreter and `libc.so` dependency), and the ELF
  segment alignment - the Android counterpart of
  D-002's native record.

## 7. The four Android environments, kept distinct

Every future Android record names exactly one of these
(D-001 Section 8 item 3). Results are never merged
across them, and no row's pass promotes another row.

| Environment | What it is | Where it runs | What a pass would prove | Status for MimirAlloc |
|---|---|---|---|---|
| **NDK command-line binary** | A bionic-linked executable built with the NDK clang driver for a named API level, run from a device shell (adb or on-device terminal) | The Android device, outside any app sandbox | The allocator builds against bionic and runs on the device's kernel/page size - the Phase 4 centre (T11) | **OWED** - no NDK (T2), no device road (T4) |
| **Packaged native app** | The allocator inside a packaged Android application (APK/AAB with native library), under the app sandbox and lifecycle | The Android device, inside the sandbox | Real application behaviour: sandbox, linker namespaces, background/low-memory lifecycle (T13, Section 12 item 6) | **OWED** - no NDK, no packaging step, no device road; no packaged Android artifact exists in the repo |
| **Termux** | The Termux terminal app on the tablet: its own prefix, its own clang (field record: Clang 21.1.8, default target android24). Two distinct sub-environments - **bionic Termux** and the separate **Termux glibc userspace** the field work also exercised - must be recorded as different environments | The Android device, inside Termux's app sandbox and permission limits | Developer-workflow compilation and execution in Termux, with permission limits documented (T12; Roadmap Section 5). **Never native-app proof** (Guide; A-003) | **OWED** for MimirAlloc. Environment field-investigated on the SM-X400; no MimirAlloc compile or run in either sub-environment exists. On the Pi itself Termux does not exist (probe: absent) - Termux checks belong on the tablet |
| **proot Linux userland** | A Linux distribution userland run under `proot` on Android (field record: proot-distro 5.10.3 installed on the tablet; its `list` command refused inside an already-traced session, Field 04) | The Android device, inside a ptrace-emulated Linux userland | Convenience testing only, independently labelled. **Never native Android proof** (Roadmap Section 1 non-goals; A-003) | **OWED** - no proot result of any kind exists for MimirAlloc; `proot` is also absent on the Pi (probe) |

## 8. The build plan, in order (executes when the NDK lands)

0. **Host check.** Confirm the NDK's prebuilt LLVM
   toolchain actually executes on the chosen build host
   (INC-D006-02). If the Pi cannot run it and Box64
   cannot either, the build host moves - the *device*
   leg does not move: tests still run on the SM-X400.
1. **Preflight + build.** Set `ANDROID_NDK_HOME`; run
   `scripts/android/build_android.sh` for API 24, then
   29, then 36. Each run must produce the ELF check, the
   four-symbol `llvm-nm` record, and the `llvm-readelf`
   dependency record of Section 6 - or be reported as
   the failure it is.
2. **Bionic inspection.** Discharge Section 6's OWED
   column from those records (with D-008's three-libc
   contract work), including ELF segment alignment for
   the 4 KiB / 16 KiB question.
3. **Device road (D-007).** With `adb` (or the on-device
   route D-007 defines), place the API-36 binary on the
   SM-X400, re-confirm the Section 12 inventory at run
   time, and run allocate/fill/verify/free plus the
   bounded multithread test on device, with logs. Only
   this step can move Phase 4 toward its exit.
4. **Termux and app legs (D-007).** Separately labelled,
   separately recorded, per Section 7. A proot run, if
   one is ever taken, is labelled convenience evidence
   and promotes nothing.

## 9. What does NOT count (held from D-001 T2/E5)

A glibc build of any kind; a Termux glibc build;
`clang --print-targets` listing `aarch64`; this slice's
E1 freestanding object (no bionic header or library was
involved); a cross-compile without the on-device run;
an emulator run. If Wave 4 ends with NDK cross-compiles
but no physical Android run, Phase 4 is UNMET and D-009
must say exactly that.

## 10. Inconsistencies recorded (INC-D006-*)

- **INC-D006-01** - Host-header fallback: with no
  Android sysroot, clang's Android-target compile
  silently substitutes HOST glibc headers before failing
  (E2). A less careful invocation could produce a
  wrong-libc object instead of an error. Only
  sysroot-pinned NDK builds count as Android builds.
- **INC-D006-02** - NDK host architecture: the proposed
  build script defaults to the NDK's `linux-x86_64`
  prebuilt toolchain path, but the campaign's build host
  (this Pi) is AArch64. Whether the NDK toolchain runs
  here (natively or via D-005's Box64 road) is
  UNVERIFIED; the script exposes `NDK_TOOLCHAIN` for
  exactly this reason. Recorded as the plan's step 0.
- **INC-D006-03** - Probe artefact: the probe's
  `env | grep -i android` matched the shell's `_`
  variable (the probe's own path) in the recorded run.
  No `ANDROID_*` variable exists; the match is the
  probe observing itself, recorded as-is in Section 1
  of the recorded run rather than cleaned up.
- **INC-D006-04** - D-001 T6 asks D-006 to choose the
  target API level(s). This slice narrows the choice to
  an evidenced matrix (24/29/36, Section 5) but does
  NOT decide a single minimum: the deciding evidence
  (per-level bionic symbol binding) cannot exist
  without the NDK. The build script's API 24 default is
  a Proposed starting point, not a decision.
- **INC-D006-05** - The probe's `dpkg` grep hits are
  false positives (`mariadb` contains "adb"). No Debian
  package on the Pi provides Android tooling; the hits
  are recorded verbatim in the recorded run so no later
  reader mistakes them for an adb presence.
- **INC-D006-06** - Name matches are not toolchains:
  the slice's wider sweep found .NET Android workload
  manifests and unrelated `android`-named files across
  the home directory. None contains an NDK, a bionic
  sysroot, or an Android clang driver. Only a tree with
  `toolchains/` and `ndk-build` counts as an NDK, and
  none exists.

## 11. Self-test checklist (solo slice - tester role)

Executed by the team lead in the tester role on the Pi,
2026-11-06, against the finished worktree:

1. HEAD verified `18028d2...` on `yrsa/mimiralloc-06`
   before any work; base matches the D-005 bank. PASS
2. Probe script executed fresh; every ABSENT in
   Section 2 of this doc traces to a probe line in
   recorded run Section 1. PASS
3. Build script preflight executed; exit 3 with the
   BLOCKED message recorded (run Section 2); the script
   is labelled PROPOSED/unexecuted in its header and in
   this doc, and no text anywhere calls it executed.
   PASS
4. E1/E2/E3 outputs read back from recorded run
   Section 3 and quoted exactly (including E2's actual
   `bits/libc-header-start.h` failure, not a paraphrase).
   PASS
5. `make clean && make test` exit 0 (run Section 4),
   re-run after the Makefile edit, exit 0 (Section 6);
   `make test-d006` exit 0 (Section 6). PASS
6. Contract table checked line by line against
   `include/mimir_alloc.h` and `docs/contracts.md`:
   every statement carries its analogy citation or its
   U-item; no bionic availability is asserted anywhere.
   PASS
7. `README.md` untouched (git status shows no README
   change); no file outside the slice's deliverables
   modified except the additive Makefile target. PASS
8. Phase 4 exit stated UNMET in the verdict, Section 9,
   and here; the Android device run is named OWED, never
   implied. PASS

**Tester verdict: PASS** - the slice claims only what
was run or read on the Pi, and its blocker is exact.

## 12. Exit status

**Phase 4 exit: UNMET.** D-006 discharges D-001's T2/T6
as far as a host without an NDK allows: the readiness
verdict is live-verified, the blocker names the single
missing component and the smallest next step, and the
build plan plus proposed script are ready to execute.
Still OWED, by name: an Android NDK, an NDK build with
its bionic inspection record, a device road to the
Samsung SM-X400, and every on-device run (D-007).
