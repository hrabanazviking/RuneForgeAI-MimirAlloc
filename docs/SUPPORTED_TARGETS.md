# MimirAlloc Supported-Target Matrix

Slice: A-003 (Phase 0 exit evidence E2; the companion baseline
deliverable, BASELINE_INVENTORY.md, is Phase 0 exit evidence E4).
Status: research document. Phase 0 as a whole is not exited by this
slice; other Phase 0 items remain with their own slices.
Derived only from this repository: the Research and Development Roadmap
(cited as Roadmap with section), the Engineering Guide (cited as Guide),
the research library under docs/research/ (cited as Research NN), and the
Android ARM64 field notes under docs/field-notes/android-arm64-2026-11/
(cited as Field NN). Pi-observed facts gathered by this team are kept in a
separate section at the end and are also logged in BASELINE_INVENTORY.md.

## 1. The honest headline

No target is supported yet. The README states the repository currently
contains documentation, artwork, and an MIT license, and that the
allocator, installation package, benchmarks, Android support, and Mojo
integration are planned, not implemented or verified yet. The Roadmap
status line agrees: MimirAlloc does not yet exist as a verified
implementation (Roadmap, title block and Section 1). Every status below
therefore uses only two labels:

- Planned: named in the Roadmap Section 5 matrix as a candidate with a
  defined initial role and required evidence. Nothing has been built or
  tested for MimirAlloc on it.
- Investigational: named as a later, convenience, device-specific, or
  research-only environment. Evidence, where any exists, is about the
  environment itself, never about MimirAlloc running on it.

No cell in this document claims a target works, was tested with
MimirAlloc, or was measured with MimirAlloc, because no document in this
repository records any such result. Cells with no documentary evidence
are marked unevidenced / TBD.

Row kinds: the Roadmap Sec. 5 rows are not all the same kind of thing.
Linux AArch64 + glibc, Linux x86-64 + glibc, Linux AArch64 + musl, and
Android ARM64 + bionic are platform rows; Termux and proot are
environment rows hosted on Android; the SM-X400 and the Raspberry Pi
are physical-device rows that instantiate platform rows. They must not
be counted as eight separate platforms.

Evidence level (Roadmap Sec. 2 labels): every row in this matrix is
Proposed. No row reaches Implemented, Host-tested, Device-tested,
Integration-proven, or Performance-measured for MimirAlloc, because no
MimirAlloc code or test exists in the repository.

## 2. Target matrix (Roadmap Section 5, verbatim roles and evidence)

