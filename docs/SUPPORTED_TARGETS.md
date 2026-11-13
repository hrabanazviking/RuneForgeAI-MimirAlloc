# MimirAlloc Supported-Target Matrix

Slice: A-003 (Phase 0 exit evidence E2; the companion baseline
deliverable, BASELINE_INVENTORY.md, is Phase 0 exit evidence E4).
Refreshed in place by slice M-006 (Wave 13, Phase 13 target-matrix
verification), 2026-11-07, at base commit
c6e57a387ecc57994913282fc23e57d198cb7744 (Wave 13, 5 of 9 banked;
M-006 assignment base 4e3bc36504f6fd4450154c7f500a3821806908d3,
reconciled forward by the keeper - see INC-M006-01 in the
verification document). This refresh discharges INC-I009-01 /
INC-D009-02. Verification record:
docs/PHASE_13_TARGET_MATRIX_M006.md. Raw evidence:
tests/abi/recorded_run_m006.txt.
Status: research document, evidence-graded. Phase 13 as a whole
is not exited by this slice; M-009 grades the Phase 13 exit.
Derived from this repository: the Research and Development
Roadmap (cited as Roadmap with section), the Engineering Guide
(cited as Guide), the research library under docs/research/
(cited as Research NN), the Android ARM64 field notes under
docs/field-notes/android-arm64-2026-11/ (cited as Field NN), the
banked slice records D-001 to D-009 with the Phase 3 and Phase 4
exit packets, and the fresh runs and probes of this slice.
Pi-observed facts are kept in a separate section at the end, as
A-003 kept them; fresh M-006 observations are dated 2026-11-07.

## 1. The honest headline

Exactly one target is supported today, in exactly one scope:
AArch64 Linux with glibc on this Pi class (Raspberry Pi 5,
16 KiB pages, Debian 12, glibc 2.36), for the native suite scope
run fresh in this slice on 2026-11-07 (make test, exit 0; the
closing D-004 suite line reads CHECKS: 4590 run, 0 failed; the
shared library exports exactly the 4 public symbols). Every
other row of this matrix is PARTIAL-RUN, ANALOGY-ONLY, BLOCKED,
OWED, or NOT-A-TARGET, and says so in its row. A reader who
reads only this headline must not be misled: no other target
carries a run of this allocator today.

Grades (the M-006 taxonomy; every row and every differing cell
carries exactly one):

- VERIFIED-RUN: the stated suite or subset was built for and
  executed on this target environment in this slice, or in a
  named banked slice whose recorded run is cited, and passed
  with the check counts quoted.
- PARTIAL-RUN: part of the target genuinely executed and part
  did not; the row states exactly which part ran and which
  part remains unrun, with both halves evidenced.
- ANALOGY-ONLY: no execution exists; the row rests on a named
  documented analogy and states what the analogy does not
  establish. No run is claimed.
- BLOCKED: execution was attempted or prepared and stopped on
  a named, verified blocker, with the smallest next step that
  removes it and whose word it waits for, where known.
- OWED: no execution exists; the discharge event is a future
  run or external provision, named in the row itself.
- NOT-A-TARGET: a roadmap-adjacent entry the project does not
  claim (research-only or out of scope), recorded so the
  matrix is complete; the row quotes its basis.

A-003 headline history: A-003 stated that no target was
supported and recorded zero measured baselines. That was true
at the Phase 0 base, when no allocator code existed. It has
been stale since the implementation waves landed; D-009
flagged the staleness (INC-D009-02) and I-009 carried it
(INC-I009-01). This refresh replaces the stale headline with
the graded matrix below; the A-003 honesty discipline
(targets kept separate from Pi-observed facts) is preserved
exactly.

Row kinds (unchanged from A-003): the Roadmap Sec. 5 rows are
not all the same kind of thing. Linux AArch64 + glibc, Linux
x86-64 + glibc, Linux AArch64 + musl, and Android ARM64 +
bionic are platform rows; Termux and proot are environment
rows hosted on Android; the SM-X400 and the Raspberry Pi are
physical-device rows that instantiate platform rows. They
must not be counted as eight separate platforms.

