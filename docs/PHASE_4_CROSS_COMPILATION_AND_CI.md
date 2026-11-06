# Phase 4 - Cross-Compilation and CI (Slice D-005)

Slice: **D-005 (team-05)**. Wave 4, Phase 4 ARM64 and
Android foundation. Deliverables: this doc,
`scripts/cross/probe_toolchains.sh`,
`scripts/cross/build_x86_64_freestanding_probe.sh`,
`scripts/cross/build_x86_64.sh` (proposed),
`tests/cross/probe_freestanding.c`,
`tests/cross/recorded_run_d005.txt`,
`.github/workflows/cross-x86_64.yml` (proposed), and an
additive `test-d005` Makefile target.
Base: `f5965c1d8adf7b8951fe4975befd1f5c47e172f4` (D-003
banked) on branch `yrsa/mimiralloc-05`, worktree
`/home/pi/mimiralloc-team-05` on Raspberry Pi
`purpleviking1`. Governing text: Roadmap Section 8
Phase 4 task T3 as decomposed in
`docs/PHASE_4_FOUNDATION_PLAN.md` (D-001), read with
Roadmap Sections 2, 5 and 12. `README.md` was not
touched by this slice. Every fact below was checked or
run on the Pi in this slice on 2026-11-06; the raw
record is `tests/cross/recorded_run_d005.txt`.

> **VERDICT.** The x86-64 road on this Pi is HALF BUILT,
> and the halves are not the ones earlier slices assumed.
> The RUN half exists and is proven: Box64 0.4.5 is
> installed, binfmt_misc is registered and enabled for
> x86-64 ELF, and this slice ran x86-64 binaries three
> ways (Sections 3, E4-E5). The BUILD half is missing
> exactly one component: an x86-64 libc sysroot
> (headers, libraries, crt files, loader). Without it
> the allocator cannot be cross-built - 2 of its 3
> library translation units fail at missing libc
> headers - and the portable suite has therefore still
> never run on x86-64. **The Phase 3 x86-64 debt owed
> since C-007/C-009 is NOT discharged by this slice.**
> **Phase 3 exit: UNMET. Phase 4 exit: UNMET.** What
> this slice adds is the precise map of the gap, an
> executed freestanding proof of the legs that exist,
> and the smallest next step that closes the gap
> (Section 5). A blocker record is a valid D-005
> outcome per the D-001 plan; a silent gap is not, and
> there is none here.

## 1. Checks performed on the Pi in this slice

Host first, load kept light throughout: `free -g`
showed 13 GiB available of 15 GiB before and after all
work; load stayed near 1.2; Samtal (127.0.0.1:8787)
answered HTTP 200 at the end of the run. Host: Linux
6.12.112-v8-16k-hailo+, aarch64, Debian 12 (bookworm),
16,384-byte pages, 4 cores.

Presence probes (full output in recorded run Section
1, produced by `scripts/cross/probe_toolchains.sh`,
which this slice wrote and executed): every compiler,
linker, sysroot path, emulator and CI path named in
Section 2. Direct experiments E1-E5 (Section 3) were
run in a scratch directory; nothing was installed, no
package state was changed, and no file outside this
worktree and the scratch directory was written.
Document reads: the D-001 plan (T3 row and Sections 2,
9), `docs/PHASE_3_EXIT_EVIDENCE.md` (the x86-64 debt),
the Makefile, and the Wave 4 queue in WAVE_PLAN.md.

## 2. The road, component by component (all verified)

