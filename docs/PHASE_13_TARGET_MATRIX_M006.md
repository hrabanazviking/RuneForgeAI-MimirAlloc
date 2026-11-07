# Phase 13 Target-Matrix Verification - Slice M-006 (Wave 13)

Slice: M-006 - target-matrix verification and the
SUPPORTED_TARGETS refresh (discharges INC-I009-01 /
INC-D009-02). Team-06, solo sequential lead.
Branch: yrsa/mimiralloc-06. Worktree: /home/pi/mimiralloc-team-06.
Device: purpleviking1 (Raspberry Pi 5 Model B Rev 1.1, user pi).
Date of evidence: 2026-11-07 (all runs and probes in this slice).
Assignment base: 4e3bc36504f6fd4450154c7f500a3821806908d3
(M-003 bank, as written in hidden_files/ASSIGNMENT.md).
Build base as executed: c6e57a387ecc57994913282fc23e57d198cb7744
(M-005 bank). The keeper reset this worktree to the M-005 bank
before the slice started, because M-004 and M-005 bank ahead of
M-006 in roadmap order and their files are disjoint from this
slice; the assignment itself anticipates this reconciliation
(its header: "the keeper reconciles the base at banking time").
The commit parent is therefore the M-005 bank, not the
assignment base. Recorded as INC-M006-01; nothing in the slice
depends on the difference (this slice touches only its three
deliverables, and SUPPORTED_TARGETS.md is byte-identical
between the two bases - pre-refresh sha256 verified below).
Governing texts read in full for this slice: Roadmap Sections
5, 10, 12 and the Phase 13 exit line; the pre-refresh
SUPPORTED_TARGETS.md and BASELINE_INVENTORY.md end to end;
the Phase 4 exit packet (D-009 synthesis) for the Android,
x86-64, and musl facts; D-005 (cross-compilation), D-006
(Android build plan), D-008 (libc/alignment) records; the
Phase 3 exit packet for the x86-64 owed statement; D-003
(page-size portability).

## 1. Method as executed (assignment Section 4, in order)

1. Base and status verified: at start, HEAD was
   c6e57a387ecc57994913282fc23e57d198cb7744 on
   yrsa/mimiralloc-06 (keeper-reconciled base; see header and
   INC-M006-01), git status showed only untracked
   hidden_files/. Samtal HTTP 200 and free -g 13 GiB available
   confirmed before any heavy step.
2. Assignment confirmed in the worktree (122 lines, sha256
   47332bcbe335dac2be6435f222911eb7ba35a97940d1def5e22ff5aceb186b76)
   before any deliverable was written.
3. Native baseline run fresh in this slice (Section 2 of
   the recorded run): make clean (exit 0), make test
   (exit 0), make lib (exit 0), nm -D --defined-only exactly
   4. Environment recorded live in the same session.
4. Governing texts read (header above); worksheet built in
   Section 2 of this document from the Roadmap Section 5
   enumeration: 8 primary rows, 2 later entries (macOS ARM64,
   Windows ARM64, closing paragraph), 2 research-only entries
   (Research 03). The banked Section 5 text matches the
   A-003 enumeration exactly; no enumeration difference, so
   no INC arises on that point.
5. Probes executed and recorded (Section 4 of this document;
   raw output in the recorded run): the banked D-005 toolchain
   probe script and the banked D-006 Android probe script
   re-run in full, plus direct command -v and path checks for
   proot, qemu, musl, RISC-V, Box64, and page size.
6. Lawful runs executed: the full native suite (step 3); the
   D-005 freestanding x86-64 probe re-run through the banked
   script (make test-d005, exit 0); the D-005 E2 API-TU
   cross-compile refreshed with the exact command recorded in
   the recorded run (exit 0), with the core and platform TUs
   failing on exactly the missing libc headers (exit 1 each);
   the full x86-64 build script run to its preflight refusal
   (exit 3, BLOCKED, recorded). Nothing was installed.
