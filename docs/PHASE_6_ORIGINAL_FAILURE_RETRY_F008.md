# Phase 6 - Original Phase 1 Failing Command Retried: Blocker Record

Slice: **F-008 (team-08)**. Wave 6, Phase 6 exit elements
**X1** (the original failing command) and **X4** (the
blocker-with-exact-results alternative). Deliverables: this
file and `tests/mojo/recorded_run_f008.txt` (the real Pi run,
215 lines, sha256
`df97ce6f8be18ada5a810317e6fe43269dc5cc2cce425d3d38e7f289d9acca33`
as recorded; verify with `sha256sum` in the worktree).
**No Makefile wiring**: this slice's deliverable is an
evidence record, not a repeatable gate; a target that runs
`mojo --version` would dress the Pi analogue up as a test,
which is exactly the misreading Section 5 forbids.
Base: `38b27b06754730504f1033f1c8d6dcd4fa9fe204` (F-006
banked; Wave 6 = 6 of 9) on branch `yrsa/mimiralloc-08`,
worktree `/home/pi/mimiralloc-team-08` on Raspberry Pi
`purpleviking1`. Date of the recorded run: 2026-11-06
(22:42 EST). Governing texts: Roadmap Section 8 Phase 6
(exit sentence) and Sections 3.1-3.3; banked F-001
`docs/PHASE_6_FOUNDATION_PLAN.md` (Sections 5-7, the
definition this slice executes); banked F-002
`docs/PHASE_6_MOJO_TOOLCHAIN_READINESS.md`; the Android
field notes `docs/field-notes/android-arm64-2026-11/`
(especially 01, 02, 03, 05); banked A-005
`docs/failure-capture.md`, A-006
`docs/failure-classification.md`, A-007
`docs/INTEGRATION_LAYER_MAP.md`. Work style: **solo careful
work** by the team lead, no sub-agents. Pi load stayed
light (sequential `--version`-class invocations and one
4 KiB probe only; `free -g`: 15 GiB total, 13 GiB available
before and after; Samtal HTTP 200 on port 8787 before and
after). Nothing was installed. No Android or Termux device
was accessed. `README.md` is byte-untouched (sha256
`bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`
in the worktree and in `HEAD`); the public header
`include/mimir_alloc.h` is byte-untouched (sha256
`55107f05b3eb8c120230f76b2193fdc953af800af57a91caa405c84971af8772`
in both places) and no C source was touched, so the
4-symbol ABI is unchanged by construction.

> **VERDICT: THE LITERAL RETRY IS BLOCKED, AND THIS FILE IS
> THE BLOCKER RECORD.** Roadmap Phase 6's exit sentence
> offers two doors: *"original failing command succeeds on
> the affected ARM environment with explicit proof of
> allocator use and no new correctness failures, or record
> a blocker with exact results."* This slice takes the
> second door, on the definition F-001 Section 5.4 handed
> down. Three independent facts block the literal retry on
> this Pi, each proven from the repo or the Pi itself in
> the recorded run:
>
> 1. **The original failing command was never recorded
>    exactly** — no argv, exit code, or stderr log exists
>    anywhere in the banked record (Section 2, fact F7).
>    There is literally no command string to retry.
> 2. **The affected environment is absent** — the failure
>    happened on a Samsung SM-X400 (Android 16) in a Termux
>    glibc userspace with a 39-bit user-VA ceiling. This Pi
>    is native Debian/glibc, its loader exists at the path
>    the tablet lacked, and its VA probe succeeds at the
>    exact address the tablet failed (Sections 3-4).
> 3. **Both failing versions are absent** — the tablet
>    failed on Mojo **1.2.0** and nightly
>    **1.3.0.dev2026110205**; the only Mojo on this Pi is
>    **1.1.0 (8189361e)** (Section 3).
>
> The closest honest retry available on the Pi — the
> present toolchain's version/launcher path, walked through
> the same three steps the field note describes — was run
> with exact commands, outputs, and exit codes (Section 3,
> recorded run Section C): **every step exits 0**. The
> tablet failure does **not** reproduce here. Per F-001
> Section 5.4 and the roadmap's X5 exclusion, that success
> is **not** X1, is not evidence the tablet failure is
> fixed, and is claimed as nothing more than the exact
> results attached to this blocker. **X1 stays UNMET;
> the X4 blocker record is this file plus the recorded
> run.** F-009 assembles the Phase 6 exit packet.

