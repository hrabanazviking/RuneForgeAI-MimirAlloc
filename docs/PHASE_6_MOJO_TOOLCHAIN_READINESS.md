# Phase 6 — Mojo Toolchain Readiness Record

Slice: **F-002 (team-02)**. Wave 6, Phase 6 Mojo toolchain
readiness — the acquisition road, the current-state audit,
and the feasibility verdict that the wiring slices
(F-003 onward) build on. This slice **installs nothing,
downloads nothing, and changes no toolchain state**: it
records the road, it does not travel it. Installs are
reserved for Volmarr's word (assignment, keeper brief).
Deliverable: `docs/PHASE_6_MOJO_TOOLCHAIN_READINESS.md`
(this file only). No code, no Makefile line, no test file
is added or changed; `make test` composition is unchanged.
Base: **211eb31c9c1332e17298d8d2ac498fb49415b39f**
(banked main: Wave 5 COMPLETE 9 of 9, E-001..E-009;
Phase 5 exit MET in Pi-native scope) on branch
`yrsa/mimiralloc-02`, worktree
`/home/pi/mimiralloc-team-02` on Raspberry Pi
`purpleviking1`. Host for every check in Section 2:
Raspberry Pi 5 Model B Rev 1.1 (`/proc/cpuinfo` Model
line, this slice), ARM64 Linux + glibc —
`Linux purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP
PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64 GNU/Linux`,
Debian GNU/Linux 12 (bookworm), glibc 2.36
(`Debian GLIBC 2.36-9+rpt2+deb12u14`), runtime page size
**16384** (`getconf PAGE_SIZE`), 4 cores — the same Pi
manifest the banked D-series/A-003 evidence recorded
(`docs/SUPPORTED_TARGETS.md` Section 5,
`docs/BASELINE_INVENTORY.md` Section 5), re-verified
unchanged by this slice. Governing text: Roadmap
Section 8, Phase 6 ("Actual Mojo integration"), read
with Roadmap Sections 2 (evidence levels), 3, 5 and 16
(source S9), the banked integration inventory
`docs/INTEGRATION_LAYER_MAP.md` (A-007), the banked
system-allocator evaluation
`docs/SYSTEM_ALLOCATOR_EVALUATION.md`, and the Android
field notes `docs/field-notes/android-arm64-2026-11/`
(especially Field 01, 03, 04, 14). `README.md` was not
touched by this slice (verified: blob
`d7949561e229f495c9aef37d75432f8c3a0f6e10` at base and
at commit; see Section 7). Work mode: solo-sequential,
no sub-agents (prior team-02 pattern); Pi load kept
light — every check below is an inventory read or a
sub-10-second toolchain invocation; Samtal answered
HTTP 200 on port 8787 before and after the work.

> **PRESENCE VERDICT — Mojo is PRESENT on this Pi.**
> A prebuilt Mojo **1.1.0 (8189361e)** for
> `linux-aarch64` is installed under a pixi workspace
> (`/home/pi/mojo-dev`), pixi **0.81.0** is installed
> (`/home/pi/.pixi/bin/pixi`), and `/usr/local/bin/mojo`
> is a 99-byte wrapper that runs the workspace
> compiler. The `modular` CLI is **absent**, there is
> **no `~/.modular` directory**, and there is **no
> `max` executable** — MAX exists only as the conda
> channel name and the env's `share/max` platform
> data (Section 2.3). Absent means absent, stated
> plainly; present means present, proven by running
> it (Section 2.4).
>
> **FEASIBILITY VERDICT — an ARM64 Mojo run on this
> machine is PROVEN possible.** In this slice, on
> this Pi, the installed compiler answered
> `mojo --version` (exit 0), executed a minimal
> program via `mojo run` (exit 0, expected output),
> and produced a native AArch64 executable via
> `mojo build` that itself ran with exit 0
> (Section 2.4). Phase 6's runtime half is therefore
> **NOT BLOCKED on this machine**. What remains
> blocked/owed is the Phase 6 *exit* half — proof
> that MimirAlloc serviced the Mojo path and that
> the original failing command succeeds — because
> no MimirAlloc wiring to any Mojo boundary exists
> (the banked surface is four namespaced symbols
> only) and the original failure was observed on a
> different device (the SM-X400 tablet), never on
> this Pi (Sections 4 and 5). No install decision
> from Volmarr is needed for F-003/F-004 to proceed
> on the installed 1.1.0; the only install-shaped
> decision in this record is the optional version
> upgrade in Section 3.4, and it stays NOT RUN.

## 1. Claim labels and how to read this record

Every load-bearing claim below carries one label:

- **PROVEN on the Pi** — run or read by this team in
  this slice, on `purpleviking1`, with the command
  and output recorded in Section 2 (or the run in
  Section 2.4).
- **documented** — stated in a banked document of
  this repository (roadmap, research library, field
  notes, prior slices), cited at the claim. Not
  re-run by this slice.