| Target | Status | Initial role (Roadmap Sec. 5) | Required evidence (Roadmap Sec. 5) | Evidence on record today |
|---|---|---|---|---|
| Linux AArch64 + glibc | Planned | First production candidate | Native CI, multithread tests, ABI and linker proof | None for MimirAlloc. Environment evidence only: a tiny glibc C program compiled and ran in a separate Termux glibc userspace on the SM-X400 tablet (Field 04); a small glibc C program also compiled and ran under the guarded-build wrapper setup (Field 06). Neither ran MimirAlloc. |
| Linux x86-64 + glibc | Planned | Development / reference platform | Same correctness tests; useful for sanitizers and differential testing | None for MimirAlloc. Note: the A.E.S.I.R. pixi.toml baseline observed in the field work listed linux-64, an x86-64 environment (Field 04). |
| Linux AArch64 + musl | Planned | Compatibility target | Correct compile and runtime; interposition assessed separately | Unevidenced / TBD. No musl build, run, or inventory is recorded anywhere in the repository. |
| Android ARM64 + bionic | Planned | Separate native port | NDK build, Android application / native test, device ABI and page-size validation | None for MimirAlloc. Environment evidence only: one bionic-linked C diagnostic (NNAPI probe) needed an explicit Android API 29 target (Field 04). No native Android MimirAlloc build or test exists. |
| Termux on Android | Investigational | Developer workflow | Native bionic compilation and execution; permission limits documented | Environment investigated on one SM-X400 (Field 01 to 14). Termux Clang 21.1.8 default target aarch64-unknown-linux-android24 (Field 04). No MimirAlloc compilation or execution in Termux is recorded. |
| Linux userland in Android proot | Investigational | Convenience test environment | Independently labeled proot results; not treated as native Android app proof | No proot result is recorded. proot-distro 5.10.3 was installed, and proot-distro list refused to run after detecting an already traced / proot-like session (Field 04). Unevidenced / TBD for any allocator result. |
| Samsung SM-X400 physical tablet | Investigational | Flagship constrained device | Measured page size, memory conditions, stress and device responsiveness | Device inventoried (Field 02) and environment probed (Field 01, 03). No MimirAlloc test, stress run, or responsiveness measurement exists. |
| Raspberry Pi ARM64 Linux | Investigational | Secondary physical ARM target | Real-device test, OS / compiler / kernel manifest and performance data | No MimirAlloc device test or performance data. This team recorded an OS / compiler / kernel manifest for the Pi named purpleviking1 on 2026-11-06; see Section 5 of this document and BASELINE_INVENTORY.md. That manifest is inventory, not a MimirAlloc test. |
| macOS ARM64 | Investigational (later) | Named only as a possible later target (Roadmap Sec. 5, closing paragraph) | Its own allocation ABI and OS interface work; not claimed until tested (Roadmap Sec. 5) | Unevidenced / TBD. Nothing else in the repository names macOS. |
| Windows ARM64 | Investigational (later) | Named only as a possible later target (Roadmap Sec. 5, closing paragraph) | Its own allocation ABI and OS interface work; not claimed until tested (Roadmap Sec. 5) | Unevidenced / TBD. Nothing else in the repository names Windows ARM64. |

## 3. Research-only targets (not in the Roadmap Section 5 matrix)

Research 03 (Target matrix) names two further rows that the Roadmap
Section 5 matrix does not. They are research subjects, not planned
MimirAlloc targets, and are recorded here so the difference is visible:

| Target | Status | What the document says | Evidence on record |
|---|---|---|---|
| RISC-V Linux (RV64) | Investigational, research only | Distinguish ISA extensions, libc, compiler, atomics, kernel; first proof is native / emulated tests clearly labelled (Research 03) | Unevidenced / TBD. No RISC-V device, build, or test is recorded. |
| Microcontroller / RTOS | Investigational, research only | Separate design profile; OS-mapping assumptions invalid; distinguish RAM map, MMU / MPU, ISR rules, no virtual memory (Research 03) | Unevidenced / TBD. No device, build, or test is recorded. |

## 4. Architecture facts, each cited to its source

Page size and address space:

- Page size must be queried at runtime (for example sysconf(_SC_PAGESIZE))
  and checked for failure; hardcoded 4096-byte page alignment must be
  avoided. A 16 KiB Android system changes ELF / load alignment and
  mapping assumptions, not just a constant (Research 03, Page size,
  address space and alignment).
- Android documents 16 KiB page support alongside 4 KiB configurations
  (Roadmap Sec. 2, source S3; also Roadmap Sec. 12: support 4 KiB and
  16 KiB system-page environments).
- The SM-X400 tablet system page size was 4,096 bytes in one probe;
  other Android devices can differ (Field 02).
- The actual page size of the specific SM-X400 had to be measured; the
  Roadmap lists it as an open measurement, not a given (Roadmap Sec. 2;
  Sec. 15 question 6).
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
  supported (Roadmap Sec. 2, source S1) - see inconsistency I-4 below.

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
MimirAlloc build):

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

## 5. Pi-observed facts (Team-03, 2026-11-06, device purpleviking1)

These facts were observed by this team on the Raspberry Pi itself and
are NOT document-derived claims. Full command log is in
BASELINE_INVENTORY.md, Section 5. They describe the Pi environment
only; no MimirAlloc code ran.

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