7. Grading: every row and differing cell graded under the
   assignment Section 3 taxonomy; worksheet in Section 2.
   Fresh runs were used wherever a leg is runnable here;
   banked citations are used only where no fresh run is
   possible (4 KiB design audit, bionic analogy table,
   Phase 11 performance data) and are named with slice,
   document, and date.
8. SUPPORTED_TARGETS.md refreshed in place under assignment
   Section 6 (Section 3 of this document is the old-vs-new
   table for every changed row).
9. This verification document written; recorded run assembled
   from the raw logs on the Pi; all deliverables
   sha256-verified on the Pi before commit.
10. Committed exactly the three Section 7 files on
    yrsa/mimiralloc-06, local commit only. Not pushed, not
    merged (the keeper banks in roadmap order).

## 2. Audit worksheet (every roadmap target)

Grades use the assignment Section 3 taxonomy. Evidence
pointers name the recorded run (tests/abi/recorded_run_m006.txt,
cited as RR with its section) or the banked citation in full.

W-01. Linux AArch64 + glibc, 16 KiB pages (this Pi, native).
Claim tested: the banked tree builds and passes its native
suite on this target today. Run: RR S1-S2 - make test exit 0;
suite lines 29 / 69 / 95 / 23 / 122 run, 0 failed; 42 run,
0 failed, 7 skipped-not-yet; closing D-004 multithread line
CHECKS: 4590 run, 0 failed; make lib exit 0; nm exactly 4
(mimir_malloc, mimir_free, mimir_calloc, mimir_realloc).
Environment: PAGESIZE 16384 measured live, glibc 2.36,
GCC 12.2.0, Clang 14.0.6, kernel 6.12.112-v8-16k-hailo+.
Grade: VERIFIED-RUN. Rationale: full suite executed in this
slice on the target itself and passed; the only cell in the
matrix that earns this grade. Native CI is not claimed (no
CI run exists; workflows are manual-only by standing rule).

W-02. Linux AArch64 + glibc, 4 KiB pages.
Claim tested: a suite run on a 4096-page host. Run: none
possible - the only host is this Pi, whose kernel is a
16 KiB-page build; page size is a kernel property, not
settable per process (D-003). Evidence: RR S1 (PAGESIZE
16384); banked citation D-003, docs/PAGE_SIZE_PORTABILITY.md
(2026-11-06): every code path runtime-queries the page size,
checked arithmetic verified at 4096 against an __int128
oracle (INC-D003-02). Grades: run cell OWED (discharge: run
the suite on a 4096-page AArch64 host and record it);
design-support cell ANALOGY-ONLY (the audit is reasoning and
arithmetic about the design, clearly separated from any run
claim; it establishes portability of the design, not a run).

W-03. Linux x86-64 + glibc.
Claim tested: the suite, or any genuine part of it, runs for
x86-64. Runs: RR S7 - freestanding probe re-run via banked
script scripts/cross/build_x86_64_freestanding_probe.sh
(make test-d005, exit 0): compile and link exit 0, run via
binfmt and via explicit Box64 0.4.5 both printed
D005-X64-PROBE-OK with exit 42. This is an emulated run of a
freestanding probe, labelled emulated everywhere. RR S6 -
D-005 E2 refreshed: API TU (src/api/mimir_alloc.c) compiled
for x86_64-linux-gnu, exit 0, object defines exactly the 4
public symbols; core TU exit 1 (string.h not found),
platform TU exit 1 (sys/mman.h not found) - exactly the
missing libc headers, agreeing with banked D-005
(docs/PHASE_4_CROSS_COMPILATION_AND_CI.md, E2;
tests/cross/recorded_run_d005.txt). RR S8 - full-build
script scripts/cross/build_x86_64.sh refused at preflight,
exit 3: missing compiler x86_64-linux-gnu-gcc and sysroot
/usr/x86_64-linux-gnu (fresh probe, RR S3, agrees).
Grades: PARTIAL-RUN (freestanding probe leg plus API-TU
cross-compile leg genuinely executed; both halves named);
hosted suite cell OWED, blocked on the missing x86-64 libc
sysroot - smallest next step install gcc-x86-64-linux-gnu,
reserved for Volmarr word, NOT given (D-005). The Phase 3
exit UNMET verdict stands on this leg (Phase 3 exit packet,
R5 / item 3). PARTIAL-RUN is the ceiling for this row in
this slice and is never rounded up.