## 1. What would have counted as a literal retry

F-001 Section 5.4 defines it, and this slice adopts the
definition verbatim in substance: on the affected
environment class — SM-X400-class Android/Termux glibc
userspace, or a recorded-equivalent 39-bit-VA ARM
environment — invoking the affected Mojo compiler (1.2.0,
or the 1.3.0.dev2026110205 nightly) through the Termux
glibc loader for `--version`, i.e. the invocation whose
behaviour field note 03 describes, with MimirAlloc wired
at whatever boundary F-003 proves, succeeding where it
previously aborted in TCMalloc `MmapAligned()`.

Two weaker shapes exist under that definition, and this
slice is explicit about which one it delivers:

- **Reconstruction** (a case file built by the A-005
  "Adding a real Mojo case" procedure, its argv labelled
  as reconstructed from field note 03's prose): **not
  built by this slice** — see INC-F008-04 for why, stated
  as a decision, not an omission.
- **Pi analogue** (running the Pi's Mojo 1.1.0
  `--version` path): **run and recorded** (Section 3).
  F-001's warning governs it: it already succeeds without
  any allocator substitution, so its success proves
  nothing about the failure, the allocator, or Phase 6.

## 2. Field-note facts, re-verified from the repo

Rule for this section: every fact is cited to the banked
file that states it, at the line or section named, and was
re-read in this slice (the recorded run Section A holds
the verbatim lines). **Documented** means the repo records
it; nothing here is re-stated as a fresh observation of
the tablet, which this slice never touched.

| # | Fact | Exact source |
|---|---|---|
| F1 | The affected device is one physical **Samsung Galaxy Tab S10 Lite Wi-Fi, model SM-X400**, device `gts10litewifi`, **Android 16/API 36**, build `X400XXS8CZI2`; eight ARM64 cores; system page size **4,096 bytes** in the recorded probe; MemTotal 5,841,464 KiB | `docs/field-notes/android-arm64-2026-11/02_TABLET_INVENTORY.md`, "Identified device" and "Memory snapshot" |
| F2 | **First failure, in prose**: "The Linux ARM64 Mojo **1.2.0** package was installed in a separate glibc Python environment. Its launcher could not directly execute the compiler because the ELF requested `/lib/ld-linux-aarch64.so.1`, which was not present at that path in this Android environment. Invoking the installed Termux glibc loader directly got farther: **before `--version`, the compiler aborted with a TCMalloc `MmapAligned()` failure** and a diagnostic saying it assumed a **48-bit** virtual address space. A tested **1.3.0.dev2026110205** nightly similarly aborted." The diagnostic's 1 GiB reservation was a virtual-address range request, not physical memory | `docs/field-notes/android-arm64-2026-11/03_VIRTUAL_ADDRESS_AND_MOJO.md`, line 5 and the paragraph following it |
| F3 | **VA probe on the tablet**: a small anonymous 4 KiB `mmap` test with fixed-no-replace semantics — request at `1 << 38` returned that address; requests at `1 << 39` and `1 << 47` returned **ENOMEM** — "consistent with a **39-bit user virtual-address boundary**", explicitly "not a complete kernel configuration audit" | Field note 03, line 11 ("Device probe") |
| F4 | **Ledger corroboration**: "Mojo stable — Linux ARM64 Mojo 1.2.0 installed in separate glibc environment; direct loader reached compiler then TCMalloc aborted before `--version`"; "Mojo nightly — 1.3.0.dev2026110205 produced the same allocator startup failure"; sequence point 4: "The Mojo packages failed at allocator initialization. An address-space probe gave evidence consistent with a 39-bit ceiling on this device." | `docs/field-notes/android-arm64-2026-11/01_EVIDENCE_AND_TIMELINE.md`, evidence-ledger rows (lines 10-11) and sequence point 4 |
| F5 | **TCMalloc experiment context**: the investigated TCMalloc source is pinned at upstream `12f255231938d30493186b0a037feedd70f5a1c1`, local experiment commit `3390b10`; the candidate address-width change was exercised **header-level only** (48 default accepted, explicit 39 accepted, 38 and 49 rejected); acceptance gates 1-6 remain open; the experiment "was never linked into the installed Mojo compiler" and is not MimirAlloc code | `docs/field-notes/android-arm64-2026-11/05_TCMALLOC_SOURCE_EXPERIMENT.md`, throughout |
| F6 | **Provenance conflict, unresolved**: public Modular source at `99ab7e90258db16bda9261b19bcb59d729ee3429` **selects jemalloc** for the Linux Mojo driver, yet the shipped 1.2.0 wheel and the 1.3.0.dev nightly **aborted in TCMalloc**; "the package's exact relationship to that public commit was not established" | Field note 03, "Public source versus package"; `docs/SUPPORTED_TARGETS.md` inconsistency I-3 (line 180) records the same conflict |
| F7 | **No exact command exists**: A-005 inconsistency 1 — "No concrete Mojo failure exists in the repo yet. There is no recorded Mojo failure command, revision, or failure log anywhere in this repository." A-006's A6-10 — the classification framework "has never been exercised against a real Mojo/tcmalloc failure, because no such failure packet exists in the repository." A-007 truth (ii) — "No reproducible Mojo/tcmalloc failure exists in the repo", Phase 1's Section 3.3 exit gate **Blocked**, resumption point "a real A-005 packet, plus the binaries it ran". A-005's "Adding a real Mojo case" section records that the Mojo capture "has **not** been run" and lists prerequisites (exact Mojo revision, implicated tcmalloc revision and build options, the build/run command exactly as issued, which process fails) that are themselves unrecorded for the tablet failure | `docs/failure-capture.md` (inconsistency 1, lines 294-297; "Adding a real Mojo case"); `docs/failure-classification.md` (A6-10, lines 564-566); `docs/INTEGRATION_LAYER_MAP.md` (truth (ii), line 20) |
| F8 | **F-001's verdict carries all of this**: INC-F001-01 (command never recorded), INC-F001-02 (environment mismatch), INC-F001-03 (version mismatch), and Section 5.4's ruling that a literal retry is "BLOCKED on the Pi", with this slice named as the owner of the blocker record | `docs/PHASE_6_FOUNDATION_PLAN.md`, Sections 5.4, 7 (row X1), 8 |

A classification note, kept at the strength the banked
framework allows: A-006's vocabulary applied to the prose
account gives the shape **address-space configuration**,
which A-006 caps at *probable* on a single packet — and
this is not even a packet. This slice assigns no stronger
classification (F-001 Section 5.2 already recorded this;
nothing in the Pi runs changes it).

## 3. The closest honest retry on the Pi — exact results

All commands below were run on the Pi in this slice; the
full verbatim record, including the absence checks, is
`tests/mojo/recorded_run_f008.txt` Sections B-C. The
field-note failure has three steps; the retry walks the
same three steps with the only toolchain the Pi holds.

**Pi toolchain as found** (recorded run Section B):
`which mojo` = `/usr/local/bin/mojo`, a wrapper whose whole
body is `exec /home/pi/.pixi/bin/pixi run --manifest-path
/home/pi/mojo-dev/pixi.toml mojo "$@"`; the manifest
declares `platforms = ["linux-aarch64"]` and dependency
`mojo = ">=1.1.0,<2"`; the environment's `conda-meta/`
holds exactly `mojo-1.1.0-release.json`,
`mojo-compiler-1.1.0-release.json`,
`mojo-python-1.1.0-release.json` (package `"version":
"1.1.0"`); the real compiler
`/home/pi/mojo-dev/.pixi/envs/default/bin/mojo` is an ARM
aarch64 ELF, dynamically linked, **interpreter
`/lib/ld-linux-aarch64.so.1`** (`file` output, recorded) —
the same interpreter path the tablet lacked.

| Step | Exact command (as run) | Output | Exit |
|---|---|---|---|
| R1, launcher path (wrapper → pixi → compiler) — mirrors the tablet's step 1, which failed at exec | `mojo --version` | `Mojo 1.1.0 (8189361e)` | **0** |
| R2, compiler binary directly | `/home/pi/mojo-dev/.pixi/envs/default/bin/mojo --version` | `Mojo 1.1.0 (8189361e)` | **0** |
| R3, explicit glibc loader invocation — mirrors the tablet's step 2, which aborted before `--version` in `MmapAligned()` | `/lib/ld-linux-aarch64.so.1 /home/pi/mojo-dev/.pixi/envs/default/bin/mojo --version` | `Mojo 1.1.0 (8189361e)` | **0** |
| R4, R1 repeated (analogue stability) | `mojo --version` | `Mojo 1.1.0 (8189361e)` | **0** |

**Absence checks run in the same recording** (Section B):
`ls /data/data/com.termux` → *No such file or directory*,
exit 2; `command -v getprop` → no output, exit 1;
`ls /system/bin` → *No such file or directory*, exit 2;
`find /home/pi -maxdepth 4` for Mojo 1.2 / 1.3.0.dev
artifacts → no output, exit 0; `ls -l
/lib/ld-linux-aarch64.so.1` → **present** on the Pi
(symlink to `aarch64-linux-gnu/ld-linux-aarch64.so.1`),
exit 0 — the exact inverse of the tablet's step-1 failure.

Reading, stated as narrowly as the evidence allows: on
this Pi, in this environment, with Mojo 1.1.0, none of the
three failure steps fails. That is a statement about the
Pi analogue only. It does not reproduce, explain, or fix
the tablet event, and — because 1.1.0 is not a failing
revision and native Linux/glibc is not the affected
environment — it cannot be promoted into one.

## 4. VA-width probe: tablet vs Pi, same method

Method (field note 03, restated in the recorded run
Section D): small anonymous `mmap`, `PROT_NONE`,
`MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE`,
released immediately after a success; probe length 4096
as in the note (the Pi's page size is 16384 and `mmap`
rounds the length up — recorded, and a comparability
limit, INC-F008-07). The probe program was written and
compiled in this slice (`cc -O2`, exit 0); its source is
printed verbatim in the recorded run.

| Request | Tablet (note 03, documented) | This Pi (this slice, recorded) |
|---|---|---|
| `1 << 38` (`0x4000000000`) | returned that address | **OK**, returned `0x4000000000` |
| `1 << 39` (`0x8000000000`) | **ENOMEM** | **OK**, returned `0x8000000000` |
| `1 << 47` (`0x800000000000`) | **ENOMEM** | **FAIL**, errno 12 (ENOMEM) |

The single load-bearing difference is the middle row:
the address whose failure on the tablet is consistent
with the 39-bit ceiling **succeeds** on the Pi. The Pi
therefore does not exhibit the address-space property
the tablet failure is attributed to, independently of
the version and userspace differences. No exact Pi VA
width is claimed: failure at `1 << 47` is consistent
with several widths and limits, and three probes are
not a kernel audit — the same restraint note 03 applies
to its own probe.

## 5. The blocker, stated precisely

**Blocked item:** Roadmap Phase 6 exit element X1 —
"original failing command succeeds on the affected ARM
environment with explicit proof of allocator use and no
new correctness failures".

**Why it is blocked (three independent legs, any one of
which suffices):**

1. **No command to retry.** The banked record holds a
   faithful prose *account* (F2-F4) and no argv, working
   directory, environment, exit code/signal, or stderr
   log (F7). Inventing an argv and calling it the
   original would be fabrication; this slice does not do
   it.
2. **No affected environment on the Pi.** Android 16,
   the Termux app context, and its separate glibc
   userspace are absent (absence checks, Section 3);
   the measurable environment properties also differ in
   the failure-relevant direction — the ELF interpreter
   is present here and was absent there, the user-VA
   ceiling probed here is not the tablet's 39-bit one
   (Section 4), and the page size is 16384 here against
   4096 probed there (F1). A retry run here is an
   analogue by construction, never the affected
   environment.
3. **No failing version on the Pi.** Mojo 1.2.0 and
   1.3.0.dev2026110205 exist nowhere on this Pi that
   this slice's inventory found (conda-meta holds only
   1.1.0 records; the artifact search is empty). Nothing
   was installed in this slice, and installing a
   different version — even if acquired — waits on
   Volmarr's word (F-002's readiness record) and would
   still land in the wrong environment (leg 2) for a
   command that does not exist (leg 1).