## 2. Target matrix (Roadmap Section 5, graded at M-006)

| Target | Grade | Initial role (Roadmap Sec. 5) | Required evidence (Roadmap Sec. 5) | Evidence on record, with pointer and date |
|---|---|---|---|---|
| Linux AArch64 + glibc, 16 KiB pages | VERIFIED-RUN | First production candidate | Native CI, multithread tests, ABI and linker proof | Fresh in this slice, 2026-11-07, on purpleviking1: make test exit 0 (8 suites: smoke, core, B-007, C-005, C-006, C-007, C-008, D-004 multithread); closing line CHECKS: 4590 run, 0 failed; make lib plus nm -D --defined-only shows exactly 4 symbols (mimir_malloc, mimir_free, mimir_calloc, mimir_realloc). tests/abi/recorded_run_m006.txt. Native CI is not claimed: no CI run exists (manual-only by standing rule); the run is a real-device local run, not CI. |
| Linux AArch64 + glibc, 4 KiB pages | OWED (run); ANALOGY-ONLY (design support) | First production candidate | Same as above, on a 4096-page host | No 4096-page host exists in this campaign. Design support only: D-003 audit proved page size is runtime-queried in every code path (src/platform sysconf(_SC_PAGESIZE) on every call) with checked arithmetic at 4096 against an __int128 oracle (docs/PAGE_SIZE_PORTABILITY.md, INC-D003-02). That audit establishes portability of the design, not a run. Discharge: run the suite on a 4096-page AArch64 host and record it. |
| Linux x86-64 + glibc | PARTIAL-RUN (freestanding + API-TU legs); OWED (hosted suite) | Development / reference platform | Same correctness tests; useful for sanitizers and differential testing | Ran (emulated, labelled as such): the D-005 freestanding probe re-ran fresh in this slice via the banked script - compile, link, run via binfmt and via explicit Box64 0.4.5, output D005-X64-PROBE-OK, exit 42 twice (make test-d005, exit 0; recorded_run_m006.txt). The API translation unit cross-compiled fresh for x86_64-linux-gnu (D-005 E2 refreshed; exit 0; object defines exactly the 4 public symbols); core and platform TUs fail on exactly the missing libc headers (string.h, sys/mman.h). Not run: the hosted suite. Blocker: no x86-64 libc sysroot on this Pi (fresh probe: x86_64-linux-gnu-gcc ABSENT, /usr/x86_64-linux-gnu ABSENT); the full-build script preflight refuses with exit 3, recorded in this slice. Smallest next step: install gcc-x86-64-linux-gnu - reserved for Volmarr word, NOT given (D-005). The row must never be read as suite-verified. Phase 3 exit UNMET stands on this leg. |
| Linux AArch64 + musl | BLOCKED | Compatibility target | Correct compile and runtime; interposition assessed separately | Fresh probe, 2026-11-07: musl-gcc ABSENT, musl-clang ABSENT, /usr/lib/musl ABSENT, no ld-musl on the system (recorded_run_m006.txt). Blocker named exactly: no musl toolchain or sysroot on this Pi. Smallest next step: musl-tools install - reserved for Volmarr word, NOT given (D-008 precedent, INC-D008-06). No musl behaviour is claimed; the D-008 contract record is cited, not extended, and nothing is inferred from glibc behaviour. |
| Android ARM64 + bionic | OWED (run); ANALOGY-ONLY (contract cells) | Separate native port | NDK build, Android application / native test, device ABI and page-size validation | Fresh probes, 2026-11-07: ndk-build, sdkmanager, aarch64-linux-android clang drivers, adb, and every NDK/SDK path ABSENT on this Pi (banked android probe script re-run; recorded_run_m006.txt), agreeing with D-006. No NDK build, no device run, no bionic sysroot exists. The D-008 bionic contract table remains analogy only (U-03) and is labelled as such here: it reasons about bionic contract behaviour, it establishes no run. Discharge: Android NDK provision on a chosen build host plus a device session per the D-007 packet. Phase 4 exit UNMET stands in part on this row. |
| Termux on Android | OWED | Developer workflow | Native bionic compilation and execution; permission limits documented | Environment investigated on one SM-X400 in the field notes (Field 01 to 14; Termux Clang 21.1.8, default target aarch64-unknown-linux-android24, Field 04) - that evidence never ran MimirAlloc. No MimirAlloc compilation or execution in Termux exists, in either its bionic or glibc userspace. Fresh probe on this Pi: no Termux environment present (expected; Termux lives on the device). Discharge: a Termux session on the physical device, separately labelled, after the NDK-binary run (D-007 order). |
| Linux userland in Android proot | OWED | Convenience test environment | Independently labeled proot results; not treated as native Android app proof | No proot result for MimirAlloc is recorded anywhere. Fresh probe, 2026-11-07: proot and proot-distro ABSENT on this Pi; the field-notes proot-distro 5.10.3 was on the tablet and refused to run there (Field 04). Discharge: an executed proot run, recorded as a proot result only (Roadmap Sec. 10 attribution). A proot result never promotes any Android row. |
| Samsung SM-X400 physical tablet | OWED | Flagship constrained device | Measured page size, memory conditions, stress and device responsiveness | Device inventoried (Field 02: Android 16 / API 36, eight cores, page size 4,096 bytes in one field probe) and environment probed (Field 01, 03). No MimirAlloc test, stress run, or responsiveness measurement on the device exists. The D-007 on-device program is ready and native-checked on the Pi (59 checks) - a Pi logic check, NOT an Android result. Discharge: a physical-device session per the D-007 packet and the Roadmap Sec. 12 checklist, with page size measured on the device in-run. |
| Raspberry Pi ARM64 Linux | VERIFIED-RUN | Secondary physical ARM target | Real-device test, OS / compiler / kernel manifest and performance data | This row is the physical instantiation of the first row, graded on the same evidence: the M-006 fresh suite run above WAS the real-device test, executed on purpleviking1 (Raspberry Pi 5 Model B Rev 1.1, kernel 6.12.112-v8-16k-hailo+, Debian 12, GCC 12.2.0, Clang 14.0.6, glibc 2.36, PAGESIZE 16384 measured live; full manifest in recorded_run_m006.txt). Performance data for this device is banked Phase 11 evidence (docs/PHASE_11_*), cited here, not re-measured in this slice. |
| macOS ARM64 | OWED | Named only as a possible later target (Roadmap Sec. 5, closing paragraph) | Its own allocation ABI and OS interface work; not claimed until tested (Roadmap Sec. 5) | Unevidenced. Nothing else in the repository names macOS. Graded OWED rather than NOT-A-TARGET because the roadmap defers this entry ("Later targets may include...") instead of excluding it. Discharge: a future charter plus a real run on macOS hardware. |
| Windows ARM64 | OWED | Named only as a possible later target (Roadmap Sec. 5, closing paragraph) | Its own allocation ABI and OS interface work; not claimed until tested (Roadmap Sec. 5) | Unevidenced. Nothing else in the repository names Windows ARM64. OWED on the same deferral basis as macOS ARM64. Discharge: a future charter plus a real run on Windows ARM64 hardware. |