W-04. Linux AArch64 + musl.
Claim tested: compile and runtime on musl. Probe: RR S3 -
musl-gcc ABSENT, musl-clang ABSENT, /usr/lib/musl ABSENT,
no ld-musl file present; apt candidate musl-tools 1.2.3-1
exists but is not installed. Banked citation: D-008,
docs/LIBC_ALIGNMENT_COMPATIBILITY.md (INC-D008-06).
Grade: BLOCKED. Rationale: the leg was prepared in D-005 /
D-008 and is stopped on one named, freshly verified blocker
(no musl toolchain or sysroot); smallest next step is the
musl-tools install, reserved for Volmarr word and NOT given.
No musl behaviour is claimed, and the D-008 record is cited,
not extended; in particular nothing is inferred from glibc
behaviour, so no ANALOGY-ONLY cell is opened for musl.

W-05. Android ARM64 + bionic (native NDK binary).
Claim tested: an NDK build and a native run. Probe: RR S4 -
the banked script scripts/android/probe_android_toolchain.sh
re-run in full, exit 0 as a probe: ndk-build ABSENT,
sdkmanager ABSENT, every aarch64-linux-android clang driver
ABSENT, adb ABSENT, ANDROID_NDK_HOME / ANDROID_HOME /
ANDROID_SDK_ROOT empty, every NDK/SDK path ABSENT. Banked
citations: D-006 (docs/PHASE_4_ANDROID_BUILD_PLAN.md; E1
freestanding object is not a build), D-007 device packet,
D-008 bionic contract table (analogy only, U-03).
Grades: run cell OWED in full (discharge: Android NDK
provision on a chosen build host - noting the D-006 caution
that current NDK prebuilts are x86-64 while this Pi is
AArch64, so the build host may move; the device leg does
not - plus a device session); contract cell ANALOGY-ONLY
(the D-008 table reasons from documented bionic behaviour;
it does not establish any run, any symbol resolution, or any
page-size fact on a device). Phase 4 exit UNMET stands in
part on this row, and the refreshed matrix keeps that
consequence visible in the row.

W-06. Termux on Android (physical SM-X400).
Claim tested: MimirAlloc compilation and execution in
Termux. Evidence: field notes only (Field 01 to 14) - the
Termux environment was investigated, but that evidence never
ran MimirAlloc, in either its bionic or its glibc userspace.
Probe: RR S3 - no Termux environment exists on this Pi
(termux-info ABSENT, no /data/data/com.termux tree), which
is expected: Termux lives on the device, and no device
session exists. Grade: OWED (discharge: a Termux session on
the physical device, separately labelled, sequenced after
the NDK-binary run per D-007).

W-07. Linux userland in Android proot.
Claim tested: any proot result for MimirAlloc. Probe:
RR S3 - proot ABSENT (command exit 1), proot-distro ABSENT
on this Pi; the field-notes proot-distro 5.10.3 was on the
tablet and refused to run in that traced session (Field 04).
No proot run of this allocator was executed in this slice:
proot is absent here, and a Pi-side proot run would in any
case be a proot result only. Grade: OWED (discharge: an
executed proot run, recorded under proot attribution per
Roadmap Sec. 10). No Android row is promoted by anything in
this worksheet entry.

W-08. Samsung SM-X400 physical tablet (device row).
Claim tested: MimirAlloc device evidence (measured page
size in-run, memory conditions, stress, responsiveness).
Evidence: device inventoried (Field 02) and environment
probed (Field 01, 03); the D-007 on-device program is ready
and its logic was native-checked on the Pi (59 checks) -
explicitly not an Android result. No MimirAlloc run on the
device exists; D-007 scored the Roadmap Sec. 12 checklist
0 of 10 satisfied by run evidence. Grade: OWED (discharge:
a physical-device session per the D-007 packet and Roadmap
Sec. 12, page size measured on the device in-run; RAM, swap,
and page cache kept distinct per Sec. 12).