**A second-order blocker, recorded so F-009 does not
have to discover it:** even with legs 1-3 cured, X1's
full reading asks for the command to succeed *with
MimirAlloc wired*. The process that aborted is the Mojo
**compiler itself**; F-001's boundary B6 (substituting
the allocator inside the compiler process) is rated
**absent / BLOCKED** — the compiler is a prebuilt,
stripped binary whose runtime library defines a
TCMalloc-derived internal surface, and MimirAlloc's
whole surface is four namespaced symbols with no
standard-name provider. The boundary F-003 actually
proved (B2+B1) wires a *generated program's own explicit*
calls; it does not reach the compiler process. X1 is
therefore blocked on the wiring question as well as on
the retry question (INC-F008-05).

**Resumption point (exact):** the literal retry becomes
runnable when (a) the SM-X400 tablet environment, or an
equivalent 39-bit-VA Android/Termux glibc environment
ratified as equivalent, is brought into scope **by
Volmarr's word** (any work on that device is his to
authorise; this campaign's venue is the Pi), with
(b) Mojo 1.2.0 or the 1.3.0.dev2026110205 nightly
installed there as they were for the field work, and
(c) the invocation captured this time through the A-005
harness so the packet Phase 1 never produced finally
exists. Until then, X1's honest state is **BLOCKED**,
and the roadmap's own X4 door — this record, with the
exact results of Sections 3-4 — is the conformant
outcome.

