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


---

## Z13 line addendum - Linux, x86-64, glibc, 4 KiB pages (purpleraider)

Added by slice Z-106 (documentation truth pass), following the
D-05 disclosure pattern of this document, for the Z13 line
only. Base: seat main 719b00d, banked Z-001..Z-105. Evidence
is the banked Z13 recorded runs and wave verdicts in this
tree, cited per claim. Where the sections above and this
addendum disagree for the x86-64 row, this addendum governs
for the Z13 host scope only. The AArch64 rows above are
unchanged and still rest on their own Pi evidence; this
campaign re-ran nothing on AArch64.

- Environment: ASUS ROG Flow Z13 (host purpleraider), AMD
  Ryzen AI MAX+ 395 with Radeon 8060S (Strix Halo, UMA),
  x86-64, page size 4,096 bytes measured live, 128 GB UMA
  class (MemTotal 127396532 kB in the Z-102 host truth),
  Ubuntu 26.10, kernel 7.3.0-10-generic, glibc 2.44,
  gcc 15.3.0. Sources: host truth annexes in
  tests/perf/recorded_run_z13_z100.txt,
  tests/perf/recorded_run_z13_z101.txt,
  tests/perf/recorded_run_z13_z102.txt and
  tests/abi/recorded_run_z13_z104.txt.
- Status: VERIFIED-RUN, scoped to this host and this tree.
  This discharges the OWED grade of the x86-64 section above
  for this scope only; that section is preserved as the
  pre-Z13 record.
- What was actually run: the full standing gate repeatedly
  on this host across the Z13 line, 4988 checks / 0 failed /
  77 by-design skips (per-slice gates recorded in the
  Z-100..Z-105 runs; the Z-106 baseline and final gates read
  the same counts). The default library exports exactly 13
  public symbols and the opt-in interposing variant exactly
  20 (Z-105, tests/abi/recorded_run_z13_z105.txt). ASan,
  UBSan and combined sanitizer builds of the full suite each
  ran 4988/0/77 with zero sanitizer reports (Z-104,
  tests/abi/recorded_run_z13_z104.txt). Two clean builds on
  this host produced byte-identical release artifacts
  (Z-105 reproducible-build proof; same-host scope only).
- Performance truth, stated without softening: against the
  best tuned glibc per workload, MimirAlloc records LOSS on
  all four oracle workloads - small_churn 29.27 vs 3.32 ns/op
  (8.82x), mixed_batch 29.35 vs 3.71 (7.91x), large_batch
  98.07 vs 8.92 (10.99x), mixed_lifecycle 517.89 vs 3.01
  (172.06x) (Z-098, folded into docs/Z13_WAVE11_VERDICT.md
  section 4.5). The one win against default glibc,
  large_batch 98.07 vs 685.65 ns/op, does not survive tuned
  glibc. Batching LOSES (Wave 11 verdict, section 4.1); the
  interposition tax is a LOSS on every short-lived shape
  measured, +16.98 percent to +130.25 percent (Z-097);
  canonical trace replay correctness is a TIE on both legs
  while trace replay speed against glibc is a LOSS (Z-094,
  Z-095). The eight-hour soak records a WIN on resident
  shape for its workload and a LOSS on batch latency
  (Z-100 recorded run, findings F3 and F7).
- Consumer sentence: on this host class today, a user can
  build with the shipped Makefile (shipped flags -O3, per
  Z-096) and expect the recorded gate result and the
  13-symbol surface; the allocator remains a research
  artifact that loses most measured workloads to glibc on
  this machine, and no performance win beyond the recorded
  cells may be claimed.
- Scope limits: this row verifies ONE host. It does not
  verify any other x86-64 machine, kernel or glibc, and it
  changes no grade for musl, Android, Termux or proot; those
  rows stand exactly as graded above. No release has been
  cut from this line: VERSION in-tree is still 0.1.0;
  0.2.0 is the decided next cut (D-017, Z-105), not a
  released version, and 1.0 remains withheld.