- **OWED** — required by the roadmap or by a later
  Wave 6 slice, not yet produced by anyone.
- **BLOCKED** — cannot proceed on current evidence
  or current state; the blocker is named with the
  claim.

Facts observed on the Pi *outside* this repository's
banked record (the operator's own `~/mojo-build`
source checkout, Section 2.5) are labelled PROVEN on
the Pi as filesystem observations, and are expressly
**not** banked evidence: no later slice may cite
them as repo evidence until a slice banks them.

## 2. Current state on the Pi (assignment item a)

All commands run 2026-11-06 (EST evening; Pi clock
`Fri Nov 6 09:08:54 PM EST 2026` / `Sat Nov 7
02:08:54 AM UTC 2026`) by this team in this slice,
in `/home/pi/mimiralloc-team-02` unless noted.
Outputs are verbatim, trimmed only where marked.

### 2.1 Host, memory, disk — PROVEN on the Pi

```
$ uname -a; uname -m; cat /etc/os-release (head)
Linux purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP PREEMPT
  Sat Oct 10 09:05:25 EDT 2026 aarch64 GNU/Linux
aarch64
PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"
$ ldd --version | head -1
ldd (Debian GLIBC 2.36-9+rpt2+deb12u14) 2.36
$ getconf PAGE_SIZE; getconf LONG_BIT; nproc
16384
64
4
$ grep -m1 Model /proc/cpuinfo   (last block)
Model           : Raspberry Pi 5 Model B Rev 1.1
$ free -g
               total        used        free      shared  buff/cache   available
Mem:              15           2           0           0          13          13
Swap:             15           1          14
$ df -h /
/dev/mmcblk0p2  469G  262G  199G  57% /
$ curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8787/
200                        (Samtal village, undisturbed)
```

### 2.2 PATH and package-manager presence — PROVEN on the Pi

Default PATH in the device shell is
`/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`
(no `~/.pixi/bin`, no conda env on it). Exact probe:

```
$ for c in mojo modular pixi max magic conda micromamba uv python3 pip3; do
    printf '%s: ' "$c"; command -v "$c" || echo NOT_FOUND; done
mojo: /usr/local/bin/mojo
modular: NOT_FOUND
pixi: NOT_FOUND
max: NOT_FOUND
magic: NOT_FOUND
conda: NOT_FOUND
micromamba: NOT_FOUND
uv: NOT_FOUND
python3: /usr/bin/python3
pip3: /usr/bin/pip3
$ dpkg -l | grep -iE 'mojo|modular|max'   (filtered)
  (no Mojo, Modular, or MAX package; hits are only the
   English word "modular" inside initramfs-tools /
   wireplumber descriptions and "Modular compiler..."
   inside LLVM package descriptions, plus libmaxminddb)
$ pip3 list | grep -iE 'mojo|modular|max'
  (no output — no pip Mojo/Modular/MAX package)
$ for c in bazel bazelisk git; do command -v "$c"; done
bazel: NOT_FOUND
bazelisk: NOT_FOUND
git: /usr/bin/git
```

Reading: the toolchain is **not** a system package
install (no dpkg/pip trace). It is a per-user pixi
install that the default PATH cannot see directly;
only the root-owned wrapper makes `mojo` resolve
(Section 2.3). `pixi: NOT_FOUND` above means "not
on PATH", **not** "not installed" — see the absolute
-path probe in Section 2.3. Any readiness claim that
stops at `command -v` would be wrong in both
directions on this machine (INC-F002-07).

### 2.3 The installed toolchain, piece by piece — PROVEN on the Pi

**The wrapper.**

```
$ ls -l /usr/local/bin/mojo; cat /usr/local/bin/mojo; file /usr/local/bin/mojo
-rwxr-xr-x 1 root root 99 Oct 10 02:33 /usr/local/bin/mojo
#!/bin/bash
exec /home/pi/.pixi/bin/pixi run --manifest-path /home/pi/mojo-dev/pixi.toml mojo "$@"
/usr/local/bin/mojo: Bourne-Again shell script, ASCII text executable
```

**pixi itself** (installed, off-PATH):

```
$ ls -l /home/pi/.pixi/bin/pixi
-rwxr-xr-x 1 pi pi 68698776 Sep 15 10:38 /home/pi/.pixi/bin/pixi
$ /home/pi/.pixi/bin/pixi --version
pixi 0.81.0
$ ls /home/pi/.pixi/bin/
gpu-query  lldb-argdumper  lldb-server  llvm-symbolizer  mojo
mojo-lldb  _mojo-lldb-dap  mojo-lldb-dap  mojo-lsp-server  pixi
trampoline_configuration/
$ file /home/pi/.pixi/bin/mojo
/home/pi/.pixi/bin/mojo: ELF 64-bit LSB executable, ARM aarch64,
  version 1 (SYSV), statically linked, stripped
$ ls -l /home/pi/.pixi/bin/mojo
-rwxr-xr-x 10 pi pi 671888 Oct 4 00:04 /home/pi/.pixi/bin/mojo
```