| Component | Status on the Pi | Evidence |
|---|---|---|
| Native compiler (aarch64 gcc 12.2.0, clang 14.0.6) | PRESENT | probe Section 1 |
| `x86_64-linux-gnu-gcc` (GCC cross) | **ABSENT** | probe; `which` empty |
| x86-64 sysroot `/usr/x86_64-linux-gnu`, target headers `/usr/include/x86_64-linux-gnu` | **ABSENT** (both paths) | probe path checks |
| clang x86-64 code generation (backend) | PRESENT - and genuinely usable for freestanding code (E1) | E1, E2, E4 |
| GNU ld x86-64 emulation | ABSENT in `ld` (aarch64 emulations only) | `ld -V` |
| GNU gold `elf64-x86-64` target | PRESENT - links freestanding x86-64 (E4) | `ld.gold -V`, E4 |
| `ld.lld`, `x86_64-linux-gnu-ld` | ABSENT | probe |
| musl (any arch): `musl-gcc`, `musl-clang`, `/usr/lib/musl` | **ABSENT** | probe |
| qemu user mode (`qemu-x86_64`, `-static`) | **ABSENT** (not installed) | probe |
| qemu system x86-64 (`qemu-system-x86_64`) | **ABSENT** | probe |
| qemu system ARM + qemu-utils | PRESENT (ARM only; useless for x86-64) | dpkg list, addendum A4 |
| **Box64 0.4.5 (`/usr/local/bin/box64`)** | **PRESENT** (pi-apps install, build 2026-11-05) | probe; E4, E5 runs |
| binfmt_misc x86-64 registration | **PRESENT, enabled**, interpreter `/usr/local/bin/box64`, magic for ELF64 x86-64 | `/proc/sys/fs/binfmt_misc/box64` in probe output |
| box86 + binfmt 32-bit x86 entry (interpreter box86) | PRESENT (32-bit leg; not needed by this slice) | probe |
| Wine 11.4 / 9.17 (x86-64 builds under /opt, run via Box64) | PRESENT - its loader is the E5 test subject | E5 |
| zig / rustc / cargo / go (alternative cross roads) | ABSENT (all four) | `which` empty |
| tcc 0.9.27 | PRESENT but AArch64-only build - not a cross road | `tcc -version` |
| CI config in repo at base (`.github/workflows`, `.gitlab-ci.yml`, `Jenkinsfile`, `.circleci`) | **ABSENT at base** - verified against the base git tree (addendum A1) | git ls-tree |
| Local CI runners (`act`, `gitlab-runner`) | ABSENT | probe |

Two rows deserve emphasis because they reverse the
picture D-001 recorded. D-001 found "no qemu" and no
emulation story at all; the emulation story was in
fact already installed, under a name no Wave 4 probe
had looked for (INC-D005-01, INC-D005-02). And the
clang backend, which D-001 correctly warned is not by
itself a toolchain (INC-D001-05), turns out to be
usable further than expected - far enough to build
and link real x86-64 binaries, just not this
allocator (E2, INC-D005-04).

## 3. Experiments (recorded run Sections 2-5)

- **E1 - freestanding code generation.** A one-line
  freestanding TU compiled with
  `clang --target=x86_64-linux-gnu -ffreestanding
  -nostdinc`: exit 0, `ELF 64-bit LSB relocatable,
  x86-64` (recorded run Section 2). Clang's x86-64
  backend is real and works on this host.
- **E2 - the allocator's own TUs, freestanding headers
  only** (`-nostdinc` + clang's resource headers, full
  `-Wall -Wextra -Werror`): the API TU
  (`src/api/mimir_alloc.c`, needs only `<stddef.h>`
  through the project headers) compiles CLEAN for
  x86-64, exit 0; its object defines exactly the four
  public symbols as text symbols - `mimir_malloc`,
  `mimir_free`, `mimir_calloc`, `mimir_realloc` -
  matching D-002's native export finding. The core TU
  fails: `fatal error: 'string.h' file not found`
  (exit 1). The platform TU fails: `fatal error:
  'sys/mman.h' file not found` (exit 1). The failures
  are exactly and only the target libc headers.
- **E3 - the same TUs with default host include
  paths.** Both fail at `fatal error:
  'bits/libc-header-start.h' file not found` (exit 1):
  clang looks for the x86-64 multiarch include
  directory, it does not exist, and the build stops.
  No silent wrong-architecture build is possible by
  this route - a small mercy, recorded so nobody has
  to wonder whether a host-header object could be
  passed off as a cross object. It could not even be
  produced.
- **E4 - freestanding executable, end to end.**
  `tests/cross/probe_freestanding.c` (raw syscalls,
  no libc) compiled with clang (exit 0), linked with
  `ld.gold -m elf_x86_64` (exit 0) into a static ELF64
  x86-64 executable, then RUN on this Pi twice: by
  direct exec (the kernel's binfmt_misc handed it to
  Box64) and by explicit `box64`. Both runs printed
  `D005-X64-PROBE-OK` and exited with the probe's own
  status 42. Packaged as
  `scripts/cross/build_x86_64_freestanding_probe.sh`
  and the `make test-d005` target; both executed
  green in the recorded run (Section 3 there).
- **E5 - a real-world dynamically linked x86-64
  binary.** `box64 /opt/wine-11.4/bin/wine --version`
  printed `wine-11.4`, exit 0 (recorded run Section
  5). That loader is a glibc dynamically linked
  x86-64 ELF (interpreter `/lib64/ld-linux-
  x86-64.so.2`): Box64's wrapped-libc run road handles
  the general case, not only static freestanding
  binaries like E4.