## 6. Inconsistencies and gaps discovered (recorded, not resolved)

- I-1. Research 03 lists RISC-V Linux and Microcontroller / RTOS in its
  target matrix; the Roadmap Section 5 matrix does not list either.
  This document keeps them as research-only (Section 3 above) rather
  than silently promoting or dropping them.
- I-2. macOS ARM64 and Windows ARM64 appear only in one closing
  sentence of Roadmap Section 5 as possible later targets. They appear
  in no other roadmap phase, test matrix, or research document in the
  repository. Status: unevidenced / TBD.
- I-3. Public Modular source inspected at commit 99ab7e90258db16bda9261b19bcb59d729ee3429 selects jemalloc for its Linux Mojo driver, yet
  the distributed Mojo 1.2.0 wheel and 1.3.0.dev2026110205 nightly both
  aborted with a TCMalloc error; the package relationship to that
  public commit was not established (Field 03; Field 01). Any baseline
  built from public source may therefore not match the shipped wheels.
- I-4. TCMalloc documents Linux AArch64 support (Roadmap Sec. 2, S1),
  yet the specific Mojo build on the SM-X400 failed on a 48-bit virtual
  address assumption (Field 03). Documented platform support and this
  device / build / ABI combination are different claims; the Roadmap
  itself frames the question exactly that way (Sec. 1 non-goals,
  Sec. 2).
- I-5. Page size is not one value across named targets: SM-X400 probed
  4,096 bytes (Field 02), Android also ships 16 KiB configurations
  (Roadmap S3), and this team observed 16,384 bytes on the Raspberry Pi
  (Section 5 above). Any design or test that assumes one page size
  contradicts the repository evidence.
- I-6. The README banner describes the project as an allocator project
  for ARM, Android, Mojo, and resource-conscious computing, while the
  README status section and the Roadmap status line both state nothing
  is implemented or verified yet. The banner is a goal statement; this
  matrix follows the status statements.
- I-7. The Roadmap Section 5 row for the Raspberry Pi requires an OS /
  compiler / kernel manifest. Until this slice, no such manifest for
  any Pi existed in the repository. Section 5 above plus
  BASELINE_INVENTORY.md Section 5 now supply one for purpleviking1;
  the required real-device MimirAlloc test and performance data remain
  unevidenced / TBD because no allocator exists to test.
- I-8. musl is a named compatibility target (Roadmap Sec. 5) but no
  document in the repository records a musl version, build, device, or
  test. Fully unevidenced / TBD.

- I-9. No document records an assumed virtual-address width per target
  row. The only measured width in the repository is the SM-X400 probe
  (Field 03); the Pi manifest records page size but no VA-width probe
  was run by this team. Per-target VA width: unevidenced / TBD, and it
  matters because a fixed-width assumption is exactly what failed in
  Field 03.
- I-10. The Microcontroller / RTOS research row (Research 03) sits
  uneasily with the baseline set: the named comparators assume an OS
  with virtual memory, while that row states OS-mapping assumptions
  are invalid. Research 11 lists TLSF as a bounded-operation design to
  study, but no document records a decision or exclusion rationale
  for MMU-less targets. Unevidenced / TBD.

## 7. What would change a status

A target moves only when its Roadmap Section 5 required evidence exists
as a logged artifact, judged by the Roadmap Section 2 evidence levels
(Proposed, Implemented, Host-tested, Device-tested, Integration-proven,
Performance-measured, Blocked) and, for devices, the Research 03 device
acceptance gate (native build plus allocate / fill / verify / free,
alignment, overflow / failure, multiple threads, cross-thread free,
repeated startup / exit, pressure recovery, and physical-device logs
recording build hash, page size, OS, ABI, and exact commands). A
cross-compile or an emulator result is a different evidence tier and
never promotes a target by itself (Research 03).