The `~/.pixi/bin/mojo` file is the small statically
linked pixi *shim* executable the banked inventory
already recorded (A-007, `docs/INTEGRATION_LAYER_MAP.md`
Section 6): it is not the compiler. The compiler is
the env binary below.

**The workspace manifest** — the single most
load-bearing acquisition artifact on this machine:

```
$ cat /home/pi/mojo-dev/pixi.toml
[workspace]
authors = ["Volmarr Wyrd <122174898+hrabanazviking@users.noreply.github.com>"]
channels = ["https://conda.modular.com/max", "conda-forge"]
name = "mojo-dev"
platforms = ["linux-aarch64"]
version = "0.1.0"
[tasks]
[dependencies]
mojo = ">=1.1.0,<2"
$ /home/pi/.pixi/bin/pixi info --manifest-path /home/pi/mojo-dev/pixi.toml
    Pixi version: 0.81.0
        Platform: linux-aarch64
  Virtual packages: __glibc=2.36 ... __archspec=1=neoverse_n1
  (archspec string is pixi/rattler's detection label,
   recorded as reported, not as a CPU claim)
  Channels: https://conda.modular.com/max, conda-forge
  Dependency count: 1   Dependencies: mojo
  Resolved platform: linux-aarch64 (glibc=2.28 minimum)
  Prefix location: /home/pi/mojo-dev/.pixi/envs/default
```

**The compiler binary.**

```
$ ls -l /home/pi/mojo-dev/.pixi/envs/default/bin/mojo
-rwxr-xr-x 3 pi pi 138588560 Sep 10 19:43 .../bin/mojo
$ file /home/pi/mojo-dev/.pixi/envs/default/bin/mojo
ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV),
dynamically linked, interpreter /lib/ld-linux-aarch64.so.1,
for GNU/Linux 3.7.0, BuildID[md5/uuid]=7e410907739ac44de3cd765083c447bc,
stripped
$ readelf -d .../bin/mojo  (NEEDED entries)
libm.so.6, libstdc++.so.6, libMSupportGlobals.so,
libAsyncRTRuntimeGlobals.so, libgcc_s.so.1, libc.so.6,
ld-linux-aarch64.so.1     (RPATH $ORIGIN/../lib ...)
$ ldd .../bin/mojo
  (all NEEDED resolve: system libc/libm from
   /lib/aarch64-linux-gnu, the rest from the env lib/)
$ ls .../.pixi/envs/default/lib/  (Mojo-relevant)
libAsyncRTRuntimeGlobals.so  libMojoJupyter.so  libMojoLLDB.so
libKGENCompilerRTShared.so   libMSupportGlobals.so  mojo/  (stdlib dir;
  std.mojoc present)  mojo-repl-entry-point
```

This matches the banked A-007 inventory of the same
two binaries (dynamic env compiler + static pixi
shim) and its L3 finding that
`libAsyncRTRuntimeGlobals.so` carries a defined
TCMalloc-derived symbol surface in-library
(**documented**, A-007 Section 7; not re-derived by
this slice). The interpreter the tablet lacked —
`/lib/ld-linux-aarch64.so.1` (Field 03) — exists
natively on this Pi, which is one concrete reason
the tablet failure does not transfer (Section 4).

**Modular home, two different things.**

```
$ ls -ld ~/.modular
ls: cannot access '/home/pi/.modular': No such file or directory
$ ls -la ~/.local/share/modular/
drwxr-xr-x 2 pi pi 4096 Oct 10 02:31 .   (empty directory)
$ cat .../envs/default/etc/conda/activate.d/10-activate-max.sh (key line)
export MODULAR_HOME="${CONDA_PREFIX}/share/max"
$ cat .../envs/default/share/max/modular.cfg  (head)
[max]
package_root = /home/pi/mojo-dev/.pixi/envs/default
name = MAX Platform
version = 1.1.0
[mojo-max]
driver_path = .../envs/default/bin/mojo
import_path = .../envs/default/lib/mojo
...
```