## 6. What this slice does not do or claim

- It does **not** present any Pi run as the original
  command, as a reproduction, or as X1 met. The R1-R4
  successes are the Pi analogue, labelled as such
  everywhere they appear, per the X5 exclusion.
- It does **not** build a reconstruction packet
  (INC-F008-04 records the decision and its reason).
- It installs nothing, downloads nothing, accesses no
  Android/Termux device, and attempts no allocator
  substitution in any Mojo process.
- It touches no source, header, Makefile, or test other
  than adding the recorded run; `make test` is run as
  the standing gate only.
- It claims no Phase 6 exit element beyond recording
  the X4 blocker with exact results. Exit accounting
  for the packet: **X1 UNMET (BLOCKED, this record);
  X2** stands as F-005 proved it in the B2+B1 scope and
  is untouched here; **X3** is untouched here (F-006's
  bounded statement stands); **X4 — the blocker record
  exists** (this file + the recorded run), for F-009 to
  assemble; **X5** guard respected.
- Carried debts stand: Phase 1 exit remains unmet
  (INC-F008-08), and with it the Phase 3 / Phase 4
  positions the earlier packets recorded. Nothing in
  this slice discharges any of them.

## 7. Inconsistencies recorded in this slice — INC-F008-NN

None of these is silently resolved. Each names its
resumption point or its owning slice.