## 3. Research-only targets (not in the Roadmap Section 5 matrix)

Research 03 (Target matrix) names two further rows that the
Roadmap Section 5 matrix does not. They are research subjects,
not MimirAlloc targets, and are recorded here so the matrix
is complete rather than silently incomplete:

| Target | Grade | What the document says | Evidence on record |
|---|---|---|---|
| RISC-V Linux (RV64) | NOT-A-TARGET | Distinguish ISA extensions, libc, compiler, atomics, kernel; first proof is native / emulated tests clearly labelled (Research 03) | Unevidenced. Fresh probe, 2026-11-07: no RISC-V toolchain on this Pi (riscv64-linux-gnu-gcc ABSENT). Exclusion basis: Research 03 only; the Roadmap Sec. 5 matrix does not name it. |
| Microcontroller / RTOS | NOT-A-TARGET | Separate design profile; OS-mapping assumptions invalid; distinguish RAM map, MMU / MPU, ISR rules, no virtual memory (Research 03) | Unevidenced. No device, build, or test is recorded. Exclusion basis as above; see also I-10, still standing. |

## 4. Architecture facts, each cited to its source

Preserved from A-003 where still true (every statement below
was re-checked at M-006); M-006 additions are marked.

Page size and address space:

- Page size must be queried at runtime (for example sysconf(_SC_PAGESIZE))
  and checked for failure; hardcoded 4096-byte page alignment must be
  avoided. A 16 KiB Android system changes ELF / load alignment and
  mapping assumptions, not just a constant (Research 03, Page size,
  address space and alignment). M-006: this discipline is now proven
  in the banked code, not only prescribed - D-003 audited every code
  path (docs/PAGE_SIZE_PORTABILITY.md) and the M-006 run measured
  PAGESIZE 16384 live on this Pi.