So: the classic Modular install home (`~/.modular`)
is **absent**; an empty `~/.local/share/modular`
directory exists; and the *effective* Modular home
of the installed toolchain is the conda env's
`share/max`, set only inside env activation (or
implicitly via the wrapper's `pixi run`). A check
that looks only for `~/.modular` would report this
machine Mojo-less. It is not (INC-F002-05).

**MAX.** No `max` executable exists anywhere in the
env `bin/` listing (full listing read by this slice:
mojo toolchain, lldb tools, python, jupyter, openssl
— no `max`). MAX presence on this Pi is exactly:
the channel name `https://conda.modular.com/max`,
the `share/max` platform data + `modular.cfg`
("MAX Platform", version 1.1.0), and an activation
script. **No MAX CLI, serving, or runtime use is
proven** (INC-F002-06).

**Account / auth posture, as observed.** No
credentials file exists at the path pixi reported
for auth storage, and no Modular account artifact
exists:

```
$ ls -l /home/pi/.rattler/credentials.json
ls: cannot access ...: No such file or directory
  (pixi info names this path as "Auth storage")
$ ls /home/pi/.cache/rattler/cache/pkgs/ | head
bzip2-...  ca-certificates-...  ...  mojo/mojo-compiler
  cache entries present (packages were fetched and
  cached without a credentials file on disk today)
```

Labelled carefully: it is **PROVEN on the Pi** that
no credentials file is present today and that the
channel packages are already fetched and cached
locally. Whether Modular's channel terms require an
account for a *fresh* fetch is **not documented in
the banked library** and is recorded as a gap, not
inferred from the cache (Section 3.3, INC-F002-04).

### 2.4 The toolchain actually runs — PROVEN on the Pi

The smallest honest runtime proof, run by this
slice (kept deliberately tiny; the representative
-program and repeated-compile work belongs to
F-004/F-006, and per the roadmap an unrelated
hello-world does **not** close Phase 6 — it only
settles the readiness question this slice owns):

```
$ mojo --version            (via /usr/local/bin/mojo wrapper)
Mojo 1.1.0 (8189361e)       exit 0
$ /home/pi/mojo-dev/.pixi/envs/default/bin/mojo --version
Mojo 1.1.0 (8189361e)       exit 0
$ cat /tmp/f002/hello.mojo
def main():
    print("f002-smoke")
$ timeout 60 mojo run /tmp/f002/hello.mojo
f002-smoke                  exit 0
$ timeout 90 mojo build /tmp/f002/hello.mojo -o /tmp/f002/hello
                            exit 0
$ file /tmp/f002/hello
ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked,
interpreter /lib/ld-linux-aarch64.so.1, not stripped
$ /tmp/f002/hello
f002-smoke                  exit 0
$ ldd /tmp/f002/hello       (key lines)
libKGENCompilerRTShared.so => (env lib/)
libMSupportGlobals.so      => (env lib/)
libAsyncRTRuntimeGlobals.so => (env lib/)
libc.so.6, libm.so.6       => /lib/aarch64-linux-gnu/
```

Two honest footnotes. (1) A first attempt written
in the older `fn main():` form failed to parse —
`error: 'fn' has been removed; use 'def' instead` —
language drift between the field-notes era and
1.1.0-era expectations is real and is recorded so
later slices write 1.1.0-true sources (INC-F002-02
covers the version skew this belongs to). (2) The
generated executable links the env runtime globals
libraries, including the one carrying the
TCMalloc-derived surface (A-007) — that linkage is
exactly the boundary territory F-001/F-003 map;
this slice proves the path *executes*, not that
any allocator substitution on it works.

### 2.5 Also on this Pi, outside the repo: a Mojo source checkout — PROVEN on the Pi (filesystem), not banked evidence

```
$ ls /home/pi/mojo-build/
build.log  clone.log  mojo-src/  run-build.sh
$ cd /home/pi/mojo-build/mojo-src && git rev-parse HEAD; git log --oneline -1
24f4ceb2ff1701aaca81dc974b7b4794d7d2a0a5
24f4ceb [Release] Pin lockfiles to Mojo 1.2.0.dev2026100406,
        MAX 26.7.0.dev2026100406
$ cat /home/pi/mojo-build/run-build.sh  (essence)
cd ~/mojo-build/mojo-src
exec ./bazelw build --config=build-mojo //Mojo/tools/mojo:mojo >> build.log
$ tail /home/pi/mojo-build/build.log  (essence)
Mojo/lib/Compiler/KGENCompiler.cpp: 14 errors generated
  (e.g. :826 'tm' does not refer to a value, struct tm note)
Target //Mojo/tools/mojo:mojo failed to build
INFO: Elapsed time: 13776.237s, Critical Path: 561.95s
ERROR: Build did NOT complete successfully
$ du -sh /home/pi/mojo-build /home/pi/.cache/bazel
352M  /home/pi/mojo-build
11G   /home/pi/.cache/bazel
```

Reading: a full Modular source checkout (the
Roadmap S9 repository, **documented** as the source
road) exists on this Pi at a 1.2.0-dev pin, and a
Bazel driver build was attempted and **failed** —
the source-build route is **BLOCKED as observed**
(Section 4). The banked statement "there is no Mojo
source or Bazel on Mimir"
(`docs/SYSTEM_ALLOCATOR_EVALUATION.md` Section 5.3)
is stale against today's filesystem (INC-F002-01).
`bazel`/`bazelisk` are still absent from PATH; the
build used the tree's `bazelw` wrapper. This
checkout is the operator's local state: no repo
slice has inventoried or banked it, and this record
does not change that.

## 3. The acquisition road (assignment item b)

Sources first, per the assignment: everything in
Sections 3.1–3.3 comes from the banked research
library, the roadmap, or artifacts the banked
record points at. Where the library is silent, this
record says so instead of inventing a road.

### 3.1 What the banked library actually documents

- **Roadmap S9 (documented):** the Modular
  repository `https://github.com/modular/modular` —
  "obtain and pin the actual Mojo source/build
  documentation for the version under test." The
  roadmap names no installer, no channel, and no
  download size for Mojo anywhere (**documented**
  absence, checked by grep over the roadmap and the
  research library by this slice).
- **Field 03/01/14 (documented):** a "Linux ARM64
  Mojo package" exists and was obtained on the
  tablet: stable wheel **1.2.0**, published SHA-256
  `cb54942fd99be346ef273cc9beae5f692c58195dcbe3798596b2e7368d4f719c`
  (fetched copy matched the publisher digest),
  plus nightly **1.3.0.dev2026110205**. Both
  *installed* in a Termux glibc Python environment
  and then aborted at compiler startup (the VA-width
  failure, Section 4). The field notes name the
  Mojo system-requirements and compilation-targets
  pages (`mojolang.org/docs/requirements/`,
  `mojolang.org/docs/tools/compilation/`) as primary
  reading and record that Modular documents Linux
  glibc requirements and describes cross-compilation
  as under development (Field 04). The notes do
  **not** record the fetch command, the package
  index URL, or a download size for the wheel.
- **A-007 / the Pi's own artifacts (documented +
  PROVEN):** the road this machine actually took is
  fully reconstructible from banked inventory plus
  the on-disk manifest/lock (Section 3.2). This is
  the only end-to-end ARM64 acquisition road for
  which this repository holds complete evidence.