**Reading of E1-E5, stated once and plainly:** code
generation exists, a linker for freestanding x86-64
exists, and execution of x86-64 binaries (static and
dynamically linked) exists and is proven. The single
missing component is the x86-64 libc sysroot that a
normal hosted build of the allocator and its suite
requires. Nothing in E1-E5 is a run of MimirAlloc or
of its test suite on x86-64, and none may be cited
as one.

## 4. Effect on the owed debts

- **Phase 3 x86-64 suite run (owed since C-007,
  tallied in C-009 as part of MET 7 / UNMET 9 /
  OWED 4 of 20): UNCHANGED - still owed in full.**
  This slice narrowed the blocker from "no x86-64
  target on the Pi" (C-009's words) to the precise
  missing component (the sysroot), and proved the run
  leg that will execute the suite once built. Neither
  fact is a suite run. Phase 3 exit stays **UNMET**
  on this leg alone, exactly as C-009 left it.
- **Phase 4 T3 (CI cross-compilation): PARTIALLY
  advanced, not discharged.** D-001's bar for T3 was
  "a repeatable, scripted cross build whose outputs
  are inspected (file format, ABI) and whose limits
  are stated". The freestanding probe build meets
  that bar for the freestanding subset (scripted,
  repeatable, `file`/`readelf`-inspected, limits
  stated in this doc). The allocator cross build and
  any CI run remain owed (Section 5). T3's CI half is
  addressed as a labelled proposal only (Section 6).
