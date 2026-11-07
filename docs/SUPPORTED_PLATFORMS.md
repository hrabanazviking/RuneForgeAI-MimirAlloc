# Supported Platforms - MimirAlloc

Dependency disclosure (read first): this document is built on the
A-003-era target matrix in `docs/SUPPORTED_TARGETS.md` at this base
(235 lines, sha256
fcd425186e05f246f48ac8457979a722ca125fb1af7e7a8acbca1a74e521c204)
corrected only by banked phase evidence in this tree (Phases 3-12
records and the Wave 13 M-002/M-003/M-004 records). The M-006 refresh
of the target matrix had NOT banked when this slice started, so no
unbanked M-006 content is used, imported, or anticipated here. Where
the stale A-003 text and the banked run record disagree, the banked
run record governs and the disagreement is stated. Reconciling this
document with the refreshed matrix, once M-006 banks, is M-009 work
(INC-M008-01).

Grades use the M-006 taxonomy language: VERIFIED-RUN, PARTIAL-RUN,
ANALOGY-ONLY, BLOCKED, OWED, NOT-A-TARGET. A grade never exceeds the
banked evidence in this tree.

## Linux, AArch64, glibc, 16 KiB pages (Raspberry Pi class)

- Environment: Linux AArch64 + glibc 2.36, system page size 16,384
  bytes, Raspberry Pi (purpleviking1 class: 4 cores, kernel
  6.12.112-v8-16k-hailo+ as recorded in banked packets).
- Status: VERIFIED-RUN.
- What was actually run: the full suite, repeatedly, to
  `CHECKS: 4590 run, 0 failed` across Phases 3-13 banked records,
  including this slice's baseline run; the dynamic ABI is exactly
  the four symbols mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc (`nm -D --defined-only` = 4).
- Reconciliation: the A-003-era matrix said no target had been run.
  That text is stale: the banked Phase 3-12 records and the exit
  packets supersede it for this row. This document says so
  explicitly, per its dependency rule.
- Consumer sentence: on this platform class today, a user can build
  with the shipped Makefile and expect the recorded suite result;
  the allocator is still a research artifact that loses to glibc on
  every measured workload (see the limitations register).

## Linux, AArch64, glibc, 4 KiB pages

- Environment: Linux AArch64 + glibc on a 4,096-byte-page host.
- Status: OWED.
- What was actually run: page-size portability is proven by design
  audit and arithmetic proof at 4,096 and 16,384 (D-003), and the
  code discovers page size at runtime with no fixed constant; the
  suite itself has not been run on a 4 KiB host (INC-D003-02).
- Consumer sentence: expect the design to port, but do not treat
  4 KiB-page AArch64 as verified until the owed host run exists.

## Linux, x86-64, glibc

- Environment: Linux x86-64 + glibc, development / reference
  platform in the roadmap matrix.
- Status: OWED.
- What was actually run: nothing. The x86-64 run of the portable
  suite is owed in full; Phase 3 exit is UNMET for exactly this
  reason (Phase 3 exit packet, C-009).
- Consumer sentence: no x86-64 result exists today; any x86-64 use
  is untested, not merely unbenchmarked.

## Linux, AArch64, musl

- Environment: Linux AArch64 + musl libc, compatibility target in
  the roadmap matrix; interposition to be assessed separately.
- Status: OWED.
- What was actually run: nothing. No musl build, run, or inventory
  is recorded anywhere in the repository (A-003 matrix, gap I-8).
- Consumer sentence: musl is a named intention, not a supported
  platform today.

## Android, ARM64, bionic (physical device)

- Environment: Android ARM64 + bionic, separate native port;
  reference device class Samsung SM-X400 (Galaxy Tab S10 Lite,
  Android 16 / API 36, 4,096-byte pages in the banked probe).
- Status: BLOCKED.
- Blocker / discharge condition: no Android NDK is present on the
  development Pi (live-verified probe, D-006) and `adb` is absent;
  the physical-device run (allocate / fill / free plus multithread,
  with logs) is owed in full. Phase 4 exit is UNMET for this reason
  (Phase 4 exit packet, D-009). The API-level / bionic contract is
  currently held by analogy only (U-03 open).
- Consumer sentence: there is no Android build to use today; the
  port exists as a plan and a device packet, not as a binary.

## Termux on Android

- Environment: Termux on the SM-X400 (environment row, not a
  platform): Clang 21.1.8, default target
  aarch64-unknown-linux-android24, per the banked field notes.
- Status: ANALOGY-ONLY.
- What was actually run: the environment was investigated in depth
  (field notes 01-14); no MimirAlloc compilation or execution in
  Termux is recorded.
- Consumer sentence: Termux evidence describes the environment,
  never MimirAlloc running in it; do not read it as support.

## Linux userland in Android proot

- Environment: proot-distro 5.10.3 on Android (convenience test
  environment row).
- Status: OWED.
- What was actually run: no proot result is recorded for
  MimirAlloc; the field notes record proot-distro refusing to run
  in an already-traced session.
- Consumer sentence: a proot result, even when one exists, will be
  labelled proot and will never be presented as native Android
  proof.

## Samsung SM-X400 physical tablet (device row)

- Environment: the physical flagship constrained device that
  instantiates the Android row above.
- Status: BLOCKED.
- Blocker: the device is absent from the development Pi
  environment; its inventory is banked (field note 02) and no
  MimirAlloc test, stress run, or responsiveness measurement on it
  exists.
- Consumer sentence: device claims await a device; none are made.

## macOS ARM64

- Status: NOT-A-TARGET. Named only as a possible later target in
  one closing paragraph of Roadmap Section 5; unevidenced / TBD;
  its own allocation ABI and OS-interface work would be required
  before any claim.

## Windows ARM64

- Status: NOT-A-TARGET. Same basis as macOS ARM64 above.

## RISC-V Linux (RV64) and Microcontroller / RTOS

- Status: NOT-A-TARGET (research only). Named in Research 03, not
  in the Roadmap Section 5 matrix; kept visible here so the
  difference is not silently dropped (A-003 gap I-1). No device,
  build, or test is recorded for either.