- Android documents 16 KiB page support alongside 4 KiB configurations
  (Roadmap Sec. 2, source S3; also Roadmap Sec. 12: support 4 KiB and
  16 KiB system-page environments).
- The SM-X400 tablet system page size was 4,096 bytes in one probe;
  other Android devices can differ (Field 02).
- The actual page size of the specific SM-X400 had to be measured; the
  Roadmap lists it as an open measurement, not a given (Roadmap Sec. 2;
  Sec. 15 question 6). M-006: still owed as an in-run record from an
  allocator session on the device (D-007 program self-records it).
- Virtual-address width depends on hardware, kernel configuration, and
  actual OS behavior; do not assume all ARM64 machines expose the same
  usable address bits (Research 03, Page size, address space and
  alignment).
- On the SM-X400, a 4 KiB fixed-no-replace mmap probe succeeded at
  2^38 and failed with ENOMEM at 2^39 and 2^47, consistent with a 39-bit
  user virtual-address boundary on that device; the notes warn this is
  not a complete kernel audit or a cross-device guarantee (Field 03;
  Field 01 evidence ledger).
- The Mojo compiler on that tablet aborted in TCMalloc MmapAligned()
  with a diagnostic saying it assumed a 48-bit virtual address space
  (Field 03). TCMalloc public documentation lists Linux AArch64 as
  a covered platform (Roadmap Sec. 2, source S1) - see
  inconsistency I-4 below. (That listing is about TCMalloc, not
  about MimirAlloc.)

ABI and libc:

- AArch64 Linux and Android ARM64 use different operating-system ABIs
  and libc environments (Guide, Important technical distinctions).
- A successful proot test does not prove a native Android application
  works (Guide; Roadmap Sec. 1 non-goals; Field 04).
- Android uses bionic and its own dynamic loader, linker namespaces,
  application sandbox, and API-level availability; a GNU/Linux AArch64
  executable can require glibc, a different loader, libraries, and
  syscall behavior (Field 04).
- The Linux ARM64 Mojo package ELF requested /lib/ld-linux-aarch64.so.1,
  which was not present at that path in the tablet Android environment;
  invoking the Termux glibc loader directly got farther (Field 03).
- Dynamic symbol interposition is not guaranteed to replace static,
  hidden, or explicitly linked allocator calls (Guide; Roadmap Sec. 5
  requires interposition to be assessed separately for musl).
- Android ABI details are Roadmap source S10 (Android NDK ABI guides).
- M-006 addition: the native ABI surface is now measured, not only
  specified - the shared library exports exactly the 4 public symbols
  (Section 2, first row), and D-008 audited the allocator dependency
  set (memcpy / memset / mmap / munmap / sysconf / pthread only;
  errno never touched).

Memory model and cores:

- Do not assume x86-like ordering; portable C / C++ atomics still need a
  correct happens-before design, and RISC-V RVWMO plus optional atomic
  extensions make actual target capability relevant (Research 03, Weak
  memory and atomics).
- The SM-X400 /proc/cpuinfo exposed eight ARM64 cores: four Cortex-A55
  and four Cortex-A78, with reported maximum frequencies of 2,002,000 kHz
  (CPU 0) and 2,400,000 kHz (CPU 4); reported maximums, not measured
  sustained clocks (Field 02).