W-09. Raspberry Pi ARM64 Linux (device row).
Claim tested: real-device test plus OS / compiler / kernel
manifest plus performance data. Evidence: the W-01 run WAS
the real-device test, executed on this device; the manifest
is RR S1 (model, kernel, OS, compilers, glibc, PAGESIZE,
nproc, memory). Performance data is banked Phase 11
evidence (docs/PHASE_11_BASELINE_BENCHMARKS.md and the
Phase 11 slice documents), cited, not re-measured - this
slice runs no benchmark campaign (assignment Section 9).
Grade: VERIFIED-RUN, as the physical instantiation of
W-01, in the same suite scope.

W-10. macOS ARM64 (later entry, Roadmap Sec. 5 closing
paragraph). Claim tested: any evidence at all. None exists
anywhere in the repository (A-003 I-2 re-checked). Grade:
OWED. Rationale for OWED over NOT-A-TARGET: the roadmap
defers this entry ("Later targets may include...") rather
than excluding it; discharge is a future charter plus a
real run on macOS hardware, after its own ABI work.

W-11. Windows ARM64 (later entry). As W-10: no evidence,
roadmap defers rather than excludes. Grade: OWED, same
discharge shape (future charter plus a real run).

W-12. RISC-V Linux (RV64) and Microcontroller / RTOS
(research-only entries, Research 03; not in the Roadmap
Sec. 5 matrix). Probe: RR S3 - riscv64-linux-gnu-gcc
ABSENT, no RISC-V compiler in /usr/bin; no MCU/RTOS device,
build, or test recorded anywhere. Grade: NOT-A-TARGET for
both. Basis quoted: Research 03 names them as research
subjects with their own first-proof conditions; the Roadmap
Sec. 5 matrix does not name either, and A-003 I-1 / I-10
record the separation and the open rationale question. The
grade records completeness of the matrix, not a claim.

## 3. Old-vs-new table (every changed SUPPORTED_TARGETS row)

Pre-refresh file: 235 lines, sha256
fcd425186e05f246f48ac8457979a722ca125fb1af7e7a8acbca1a74e521c204
(verified on the Pi at the build base before editing).
Post-refresh file: 343 lines, sha256
162530989ac4020208bee9426490f22a4ef04ac564acd3d582cebacdc4809a36.