- **The gap, recorded plainly (INC-F002-04):** no
  banked document contains a step-by-step install
  transcript for Mojo on Linux ARM64 — no canonical
  command sequence, no total download size, no
  account requirement statement, and no mention of
  any direct-installer or `modular`-CLI road. The
  sequence in Section 3.4 is therefore a
  reconstruction from the observed install, labelled
  as such, and marked NOT RUN.

### 3.2 The documented road this Pi took (pixi / conda channel)

Package and channel names exactly as documented in
the artifacts (pixi.toml, pixi.lock, conda-meta —
all **PROVEN on the Pi** as on-disk records):

- Package manager: **pixi** (observed 0.81.0).
- Workspace: `mojo-dev`, `platforms =
  ["linux-aarch64"]`.
- Channels, in order: **`https://conda.modular.com/max`**,
  then **`conda-forge`**.
- Dependency: **`mojo = ">=1.1.0,<2"`**, resolved by
  `pixi.lock` to, from the Modular channel,
  `linux-aarch64/mojo-1.1.0-release.conda`,
  `linux-aarch64/mojo-compiler-1.1.0-release.conda`,
  `noarch/mojo-python-1.1.0-release.conda`,
  `noarch/mblack-26.6.0-release.conda`, plus ~46
  conda-forge packages (python 3.14.7, libgcc/
  libstdcxx 16.2.0, krb5, zeromq, jupyter_client,
  and relatives) — 50 locked packages in total.
- Identity pins in the lock: `mojo` SHA-256
  `9a9b46c8ec0068928fe35b2fd5cb4dca30bac7092f70e06c41273bdbc2d83d79`;
  `mojo-compiler` SHA-256
  `6d764a80411401b0e04ef4c8e779d7b09564cb0011d382986aefe189378451d1`.
- The tablet wheel road (Section 3.1) is a *second*,
  distinct documented road — a Python-environment
  wheel install on Android/Termux-glibc — and is
  **not** interchangeable with this one: different
  version (1.2.0 vs 1.1.0), different host ABI
  situation, different outcome (startup abort vs
  clean run). Later slices must name which road
  any claim belongs to.

### 3.3 Licence and account posture, as documented