- Large and small cores can differ in speed and cache behavior, and a
  thread can migrate; benchmark affinity-controlled and normal
  scheduling separately (Research 03, Heterogeneous cores).

Toolchain notes (all from the tablet field work, not from any
MimirAlloc build; preserved unchanged from A-003):

- Termux Clang 21.1.8, default target aarch64-unknown-linux-android24;
  CMake 4.4.5; Ninja 1.13.2; glslang 16.6.0; Vulkan headers 1.4.367;
  proot-distro 5.10.3; patchelf 0.19.3 (Field 04).
- Separate Termux glibc repository: glibc 2.44, GCC 14.2.1, Python
  3.12.9 (Field 04).
- Termux Bazel 9.3.1 and OpenJDK 21 were installed for the guarded
  TCMalloc build attempt (Field 06).
- Leaving the inherited Android LD_PRELOAD in place caused glibc
  commands to fail with an invalid ELF header in that context;
  environment hygiene is part of a reproducible experiment (Field 04).

## 5. Pi-observed facts (kept separate from target claims)

### 5.1 Team-03, 2026-11-06, device purpleviking1 (A-003, preserved)

These facts were observed by Team-03 on the Raspberry Pi itself
and are NOT document-derived claims. Full command log is in
BASELINE_INVENTORY.md, Section 5. At A-003 they described the Pi
environment only; no MimirAlloc code ran.

- uname -m printed aarch64.
- Kernel: Linux purpleviking1 6.12.112-v8-16k-hailo+ aarch64 (full
  uname -a string in BASELINE_INVENTORY.md).
- getconf PAGE_SIZE printed 16384, that is a 16 KiB page size on this
  Pi, under a kernel whose name carries 16k. This differs from the
  SM-X400 tablet probe of 4,096 bytes (Field 02) and confirms in
  practice why no single page size may be assumed (see I-5).
- getconf LONG_BIT printed 64. nproc printed 4.
- Toolchain present on the Pi: GCC 12.2.0 (Debian 12.2.0-14+deb12u1),
  Clang 14.0.6 (Debian), glibc 2.36 (Debian GLIBC 2.36-9+rpt2+deb12u14),
  OS Debian GNU/Linux 12 (bookworm).

### 5.2 M-006 refresh observations, 2026-11-07, device purpleviking1

Observed fresh in this slice; full raw record in
tests/abi/recorded_run_m006.txt. Unlike 5.1, MimirAlloc code DID
run in this slice - the suite run graded in Section 2.

- uname -a: Linux purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP
  PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64 GNU/Linux.
  uname -m: aarch64. Unchanged from 5.1.
- getconf PAGESIZE and getconf PAGE_SIZE both printed 16384.
  getconf LONG_BIT printed 64. nproc printed 4.
- gcc / cc: Debian 12.2.0-14+deb12u1 (12.2.0). clang: Debian
  14.0.6. ldd: Debian GLIBC 2.36-9+rpt2+deb12u14 (2.36).
  OS: Debian GNU/Linux 12 (bookworm). All unchanged from 5.1.
- Native suite: make clean, then make test exit 0; suite lines
  29 / 69 / 95 / 23 / 122 / 42 run with 0 failed (42-line suite
  carries 7 skipped-not-yet), closing D-004 line CHECKS: 4590
  run, 0 failed. make lib exit 0; nm -D --defined-only on
  build/libmimiralloc.so lists exactly 4 symbols: mimir_malloc,
  mimir_free, mimir_calloc, mimir_realloc.
- x86-64 run leg present: box64 at /usr/local/bin/box64, Box64
  arm64 v0.4.5, binfmt_misc entry enabled. Freestanding probe
  re-run PASS (Section 2). qemu-x86_64 ABSENT.
- x86-64 build leg absent: x86_64-linux-gnu-gcc ABSENT,
  /usr/x86_64-linux-gnu ABSENT, /usr/include/x86_64-linux-gnu
  ABSENT. Full-build script preflight exit 3 (BLOCKED), recorded.
- musl absent: musl-gcc ABSENT, musl-clang ABSENT,
  /usr/lib/musl ABSENT, no ld-musl file on the system.
  apt candidate musl-tools 1.2.3-1 exists but is NOT installed;
  install is reserved for Volmarr word.