| Row / section | A-003 (old) | M-006 (new) | Why the evidence changed it |
|---|---|---|---|
| Headline (S1) | No target is supported; zero measured baselines; labels Planned / Investigational only | Exactly one VERIFIED-RUN scope (AArch64 Linux/glibc, 16 KiB, this Pi class, native suite); six-grade taxonomy stated | The allocator now exists and its native suite ran fresh in this slice (W-01); the stale headline was INC-I009-01 / INC-D009-02 |
| Linux AArch64 + glibc | Planned; no MimirAlloc evidence | Split by page size: 16 KiB VERIFIED-RUN; 4 KiB OWED with ANALOGY-ONLY design support | Fresh suite run at 16384 (W-01); no 4096-page host exists (W-02, D-003) |
| Linux x86-64 + glibc | Planned; no MimirAlloc evidence | PARTIAL-RUN (freestanding probe re-run PASS, emulated; API-TU cross-compile refreshed) + hosted suite OWED on the missing sysroot | D-005 legs re-run / refreshed fresh in this slice (W-03) |
| Linux AArch64 + musl | Planned; unevidenced / TBD | BLOCKED, blocker and reserved install named | Fresh probe re-verified the D-008 absence (W-04) |
| Android ARM64 + bionic | Planned; environment evidence only | OWED (run) + ANALOGY-ONLY (contract cells), Phase 4 consequence visible | Fresh probe re-verified the D-006 absence; no run exists (W-05) |
| Termux on Android | Investigational; environment investigated, no MimirAlloc run | OWED, discharge named | No Termux run of the allocator exists (W-06) |
| proot userland | Investigational; no proot result recorded | OWED, discharge and attribution named | proot freshly ABSENT here; still no result anywhere (W-07) |
| Samsung SM-X400 | Investigational; inventoried, no MimirAlloc test | OWED, D-007 discharge named | Still no device run (W-08) |
| Raspberry Pi ARM64 Linux | Investigational; manifest supplied by A-003, test TBD | VERIFIED-RUN (instantiation of the glibc row), manifest refreshed, Phase 11 performance cited | The M-006 suite run is the real-device test (W-09); discharges the test half of A-003 I-7 |
| macOS / Windows ARM64 | Investigational (later); unevidenced / TBD | OWED (deferred, not excluded), basis stated | Taxonomy change only; evidence unchanged (W-10, W-11) |
| RISC-V / MCU-RTOS (S3) | Investigational, research only | NOT-A-TARGET, roadmap basis quoted | Taxonomy change only; evidence unchanged (W-12) |
| Architecture facts (S4) | A-003 facts | Preserved; two marked M-006 additions (runtime page-size discipline now proven in code, D-003; native ABI surface now measured, D-008 + nm) and one wording repair (TCMalloc coverage sentence, so the language gate holds) | D-003 / D-008 banked after A-003 |
| Pi-observed facts (S5) | Team-03 2026-11-06 manifest | Preserved verbatim as S5.1; S5.2 added with the 2026-11-07 facts and probe outcomes | Fresh observations of this slice |
| Inconsistencies (S6) | I-1 to I-10 recorded, unresolved | Each revisited with an M-006 status: I-7 test half discharged; I-8 graded BLOCKED; I-9 carried as INC-M006-04; the rest stand as recorded | Assignment Section 6 re-check duty |
| What would change a status (S7) | A-003 promotion rule | Restated as what would change a grade, in taxonomy terms, same evidence gates | Grade taxonomy replaces the two A-003 labels |

## 4. Probe inventory (compact; raw output in the recorded run)

Host and native: uname aarch64 / Linux 6.12.112-v8-16k-hailo+;
PAGESIZE 16384 (both getconf spellings); LONG_BIT 64; nproc 4;
gcc/cc 12.2.0 PRESENT; clang 14.0.6 PRESENT;
aarch64-linux-gnu-gcc PRESENT; glibc 2.36 (ldd); Debian 12.
x86-64 build leg: x86_64-linux-gnu-gcc ABSENT;
/usr/x86_64-linux-gnu ABSENT; /usr/include/x86_64-linux-gnu
ABSENT; clang x86-64 backend PRESENT; ld.gold PRESENT with
elf64-x86-64 emulation; full-build preflight exit 3.
x86-64 run leg: box64 PRESENT (/usr/local/bin/box64, Box64
arm64 v0.4.5), binfmt_misc box64 entry enabled; box86 and
wine PRESENT; qemu-x86_64 / qemu-x86_64-static /
qemu-system-x86_64 ABSENT. Freestanding probe re-run PASS.
musl: musl-gcc ABSENT; musl-clang ABSENT; /usr/lib/musl
ABSENT; /usr/aarch64-linux-musl ABSENT; no ld-musl file;
apt candidate musl-tools 1.2.3-1 (not installed, reserved).
Android: ndk-build ABSENT; sdkmanager ABSENT; adb ABSENT;
aarch64-linux-android21-clang and -android-clang ABSENT;
aarch64-linux-android24-clang ABSENT; ANDROID_NDK_HOME,
ANDROID_HOME, ANDROID_SDK_ROOT empty; /home/pi/Android,
/home/pi/android-ndk, /opt/android-sdk, /opt/android-ndk,
/usr/lib/android-sdk, /usr/lib/android-ndk, /usr/aarch64-linux-android
all ABSENT (banked D-006 probe script re-run in full).
Environments: proot ABSENT; proot-distro ABSENT;
termux-info ABSENT; /data/data/com.termux ABSENT.
Other ISAs: riscv64-linux-gnu-gcc ABSENT; no riscv compiler
in /usr/bin; qemu-aarch64 ABSENT.
Reserved-install candidates (read-only apt-cache policy):
gcc-x86-64-linux-gnu candidate 4:12.2.0-3 (not installed);
musl-tools candidate 1.2.3-1 (not installed). Nothing was
installed, downloaded, or changed by any probe.