- Prebuilt packages: **`LicenseRef-Modular-Proprietary`**
  — the licence string recorded verbatim in
  `pixi.lock` and in the env's
  `conda-meta/mojo-1.1.0-release.json` and
  `mojo-compiler-1.1.0-release.json` (**PROVEN on
  the Pi** as the packages' own metadata). The
  banked library contains no licence text, no
  redistribution analysis, and no account-terms
  statement for these packages (**documented**
  absence). Any use beyond the installed,
  on-machine development use already exercised is
  therefore **OWED** a terms check before it
  happens; this record grants nothing.
- Source road: the checkout's `LICENSE` states the
  Modular repository is licensed under the **Apache
  License v2.0 with LLVM Exceptions** (**PROVEN on
  the Pi**, file read in the local checkout;
  consistent with Roadmap S9 pointing at a public
  repository). The two roads carry different
  licence postures (INC-F002-09).
- Account posture: **UNKNOWN from the banked
  library** (no document addresses it), and
  **PROVEN on the Pi** only in the narrow sense of
  Section 2.3: the observed install left no
  credentials file, and the packages sit in the
  local rattler cache. Whether a fresh fetch from
  the Modular channel requires an account today is
  **OWED** — to be settled by documentation or by
  Volmarr's word at install time, never assumed.

### 3.4 The exact sequence an operator WOULD run — NOT RUN

> **NOT RUN in this slice. Reserved for Volmarr's
> word.** The sequence below reconstructs the
> observed install (Section 3.2) for a *fresh*
> machine or a *clean re-install*. On this Pi every
> step's result already exists; running it here
> today would be a re-install/upgrade action, not
> a readiness check. The upgrade variant (step 4b)
> is the only install-shaped decision this record
> puts before Volmarr, and only if version parity
> with the field-notes failure (1.2.0) is wanted —
> F-003/F-004 do not need it (Section 5).

```bash
# 1. Obtain pixi itself (operator's usual pixi install
#    route for linux-aarch64; the banked library does
#    not document this step — INC-F002-04).
#    Result on this Pi: ~/.pixi/bin/pixi, pixi 0.81.0.

# 2. Create the workspace manifest ~/mojo-dev/pixi.toml
#    with exactly the content recorded in Section 2.3
#    (channels conda.modular.com/max + conda-forge,
#    platforms = ["linux-aarch64"], mojo = ">=1.1.0,<2").

# 3. Resolve and install (fetches the 50 locked
#    packages, ~231 MiB per Section 5):
pixi install --manifest-path ~/mojo-dev/pixi.toml

# 4a. Verify (read-only; these three WERE run by this
#     slice against the existing install):
mojo --version
mojo run <minimal-program>.mojo
mojo build <minimal-program>.mojo -o <output>

# 4b. OPTIONAL upgrade to a named newer version
#     (e.g. 1.2.0 for field-notes parity) — edit the
#     pixi.toml bound or add an exact pin, then:
pixi update mojo --manifest-path ~/mojo-dev/pixi.toml
#     NOT RUN. Changes the compiler every later
#     Phase 6 comparison is pinned to (INC-F002-02).
```

No step in this sequence was executed by this slice
except the read-only verifications in 4a, which ran
against the pre-existing install and changed no
toolchain state (the build outputs went to `/tmp`).

## 4. Feasibility verdict for this Pi (assignment item c)

**Verdict: PROVEN possible** — for the prebuilt
pixi/conda road, on this machine, at the installed
version. The evidence, in order of strength:

1. `mojo --version` → `Mojo 1.1.0 (8189361e)`,
   exit 0, both through the wrapper and by direct
   env-binary invocation (**PROVEN**, Section 2.4).
2. `mojo run` of a minimal program → expected
   output, exit 0 (**PROVEN**).
3. `mojo build` → native AArch64 ELF (dynamically
   linked, system loader), which itself ran with
   exit 0 (**PROVEN**). The Pi's OS/libc situation
   is the favourable one in the banked matrix:
   native Debian 12 + glibc 2.36 with
   `/lib/ld-linux-aarch64.so.1` present
   (`docs/SUPPORTED_TARGETS.md`: "Linux AArch64 +
   glibc — First production candidate",
   **documented**), and the resolved conda platform
   floor (glibc 2.28) sits below the Pi's glibc
   2.36 (**PROVEN**, pixi info + ldd).

**What the verdict does NOT cover** (each labelled):

- The **tablet road** stays exactly as banked:
  Mojo 1.2.0 and the 1.3.0.dev nightly aborted
  before `--version` in TCMalloc `MmapAligned()`
  on the SM-X400's 39-bit VA environment
  (**documented**, Field 03). Nothing on this Pi
  reproduces or refutes that failure; the Pi is a
  different target row (native glibc vs
  Termux-glibc on Android) and the roadmap forbids
  transferring the result (Roadmap Sections 1, 5).
  The Phase 6 exit's "original failing command"
  therefore has **no Pi-native reproduction** —
  **OWED** to F-008, on whichever ARM environment
  it can honestly be re-run.
- The **source-build road** on this Pi is
  **BLOCKED as observed**: the one recorded attempt
  (Section 2.5) failed after ~3.8 hours of Bazel
  elapsed time with compiler errors in
  `KGENCompiler.cpp`. Diagnosis is **OWED** (no
  slice owns it; it is the operator's local tree).
  This blocks only the source route — e.g. a
  source-level jemalloc/TCMalloc selection change
  (SYSTEM_ALLOCATOR_EVALUATION Section 5.2) — not
  the prebuilt runtime road.
- **MimirAlloc servicing Mojo allocations** is
  **BLOCKED on wiring that does not exist**, not on
  the toolchain: the banked surface is four
  namespaced symbols (`mimir_malloc`, `mimir_free`,
  `mimir_calloc`, `mimir_realloc`; Phase 5 exit,
  Pi-native scope), no standard-name interposition
  exists, and the generated-program path links the
  env runtime globals (Section 2.4) whose allocator
  behaviour F-001/F-003 must map before F-005 can
  prove use. Toolchain readiness removes the
  *toolchain* excuse; it does not move the exit.
- **Android/bionic Mojo** anything: **UNKNOWN /
  unevidenced** in the banked record (SUPPORTED_
  TARGETS keeps it Planned with no MimirAlloc or
  Mojo-native result) and untouched by this slice.

**Smallest unblocking step.** For the runtime half:
none — it is not blocked on this machine, and no
alternative ARM environment needs to be named
because the primary one works. For the Phase 6
exit: the next step is not an install at all — it
is F-001's boundary decision landing, after which
F-003 builds the wiring harness against the
*installed* 1.1.0. The only decision reserved to
Volmarr in this whole record is the optional 1.2.0
upgrade of Section 3.4 step 4b, needed only if he
wants the Pi compiler to match the field-notes
failure version before F-008 retries it.

## 5. Disk / load budget (assignment item d)

All sizes **PROVEN on the Pi** (lockfile arithmetic
and `du` in this slice) unless marked documented.

| Item | Size | Source |
|---|---|---|
| `mojo-1.1.0-release.conda` (download) | 114,828,378 B (~109.5 MiB) | pixi.lock `size:` |
| `mojo-compiler-1.1.0-release.conda` (download) | 67,816,551 B (~64.7 MiB) | pixi.lock `size:` |
| All 50 locked packages (download total) | 242,292,831 B (231.1 MiB) | sum of pixi.lock `size:` fields, this slice |
| Installed env on disk (`~/mojo-dev`) | 1,011 MiB | `du -sh` |
| pixi global dir (`~/.pixi`) | 210 MiB | `du -sh` |
| Rattler package cache (`~/.cache/rattler`) | 964 MiB | `du -sh` |
| **Prebuilt road, total footprint already on disk** | **~2.2 GiB** (env + pixi + cache; already spent, not a new cost) | sum of the three rows above |
| Source road already on disk (`~/mojo-build` + `~/.cache/bazel`) | 352 MiB + 11 GiB | `du -sh` (operator's local state, Section 2.5) |
| Disk free at check time | 199 GiB of 469 GiB (57% used) | `df -h /` |
| RAM at check time | 15 GiB total, 13 GiB available | `free -g` |

Budget reading. A *fresh* prebuilt install would
cost ~231 MiB of download and ~1.2 GiB of new disk
(env + pixi, cache shared) — under 1% of the free
199 GiB; disk is not a constraint on this Pi for
the prebuilt road (**PROVEN** arithmetic on the
sizes above). Load: `mojo --version`/`run`/`build`
of a minimal program each completed in seconds in
this slice and Samtal stayed at HTTP 200; the
heavy cost in this territory is the *source* road —
the recorded Bazel attempt ran 13,776 s elapsed —
which is precisely why the assignment keeps builds
small and why no Wave 6 slice should re-attempt a
source build on this working Pi without Volmarr's
word. A full *fresh-machine* download total beyond
the lock sum (pixi's own binary, ~69 MiB observed)
is the only number here not in the lock; the
library documents no other size (**documented**
absence). Nothing in this budget starves Samtal:
the readiness state is read-mostly, and the
already-installed footprint is sunk cost.

## 6. Inconsistencies and gaps (assignment item e)

None below is silently resolved. INC-F002-01 and
INC-F002-10 are the banking-reconciliation items.

- **INC-F002-01 — banked presence claims are stale
  in both directions.** `docs/BASELINE_INVENTORY.md`
  B-08 records "No successful Mojo build, compile,
  or inference run is recorded on any target", and
  `docs/SYSTEM_ALLOCATOR_EVALUATION.md` Section 5.3
  records "there is no Mojo source or Bazel on
  Mimir". On today's Pi: a prebuilt Mojo compiles
  and runs programs (Section 2.4, PROVEN) and a
  full source checkout with a `bazelw` build
  attempt exists (Section 2.5, PROVEN as
  filesystem). Both banked sentences were true of
  the *repo evidence* when written; both are false
  as *machine* claims now. Any later slice citing
  them must scope them to their writing date.
  Reconciliation owed at Wave 6 banking (with
  F-001, INC-F002-10).
- **INC-F002-02 — three Mojo versions in play, no
  pinned Phase 6 version yet.** Installed prebuilt:
  1.1.0. Field-notes failure: 1.2.0 + 1.3.0.dev2026110205
  (tablet). Local source checkout: 1.2.0.dev2026100406
  / MAX 26.7.0.dev2026100406. Roadmap Phase 6 demands
  same-revision comparison with and without the
  replacement; until a slice pins THE version,
  cross-version claims are invalid. The `fn`→`def`
  parse failure in Section 2.4 is the same skew
  showing at language level.
- **INC-F002-03 — source-build route failed on this
  Pi; failure is unowned.** `~/mojo-build/build.log`
  ends in `Target //Mojo/tools/mojo:mojo failed to
  build` (14 errors, `KGENCompiler.cpp`). No repo
  slice owns diagnosing it, the log is not in the
  repo, and the banked record neither promises nor
  forbids a retry. Status: BLOCKED as observed;
  diagnosis OWED; retry is Volmarr's-word territory
  (load, Section 5).
- **INC-F002-04 — no canonical install transcript
  exists in the banked library.** No step-by-step
  ARM64 install sequence, no fetch command for the
  tablet wheel, no total-size or account statement,
  no direct-installer/`modular`-CLI road is
  documented anywhere in the repo (grep-checked by
  this slice). Section 3.4 is a reconstruction from
  the observed install, not a quotation. The gap is
  the finding; it is not filled by invention.
- **INC-F002-05 — "Modular home" names three
  different states.** `~/.modular` absent; `~/.local/
  share/modular` present but empty; effective
  `MODULAR_HOME` = env `share/max`, set only by the
  env activation script. Presence checks keyed to
  any one of the three disagree with each other.
- **INC-F002-06 — MAX is a channel and a data
  directory, not a tool, on this Pi.** No `max`
  executable; "MAX Platform 1.1.0" exists only as
  `share/max/modular.cfg` data for the Mojo env.
  Any "MAX is installed/running" claim would be
  false on this evidence.
- **INC-F002-07 — readiness depends on two fragile
  indirections.** `mojo` resolves only through a
  root-owned 99-byte wrapper in `/usr/local/bin`,
  and pixi resolves only by absolute path or via
  that wrapper; neither `~/.pixi/bin` nor the env
  is on the default PATH, and the manifest lives in
  the operator's home (`~/mojo-dev/pixi.toml`), not
  in this repo. Remove or move any one of wrapper,
  manifest, or env and this machine presents as
  Mojo-less. The repo records the *facts* (A-007,
  this slice) but pins none of the artifacts.
- **INC-F002-08 — the Phase 6 "original failing
  command" has no home environment yet.** The only
  recorded failure is the tablet's (Field 03), in
  an environment (Android + Termux glibc) that is
  neither this Pi nor a banked MimirAlloc target
  run. F-008 must either reproduce a failure on an
  environment it names or record the exit's blocker
  branch ("or record a blocker with exact
  results", Roadmap Phase 6) — a Pi hello-world,
  however green, is expressly not the exit
  (Roadmap Phase 6, last sentence).
- **INC-F002-09 — licence posture splits by road
  and is unreviewed.** Prebuilt: LicenseRef-Modular
  -Proprietary (package metadata). Source: Apache
  License v2.0 with LLVM Exceptions (repo LICENSE).
  The banked library holds no analysis of what
  either means for building/redistributing a
  MimirAlloc-linked Mojo artifact. OWED before any
  redistribution-shaped step; no effect on the
  on-machine readiness verdict.
- **INC-F002-10 — reconciliation with F-001 is owed
  at banking.** F-001 (team-01) inventories this
  same toolchain in parallel from the same base.
  Where its inventory and this record disagree on
  any version, size, or presence fact, the keeper
  reconciles at banking from the commands in
  Section 2 (each is re-runnable read-only) rather
  than averaging the two texts. This slice depended
  on no F-001 commit, per the assignment.

## 7. Slice hygiene

- Files changed: this file only
  (`docs/PHASE_6_MOJO_TOOLCHAIN_READINESS.md`,
  added). The stale `hidden_files/ASSIGNMENT.md` in
  the worktree was replaced with this slice's
  assignment (untracked keeper file, not part of
  the commit, as in prior waves).
- `README.md`: byte-untouched — blob
  `d7949561e229f495c9aef37d75432f8c3a0f6e10` at base
  211eb31 and in this commit; working-tree sha256
  `bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`
  before and after.
- `make test`: run in this worktree before the
  commit: exit 0, composition unchanged (the eight
  banked test executables; final binary RESULT:
  PASS, 4590 checks, 0 failed). Docs-only slice;
  the run proves the tree is unharmed, not
  anything about Mojo.
- One commit on `yrsa/mimiralloc-02`, parent exactly
  211eb31. Not pushed, not merged — the keeper
  banks after F-001 in roadmap order.
- Nothing installed, downloaded, upgraded, or
  removed in this slice. Toolchain invocations were
  read-only or wrote only under `/tmp/f002`.
  Samtal HTTP 200 before and after; `free -g`
  showed 13 GiB available at check time.