- **INC-F008-01 — The original failing command is still
  unrecorded (carries INC-F001-01).** This slice
  re-verified the absence from the primary banked
  sources (F7) rather than trusting F-001's summary:
  A-005, A-006, and A-007 concur, and the only failure
  packet in `evidence/runs/` is the synthetic smoke
  case. Owner: F-009 (restate in the packet); cure only
  via Section 5's resumption point.
- **INC-F008-02 — Environment mismatch persists
  (carries INC-F001-02).** The exit text says "the
  affected ARM environment"; Wave 6's venue is the Pi.
  This slice adds fresh Pi-side measurements to the
  mismatch (loader present; 2^39 probe succeeds; page
  size 16384 vs 4096) — the mismatch is now measured,
  not merely asserted — but no banked document
  reconciles the two environments, and this slice does
  not reconcile them by fiat. Owner: F-009's scope
  paragraph; a venue change needs Volmarr's word.
- **INC-F008-03 — Version mismatch persists (carries
  INC-F001-03).** Failing revisions 1.2.0 /
  1.3.0.dev2026110205; Pi holds 1.1.0 only (fresh
  inventory in the recorded run Section B). Any
  same-revision comparison in Wave 6 is same-revision
  only within 1.1.0. Acquiring a failing revision waits
  on Volmarr's word per F-002, and would not by itself
  cure INC-F008-01 or INC-F008-02.