## 5. Language gate (assignment Section 8)

Full-text case-insensitive search of the refreshed
docs/SUPPORTED_TARGETS.md for "supported" and "supports",
run on the Pi against the committed text. Hits: 3, all in
the first category list below; "supports": 0 hits.

1. Line 1, the document title "MimirAlloc Supported-Target
   Matrix" - the inherited A-003 document name (and the
   Roadmap Section 5 heading name); a name, not a claim.
2. Section 1 headline: "Exactly one target is supported
   today, in exactly one scope:" followed immediately by the
   exact scope (AArch64 Linux/glibc, Raspberry Pi 5 class,
   16 KiB pages, Debian 12, glibc 2.36, native suite scope
   run fresh in this slice). This is the one claim use, and
   it is permitted by the taxonomy for the VERIFIED-RUN row
   in its exact run scope.
3. Section 1 history paragraph: "A-003 stated that no target
   was supported and recorded zero measured baselines" - a
   historical report of the A-003 headline, in negated form;
   it denies support and cannot mislead.
No other use exists in the refreshed document. The A-003
TCMalloc sentence that used the word about TCMalloc was
reworded to "a covered platform" in this refresh so the
gate holds without exception-handling (old-vs-new table,
Section 3).

## 6. INC-M006 register

- INC-M006-01 (base reconciliation, recorded not a defect):
  the assignment names base 4e3bc36504f6fd4450154c7f500a3821806908d3
  and its method step 1 expects HEAD to equal it; the keeper
  reset this worktree to c6e57a387ecc57994913282fc23e57d198cb7744
  (M-005 bank) before the slice, per the assignment own
  anticipation that M-004 / M-005 bank ahead with keeper
  reconciliation. The slice was built on the keeper base;
  the commit parent is c6e57a3. SUPPORTED_TARGETS.md is
  byte-identical between the two bases (pre-refresh sha256
  fcd42518... verified at the build base), so no grade or
  statement in this slice depends on the difference.
- INC-M006-02 (probe methodology, fixed in-slice): the
  banked D-005 recorded run preserves the E2 results but not
  the exact E2 command line. The first reconstruction in
  this slice passed the clang resource directory with -I and
  failed on resource-header macro redefinition under
  -Werror - a probe-command error, not a tree defect (the
  native build was green in the same session). The correct
  form passes the resource directory with -isystem; it
  reproduced D-005 E2 exactly (API TU exit 0, 4 symbols;
  core / platform exit 1 on the missing libc headers) and
  is recorded verbatim in the recorded run, Section 5, so
  no future slice has to reconstruct it.
- INC-M006-03 (BASELINE_INVENTORY staleness, owed to a
  future slice): BASELINE_INVENTORY.md is NOT rewritten by
  this slice (assignment Section 6), but its headline is
  stale in the same way A-003 was: Section 1 states no
  measured baseline, benchmark result, or harness is
  tracked, and B-07 records the Phase 3 minimal allocator
  as not existing yet. Both were true at the Phase 0 base;
  the allocator has existed since Phase 3 and measured
  baselines are banked in Phase 11. A future slice should
  refresh the inventory or mark it superseded in place;
  nothing in this slice relies on the stale statements
  (baseline facts used here are cited by section and were
  re-verified live where they matter).
- INC-M006-04 (carried from A-003 I-9): per-target
  virtual-address width remains unevidenced / TBD. The only
  measured width in the repository is still the SM-X400
  field probe (Field 03). No width is assumed anywhere in
  the refreshed matrix.
- INC-M006-05 (count presentation, reading caution): the
  campaign headline "CHECKS: 4590 run, 0 failed" is the
  D-004 multithread suite own closing line inside make
  test, not a sum over the 8 suites make test runs (their
  own lines are 29 / 69 / 95 / 23 / 122 / 42, all 0 failed,
  preserved individually in the recorded run). The
  convention is the assignment own expectation and every
  earlier packet uses it the same way; it is recorded here
  so no reader mistakes 4590 for a cross-suite total.
