# Phase 6 - Foundation Plan: Actual Mojo Integration

Slice: **F-001 (team-01)**. Wave 6, Phase 6 foundation plan.
Deliverable: `docs/PHASE_6_FOUNDATION_PLAN.md` (this file only).
Base: `211eb31c9c1332e17298d8d2ac498fb49415b39f` (E-009 banked;
Wave 5 COMPLETE 9 of 9) on branch `yrsa/mimiralloc-01`, worktree
`/home/pi/mimiralloc-team-01` on Raspberry Pi `purpleviking1`.
Governing text: Roadmap Section 8 Phase 6 ("Actual Mojo
integration"), read with Roadmap Sections 2 (evidence levels),
3.1-3.3 (the Phase 1 failure investigation), 5 and 6, the banked
Phase 1 documents `docs/failure-capture.md` (A-005),
`docs/failure-classification.md` (A-006) and
`docs/INTEGRATION_LAYER_MAP.md` (A-007), the Android field notes
`docs/field-notes/android-arm64-2026-11/` (especially 01, 03, 05,
11, 13), and the banked Phase 5 packet
`docs/PHASE_5_EXIT_EVIDENCE.md` (E-009). Wave 6 slice queue:
WAVE_PLAN.md in the coordinator checkout `~/mimiralloc-forge`
(F-002..F-009), read by this team on 2026-11-06. `README.md` was
not touched by this slice. Work style: solo careful work, no
sub-agents, no build of the allocator run by this slice beyond
the `make test` gate at the end; the Mojo probes in Section 2
are `--version` / `--help` / target-listing invocations only.
Pi load stayed light; Samtal lives on this Pi and was not
starved (`free -g`: 15 GiB total, 13 GiB available).

> **PHASE 6 IS NOT EXITED, AND THIS PLAN EXITS NOTHING.**
> The Roadmap Phase 6 exit sentence (Section 8, Phase 6) is,
> verbatim: *"original failing command succeeds on the
> affected ARM environment with explicit proof of allocator
> use and no new correctness failures, or record a blocker
> with exact results. A successful unrelated Mojo hello-world
> does not close this milestone."* At this base **no** Phase 6
> exit element is met, and the plan's central findings are
> three blockers stated before anything else:
>
> 1. **The original failing command was never recorded
>    exactly.** No banked document holds its argv, its exit
>    code, or a failure log (Section 5). That is INC-F001-01,
>    not a gap this plan fills by invention.
> 2. **The affected environment is not this Pi.** The failure
>    on record happened on a Samsung SM-X400 tablet in an
>    Android/Termux glibc environment, on Mojo 1.2.0 and a
>    1.3.0.dev nightly. This Pi runs native Linux/glibc, holds
>    Mojo **1.1.0**, and `mojo --version` **succeeds** here
>    (Sections 3 and 5). INC-F001-02, INC-F001-03.
> 3. **No boundary is PROVEN-available today** by which a
>    Mojo program's allocations reach MimirAlloc. The
>    allocator surface is exactly four namespaced symbols
>    (`mimir_malloc`, `mimir_free`, `mimir_calloc`,
>    `mimir_realloc`); there is no standard-name interposition
>    anywhere in the banked work, and no Mojo allocator hook
>    is described in any banked source (Section 6).
>
> **CARRIED DEBT, STATED BEFORE ANYTHING ELSE:** Phase 5 exit
> is MET in Pi-native scope only (E-009). Phase 4 exit remains
> **UNMET** and Phase 3 exit remains **UNMET** in the banked
> record (E-009 carried-owed register O-01..O-18; the physical
> Android run and the x86-64 suite run are still OWED).
> Phase 1's own exit also remains unmet in the banked record:
> A-005 recorded three unreconciled Phase 1 exit gates and no
> concrete Mojo failure packet (Section 5.4). Nothing in this
> plan or in Wave 6 discharges any of that.

## 1. What this plan does

1. Records the Mojo toolchain inventory **as found on the Pi**
   in this slice, command by command (Section 3).
2. States what the banked sources do and do not say about Mojo
   on Linux ARM64, with file-and-section citations (Section 4).
3. Restates the Phase 1 problem exactly as the banked record
   holds it, separates the two records that must not be merged,
   and defines what would count as "the original failing
   command" for F-008 (Section 5).
4. Enumerates the candidate integration boundaries against a
   namespaced-only allocator surface, each rated honestly
   (Section 6).
5. Maps every Phase 6 task and exit element to the Wave 6
   slice (F-002..F-009) that will prove or block it
   (Section 7).
6. Records every inconsistency found as INC-F001-NN
   (Section 8). Claim labels throughout: **PROVEN on the Pi**
   (run or inspected by this slice, or by a banked slice whose
   recorded run is cited), **documented** (a banked source says
   so; not re-run here), **OWED** (absent; named owner), or
   **BLOCKED** (absent with a named resumption point).

## 2. Evidence actually checked on the Pi in this slice

All checks below were run by this team on 2026-11-06
(21:0x-21:1x EST) in `/home/pi/mimiralloc-team-01` or against
the named absolute paths, sequentially, load kept light.
Nothing was installed; installs are reserved for Volmarr's
word and none was needed to answer the inventory questions.

- Host: `uname -m` = `aarch64`; `uname -a` = `Linux
  purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP PREEMPT Sat Oct
  10 09:05:25 EDT 2026 aarch64 GNU/Linux`; `/etc/os-release`
  = Debian GNU/Linux 12 (bookworm); `getconf PAGESIZE` =
  `16384`; `free -g` = 15 GiB total, 13 GiB available.
- `git log --oneline -1` at slice start =
  `211eb31 E-009: Phase 5 exit evidence packet ...`;
  branch `yrsa/mimiralloc-01`; worktree clean except the
  untracked `hidden_files/` assignment directory (the
  standing keeper pattern).
- Every command and output quoted in Section 3 was run in
  this slice. Banked-document claims are cited to their file
  and are labelled **documented**, never re-stated as fresh
  runs.

## 3. (a) Mojo toolchain inventory on the Pi

### 3.1 Headline verdict

**Mojo IS present on the Pi, and it runs.** Exactly one Mojo
installation exists: a pixi/conda environment at
`/home/pi/mojo-dev`, providing **Mojo 1.1.0 (8189361e)** for
`linux-aarch64`. **MAX is NOT installed** as a usable package,
the **`modular` CLI is absent**, **`pixi` is not on PATH**
(it exists only at an absolute path), and **`~/.modular` does
not exist**. Each component is stated separately below;
presence of one is never evidence for another.

### 3.2 Component-by-component record

| Component | State on the Pi | Exact evidence (this slice) |
|---|---|---|
| `mojo` on PATH | **Present, as a wrapper script** | `which mojo` = `/usr/local/bin/mojo`; `ls -l` = 99 bytes, root-owned, `file` = Bourne-Again shell script; its whole body is: `exec /home/pi/.pixi/bin/pixi run --manifest-path /home/pi/mojo-dev/pixi.toml mojo "$@"` |
| `mojo --version` | **Works, PROVEN on the Pi** | Output: `Mojo 1.1.0 (8189361e)`, exit 0. The compiler starts on this Pi; the tablet startup failure of Section 5 does **not** reproduce here |
| Real compiler binary | **Present** | `/home/pi/mojo-dev/.pixi/envs/default/bin/mojo`: ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked, interpreter `/lib/ld-linux-aarch64.so.1`, stripped, 133M. `readelf -d`: DT_NEEDED includes `libMSupportGlobals.so`, `libAsyncRTRuntimeGlobals.so`, `libc.so.6`; RPATH `$ORIGIN/../lib:...` (matches A-007's L2 inventory, **documented** there, re-verified here) |
| Second `mojo` binary | **Present, different kind** | `/home/pi/.pixi/bin/mojo`: ELF 64-bit LSB executable, ARM aarch64, **statically linked**, stripped, 657K, one of several hardlinked names in `~/.pixi/bin/`. A-007 Section 4 already recorded that the two binaries give different linkage findings; a claim about "the Mojo binary" must name which one |
| `pixi` | **Present only at an absolute path; absent from PATH** | `which pixi` and `command -v pixi` fail (PATH is `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin` in this shell). `/home/pi/.pixi/bin/pixi --version` = `pixi 0.81.0`; the binary is 68,698,776 bytes. The `mojo` wrapper works only because it names pixi by absolute path |
| `modular` CLI | **Absent** | `modular --version` = `command not found`. The only modular-named file in the environment's `bin/` is `modular-crashpad-handler`, which is not a CLI for managing the toolchain |
| `~/.modular` | **Absent** | `ls -la ~/.modular` = `No such file or directory`. The toolchain was provisioned through pixi/conda, not through a Modular user install; no inference from `~/.modular` is available to any Wave 6 slice |
| `max` binary / MAX package | **Absent** | `which max` fails; no `max*` binary in the environment `bin/`; `ls .../lib/libmax*` = `No such file or directory`. Strongest evidence is Mojo's own output: `mojo build --print-supported-targets` printed `isMaxInstalled: max.package_root resolved to /home/pi/mojo-dev/.pixi/envs/default but /home/pi/mojo-dev/.pixi/envs/default/lib/libmax.so does not exist` before listing targets (exit 0). A `share/max/cache/.mojo_cache` directory exists; a cache directory is not an installed MAX |
| Mojo workspace manifest | **Present** | `/home/pi/mojo-dev/pixi.toml`: workspace `mojo-dev`, `platforms = ["linux-aarch64"]`, channels `https://conda.modular.com/max` and `conda-forge`, dependency `mojo = ">=1.1.0,<2"`. Environment size on disk: `du -sh /home/pi/mojo-dev` = 1011M |
| Package record | **Present** | `.../conda-meta/mojo-1.1.0-release.json`: `arch: aarch64`, `platform: linux`, `subdir: linux-aarch64`, version `1.1.0`, size 114,828,378 bytes, `license: LicenseRef-Modular-Proprietary`, depends on `mojo-compiler ==1.1.0`. The licence posture itself is F-002's slice; this plan records only the string as found |
| Mojo standard library | **Compiled form only** | The environment `lib/` holds `std.mojoc` and the runtime libraries (`libKGENCompilerRTShared.so`, `libMSupportGlobals.so`, `libAsyncRTRuntimeGlobals.so`, `libMojoJupyter.so`, `libMojoLLDB.so`); **no `.mojo` stdlib sources** were found in the environment. Consequence: no Wave 6 slice can inspect Mojo stdlib source on this Pi to settle allocator-hook questions (Section 6, option B5; INC-F001-06) |
| `mojo build` linker surface | **Present, PROVEN on the Pi** | `mojo build --help` lists, under Linker options: `-Xlinker <ARG>` (pass ARG to the linker) and `--lld-path <PATH>`. It also lists `--emit shared-lib` / `object` / `exe`. No allocator-selection option of any kind appears in `mojo build --help` or `mojo run --help` as printed in this slice |
| Registered targets | **Recorded** | `mojo build --print-supported-targets` (after the MAX-absence line above) lists `aarch64`, `aarch64_32`, `aarch64_be`, `arm64`, `arm64_32`, riscv and x86 families, exit 0. A target *name* in a list is not a support promise for this device; see Section 4 |
| Other Mojo on the Pi | **Present outside the repo** | Mojo source files exist under `/home/pi/c2/` (an Aesir checkout, e.g. `aesir_engine/*.mojo`, `armvegr/*.mojo`). They are **not** in this repository, carry no MimirAlloc provenance record, and are evidence of nothing in this slice beyond existing. See INC-F001-11 |
| Mojo in the research library / prior notes | **Versions differ from the Pi's** | The field notes record Mojo **1.2.0** and **1.3.0.dev2026110205** - on the tablet, not the Pi (Section 5). A-005's recorded manifest already listed `toolchain.mojo` = `Mojo 1.1.0 (8189361e)` for this Pi (**documented**, `docs/failure-capture.md`). No other Mojo version exists anywhere on the Pi that this inventory found |

## 4. (b) ARM64 support evidence for Mojo, from banked sources

Rule for this section: a source is cited for what it actually
says, in the file and section named. Where the banked sources
are silent, this section says so. No web source was fetched
by this slice; the field notes' external links are recorded
as links, not as fetched evidence.

### 4.1 What the sources do say

1. **The installed artifact itself is the strongest evidence,
   and it is Pi evidence, not a documented promise.** The
   `pixi.toml` in `/home/pi/mojo-dev` declares
   `platforms = ["linux-aarch64"]` and the conda package
   record declares `subdir: linux-aarch64` (Section 3.2).
   Together with `mojo --version` exiting 0 on this Pi, this
   is **PROVEN on the Pi**: a published linux-aarch64 Mojo
   1.1.0 package exists, is installed here, and its compiler
   starts on native ARM64 Linux/glibc. It proves nothing
   about Android/bionic, about other versions, or about
   allocator behaviour.
2. **Roadmap Section 2, table row 1 - about TCMalloc, not
   Mojo.** "TCMalloc lists Linux AArch64 as supported [S1],
   and its public implementation contains AArch64-specific
   code", with the explicit non-claim that this does not
   establish any particular Mojo revision working anywhere.
   Any Wave 6 text that cites this row as *Mojo* ARM64
   support evidence misreads it (the same caution is in
   `docs/SUPPORTED_TARGETS.md`, inconsistency I-4,
   **documented**).
3. **Field note 04 (`docs/field-notes/android-arm64-2026-11/
   04_ANDROID_AND_LINUX_TOOLCHAINS.md`)** records, as an
   external-reference summary, that Modular documents Linux
   glibc requirements and describes cross-compilation support
   as under development, citing the Mojo system-requirements
   and compilation-targets pages. **Documented** (a summary
   of external pages, in the note); the pages themselves are
   not in the repository and were not fetched by this slice.
4. **Field note 03, "Primary reading"**, and **field note 14,
   Section on sources**, list the same two Modular pages
   (requirements; compilation targets) as the places where
   supported environments are defined. Neither note quotes a
   Mojo Linux-ARM64 support matrix into the repository.
5. **`docs/SUPPORTED_TARGETS.md`** is a matrix of
   *MimirAlloc* targets, not of Mojo support. Its Mojo rows
   are failure-side only: the Linux ARM64 Mojo package ELF
   requested `/lib/ld-linux-aarch64.so.1` on the tablet
   (citing field note 03), and inconsistency I-3 records the
   jemalloc/TCMalloc provenance conflict (Section 5.3).
6. **`docs/INTEGRATION_LAYER_MAP.md` (A-007), Sections 4-7**
   inventories the Pi's Mojo binaries per exact binary
   (dynamically linked pixi-environment compiler; statically
   linked `~/.pixi/bin/mojo`) and the runtime libraries'
   allocator symbols. That is linkage evidence about the
   installed 1.1.0, **documented** - not a support statement
   by any vendor, and not evidence about the failing tablet
   wheels.

### 4.2 Where the sources are silent - stated plainly

- **No banked source states a Mojo supported-platform matrix
  for Linux ARM64**: no minimum kernel, glibc, page-size, or
  VA-width requirement for Mojo on ARM64 appears anywhere in
  `docs/`, `docs/research/`, or the field notes as text. The
  research library's ARM file (`docs/research/
  03_ARM_RISCV_ANDROID.md`) discusses AArch64 address widths,
  page sizes and ABI discipline generally and contains **no
  Mojo support claim at all**.
- **No banked source describes Mojo allocator customisation
  on any platform** - see Section 6, option B5.
- **No banked source promises that the tablet class of
  environment (Android/Termux glibc, 39-bit user VA) is a
  supported Mojo environment.** Field note 03's evidence
  points the other way for the versions tried there.

Verdict for (b): Mojo on **native Linux ARM64/glibc** is
**PROVEN on the Pi** at version 1.1.0 by the installed
artifact's own behaviour. Every wider ARM64 support claim -
Android, other versions, other VA widths - is **documented
at most, and mostly silent** in the banked sources. F-002
must not promote the Pi artifact into a support matrix.

## 5. (c) The Phase 1 problem, restated exactly

Two records exist. They describe different things, and Wave 6
fails honestly only if it never merges them.

### 5.1 Record one: the repository's Phase 1 record - no failure packet exists

- `docs/failure-capture.md` (A-005), in its opening sections
  and its "Roadmap inconsistencies" list, states: the only
  captured run is a **synthetic smoke case** (a script that
  prints two lines and exits 3 on purpose), with manifest
  claim scope *"harness-evidence-only: proves this command
  ran here with this result; not evidence of the
  Mojo/tcmalloc blocker"*; and, verbatim in substance,
  **"No concrete Mojo failure exists in the repo yet. There
  is no recorded Mojo failure command, revision, or failure
  log anywhere in this repository."** Its "Adding a real Mojo
  case" section lists the prerequisites for a real capture -
  exact Mojo revision, implicated tcmalloc revision and build
  options, the build/run command exactly as issued, which
  process fails - and records that the Mojo capture **has not
  been run**.
- `docs/failure-classification.md` (A-006) records the same
  limit (its A6-10): the classification framework "has never
  been exercised against a real Mojo/tcmalloc failure,
  because no such failure packet exists in the repository."
- `docs/INTEGRATION_LAYER_MAP.md` (A-007), Section 1, truth
  (ii): **"No reproducible Mojo/tcmalloc failure exists in
  the repo"**, so the Roadmap Section 3.3 exit gate is
  **Blocked**, with the resumption point named: a real A-005
  packet, plus the binaries it ran.

### 5.2 Record two: the field-notes account - a real failure, in prose, on a tablet

`docs/field-notes/android-arm64-2026-11/03_VIRTUAL_ADDRESS_
AND_MOJO.md` ("Virtual addresses and the Mojo failure"),
corroborated by note 01's evidence ledger, records - scoped
to **one physical Samsung Galaxy Tab S10 Lite (SM-X400),
Android 16, in a Termux app context with a separate glibc
userspace**:

1. The **Linux ARM64 Mojo 1.2.0** package was installed in
   that glibc environment. Its launcher could not directly
   execute the compiler, because the ELF requested
   `/lib/ld-linux-aarch64.so.1`, which was not present at
   that path in that Android environment.
2. Invoking the installed Termux glibc loader directly got
   farther: **before `--version`, the compiler aborted with
   a TCMalloc `MmapAligned()` failure and a diagnostic saying
   it assumed a 48-bit virtual address space.** The
   diagnostic's 1 GiB reservation was a virtual-address
   range request, not physical memory.
3. A tested **1.3.0.dev2026110205 nightly** aborted
   similarly.
4. A fixed-no-replace anonymous `mmap` probe on that device:
   a request at `1 << 38` returned that address; requests at
   `1 << 39` and `1 << 47` returned ENOMEM - consistent with
   a **39-bit user virtual-address boundary** on that
   device, explicitly not a complete kernel audit.
5. In the investigated TCMalloc source, the diagnostic's
   suggestion of `TCMALLOC_ADDRESS_BITS` did not match a
   configurable ARM64 constant in the pinned revision
   (note 03; the pinned-revision header experiment is note
   05, upstream TCMalloc `12f255231938d30493186b0a037feedd70f5a1c1`,
   local experiment commit `3390b10` - header-level checks
   only, gates 1-6 still open).

Classification, in A-006's vocabulary applied to this prose
account only: the shape matches **address-space
configuration** (which A-006 caps at *probable* on a single
packet - and this is not even a packet). No stronger
classification is recorded anywhere, and this plan assigns
none.

### 5.3 The unresolved provenance conflict inside record two

Field note 03 and `docs/SUPPORTED_TARGETS.md` I-3 record:
public Modular source inspected at commit
`99ab7e90258db16bda9261b19bcb59d729ee3429` **selects jemalloc
for the Linux Mojo driver**, yet the shipped 1.2.0 wheel and
the 1.3.0.dev nightly **aborted in TCMalloc**. The package's
relationship to that public commit was not established.
Until it is, "replace the allocator in the Mojo compiler"
does not name a definite thing even for the tablet wheels -
and it names nothing at all for the Pi's 1.1.0, whose runtime
library `libAsyncRTRuntimeGlobals.so` *defines* a
TCMalloc-derived internal surface (A-007 Section 7,
**documented**) while its compiler binary resolves ordinary
allocations dynamically against glibc.

### 5.4 The exact-command verdict, and what F-008 may honestly do

**Verdict: the original failing command was never recorded
exactly. PROVEN as a statement about the banked record**
(A-005's own inconsistency 1, A-006, A-007 all concur; this
slice re-checked the three documents and the field notes).
Specifically absent everywhere: the argv array as issued,
the exact Termux loader invocation string, the working
directory and environment, the exit code / signal, and any
captured stderr log. The field notes give the *behaviour* in
prose (Section 5.2) and name the versions and the device.
That is a faithful failure *account*. It is not a repeatable
validation command, and Roadmap Phase 1's exit ("a documented
causal hypothesis and a repeatable validation command") is
therefore still unmet on this point, alongside the three
unreconciled Phase 1 exit gates A-005 recorded.

**What would count as "the original failing command" for
F-008** - the definition this plan hands down, so F-008 does
not have to invent one:

- **Literal retry (the only reading that satisfies exit
  element X1 as written):** on the affected environment
  class - SM-X400-class Android/Termux glibc userspace, or a
  recorded-equivalent 39-bit-VA ARM environment - invoking
  the affected Mojo compiler (1.2.0, or the
  1.3.0.dev2026110205 nightly) through the Termux glibc
  loader for `--version`, i.e. the invocation whose
  behaviour note 03 describes, with MimirAlloc wired at
  whatever boundary F-003 proves, succeeding where it
  previously aborted in TCMalloc `MmapAligned()`.
  **None of that environment, and neither version, exists on
  this Pi.** A literal retry is therefore **BLOCKED on the
  Pi**, with this paragraph as its resumption point: it
  needs the tablet environment (or a ratified equivalent)
  and Volmarr's word for any work there.
- **Reconstruction (weaker, must be labelled as such):** a
  case file under `scripts/failure_cases/` built by the
  A-005 "Adding a real Mojo case" procedure, stating in its
  notes that the argv is *reconstructed from field note 03's
  prose*, run through the A-005 harness so a real packet
  finally exists. A reconstruction can create the missing
  Phase 1 packet; it cannot retroactively become "the
  original command", and F-008 must not describe it as one.
- **Pi analogue (weakest; explicitly not X1):** running the
  Pi's Mojo 1.1.0 `--version` or a minimal compile. This
  already succeeds *without* any allocator substitution
  (Section 3.2), so its success proves nothing about the
  failure, the allocator, or Phase 6. Recording it as an
  exit proof would be exactly the unrelated-success claim
  the roadmap's hello-world exclusion forbids.

## 6. (d) Integration boundary options

The surface every option must work against, stated once:
`libmimiralloc.so` / the static archive export **exactly
four** namespaced symbols - `mimir_malloc`, `mimir_free`,
`mimir_calloc`, `mimir_realloc` (`include/mimir_alloc.h`;
Phase 5 exit packet E-009, `nm -D` exactly 4). **No
standard-name symbol exists** - no `malloc`, `free`,
`calloc`, or `realloc` is provided by MimirAlloc anywhere in
the banked work, and Phase 5's interposition route R4
(`LD_PRELOAD` of standard names) was **rejected by E-001 with
reason recorded**, not merely left unbuilt. Clean-room rule
for this whole section: Mojo *documentation* and the
installed package's *observable behaviour* (help text, ELF
inspection) are evidence; Mojo source is not copied, and no
option below is described from Mojo source - none is
available on the Pi in any case (Section 3.2, stdlib row).

Ratings: **PROVEN-available** (mechanism demonstrated on the
Pi) / **documented-only** (a banked source or the tool's own
help describes it; undemonstrated for this purpose) /
**absent** (no banked source describes it and the artifact
does not offer it) / **requires-new-surface** (possible only
if a future slice adds symbols or code MimirAlloc does not
have, under its own contract decision).

| # | Boundary | What it would reach | Rating | Basis |
|---|---|---|---|---|
| B1 | **Explicit adoption from Mojo source**: a Mojo program calls the four `mimir_*` functions directly, as an external C call, for allocations it makes itself | Only the allocations the Mojo program itself explicitly requests. **Not** the compiler's allocations, **not** the Mojo stdlib's internal allocations | **documented-only, mechanism incomplete**: no banked source describes a Mojo external/FFI call path to a user C library, and the installed stdlib is compiled-only, so the Mojo-side call mechanism cannot be confirmed from the Pi artifact. The C side is proven (Phase 5 consumers call exactly these symbols). F-003/F-004 must demonstrate the Mojo-side call or record it blocked; until then B1 is a design, not a boundary |
| B2 | **Documented build configuration for generated programs**: `mojo build -Xlinker <ARG>` (and `--lld-path`) to place `libmimiralloc` on a generated executable's link line / rpath | The generated executable's link, in the same R2 shape Phase 5 proved for C consumers (DT_NEEDED + rpath). By itself it makes the library *present*; combined with B1 it makes explicit calls *resolvable* | **PROVEN-available as a flag; unproven as servicing.** `-Xlinker` appears in `mojo build --help` on the Pi (**PROVEN on the Pi**, Section 3.2). No generated Mojo executable linked this way exists yet; presence on a link line is not proof any allocation is serviced (A-007's discipline: a name match is a lead, never a finding). This is the narrow, honest reading of the roadmap's "using documented build configuration" for Phase 6 |
| B3 | **`LD_PRELOAD` interposition of standard names** against the Mojo compiler, its runtime libraries, or a generated executable | In principle, every dynamic standard-name allocation in the preloaded process | **absent with the current surface; requires-new-surface.** The banked `.so` exports no standard names, so preloading it binds nothing - the Phase 5 R4 rejection (E-001) already decided this route needs a standard-name shim that does not exist. Creating one is a new contract/ABI decision (a future slice, with its own exit scope), not a Wave 6 wiring step. A-007 Section 10 row M1 additionally lists the standing limits: statically linked consumers, secure execution, hidden/`-Bsymbolic` symbols, internal allocators, and Android app processes are each unaffected by preload |
| B4 | **Link-time wrapping** of a generated executable (`-Xlinker --wrap=malloc` and kin, forwarding to a shim that calls `mimir_*`) | Standard-name calls originating in the generated executable's own objects | **requires-new-surface.** The linker mechanism is generic and could be passed through B2's flag, but the `__wrap_*` shim translation unit is new code providing standard-name entry points in effect - the same new-surface decision as B3, reached by another road - and it reaches neither the compiler nor prelinked runtime libraries. No banked source describes or authorises it; Wave 6 does not adopt it by silence |
| B5 | **Documented Mojo allocator hooks / custom-allocator support in the Mojo stdlib** (the option the roadmap would most prefer, if it existed) | Potentially the generated program's ordinary allocations, through a supported configuration | **absent in the banked sources.** A search of `docs/` and `docs/research/` in this slice found **no** description of any Mojo allocator hook, pluggable allocator, or custom-allocator configuration, and no allocator option appears in `mojo build --help` / `mojo run --help` as printed on the Pi. The installed stdlib's compiled-only form prevents settling the question from the artifact. Rating stays **absent** - not "does not exist in Mojo", but "not evidenced anywhere this project may honestly cite". If F-002 finds genuine Modular documentation of such a hook, this row is revised by supersession with the citation attached |
| B6 | **Substituting the allocator inside the Mojo compiler process itself** (the process that actually aborted on the tablet) | The compiler's own startup allocations, including its bundled TCMalloc-derived runtime surface | **absent / BLOCKED.** The compiler is a prebuilt, stripped binary; A-007 Sections 6-7 record that its runtime library *defines* `TCMallocInternal*` / `TcmallocSlab_*` symbols internally - an internal allocator is precisely the case preload and DT_NEEDED routes cannot reach (A-007 Section 10, rows M1/M2 cannot-effect conditions). Rebuilding the compiler from source is **BLOCKED** on the Pi: no Bazel binary and no Mojo source checkout exist there (A-007 L1/L2, **documented**). And per Section 5.3, which allocator a given Mojo artifact even uses is provenance-unresolved |
| B7 | **DT_NEEDED / rpath replacement of a runtime library** (A-007 row M2 applied to the L3 libraries) | The L3 libraries' dynamic glibc calls (`libKGENCompilerRTShared.so`, `libMSupportGlobals.so` carry UND `malloc`/`calloc`/`free`/`realloc`) | **absent with the current surface; requires-new-surface.** Those UND names can only bind to a provider of the *standard* names. `libmimiralloc.so` provides none of them, so there is nothing to substitute. Same new-surface decision as B3 |

**Headline for (d):** no option is PROVEN-available today
for making Mojo's own allocations reach MimirAlloc. The only
PROVEN-present mechanism is B2's linker flag, and the only
honest near-term proof target is **B2 + B1**: a generated
Mojo program whose *own explicit* allocations are serviced
by the four namespaced symbols, proven the F-005 way. That
target, if F-003/F-004 reach it, services a generated
program's explicit calls only. It does not touch the
compiler, the stdlib's internal allocations, or the tablet
failure, and no Wave 6 slice may describe it as doing so.

## 7. (e) Exit-evidence matrix for Phase 6

Roadmap Phase 6 tasks, decomposed from its Section 8
paragraph: **T1** wire the demonstrated build or runtime
boundary using documented build configuration. **T2** rebuild
from clean sources, or use the appropriate published package
only where legally and technically possible. **T3** run a
minimal Mojo program. **T4** compile a representative program
repeatedly. **T5** execute relevant upstream tests. **T6**
compare against the same revision without the replacement.
**T7** capture proof MimirAlloc actually serviced
allocations. Exit elements: **X1** the original failing
command succeeds on the affected ARM environment; **X2**
explicit proof of allocator use; **X3** no new correctness
failures; **X4** OR a blocker is recorded with exact results;
**X5** the exclusion: an unrelated Mojo hello-world does not
close the milestone. Slice shapes are the WAVE_PLAN.md Wave 6
queue (Section: "WAVE 6 QUEUE DERIVED 21:0x EST"), quoted in
substance in each row.

| Item | Proving / blocking slice | What that slice must record | State at this base |
|---|---|---|---|
| T1, B2+B1 boundary | **F-003** (boundary wiring build - the demonstrated boundary as a buildable harness per F-001/F-002 evidence, or the boundary design with static proof if the toolchain is absent) | The exact `mojo build` command line including every `-Xlinker` argument; the generated executable's `readelf -d` / `nm -D` record; or, if the Mojo-side call mechanism (B1) cannot be demonstrated, the boundary design with static proof and the exact point of failure. Note: the toolchain is **not** absent (Section 3), so F-003's primary branch applies; its fallback wording is INC-F001-07 | **OWED** - no generated Mojo executable exists in the repo at all (A-007 L4, **documented**) |
| T2, toolchain legality/readiness | **F-002** (Mojo toolchain readiness - exact acquisition road, sizes and licence posture; install NOTHING, installs reserved for Volmarr's word; absent components recorded BLOCKED with smallest next step) | Whether the installed 1.1.0 (Section 3) is the package Phase 6 uses, or a different version is required - and if a different version (e.g. the failing 1.2.0) is required, its acquisition road, size, licence posture from the package record, and the fact that installing it waits on Volmarr's word. MAX and the `modular` CLI are absent components F-002 must record BLOCKED-or-not-needed explicitly, never silently | **OWED** - inventory done here (Section 3); readiness decision not taken |
| T3, minimal program | **F-004** (minimal Mojo program run with and without MimirAlloc, recorded, or blocker record) | The program source, both run commands, both outputs and exit codes. The "with" leg is only meaningful through the F-003 boundary; a minimal program run *without* any boundary is the baseline leg, not a result | **OWED** |
| T7 + X2, allocator-use proof | **F-005** (allocator-use proof on the Mojo path - audit counters / binding evidence adapted from E-005/E-004) | Proof of the E-004/E-005 strength, adapted honestly: E-004's loader binding proof (`DT_NEEDED` + exactly-one-binding-line-per-symbol under `LD_DEBUG=bindings`) transfers to a generated executable; E-005's internal counters do **not** transfer directly - they are local symbols in the `.so`, unreadable by any consumer (E-005 Section 0), so F-005 must use E-005's *dynamic leg* shape (implementation-only effects: descriptor validation and mapped-bytes movement) or an equivalent. A counter a consumer wraps around its own call sites proves call sites, not servicing (E-005 Section 1.1) | **OWED** - proof *method* exists and is banked (E-004/E-005); no Mojo-path proof exists |
| T4 + T6, repeated compile, same-revision comparison | **F-006** (representative program repeated-compile comparison, same revision with and without the replacement) | The representative program named and justified (INC-F001-11 governs any use of the out-of-repo `/home/pi/c2` sources); the same Mojo revision (the Pi's 1.1.0, unless F-002 changes it) on both legs; repetition count, commands, outcomes. A comparison across different Mojo versions is invalid by construction (INC-F001-03) | **OWED** |
| T5, upstream tests | **F-007** (relevant upstream test execution record) | The suite named *before* running: which upstream, which revision, obtained how. No Mojo source checkout or upstream test suite exists on the Pi (A-007 L1/L2), and the roadmap never names the suite (INC-F001-08). The honest expected outcome, absent a ratified suite, is a **BLOCKED** record naming exactly that - not a substitute suite invented by the slice | **OWED, expected BLOCKED** unless F-002/F-007 first ratify a suite |
| X1, original command | **F-008** (original Phase 1 failing command retried on the Pi ARM environment, exact results or blocker) | Under Section 5.4's definition: a literal retry is **BLOCKED on the Pi** (affected environment and versions absent). F-008's deliverable is therefore the blocker record with exact results - the Pi analogue run, its exact commands and outputs, and the statement that success there is not X1 - **or** a labelled reconstruction packet via the A-005 procedure. Either is honest; describing a Pi 1.1.0 success as X1 met is not | **BLOCKED** (INC-F001-01, -02, -03) - blocker definition already recorded here so F-008 executes it, not re-derives it |
| X3, no new correctness failures | **F-006 + F-007 + F-009** (comparison legs and the packet's synthesis) | The with/without outcome diff from F-006 and whatever test record F-007 honestly holds. X3 cannot be stronger than T5's record: if F-007 is BLOCKED, X3 is at most "no new failures observed in the F-004/F-006 runs", stated in exactly those words | **OWED** |
| X4, blocker-with-exact-results alternative | **F-009** (Phase 6 exit evidence packet), assembling F-008's blocker and every other slice's exact results | A packet of the E-009 kind: every exit element marked MET / UNMET / PARTIAL / OWED / BLOCKED with its recorded evidence cited, and the scope paragraph stated before the verdict | **OWED** |
| X5, hello-world exclusion | **All Wave 6 slices; enforced by F-009** | Any minimal-program success (F-004) is recorded as a T3 baseline/boundary data point only. F-009 must refuse to count it toward X1. This plan records the refusal in advance so the packet does not have to invent the discipline under deadline | **Guard recorded here** |

**Reading of the matrix, plainly:** on the evidence at this
base, Wave 6 can genuinely prove T1-T4 and T6/T7 in the
B2+B1 scope (a generated program's explicit allocations),
can record T5 only as a ratification-or-blocker exercise,
and can close X1 only through the X4 door - a blocker
recorded with exact results - unless the tablet environment
is brought into scope by Volmarr's word. That is a
roadmap-conformant outcome: the exit sentence itself
provides the blocker alternative. What Wave 6 may not do is
present the Pi-scope proofs as the tablet failure fixed.

## 8. (f) Inconsistencies recorded in this slice - INC-F001-NN

None of these is silently resolved by this plan. Each names
its resumption point or its owning slice.

- **INC-F001-01 - The original failing command was never
  recorded exactly.** No argv, exit code, or failure log
  exists in the banked record (A-005 inconsistency 1;
  Section 5.4). This blocks the literal reading of exit X1
  and of Roadmap Phase 1's "repeatable validation command".
  Owner: F-008, by blocker record or labelled reconstruction
  - never by invention.
- **INC-F001-02 - Environment mismatch between the exit text
  and the Wave 6 venue.** The exit says "the affected ARM
  environment"; the affected environment on record is the
  SM-X400's Android/Termux glibc userspace (39-bit user VA,
  4 KiB-class pages per the field record), while Wave 6
  executes on native Linux/glibc on the Pi (16,384-byte
  pages, `mojo --version` already succeeding). No banked
  document reconciles the two. Owner: F-009's scope
  paragraph; a venue change needs Volmarr's word.
- **INC-F001-03 - Version mismatch.** The failing versions
  are Mojo 1.2.0 and 1.3.0.dev2026110205 (tablet); the only
  Mojo on the Pi is 1.1.0. Roadmap T6 demands comparison
  "against the same revision"; any Wave 6 comparison is
  same-revision only *within* the Pi's 1.1.0 and says nothing
  about the failing revisions. Owner: F-002 (readiness
  decision), F-006 (comparison discipline).
- **INC-F001-04 - MAX is absent while Mojo is present.** The
  environment is a mojo-only conda install; Mojo itself
  reports `libmax.so` missing. Any Phase 6 wording that
  assumes a MAX installation on this Pi (the channels URL
  even contains `max`) is wrong. Owner: F-002, to record
  BLOCKED-or-not-needed explicitly.
- **INC-F001-05 - Toolchain launch paths are not PATH
  paths.** `pixi` and `modular` do not resolve on PATH;
  `mojo` resolves only to a wrapper that invokes pixi by
  absolute path against `/home/pi/mojo-dev/pixi.toml`;
  `~/.modular` does not exist. A Wave 6 slice that writes
  `pixi run ...` or assumes a Modular user install will fail
  for a mundane reason and must not misread that failure as
  a Mojo result. Owner: F-002 (record the actual launch
  path), all slices (use it).
- **INC-F001-06 - No Mojo allocator hook is evidenced, and
  the Pi cannot settle the question.** Banked sources
  describe no hook (Section 6, B5); the installed stdlib is
  compiled-only (`std.mojoc`, no sources), so absence of
  evidence here is not evidence of absence in Mojo itself.
  The honest rating is **absent in the banked sources**.
  Owner: F-002, to check genuine Modular documentation or
  record the absence as the standing result.
- **INC-F001-07 - F-003's queue fallback does not match
  reality.** The WAVE_PLAN queue phrases F-003's alternative
  deliverable as arising "if the toolchain is absent". The
  toolchain is present; what is absent is a *proven
  boundary*. This plan reads F-003's primary branch (a
  buildable harness) as governing, with the design-with-
  static-proof deliverable available if the *boundary* -
  not the toolchain - proves undemonstrable. The reading is
  recorded here so the keeper can correct it if wrong; it is
  not a silent reinterpretation.
- **INC-F001-08 - "Relevant upstream tests" is undefined.**
  Roadmap Phase 6 names no suite, revision, or location for
  T5; no Mojo source checkout or test suite exists on the
  Pi. F-007 cannot invent a suite and call it upstream.
  Owner: F-007, ratify-then-run or record BLOCKED.
- **INC-F001-09 - Phase 6 inherits Phase 1's unmet exit.**
  A-005 recorded three unreconciled Phase 1 exit gates
  (Roadmap Sections 3.3, 8 Phase 1, and 17) and no failure
  packet; Phase 6's X1 presupposes exactly the artifact
  Phase 1 never produced. Wave 6 proceeding does not cure,
  discharge, or soften Phase 1's position, and F-009 must
  restate it, as E-009 restated Phases 3 and 4.
- **INC-F001-10 - TCMalloc provenance is unresolved, so
  "the replacement" is undefined for the compiler.** Public
  source selects jemalloc; shipped wheels abort in TCMalloc;
  the Pi's 1.1.0 runtime library defines a TCMalloc-derived
  internal surface (Sections 5.3, 6 B6). Until a per-binary
  provenance exists for an artifact Phase 6 actually runs,
  no slice may claim to have replaced "Mojo's allocator" in
  the compiler. Owner: F-002/F-003 evidence, per exact
  binary, or an explicit not-applicable record.
- **INC-F001-11 - Representative Mojo programs exist on the
  Pi only outside the repository.** `/home/pi/c2/` holds
  Aesir Mojo sources with no MimirAlloc provenance or
  clean-room record. F-006 choosing one as its
  "representative program" is a recorded decision the slice
  must make and justify - or it writes its own in-repo
  representative program - never a silent adoption.
- **INC-F001-12 - The published package's licence posture
  is recorded but not evaluated.** The conda record states
  `LicenseRef-Modular-Proprietary` (Section 3.2). Roadmap T2
  permits using the published package "only where legally
  and technically possible"; whether that condition is met
  for Phase 6's intended uses is an evaluation this plan
  does not perform. Owner: F-002.

## 9. What this plan does not do

- It builds no boundary, runs no Mojo program, and links
  nothing. The `mojo` invocations in Section 2/3 are
  inventory probes (`--version`, `--help`, target listing)
  only.
- It installs nothing, downloads nothing, and fetches no
  external documentation. F-002's acquisition road is a
  record, and any install waits on Volmarr's word.
- It adds no symbol, no shim, and no standard-name surface.
  B3/B4/B7's new-surface decisions are named as future
  decisions, not taken here.
- It does not touch `README.md`, the Makefile, any source,
  header, or test. Its only deliverable is this file.
- It claims no Phase 6 exit element, no Android result, and
  no fix for the tablet failure. Carried debts stand as
  stated in the headnote: Phase 4 exit UNMET, Phase 3 exit
  UNMET, Phase 1 exit unmet, each with its banked register.