- **Phase 4 exit (E1-E5 in D-001): UNMET, unchanged.**
  Nothing in this slice touches the Android legs;
  the negative gate E5 ("a cross-compile alone does
  not pass") is reaffirmed - and this slice did not
  even produce the allocator cross-compile.
- **Supported targets (Roadmap Section 5): no row
  changes.** Linux x86-64 + glibc remains *Proposed*:
  portable code exists, an emulated run road now
  provably exists, and the build evidence still does
  not. Linux AArch64 + musl remains *Proposed*: the
  musl leg is D-008's, and this slice adds only the
  warning in INC-D005-06 about which musl package
  would and would not help.

## 5. Blocker record - the full allocator cross build

**Blocked on exactly one component:** an x86-64 libc
sysroot for the build host - target headers
(`<string.h>`, `<sys/mman.h>`, `<pthread.h>`, the
`bits/` tree), target libraries (glibc, libpthread),
crt startup objects, and the x86-64 dynamic loader
for the link step. Verified absent in this slice:
`x86_64-linux-gnu-gcc`, `/usr/x86_64-linux-gnu`,
`/usr/include/x86_64-linux-gnu`, `x86_64-linux-gnu-ld`,
any musl tool or sysroot, `qemu-x86_64` (a runner
only - and not needed, since Box64 is present).

**What would be needed (read-only `apt-cache policy`
results from this slice, recorded run Section 1):**
Debian bookworm offers, for this arm64 host:
`gcc-x86-64-linux-gnu` (candidate 4:12.2.0-3, which
pulls the cross binutils, `libc6-dev-amd64-cross`
2.36-8cross1 and the sysroot as dependencies) and
`qemu-user` / `qemu-user-static` (candidate
1:7.2+dfsg-7+deb12u18+b3) as an alternative runner.
`musl-tools` (candidate 1.2.3-1) exists but is a
host-architecture (aarch64) musl - see INC-D005-06.

**Smallest next step (one command, then one script):**

1. `sudo apt-get install -y gcc-x86-64-linux-gnu`
   on the Pi (needs Volmarr's word: it changes system
   package state on his home machine; this slice
   deliberately did not run it - a research-phase
   blocker record, not a system change, is D-005's
   deliverable, and the campaign rule is to keep the
   Pi's load and state untouched beyond the slice).
   No qemu package is required: Box64 is already the
   proven runner.
2. `./scripts/cross/build_x86_64.sh` - the proposed
   script in this slice then builds the suite for
   x86-64, `file`-verifies every binary, and runs it
   under Box64, recording
   `tests/cross/recorded_run_x86_64_suite.txt`. Only
   that recorded run discharges the Phase 3 debt.

**Not blockers, despite appearances:** clang's target
list (E1-E3 show exactly how far it goes), tcc
(AArch64-only), Wine's x86-64 libraries (Wine's own
unix libraries for Wine's own use; no libc headers,
no crt, no general sysroot - checked under
`/opt/wine-11.4`), and Box64's wrapped-library set in
`/usr/lib/box64-x86_64-linux-gnu` (eight runtime
libraries for emulation, not a build sysroot).

## 6. Scripts and CI - delivered, with execution status

| File | Status | What it is |
|---|---|---|
| `scripts/cross/probe_toolchains.sh` | **EXECUTED** (exit 0; output = recorded run Section 1) | Read-only probe printing PRESENT/ABSENT for every road component in Section 2, plus apt candidates. Safe to re-run any time; findings are data, exit stays 0. |
| `scripts/cross/build_x86_64_freestanding_probe.sh` | **EXECUTED SUCCESSFULLY** (exit 0; also via `make test-d005`) | Builds `tests/cross/probe_freestanding.c` for x86-64 (clang + gold), inspects the ELF, runs it via binfmt and via Box64, and asserts the exact output string and exit status 42 both times. Proves codegen + link + emulated run for FREESTANDING code only. |
| `tests/cross/probe_freestanding.c` | Source of the E4 probe; built and run as above | Raw-syscall x86-64 program: prints `D005-X64-PROBE-OK`, exits 42. Not the allocator; not a test of it. |
| `scripts/cross/build_x86_64.sh` | **PROPOSED - build path UNEXECUTED.** Its preflight was run and refused with exit 3 (`BLOCKED ... Missing: compiler:x86_64-linux-gnu-gcc sysroot:/usr/x86_64-linux-gnu`), recorded run Section 4 | The full allocator + suite cross build and Box64/qemu run, ready for the day the sysroot is installed (Section 5). Never present its preflight refusal as a build. |
| `.github/workflows/cross-x86_64.yml` | **PROPOSED - UNEXECUTED.** No CI exists in this repo; nothing has run under it | Manual-only (`workflow_dispatch`, per the project's standing rule - never on push). Job 1 would run the suite natively on an x86-64 hosted runner (the owed Phase 3 run, hosted-runner evidence). Job 2 is a reverse-cross BUILD-leg smoke check only - its emulated run leg is not wired (INC-D005-05). |
| `Makefile` target `test-d005` | **EXECUTED** (exit 0) | Additive standalone target running the freestanding probe script. Deliberately NOT part of `make test`: a cross probe must never gate the native suite. |

**CI position, plainly:** this repository still has
no working CI, and this slice does not change that.
The workflow file is a design recorded in the repo,
labelled proposed in its own header, inert until a
human enables it (manual trigger only). Local
recorded runs remain this project's evidence standard
(INC-D001-04 stands: a local `make test`, however
green, is not CI and is not worded as CI here).

## 7. Native regression (the slice must not break the Pi build)

Recorded run Section 6, all on the Pi at this base
plus this slice's files: `make clean && make test`
exit 0 - smoke 18/18; core 29 run 0 failed; B-007 70
run 0 failed; C-005 95 run 0 failed; C-006 23 run 0
failed; C-007 suite 40 passed, 0 failed, 70 skipped,
9 inspected (122 checks, 0 failed); C-008 42 run, 0
failed, 7 skipped-not-yet. `make test-d003` exit 0
(141 checks, 0 failed) after this slice's Makefile
edit. `make test-d005` exit 0. Samtal HTTP 200 at
the end of the run; 13 GiB available throughout.

## 8. Inconsistencies recorded by this slice (INC-D005-*)

1. **INC-D005-01 - D-001'S "NO QEMU" WAS IMPRECISE.**
   QEMU *is* installed on the Pi - `qemu-system-arm`
   10.0.2 and `qemu-utils` (addendum A4). What is
   absent is every x86-64-relevant piece:
   `qemu-x86_64`, `qemu-x86_64-static` and
   `qemu-system-x86_64`. D-001's T3 conclusion (no
   qemu run road for x86-64) stands; its blanket
   wording does not, and is corrected here so no
   later slice quotes "no qemu" as a host fact.
2. **INC-D005-02 - THE RUN ROAD NOBODY RECORDED.**
   Box64 0.4.5 with enabled binfmt_misc registration
   has been installed on the Pi since 2026-11-05
   (pi-apps), before Wave 4 began, yet no campaign
   document records it: D-001's probe list checked
   qemu for emulation and stopped there. The framing
   this corrects is real - every earlier document
   treats the x86-64 problem as build-host-plus-run-
   host both missing. Only the build half is missing.
   The keeper's Phase 3-debt wording, and any future
   slice brief, should say: *run road exists (Box64,
   proven D-005); sysroot missing.*
3. **INC-D005-03 - THIS SLICE'S OWN PROBE OUTPUT HAS
   AN ORDERING ARTEFACT.** Recorded run Section 1
   lists `.github/workflows` as PRESENT because this
   slice created that directory (for its proposed
   workflow file) before running the probe script.
   At base there was no `.github` at all - verified
   against the base git tree (addendum A1-A2). The
   probe script is honest about the tree it runs in;
   the artefact is in the sequencing, and it is
   recorded here rather than silently re-run away.
4. **INC-D005-04 - "THE ALLOCATOR CROSS-COMPILES"
   WOULD BE A FALSE HEADLINE FROM TRUE PARTS.** The
   API TU genuinely compiles for x86-64 today (E2).
   The allocator does not: its core and platform TUs
   fail without a sysroot, and no x86-64 allocator
   binary of any kind exists. Any summary of D-005
   that drops the TU-level split misstates the
   slice. (This is INC-D001-05's trap one level
   deeper: not just a target list, but a real object
   file, inviting a real overclaim.)
5. **INC-D005-05 - THE PROPOSED WORKFLOW'S SECOND
   JOB IS BUILD-LEG ONLY AS WRITTEN.** The Makefile
   runs test binaries directly, so on an x86-64
   hosted runner the aarch64 binaries would need an
   explicit `qemu-aarch64` invocation the workflow
   does not yet wire (noted in the file itself).
   Recorded so the file is never enabled under the
   belief that its emulated RUN leg exists.
6. **INC-D005-06 - `musl-tools` WOULD NOT BUY AN
   x86-64 MUSL ROAD.** The Debian candidate is a
   host-architecture (aarch64) musl toolchain; it
   would serve the Linux AArch64 + musl row (D-008's
   leg), not x86-64. Bookworm ships no
   `gcc-x86-64-linux-musl` package (checked:
   `apt-cache` policy and search both empty,
   addendum A3). An x86-64 musl road would need a
   non-Debian toolchain (e.g. a musl.cc tarball or
   zig, both absent) and is out of this slice.

## 9. Tester verification (self-test checklist, solo lead)

This slice ran solo under the brief's fallback: the
tester role is discharged by this checklist, every
item executed on the Pi against the final tree.

1. HEAD == base `f5965c1` and branch ==
   `yrsa/mimiralloc-05` before building. PASS
   (verified at slice start and re-verified before
   commit).
2. `README.md` untouched: `git status` shows no
   README change; README blob unchanged. PASS
3. Executed scripts run from the committed paths:
   `probe_toolchains.sh` exit 0;
   `build_x86_64_freestanding_probe.sh` exit 0;
   `make test-d005` exit 0. PASS (recorded run
   Sections 1, 3).
4. Proposed script honesty: `build_x86_64.sh` exits
   3 with the BLOCKED preflight message; its header
   and this doc label it unexecuted. PASS (Section 4).
5. Probe claims re-derived independently of the
   scripts: E1-E3 compiler behaviour, E4 output and
   exit 42 twice, E5 wine version - all reproduced
   in the recorded run from raw commands. PASS
6. Native regression green after all edits:
   `make test` exit 0 with the Section 7 counts;
   `make test-d003` exit 0. PASS (Section 6).
7. No claim in this doc exceeds the recorded run:
   doc greps clean for any statement that the suite
   or the allocator ran on x86-64. PASS
8. Pi left healthy: Samtal HTTP 200, 13 GiB
   available, no packages installed, no state changed
   outside the worktree and scratch dirs. PASS

## 10. Bottom line

D-005 set out to find whether MimirAlloc could be
cross-built for x86-64 on the Pi and its suite run
there. The honest answer is split down the middle:
the Pi can already *run* x86-64 - Box64 and binfmt
were sitting installed and unrecorded, and this
slice proved them with a purpose-built freestanding
binary and with Wine's real-world loader - and it
can *generate* x86-64 code, up to and including one
of the allocator's three library units. It cannot
*build* the allocator, because a build needs an
x86-64 libc sysroot and none exists here. That one
missing component, one `apt-get install` away and
reserved for Volmarr's word, is the entire remaining
distance to discharging the Phase 3 x86-64 debt.
Until it lands: **Phase 3 exit UNMET, Phase 4 exit
UNMET, x86-64 suite run OWED in full** - and the
blocker is now a named component with a proven
runner waiting for it, not an unknown.