- **INC-F008-04 — No reconstruction packet was built,
  by decision.** F-001 offered F-008 two honest shapes:
  the blocker record, or a labelled reconstruction via
  the A-005 procedure. This slice delivers the blocker
  record only, because a reconstruction's load-bearing
  parts are themselves unrecorded: A-005's prerequisites
  demand the implicated tcmalloc revision and build
  options for the failing wheels, which F6 shows are
  provenance-unresolved (public source selects jemalloc;
  the wheels aborted in TCMalloc). A case file written
  today would have to invent exactly the fields whose
  absence is the finding. If Volmarr wants a
  reconstruction anyway, it must list its invented
  fields in its notes and be labelled a reconstruction
  in every reference — never "the original command".
- **INC-F008-05 — X1 is blocked twice: no retry, and no
  compiler-process boundary.** Section 5's
  second-order blocker: the aborting process is the
  compiler; F-001 B6 is absent/BLOCKED against the
  current 4-symbol namespaced surface; F-003's proven
  B2+B1 boundary reaches generated programs' explicit
  calls only. Any future literal-retry plan must name
  its compiler-process wiring explicitly or record
  this leg as still blocked. Owner: F-009 (state both
  legs); any new-surface decision is a future slice's,
  under its own contract decision.
- **INC-F008-06 — Recording blemish in the recorded
  run's `readelf` window.** Section B's
  `readelf -l ... | grep -A1 INTERP` captured the INTERP
  segment header but not the following
  "[Requesting program interpreter: ...]" line. The
  interpreter fact is nevertheless proven inside the
  same file by the `file` output ("interpreter
  /lib/ld-linux-aarch64.so.1") and by the loader's
  presence at that path (`ls -l`, exit 0), so this is a
  blemish in one window, not an evidence gap. It is
  recorded here rather than repaired after the fact:
  the recorded run stands as run.
- **INC-F008-07 — VA-probe comparability limits.** The
  Pi probe used length 4096 on a 16384-byte-page kernel
  (the note's tablet probe ran on a 4096-byte-page
  system), probed single addresses, and — like the
  tablet probe — is not a kernel audit. It establishes
  exactly one comparative claim: at `1 << 39`, the
  tablet probe failed with ENOMEM and the Pi probe
  succeeded. No exact Pi user-VA width is measured or
  claimed anywhere in this slice.
- **INC-F008-08 — Phase 6 still inherits Phase 1's
  unmet exit (carries INC-F001-09).** A-005 recorded
  three unreconciled Phase 1 exit gates and no failure
  packet; X1 presupposed the packet. This X4 blocker
  record is Phase 6's conformant outcome; it does not
  cure, discharge, or soften Phase 1's position, and
  F-009 must restate it, as this slice does here.