No allocator defect was discovered by this slice; the
category "defect found in the tree" is empty, explicitly.

## 7. Discharge statement (INC-I009-01 / INC-D009-02)

INC-I009-01 (SUPPORTED_TARGETS.md stale - carried by I-009
from the Phase 9 packet and re-flagged in the Phase 10,
Phase 11, and Phase 12 packets) and INC-D009-02 (the same
debt as recorded by D-009 from INC-D001-02: the file was
never refreshed after A-003) are DISCHARGED by this slice.
Evidence: docs/SUPPORTED_TARGETS.md refreshed in place at
the build base, post-refresh sha256
162530989ac4020208bee9426490f22a4ef04ac564acd3d582cebacdc4809a36
(343 lines; pre-refresh 235 lines, sha256
fcd425186e05f246f48ac8457979a722ca125fb1af7e7a8acbca1a74e521c204).
The staleness both INCs referred to was the A-003 headline
(no target supported, zero measured baselines) and every
matrix row written under it; all of those rows are re-graded
in the refreshed document from the evidence in Section 2 of
this document. The keeper records this discharge in
WAVE_PLAN at banking.

## 8. Verification gates (assignment Section 8, results)

- Native gate: PASS - make test exit 0; CHECKS 4590 run /
  0 failed present in the recorded run; nm -D
  --defined-only exactly 4, executed in this slice.
- Completeness gate: PASS - all 12 enumerated entries
  (8 primary, 2 later, 2 research-only) appear in the
  worksheet (Section 2) and in the refreshed matrix with a
  taxonomy grade; glibc page-size cells graded separately.
- Evidence gate: PASS - every VERIFIED-RUN / PARTIAL-RUN
  cell points at the recorded run of this slice or a fully
  cited banked record; every BLOCKED / OWED cell names its
  blocker or discharge condition in its row; the single
  ANALOGY-ONLY claim class (bionic contract cells, 4 KiB
  design support) labels its analogy and its limits.
- Language gate: PASS - Section 5 of this document; 3 hits,
  all classified, one permitted claim use in exact scope.
- Purity gates: PASS - README.md diff vs build base 0
  lines; include/mimir_alloc.h diff 0; the commit contains
  exactly the three deliverables; the SUPPORTED_TARGETS
  diff is fully explained by the Section 3 table.
- Health gate: PASS - Samtal HTTP 200 at close; free -g
  13 GiB available at close; nothing installed; no process
  left running by this slice (all probes are one-shot).

## 9. Plain-language verdict for Volmarr

What is actually verified today: the allocator genuinely
runs on your Pi class of machine - AArch64 Linux with
glibc, 16 KiB pages - and its full native test suite passed
fresh today, 4,590 checks in the closing multithread suite
with zero failures anywhere, exporting exactly its four
public functions. That is the one solid green cell, and the
refreshed document claims exactly that and no more.
What is partial: x86-64. A small freestanding x86-64 probe
really did run on the Pi today through Box64 (emulation,
labelled as such), and one piece of the allocator - its
public API file - really does cross-compile for x86-64.
The full x86-64 test suite has never run: the Pi lacks the
x86-64 system library files, and the package that provides
them (gcc-x86-64-linux-gnu) waits on your word, as it has
since D-005. Nothing was installed.
What waits: musl is blocked on the same kind of decision
(musl-tools, your word, since D-008). Android is owed in
full - no Android toolkit on the Pi, no run on the Samsung
tablet yet; the plan and the on-device test program are
ready and waiting for a toolkit plus a device session.
The same goes for Termux, proot, the tablet itself, and a
4 KiB-page run. macOS and Windows ARM64 are deferred future
entries, and RISC-V / microcontroller work is research
reading, not a target. The two stale-document debts this
slice was chartered to clear (INC-I009-01 / INC-D009-02)
are discharged: the supported-targets document now tells
the truth as of today, row by row.