- Android absent: ndk-build, sdkmanager, adb, and every
  aarch64-linux-android clang driver ABSENT; ANDROID_NDK_HOME,
  ANDROID_HOME, ANDROID_SDK_ROOT all empty; no NDK/SDK path
  present (banked probe script re-run in full).
- proot absent on this Pi: proot ABSENT, proot-distro ABSENT,
  termux-info ABSENT, no /data/data/com.termux tree.
- RISC-V absent: riscv64-linux-gnu-gcc ABSENT, no riscv
  compiler in /usr/bin.
- House health during the slice: free -g showed 13 GiB
  available before and after; Samtal on port 8787 answered
  HTTP 200 before and after. Nothing was installed.

## 6. Inconsistencies and gaps, revisited at M-006

A-003 recorded I-1 to I-10 (the J-1 to J-5 era material lives
in BASELINE_INVENTORY.md and is not rewritten here; staleness
found there is INC-M006-03 in the verification document). Each
item below was re-checked where this refresh touches it:

- I-1. Research 03 lists RISC-V Linux and Microcontroller / RTOS in
  its target matrix; the Roadmap Section 5 matrix does not list
  either. Status at M-006: stands, resolved by separation - both
  rows are graded NOT-A-TARGET in Section 3, neither promoted nor
  silently dropped.
- I-2. macOS ARM64 and Windows ARM64 appear only in one closing
  sentence of Roadmap Section 5 as possible later targets, and in
  no other roadmap phase, test matrix, or research document.
  Status at M-006: stands; both rows graded OWED (deferred, not
  excluded) in Section 2.
- I-3. Public Modular source selects jemalloc for its Linux Mojo
  driver, yet the distributed Mojo wheels aborted with a TCMalloc
  error; the package relationship was not established (Field 03;
  Field 01). Status at M-006: unchanged; out of this slice scope.
  Any baseline built from public source may still not match the
  shipped wheels.
- I-4. TCMalloc documents Linux AArch64 coverage (Roadmap Sec. 2,
  S1), yet the specific Mojo build on the SM-X400 failed on a
  48-bit virtual address assumption (Field 03). Status at M-006:
  unchanged. Documented platform coverage and a specific device /
  build / ABI combination remain different claims.
- I-5. Page size is not one value across named targets: SM-X400
  probed 4,096 bytes (Field 02), Android also ships 16 KiB
  configurations (Roadmap S3), and 16,384 bytes is measured on
  this Pi (Section 5). Status at M-006: confirmed again by the
  fresh PAGESIZE reading; the 4 KiB allocator run stays OWED.
- I-6. The README banner describes the project as an allocator
  project for ARM, Android, Mojo, and resource-conscious
  computing, while the A-003-era status statements said nothing
  was implemented or verified. Status at M-006: the banner is
  still a goal statement; the status half is now Section 1 of
  this document. The README itself is untouched by this slice.
- I-7. The Roadmap Section 5 row for the Raspberry Pi requires an
  OS / compiler / kernel manifest plus a real-device test and
  performance data. Status at M-006: DISCHARGED for the test
  and manifest halves - the manifest is Section 5 plus the
  recorded run, and the real-device suite test ran in this
  slice. Performance data remains banked Phase 11 evidence,
  cited rather than re-measured.
- I-8. musl is a named compatibility target (Roadmap Sec. 5) but
  no document records a musl version, build, device, or test.
  Status at M-006: stands, now graded BLOCKED with the blocker
  and the reserved install named in the row (Section 2).
- I-9. No document records an assumed virtual-address width per
  target row; the only measured width is the SM-X400 probe
  (Field 03). Status at M-006: stands. Per-target VA width is
  still unevidenced / TBD and is carried as INC-M006-04; no
  width is assumed anywhere in this refresh.
- I-10. The Microcontroller / RTOS research row sits uneasily
  with the baseline set (comparators assume an OS with virtual
  memory; that row states OS-mapping assumptions are invalid),
  and no document records a decision or exclusion rationale for
  MMU-less targets. Status at M-006: stands. The NOT-A-TARGET
  grade records the exclusion; the rationale decision itself
  remains owed to any future slice that proposes promotion.

## 7. What would change a grade

A row moves only when its Roadmap Section 5 required evidence
exists as a logged artifact, judged by the Roadmap Section 2
evidence levels (Proposed, Implemented, Host-tested,
Device-tested, Integration-proven, Performance-measured,
Blocked) and, for devices, the Research 03 device acceptance
gate (native build plus allocate / fill / verify / free,
alignment, overflow / failure, multiple threads, cross-thread
free, repeated startup / exit, pressure recovery, and
physical-device logs recording build hash, page size, OS, ABI,
and exact commands). A cross-compile or an emulator result is
a different evidence tier and never promotes a target by
itself (Research 03). In this document grading language: a
BLOCKED row moves when its named install or provision is
granted and the run then happens; an OWED row moves when its
named discharge event happens and is recorded; an ANALOGY-ONLY
cell is replaced only by a real run on that target, never by
stronger reasoning; a PARTIAL-RUN row completes only when its
named unrun half runs and passes in its own recorded run.

## 8. Z13 line refresh (added by Z-106, documentation truth pass)

Base: seat main 719b00d, banked Z-001..Z-105. Method: the
M-006 discipline of this document - a grade never exceeds
the banked evidence in this tree, and every changed cell
cites its recorded run. This section adds exactly one new
row and changes no other grade. The Section 1 headline is
therefore extended, not replaced: a second target is now
supported, in exactly one further scope, stated in the row.

| Target | Grade | Role on this line | Evidence on record, with pointer and date |
|---|---|---|---|
| Linux x86-64 + glibc, 4 KiB pages, ASUS ROG Flow Z13 (purpleraider) | VERIFIED-RUN | Z13 hardware line: primary measured target of the MimirAlloc Z13 campaign | Native hosted runs on the physical host (no emulation): the standing gate scripts/z13/run_gate.sh reads 4988 checks / 0 failed / 77 by-design skips repeatedly across the line, including the Z-100..Z-105 slice gates and the Z-106 baseline and final gates, all on this host (Ubuntu 26.10, kernel 7.3.0-10-generic, glibc 2.44, gcc 15.3.0, page size 4096 measured live; host truth annexes in tests/perf/recorded_run_z13_z100.txt and tests/abi/recorded_run_z13_z104.txt). Default library exports exactly 13 symbols; opt-in interposing variant exactly 20 (Z-105, tests/abi/recorded_run_z13_z105.txt). Sanitizer matrix clean: ASan, UBSan and combined builds each 4988/0/77, zero reports (Z-104). Reproducible on this host: two clean builds byte-identical (Z-105, same-host scope only). Performance is graded separately and honestly in docs/SUPPORTED_PLATFORMS.md (Z13 addendum): LOSS against best tuned glibc on all four oracle workloads (Z-098 / Wave 11 verdict section 4.5). Native CI is not claimed: no CI run exists (manual-only by standing rule); these are real-device local runs, not CI. |

What this refresh does NOT do:

- It does not re-grade the AArch64 rows. The Pi line keeps
  its VERIFIED-RUN grade on its own M-006 evidence; the Z13
  campaign ran no AArch64 suite, and no Z13 number may be
  read as an AArch64 result.
- It does not discharge the 4 KiB-page AArch64 row, the musl
  row, the Android row, Termux, proot, or the SM-X400 row.
  Those grades stand exactly as Sections 2 and 3 record
  them; the Z13 host is an x86-64 host, and a run there is
  evidence for the x86-64 row only.
- It does not generalise beyond the one named host: other
  x86-64 machines, kernels and glibc versions are unrun on
  this line until a run on them is recorded.
- It records no release: VERSION in-tree remains 0.1.0.
  The next cut is decided as 0.2.0 (D-017, Z-105) and is
  executed at the cut itself, which is the project owner
  act; no released 0.2.0 exists at this base, and 1.0
  remains withheld (contracts.md section 5.5 promotion
  unmet, U-03 unresolved).
