# MimirAlloc Integration Layer Map

| Field | Value |
|---|---|
| Slice | A-007 (Roadmap Section 3.3) |
| Document | Opening sections of `docs/INTEGRATION_LAYER_MAP.md` |
| Roadmap base | `b0b837453da351700987bb563126c90be2b8978f` |
| Derived from | Roadmap §3.3 + §2 evidence levels + A-002 ledgers (`8b7bc19`) + A-003 targets (`aa8c022`) + A-005 harness (`c4298b3`) |
| Evidence status summary | **Blocked.** No reproducible Mojo/tcmalloc failure packet exists in the repo; A-005's only packet is synthetic, with claim scope harness-evidence-only. The summary matrix in Section 4 is a device-observed inventory of the installed toolchain on Mimir, not a mapping of a failing build. no standalone tcmalloc library or binary is present on Mimir (TCMalloc-derived symbols embedded in a Mojo runtime library are recorded in Section 7). No finding in this document is Integration-proven or Performance-measured. |

> Honesty disclaimer: this document does not claim that MimirAlloc fixes Mojo, that any consumer below uses tcmalloc, or that any substitution point named below can change a failure that has not yet been captured. Where evidence has not been captured, this document says so in those words: NOT YET CAPTURED.

## 1. Honest headline

Three truths govern everything that follows:

| # | Truth | Evidence |
|---|---|---|
| (i) | This is a mapping **plan** plus a device-observed **inventory** of the installed toolchain. It is not a map of a known failing build, because no failing build has been identified to map. | Roadmap §3.3 deliverables and exit gate; Section 4 inventory labelled as such |
| (ii) | **No reproducible Mojo/tcmalloc failure exists in the repo.** A-005's only packet is synthetic, with claim scope harness-evidence-only, so §3.3's exit gate is **Blocked**. The resumption point is: a real A-005 packet, plus the binaries it ran. | A-005 harness (`c4298b3`); evidence level: **Blocked** per Roadmap §2 |
| (iii) | **No standalone tcmalloc library or binary is present on Mimir.** TCMalloc-derived symbols embedded inside libAsyncRTRuntimeGlobals.so are recorded in Section 7; no linkable tcmalloc exists here. Nothing can be mapped "against tcmalloc" on this device today. | Verified on device: `find` in the pixi / `mojo-dev` environment is empty for tcmalloc, and `ldconfig` output is empty for tcmalloc |

Consequence: every layer verdict in Section 4 is UNDETERMINED (Blocked) for the §3.3 gate, because no failure packet names a failing layer. That is the honest result of this slice, not a gap to be filled by inference.

## 2. Purpose and scope

Roadmap §3.3 exists because there may be several independent allocator consumers, and a substitution that reaches one consumer may not reach another at all. For each consumer, this map determines — from exact exported and unresolved symbols — whether the allocator is:

- statically linked,
- dynamically linked,
- explicitly called,
- interposed, or
- absent.

> "A library with the same function names alone is not proof it is used."

That sentence is the discipline of this whole document. A name match is a lead, never a finding.

### In scope

| In scope | Notes |
|---|---|
| Five consumers (Layers L1–L5 in Section 4) | Build tools; Mojo compiler executable; Mojo standard/runtime libraries; generated Mojo executable; Aesir-specific allocation |
| ELF / linkage inspection | Using tools such as `readelf`, `nm`, `objdump`, compiler link maps, and build-system query/aquery facilities, per Roadmap §3.3 |
| Android namespace / sandbox notes | On Android, use NDK-compatible tools and inspect namespace and app sandbox restrictions; dynamic preloading may not affect a statically linked executable, private/hidden symbols, secure execution, internal allocators, or Android app processes |
| Exit-gate decision | Whether someone else can reproduce the failure and explain why the selected integration point can or cannot change it |

### Out of scope

| Out of scope | Reason |
|---|---|
| Building MimirAlloc | This slice maps integration layers; it builds nothing |
| Benchmarking | No Performance-measured claim is made or implied |
| Claiming any Mojo fix | No failure has been captured, so no fix can be claimed |
| Reading tcmalloc source | Clean-room discipline, Roadmap §4: provenance and clean-room engineering govern this project |

## 3. Vocabulary and rules

### Linkage findings

| Finding | Definition | Symbol / artifact evidence that forces it |
|---|---|---|
| Statically linked | The allocator is built into the consumer executable itself. | Allocator symbols are **defined in the executable itself** (for example `T`/`D` in the executable's own symbol table), and there is **no matching `DT_NEEDED`** entry for an allocator library. |
| Dynamically linked | The consumer loads an allocator provider library at runtime. | The provider appears in **`DT_NEEDED`**; the allocator symbols are **`U` (unresolved) in the consumer** and **`T`/`D` (defined) in the library**. |
| Explicitly called | Code in the consumer directly calls a named allocator API. | A **named allocator API is called via PLT / direct reference** in the consumer, evidenced in its relocations / PLT and unresolved symbols — not merely present as a name. |
| Interposed | A non-default provider actually binds the consumer's allocator calls at runtime. | A **non-default provider actually binds at runtime** — this **requires loader trace / maps proof** (for example a loader trace or the process maps for the running process). Static symbol evidence alone can never force this finding. |
| Absent | No allocator provider is in the consumer's closure. | **No allocator symbols and no allocator library in the closure.** If the consumer allocates another way, that way — for example internal allocation or direct `mmap` — **must be stated, not assumed**. |

### Rules

| Rule | Statement |
|---|---|
| Separate symbol records | Record **exported** symbols and **unresolved** symbols separately, for every binary examined. They answer different questions and are never merged into one list. |
| Visibility and binding | Record hidden visibility, `-Bsymbolic`, and version scripts where present. Any of them can prevent a substitution from binding, even when names match. |
| Citation or absence | Every finding cites an artifact, or reads **NOT YET CAPTURED**. An uncited finding is not a finding. |
| Evidence-level discipline (Roadmap §2) | Never promote a compile result to interposition proof; never promote a synthetic result to application compatibility; never promote a proot result to Android proof. The same discipline bars promoting an emulator result to physical-device proof. Evidence labels used in this project: Proposed, Implemented, Host-tested, Device-tested, Integration-proven, Performance-measured, Blocked. |
| Exact binary | A finding applies to the exact binary examined — its path, version, and hash where captured — never to a product name in general. Section 4, Layer L2, is the standing example: two different Mojo binaries on the same device give different findings. |

## 4. Summary matrix

> **Matrix label — read first:** the findings in this matrix describe the **installed toolchain inventory** observed on Mimir. They do **not** describe the unknown failing build, because no failing build has been captured (see Section 1, truth (ii)). No row below names a failing layer.

| Layer | Consumer | Failing? | Primary linkage finding | Allocator provider today | Candidate substitution points | Verdict | Evidence | Status |
|---|---|---|---|---|---|---|---|---|
| L1 | Build tools | Not known — no failure packet names this layer | UNDETERMINED | UNDETERMINED | NOT YET CAPTURED — none identified on this device | UNDETERMINED (Blocked) | Bazel is **absent on Mimir (caches only)**; no build-tool binary was available to inspect | Blocked |
| L2 | Mojo compiler | Not known — no failure packet names this layer | **Per exact binary — two candidates:** (a) pixi environment compiler: dynamically linked; (b) `~/.pixi/bin/mojo`: statically linked, stripped | glibc (for the dynamically linked pixi environment compiler) | NOT YET CAPTURED — candidate points differ per exact binary and none is proven against a failure | UNDETERMINED (Blocked) | Device-observed inventory of the two named binaries; the finding applies per exact binary, not to "Mojo" in general | Blocked |
| L3 | Runtime libs | Not known — no failure packet names this layer | Dynamically linked | glibc | NOT YET CAPTURED — no substitution has been tested against a failure | UNDETERMINED (Blocked) | UND (unresolved) glibc `malloc` / `calloc` / `free` observed in `libKGENCompilerRTShared.so` and `libMSupportGlobals.so` | Blocked |
| L4 | Generated executable | Not known — no failure packet names this layer | UNDETERMINED | UNDETERMINED | NOT YET CAPTURED — no generated executable exists on this device to inspect | UNDETERMINED (Blocked) | **No ARM generated executable exists** on Mimir | Blocked |
| L5 | Aesir | Not known — no failure packet names this layer | UNDETERMINED | UNDETERMINED | NOT YET CAPTURED — no Aesir ELF or process was captured to inspect | UNDETERMINED (Blocked) | Only Python / source material is present on Mimir; **no Aesir ELF / process captured** | Blocked |

Reading the Verdict column: UNDETERMINED (Blocked) in every row is a single, shared verdict about the §3.3 exit gate, not five separate failures. The gate cannot be passed until a real A-005 failure packet names a failing layer and supplies the binaries it ran. At that point this matrix is re-issued for that exact build, and the inventory rows above are not carried forward as findings about it.


## 5. Layer L1 — The Build Tools Themselves (Bazel, Compiler, Linker, Dependencies)

Per roadmap §3.3, consumer 1 is the build tooling itself: Bazel (or
Bazelisk), the host compiler, the linker, and any build-time dependencies.
These tools allocate heavily while they run, and if a build failure is an
allocator failure inside the tool rather than in the code being built, this
is the layer where that would be proven. Mapping L1 therefore means pinning
the exact tool binaries that a failing build actually executed, and only
then inspecting their linkage. On Mimir today there is no Bazel or
Bazelisk binary on `PATH` and no failing Mojo build in progress to map the
tools against, so this layer is mapped as far as presence and caches allow
and is otherwise Blocked.

| Item to identify | Linkage possibilities to test | Inspection procedure (exact commands) | Allocator symbols sought | Evidence artifact (future path under evidence/integration/) | Status today |
|---|---|---|---|---|---|
| Bazel / Bazelisk binary on PATH | Static; Dynamic; Absent | `which bazel bazelisk` | n/a until a binary is pinned (then `malloc`, `calloc`, `free`, `realloc`) | `evidence/integration/l1-bazel-which.txt` | Blocked — absent on PATH (see Observed) |
| Bazelisk download cache | Cached binary present but not installed/linked on PATH | `ls ~/.cache/bazelisk/downloads`; `ls -R ~/.cache/bazelisk/downloads` | n/a (cache inventory only) | `evidence/integration/l1-bazelisk-cache.txt` | Observed — caches only, no runnable tool on PATH |
| Host compiler (clang, gcc) | Dynamic (host distro build); Static | `which clang gcc`; `file $(which clang)`; `readelf -d $(which clang)` | `malloc`, `calloc`, `free`, `realloc` (UND vs defined) | `evidence/integration/l1-compiler-linkage.txt` | Present as host tools; not mapped — no failing build exists to map them against |
| Host linker (ld) | Dynamic (host distro build); Static | `which ld`; `file $(which ld)`; `readelf --dyn-syms $(which ld)` | `malloc`, `calloc`, `free`, `realloc` | `evidence/integration/l1-linker-linkage.txt` | Present as host tool; not mapped — same reason as compiler |
| Bazel query/aquery facilities for a Mojo build graph | n/a (build-system query, not a binary linkage) | `bazel query ...`; `bazel aquery ...` inside a Mojo source checkout | Link-action allocator references in aquery output | `evidence/integration/l1-aquery.txt` | Blocked — no Bazel binary and no Mojo source checkout build on the Pi |

**Observed on Mimir (2026-11-06)** — commands re-run for this slice:

```text
$ which bazel bazelisk ; echo "bazel exit=$?"
bazel exit=1                      # neither name resolved on PATH
$ ls ~/.cache/bazelisk/downloads
metadata
sha256
_tmp
$ ls -R ~/.cache/bazelisk/downloads   (abridged)
metadata/bazelbuild/bazel-46c5e789f84c7bf4ba1edb105eefa7bc4ebc841b-linux-arm64
metadata/buildbuddy-io/bazel-5.0.382-linux-arm64
sha256/5745384d4c010d4324120616f64dee1a75af984dc621075d5d4c755f84d486f3/bin/bazel
sha256/c09c1f4a1a80bef26b7416b4569296eb037a90f3daa84ccb520be05236a2e550/bin/...
$ which clang gcc ld
/usr/bin/clang
/usr/bin/gcc
/usr/bin/ld
```

Reading: a Bazelisk cache survives on disk (metadata for two linux-arm64
Bazel builds and at least one cached `bin/bazel` payload), but no `bazel`
or `bazelisk` command is installed on `PATH`, so no build tool can be
executed, pinned, or symbol-inspected as a running consumer today. The
compiler and linker exist only as ordinary host tools; with no failing
build, there is no evidence tying any allocation behaviour to them.

**Status today: Blocked** (no Bazel binary; caches only; compiler/linker
present as host tools but no failing build exists to map them against).

**What would change our mind:** a `bazel`/`bazelisk` binary installed on
`PATH` (path + sha256 + BuildID recorded) *and* a real Mojo build failure
reproduced on the Pi whose tool processes can be pinned to that binary —
then the linkage rows above can be filled from evidence instead of
possibility.

---

## 6. Layer L2 — The Mojo Compiler Executable

Per roadmap §3.3, consumer 2 is the Mojo compiler executable itself, in
either of its forms: a prebuilt compiler installed from a package, or a
compiler being built from source. The distinction matters because the two
forms can have opposite linkage: a source/Bazel build might carry its
allocator statically or explicitly, while a shipped prebuilt may simply
call the system allocator dynamically. On Mimir the installed compiler is
a prebuilt pixi/conda-environment compiler, **not** a Bazel/source build —
and Mimir in fact presents *three* different `mojo` entry points whose
linkage disagrees with each other, which is itself the central L2 finding.

| Item to identify | Linkage possibilities to test | Inspection procedure (exact commands) | Allocator symbols sought | Evidence artifact (future path under evidence/integration/) | Status today |
|---|---|---|---|---|---|
| Env compiler ELF `/home/pi/mojo-dev/.pixi/envs/default/bin/mojo` | Dynamic; Explicit-call; Static | `file <path>`; `ls -l <path>`; `sha256sum <path>`; `readelf -n <path>` (BuildID); `readelf -d <path>` | `malloc`, `calloc`, `free`, `realloc` as UND dynamic symbols | `evidence/integration/l2-env-mojo-linkage.txt` | Dynamic + Explicit-call candidate — for the INSTALLED toolchain only (see Observed) |
| Pixi shim ELF `/home/pi/.pixi/bin/mojo` | Static; Dynamic | `file <path>`; `ls -l <path>`; `sha256sum <path>`; `readelf -d <path>`; `readelf --dyn-syms <path>` | `malloc`, `calloc`, `free`, `realloc` (none inspectable if static/stripped) | `evidence/integration/l2-pixi-bin-mojo-linkage.txt` | Static / Absent-of-inspectable-symbols |
| PATH entry `/usr/local/bin/mojo` | Not an ELF consumer (script/wrapper) | `cat /usr/local/bin/mojo`; `wc -c /usr/local/bin/mojo` | n/a | `evidence/integration/l2-path-wrapper.txt` | Observed — 99-byte bash wrapper, not an ELF |
| A from-source (Bazel) compiler build | Static; Dynamic; Explicit-call | Requires §5's missing Bazel + a Mojo source checkout; then same `file`/`readelf` procedure on the built binary | `malloc`, `calloc`, `free`, `realloc`, plus any bundled-allocator symbols | `evidence/integration/l2-sourcebuild-mojo-linkage.txt` | Blocked — no source build exists on the Pi |

**Observed on Mimir (2026-11-06)** — commands re-run for this slice:

```text
$ file /home/pi/mojo-dev/.pixi/envs/default/bin/mojo /home/pi/.pixi/bin/mojo
/home/pi/mojo-dev/.pixi/envs/default/bin/mojo: ELF 64-bit LSB pie executable, ARM aarch64,
  version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1,
  for GNU/Linux 3.7.0, BuildID[md5/uuid]=7e410907739ac44de3cd765083c447bc, stripped
/home/pi/.pixi/bin/mojo: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV),
  statically linked, stripped
$ ls -l /home/pi/mojo-dev/.pixi/envs/default/bin/mojo /home/pi/.pixi/bin/mojo
-rwxr-xr-x  3 pi pi 138588560 Sep 10 19:43 /home/pi/mojo-dev/.pixi/envs/default/bin/mojo
-rwxr-xr-x 10 pi pi    671888 Oct  4 00:04 /home/pi/.pixi/bin/mojo
$ sha256sum /home/pi/mojo-dev/.pixi/envs/default/bin/mojo /home/pi/.pixi/bin/mojo
be46936245ae995680155dcc2f61af5dac8678254c634110e99ceff25ce57c39  .../envs/default/bin/mojo
4ec66c0cc2229be5895201ab8b49547d0b0810c5a4845a6f8a84cfdcee5482e8  /home/pi/.pixi/bin/mojo
$ readelf -n /home/pi/mojo-dev/.pixi/envs/default/bin/mojo
  .note.gnu.build-id: Build ID: 7e410907739ac44de3cd765083c447bc
$ readelf -d /home/pi/mojo-dev/.pixi/envs/default/bin/mojo   (NEEDED entries)
  (NEEDED) libm.so.6
  (NEEDED) libstdc++.so.6
  (NEEDED) libMSupportGlobals.so
  (NEEDED) libAsyncRTRuntimeGlobals.so
  (NEEDED) libgcc_s.so.1
  (NEEDED) libc.so.6
  (NEEDED) ld-linux-aarch64.so.1
  (RPATH)  $ORIGIN/../lib:$ORIGIN/mojo.runfiles/_main/_solib_aarch64/_USupport:
           $ORIGIN/mojo.runfiles/_main/_solib_aarch64/_UAsyncRT
$ readelf --dyn-syms /home/pi/mojo-dev/.pixi/envs/default/bin/mojo | grep -E 'malloc|calloc|free|realloc'
     227: 0000000000000000 0 FUNC GLOBAL DEFAULT UND free@GLIBC_2.17 (2)
     269: 0000000000000000 0 FUNC GLOBAL DEFAULT UND malloc@GLIBC_2.17 (2)
     275: 0000000000000000 0 FUNC GLOBAL DEFAULT UND calloc@GLIBC_2.17 (2)
  (no realloc line was returned for this binary)
$ readelf -d /home/pi/.pixi/bin/mojo
There is no dynamic section in this file.
$ readelf --dyn-syms /home/pi/.pixi/bin/mojo
  (no output — no dynamic symbols)
$ cat /usr/local/bin/mojo ; wc -c /usr/local/bin/mojo
#!/bin/bash
exec /home/pi/.pixi/bin/pixi run --manifest-path /home/pi/mojo-dev/pixi.toml mojo "$@"
99 /usr/local/bin/mojo
```

Reading: the environment compiler is dynamically linked, its `DT_NEEDED`
set includes `libc.so.6`, `libstdc++.so.6`, `libMSupportGlobals.so` and
`libAsyncRTRuntimeGlobals.so`, and it imports `malloc`/`calloc`/`free` as
UND symbols versioned `@GLIBC_2.17` — a Dynamic + Explicit-call candidate
for the installed toolchain only. Its RPATH still mentions
`mojo.runfiles` paths (a Bazel-style build origin), but the binary as it
exists on Mimir was delivered and is used as an installed prebuilt; no
source build was performed or observed here. The `~/.pixi/bin/mojo` ELF
is statically linked and stripped, with no dynamic section and no dynamic
symbols at all, so no symbol-level allocator claim can be made about it
either way. The ambiguity is a finding in its own right: **the Layer-2
classification depends on which exact binary + BuildID is pinned** — the
two ELF `mojo`s on this one machine classify differently, and the `mojo`
on `PATH` is neither of them, being a 99-byte bash wrapper that `exec`s
`pixi run`.

**Status today:** installed prebuilt pixi/conda compiler mapped (Dynamic +
Explicit-call candidate, installed toolchain only); second ELF Static /
Absent-of-inspectable-symbols; PATH entry not an ELF consumer; source-build
form Blocked. Gate verdict for this layer: UNDETERMINED (Blocked) - no failure packet names it.

**What would change our mind:** pinning, by exact path + sha256 + BuildID,
which `mojo` binary a future failing compile actually executed — or a real
from-source compiler build on the Pi — either of which would collapse the
present two-classification ambiguity into one evidenced classification.

---

## 7. Layer L3 — The Mojo Standard / Runtime Libraries

Per roadmap §3.3, consumer 3 is the Mojo standard/runtime libraries that
get linked into generated applications: principally
`libKGENCompilerRTShared.so` (the compiler runtime), `libMSupportGlobals.so`
(support globals) and `libAsyncRTRuntimeGlobals.so` (the async runtime
globals) under the environment's `lib/` directory. Whatever allocator
behaviour these libraries carry is inherited by every generated program,
so this layer is where a substitution would propagate furthest. On Mimir
all three are dynamically linked AArch64 shared objects that import the
glibc allocator — and one of them also carries a bundled TCMalloc-derived
implementation *inside itself*, which the symbol tables below show
directly.

| Item to identify | Linkage possibilities to test | Inspection procedure (exact commands) | Allocator symbols sought | Evidence artifact (future path under evidence/integration/) | Status today |
|---|---|---|---|---|---|
| `libKGENCompilerRTShared.so` | Dynamic; Explicit-call (incl. calls into L3 sibling globals) | `ls <env>/lib/`; `file <lib>`; `readelf -W --dyn-syms <lib> \| grep -iE 'malloc\|calloc\|free\|realloc'` | `malloc`, `calloc`, `free`, `realloc` UND; `TCMallocGlobals::tc_new`/`tc_delete` UND; defined `KGEN_CompilerRT_AlignedFree` | `evidence/integration/l3-kgen-symbols.txt` | Dynamic — UND glibc allocator symbols observed |
| `libMSupportGlobals.so` | Dynamic; Explicit-call | `file <lib>`; `readelf -W --dyn-syms <lib> \| grep -iE 'malloc\|calloc\|free\|realloc'` | `malloc`, `free`, `realloc` UND (test also for `calloc`) | `evidence/integration/l3-msupport-symbols.txt` | Dynamic — UND glibc allocator symbols observed |
| `libAsyncRTRuntimeGlobals.so` | Dynamic; Explicit-call; bundled/internal allocator (defined symbols) | `file <lib>`; `readelf -W --dyn-syms <lib> \| grep -iE 'malloc\|calloc\|free\|realloc'` | `malloc`, `free`, `realloc` UND; defined `TCMallocInternal*`, `TcmallocSlab_*`, `TCMallocGlobals::tc_new`/`tc_delete`; WEAK UND `tcmalloc_internal::*` | `evidence/integration/l3-asyncrt-symbols.txt` | Dynamic + internal TCMalloc-derived symbols defined in-library (see Observed) |
| Mojo standard library form in the env | Compiled-only vs source shipped | `ls <env>/lib/mojo/`; `find <env>/lib/mojo -name '*.mojo'` | n/a (artifact-form check) | `evidence/integration/l3-stdlib-form.txt` | Compiled only — `std.mojoc`, no `.mojo` sources in env |

**Observed on Mimir (2026-11-06)** — commands re-run for this slice
(`L` = `/home/pi/mojo-dev/.pixi/envs/default/lib`):

```text
$ ls $L | grep -iE 'MSupport|KGEN|AsyncRT'
libAsyncRTRuntimeGlobals.so
libKGENCompilerRTShared.so
libMSupportGlobals.so
$ file $L/libKGENCompilerRTShared.so $L/libMSupportGlobals.so $L/libAsyncRTRuntimeGlobals.so
libKGENCompilerRTShared.so:    ELF 64-bit LSB shared object, ARM aarch64, dynamically linked,
  BuildID[md5/uuid]=65cda483c924ccc208a070bafe3c5fca, stripped
libMSupportGlobals.so:         ELF 64-bit LSB shared object, ARM aarch64, dynamically linked,
  BuildID[md5/uuid]=46f1ef26270972f14a7cfec29b94fdb5, stripped
libAsyncRTRuntimeGlobals.so:   ELF 64-bit LSB shared object, ARM aarch64, dynamically linked,
  BuildID[md5/uuid]=6d4f622743740a95332b12a1f36cbbc8, stripped
$ readelf -W --dyn-syms $L/libKGENCompilerRTShared.so | grep -iE 'malloc|calloc|free|realloc'  (key lines)
     105: UND free@GLIBC_2.17
     147: UND malloc@GLIBC_2.17
     181: UND realloc@GLIBC_2.17
     247: UND calloc@GLIBC_2.17
     106: UND _ZN1M7AsyncRT15TCMallocGlobals6tc_newEmm
     107: UND _ZN1M7AsyncRT15TCMallocGlobals9tc_deleteEPv
     161: UND _ZN1M7AsyncRT15TCMallocGlobals6tc_newEmmm
     404: DEFINED (sec 12) KGEN_CompilerRT_AlignedFree
$ readelf -W --dyn-syms $L/libMSupportGlobals.so | grep -iE 'malloc|calloc|free|realloc'  (key lines)
      20: UND free@GLIBC_2.17
      39: UND malloc@GLIBC_2.17
      44: UND realloc@GLIBC_2.17
  (no calloc line was returned for this library)
$ readelf -W --dyn-syms $L/libAsyncRTRuntimeGlobals.so | grep -iE 'malloc|calloc|free|realloc'  (key lines)
      39: UND free@GLIBC_2.17
      49: UND malloc@GLIBC_2.17
      51: UND realloc@GLIBC_2.17
      72: UND WEAK _ZN8tcmalloc17tcmalloc_internal18default_subreleaseEv
      85: UND WEAK _ZN8tcmalloc17tcmalloc_internal6subtle6percpu10VirtualCpu15TestSynchronizeEv
      89: UND WEAK _ZTH16tcmalloc_sampler
     107: DEFINED _ZN1M7AsyncRT15TCMallocGlobals6tc_newEmmm
     124: DEFINED _ZN1M7AsyncRT15TCMallocGlobals6tc_newEmm
     158: DEFINED _ZN1M7AsyncRT15TCMallocGlobals9tc_deleteEPv
     123: DEFINED TCMallocInternalDeleteArraySized
     139: DEFINED TCMallocInternalCfree
     157: DEFINED TCMallocInternalNewArray
     122: DEFINED TcmallocSlab_Internal_PopBatch
     129: DEFINED TcmallocSlab_Internal_PushBatch
  (further defined TCMallocInternal* New/Delete/Aligned variants follow the same pattern)
$ ls $L/mojo/ ; find $L/mojo -name '*.mojo'
std.mojoc
  (find returned no .mojo files)
```

Reading: all three runtime libraries are dynamically linked and import
glibc allocator entry points as UND `@GLIBC_2.17` symbols — `free`,
`malloc`, `calloc`, `realloc` in KGEN; `free`, `malloc`, `realloc` in
MSupportGlobals (no `calloc` line returned); `free`, `malloc`, `realloc`
in AsyncRTRuntimeGlobals. Beyond that, the async runtime library *defines*
a TCMalloc-derived allocator surface internally (`TCMallocInternal*`,
`TcmallocSlab_Internal_*`, and `AsyncRT::TCMallocGlobals::tc_new` /
`tc_delete`, the last two also imported as UND by KGEN), while holding a
few WEAK UND references back into `tcmalloc_internal::*`. So at L3 the
allocator story is two-layered: dynamic glibc calls *and* an in-library
TCMalloc-derived path — a same-named-symbols caveat applies in reverse
here: defined internal symbols prove the code is linked in, not yet which
path any given allocation takes at run time. The standard library itself
ships compiled only: `lib/mojo/` contains `std.mojoc` and no `.mojo`
sources exist in the environment.

**Status today:** mapped at symbol level for the installed environment —
Dynamic (UND glibc allocator symbols in all three libraries) plus a
defined internal TCMalloc-derived surface in `libAsyncRTRuntimeGlobals.so`;
run-time path attribution still open. Gate verdict for this layer: UNDETERMINED (Blocked) - no substitution has been tested against a failure.

**What would change our mind:** a trace (e.g. `ltrace`/PLT-relocation
resolution or an instrumented run of a generated program) showing which of
the two L3 paths — glibc UND calls or the internal TCMalloc-derived entry
points — actually serves allocations, or a different env build whose
symbol tables contradict the ones quoted above.

---

## 8. Layer L4 — The Generated Mojo Executable

Per roadmap §3.3, consumer 4 is the generated Mojo executable itself: the
binary the compiler emits for a user's program, together with its
application-level allocator calls. This is the layer where allocator
behaviour is finally attributable to *the application* as opposed to the
runtime libraries (L3) it links — but only if attribution is done
carefully, separating calls originating in application code from calls
originating in L3. On Mimir today this layer cannot be mapped at all: no
ARM generated executable exists on the Pi, and the only generated
executables found are x86-64 binaries, which cannot run or be meaningfully
attributed on this AArch64 host.

| Item to identify | Linkage possibilities to test | Inspection procedure (exact commands) | Allocator symbols sought | Evidence artifact (future path under evidence/integration/) | Status today |
|---|---|---|---|---|---|
| A Mojo source program to compile | n/a (input artifact) | `find ~/mojo-dev -name '*.mojo' \| head`; `ls ~/mojo-dev` | n/a | `evidence/integration/l4-mojo-sources.txt` | None in `~/mojo-dev` (see Observed) |
| Generated ARM (AArch64) Mojo executable | Dynamic; Static; Explicit-call; Interposed | Once one exists: build per the Mojo cookbook, then `file <exe>`; `readelf -d <exe>`; `readelf --dyn-syms <exe>`; PLT inspection via `objdump -d <exe>` / `readelf -r <exe>`; retain the link map | `malloc`, `calloc`, `free`, `realloc`, `posix_memalign`, `aligned_alloc` | `evidence/integration/l4-generated-exe-linkage.txt` | Blocked — no ARM generated executable exists on the Pi |
| Attribution: application code vs L3 runtime | Explicit-call from app vs inherited from L3 libs | Compare the executable's own UND symbols and PLT relocations against the §7 L3 tables; use the link map to assign each allocator reference to app object or runtime library | Same as above, recorded per-origin | `evidence/integration/l4-attribution.txt` | Blocked — nothing to attribute yet |
| Existing executables under `~/c9base` | Wrong-architecture — excluded from L4 evidence | `file ~/c9base/main` | n/a (excluded) | `evidence/integration/l4-wrong-arch.txt` | Observed — x86-64 only (see Observed) |

**Observed on Mimir (2026-11-06)** — commands re-run for this slice:

```text
$ find ~/mojo-dev -name '*.mojo' | head
  (no output — no .mojo files under ~/mojo-dev)
$ ls ~/mojo-dev
pixi.lock
pixi.toml
$ ls -l ~/c9base/main ; file ~/c9base/main
-rwxr-xr-x 1 pi pi 159080 Oct 10 15:44 /home/pi/c9base/main
/home/pi/c9base/main: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
  dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
  BuildID[sha1]=47ff4d675fe6598fac2b252ef9c2423a28ba9de7, for GNU/Linux 3.2.0, not stripped
```

Reading: `~/mojo-dev` holds only `pixi.toml` and `pixi.lock` — there is
not even a `.mojo` source file in the compiler's own project directory to
compile, and no AArch64 generated executable was found on the Pi. The one
generated-style executable inspected, `~/c9base/main`, is an x86-64 ELF:
wrong-architecture evidence only. It proves such artifacts were produced
*somewhere*, on an x86-64 host; it proves nothing about generated-code
allocator behaviour on ARM. When an ARM generated executable does exist,
the procedure is: build it following the Mojo cookbook on the Pi itself,
record the exact compiler binary pinned per §6, inspect its PLT and
relocations (`readelf -r`, `objdump -d`), keep the link map, and only then
attribute each allocator call to the application or to L3 by origin —
never by symbol name alone.

**Status today: Blocked** — no ARM generated executable exists on the Pi;
the only generated executables found are x86-64 (wrong arch).

**What would change our mind:** a Mojo program compiled on Mimir with the
§6-pinned compiler producing an AArch64 executable whose `file`,
`readelf -d`, dynamic symbols, PLT relocations and link map are captured
into the evidence paths above.

---

## 9. Layer L5 — Aesir-Specific Allocation (Host Process and Android Process)

Per roadmap §3.3, consumer 5 is Aesir-specific allocation: allocation done
by Volmarr's Aesir engine code inside whatever process hosts it. This
layer is really two different questions that must never be merged — Aesir
inside a Linux host process, and Aesir inside an Android process — because
the libc (glibc vs bionic), the loader namespaces, the sandbox, and even
whether dynamic interposition is permitted at all differ between them. On
Mimir today neither question can be answered from binaries: the Aesir
material present is Python/source-level, no Aesir ELF built for ARM was
found, and no Android process has been captured. The Linux-host and
Android rows are therefore kept strictly separate below.

| Item to identify | Linkage possibilities to test | Inspection procedure (exact commands) | Allocator symbols sought | Evidence artifact (future path under evidence/integration/) | Status today |
|---|---|---|---|---|---|
| Aesir material in `~/aesir-build` (Linux-host row) | n/a until an ELF exists (source-level material has no linkage) | `ls ~/aesir-build \| head`; `ls -ld ~/aesir-build/*` | n/a | `evidence/integration/l5-aesir-build-inventory.txt` | Python/source-level only (see Observed) |
| An Aesir ELF binary for ARM (Linux-host row) | Dynamic; Static; Explicit-call | `find ~ -maxdepth 3 -iname '*aesir*'`; `file` on every candidate ELF; then `readelf -d` + `readelf --dyn-syms` per candidate | `malloc`, `calloc`, `free`, `realloc` | `evidence/integration/l5-aesir-elf-linkage.txt` | Blocked/Unverified — no ARM Aesir ELF found; candidates found are x86-64 |
| Aesir inside an Android process (Android row — separate, never merged with the Linux-host row) | Dynamic (bionic); Static; interposition generally unavailable inside Android app sandboxes | Requires an Android device/emulator + NDK: `echo $ANDROID_NDK_HOME`; inspect with NDK `llvm-readelf`/`llvm-nm`/`llvm-objdump`; record linker namespace and app sandbox restrictions for the exact process | `malloc`, `calloc`, `free`, `realloc` (bionic forms) | `evidence/integration/l5-android-process.txt` | Blocked — no NDK on the Pi, no Android process captured |
| proot / Termux runs of Aesir code (labelling row) | Whatever the inner binary shows — results are proot/Termux results only | Same ELF inspection as the Linux-host row, with the environment recorded in the artifact header | Same as Linux-host row | `evidence/integration/l5-proot-termux.txt` | Not run for this slice; any future result here is never Android app proof |

**Observed on Mimir (2026-11-06)** — commands re-run for this slice:

```text
$ ls ~/aesir-build | head
a143gz
a165parts
a172parts
arms141.py
arms142.py
arms143.py
arms143.py.gz
arms144parts
arms144.py
arms144.py.gz
... (further arms*.py / arms*.py.gz files and *parts directories follow the same pattern)
$ find ~ -maxdepth 3 -iname '*aesir*'    (abridged — documents and source trees)
~/c2/PROJECT_AESIR_*.md            (multiple status/roadmap documents)
~/c2/aesir_engine/aesir_main
~/c2/aesir_engine/aesir.mojo
~/c2/aesir_main
~/c9base/aesir_engine/aesir_main
~/c9base/aesir_engine/aesir.mojo
~/c9base/aesir_main
$ file ~/c9base/aesir_main
/home/pi/c9base/aesir_main: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
  dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2,
  BuildID[sha1]=67fa762ea82f125684f3b041fd8fdec46de39eb6, for GNU/Linux 3.2.0, not stripped
$ echo $ANDROID_NDK_HOME
[]                                # empty — no NDK configured on the Pi
```

Reading: `~/aesir-build` is a working tree of Python files (`arms*.py`,
some gzipped, with part directories) — source-level material with no ELF
linkage to inspect. Aesir-named *binaries* do exist elsewhere on the Pi
(`aesir_main` under `~/c2` and `~/c9base`, beside `aesir.mojo` sources),
but the one inspected is an x86-64 ELF, i.e. built for a different host;
no Aesir ELF for AArch64 was found near the home top level, and no Aesir
ELF of any architecture was observed running in a process on Mimir. For
the Android row there is nothing at all: no NDK is configured on the Pi
(`ANDROID_NDK_HOME` empty, and the §5/§9 baseline found no NDK tools), and
no Android device process has been captured. The Android procedure, when
its turn comes, requires the NDK `llvm-*` inspection tools and explicit
recording of the linker namespace and app-sandbox restrictions for the
exact process inspected; a proot or Termux run, however it behaves, is a
proot/Termux result and is never Android app proof.

**Status today: Blocked/Unverified** — Aesir material on Mimir is
Python/source-level; no Aesir ELF (ARM) or Android process captured;
Linux-host and Android rows kept separate throughout.

**What would change our mind:** for the Linux-host row, an Aesir engine
built on/for AArch64 Linux whose ELF (path + sha256 + BuildID) is captured
in a running host process and symbol-inspected; for the Android row, an
Aesir-bearing Android process inspected with NDK `llvm-*` tools with its
namespace/sandbox recorded. Either alone changes only its own row.

## 10. Substitution-point analysis framework

This section is the decision framework for roadmap §3.3. It does not
assert any finding about the real Mojo failure: there is no A-005
failure packet yet, so every Result cell in this section reads
UNDETERMINED until §12 Step 0 passes.

### 10.1 Layers, consumers and substitution points

Roadmap §3.3 names five independent allocator consumers. This document
numbers them L1..L5 and gives each exactly one substitution point,
S1..S5, with the same number:

| Layer | §3.3 consumer | Substitution point |
|---|---|---|
| L1 | The build tools themselves: Bazel, compiler, linker, or dependencies | S1 |
| L2 | The Mojo compiler executable: a prebuilt compiler or a compiler being built from source | S2 |
| L3 | The Mojo standard/runtime libraries linked into generated applications | S3 |
| L4 | The generated Mojo executable and its application-level allocator calls | S4 |
| L5 | Aesir-specific allocation within a host or Android process | S5 |

For each layer the §12 procedure records one linkage finding, using
only this vocabulary from §3.3: **Statically linked**, **Dynamically
linked**, **Explicitly called**, **Interposed**, **Absent**, or
**Undetermined** (the pre-packet state; not a §3.3 finding).

### 10.2 Mechanisms M1..M5

| ID | Mechanism | Layer scope |
|---|---|---|
| M1 | LD_PRELOAD preload (dynamic interposition at process start) | Any dynamically linked layer; chiefly S1, S2, S4, S5 (Linux host) |
| M2 | DT_NEEDED dependency replacement / rpath (DT_RPATH, DT_RUNPATH, link-order or sysroot substitution of the allocator library) | Dynamically linked layers S1..S5 where the allocator arrives as a shared-library dependency |
| M3 | Link-time selection, including Bazel `malloc` attribute / `linkopts`, `-l` selection, `--wrap`, and static archive choice at (re)link time | Layers that can be relinked: S1, S3, S4; S2 only when the compiler itself is built from source |
| M4 | Explicit API adoption: the consumer is changed (source or configuration) to call a named allocator API directly | Any layer whose source/configuration is controlled by this project: chiefly S3, S4, S5 |
| M5 | Android-only routes: NDK link selection, app-process / zygote constraints, bionic allocator behaviour. Kept separate from M1..M4 and never inferred from Linux-host results | S5 (Android), and S2/S4 only for an Android-target build |

### 10.3 Per-layer mechanism table

Result is UNDETERMINED for every row today: no packet, no binding
audit, no decisive test has been run for the real failure.

| Mechanism | Takes effect when | Cannot take effect when | Decisive test | Result |
|---|---|---|---|---|
| M1 LD_PRELOAD preload | The failing process is dynamically linked, runs without secure execution, resolves its allocation symbols through the dynamic linker, and the preload library is ABI-compatible with the process | Statically linked executable; secure execution (AT_SECURE / setuid / setgid / file capabilities — the loader ignores or restricts LD_PRELOAD); the allocator symbols are private/hidden or bound locally (-Bsymbolic); the consumer uses an internal allocator or direct mmap and never calls the interposed API; Android app processes (see M5) | Run the failing process with the comparator allocator preloaded; LD_DEBUG=bindings output plus /proc/<pid>/maps must show the comparator loaded, a binding audit / allocation counter must prove the comparator serviced the failing process's allocations, and the failure signature must change | UNDETERMINED (Blocked — no packet) |
| M2 DT_NEEDED dependency replacement / rpath | The failing binary or library carries a DT_NEEDED entry (or transitive dependency) for the allocator library, and search order (DT_RPATH / DT_RUNPATH / LD_LIBRARY_PATH / cache / default paths) can be made to resolve a compatible replacement first | Statically linked executable (no DT_NEEDED at all); the allocator is linked as a static archive into the binary even though the binary is otherwise dynamic; secure execution (secure mode ignores user-controlled search paths); version-script / symbol-version mismatch makes the replacement unloadable; Android app processes, whose library search is fixed by the app namespace (see M5) | readelf -d shows the DT_NEEDED, DT_RPATH and DT_RUNPATH entries; the resolved path is proven (host ldd or loader debug output); the same binding-audit + failure-signature-change test as M1 then decides | UNDETERMINED (Blocked — no packet) |
| M3 Link-time selection (incl. Bazel malloc attribute / linkopts) | The layer is (re)built by us from source or by a build rule we control: a Bazel target accepts a `malloc` attribute or `linkopts`, or a link line we control can select, wrap, or omit an allocator archive/library | The binary is prebuilt and not rebuildable by us (a prebuilt compiler usually is not); no build system is present to express the selection (Bazel is absent on Mimir today); the failing layer is a third-party binary or library consumed as-is; static-vs-dynamic choice was already frozen inside a shipped artifact | Inspect the link line / link map (or `bazel aquery` output where Bazel exists) showing the allocator selection taking effect; rebuild; rerun the failing command and apply the binding-audit + failure-signature-change test | UNDETERMINED (Blocked — no packet) |
| M4 Explicit API adoption | The consumer's source or supported configuration is controlled by this project and can be changed to call the replacement allocator's API (or a namespaced API such as the §6/§7 candidate surface) at the allocation call sites | The consumer is a closed prebuilt binary with no supported allocator-selection configuration; the allocation happens inside a third-party library that exposes no allocator hook; adoption would create mixed ownership with a non-adopting layer (see §10.4, §6.2 rule); Android app code cannot adopt for allocations made by the runtime/zygote before app code runs | Source/configuration diff identifies every adopted call site; symbol pass shows the explicit symbols referenced; runtime counter/audit proves the adopted allocator services the allocations; failure signature changes on rerun | UNDETERMINED (Blocked — no packet) |
| M5 Android-only routes (kept separate) | An actual Android target exists: NDK-built native code, an Aesir host or Android process, inspected with NDK llvm-* tools, and tested in the real Android environment (app sandbox or native process, as applicable) | Installed Android app processes: zygote-forked, per-app mount/library namespaces and SELinux policy mean there is no LD_PRELOAD route for installed apps; allocations made by the zygote/runtime before app code loads cannot be substituted by the app; no Android NDK is present on Mimir today, so no Android finding can be produced on this device at all | NDK llvm-readelf / llvm-nm / llvm-objdump inspection of the Android binary plus a native test run in the actual Android environment showing the selected allocator servicing the process's allocations and the failure signature changing there — a Linux-host result is NOT this test | UNDETERMINED (Blocked — no packet, no NDK on Mimir) |

### 10.4 Cannot-take-effect conditions (consolidated)

These are the §3.3 limits, stated once so §12 can cite them by number.
Each one defeats the mechanisms listed, at any layer:

| # | Condition (§3.3) | Defeats | Notes |
|---|---|---|---|
| C1 | Statically linked executable | M1, M2 | No dynamic linker runs; only M3/M4 remain, and only if the layer is rebuildable by us (§10.6 rule 3) |
| C2 | Private/hidden symbols, or symbols bound locally with -Bsymbolic | M1 (and M2 for the affected symbols) | The call never reaches the dynamic symbol table the substitution relies on |
| C3 | Secure execution: AT_SECURE, setuid/setgid, file capabilities | M1; M2 user-controlled search paths | The loader deliberately ignores user-supplied preloads and paths |
| C4 | Internal allocators / direct mmap bypass | M1, M2, M4 (at those call sites) | The consumer manages its own memory and never calls the substituted API |
| C5 | Android app processes: zygote namespaces, SELinux, no LD_PRELOAD for installed apps | M1; M2/M4 for runtime-owned allocations | Android is handled only through M5, as a separate port |
| C6 | Cross-allocator ownership (§6.2): a pointer freed by a different allocator than the one that created it | Partial-layer substitution itself | §6.2: "A pointer must be freed by an allocator compatible with the one that created it." Substituting only some of L3/L4/L5 so that allocation and free land in different allocators is a DEFECT, not a partial success (see §10.6 rule 5) |

A further §3.3 caution applies to every mechanism: a library with the
same function names alone is not proof it is used. Same names present
is NOT proof of substitution; only the decisive test in §10.3 is.

### 10.5 Layer × mechanism applicability (pre-packet)

Legend: `?` = undetermined until the §12 passes run; `—` = mechanism
does not apply to that layer by construction.

| Layer | M1 | M2 | M3 | M4 | M5 |
|---|---|---|---|---|---|
| L1 build tools (S1) | ? | ? | ? | ? | — |
| L2 Mojo compiler (S2) | ? | ? | ? (source build only) | ? | ? (Android-target build only) |
| L3 Mojo runtime libraries (S3) | ? | ? | ? | ? | — |
| L4 generated executable (S4) | ? | ? | ? | ? | ? (Android-target build only) |
| L5 Aesir host / Android (S5) | ? (Linux host) | ? | ? | ? | ? (Android) |

### 10.6 Decision rule

Apply in order, per failing layer. Stop at the first rule that
decides the layer.

1. No packet ⇒ UNDETERMINED (Blocked), stop. If no real A-005 failure
   packet has passed the §12 Step 0 gate, no substitution judgement
   is made for any layer.
2. Absent ⇒ no effect. If the linkage finding for the layer is
   Absent, substitution cannot change the failure at that layer: the
   allocator under investigation is not in that layer's allocation
   path.
3. Statically linked ⇒ M1/M2 not viable. If the linkage finding is
   Statically linked, M1 and M2 are not viable at that layer; M3/M4
   are considered only if the layer is rebuildable by us — a prebuilt
   compiler usually is not.
4. Dynamic/Explicit ⇒ decisive test or nothing. If the linkage
   finding is Dynamically linked, Explicitly called, or Interposed,
   substitution at that layer is viable only if a comparator-allocator
   decisive test passes: a binding audit proving the comparator
   serviced the failing process's allocations, plus the failure
   signature changing under the comparator. Same names present is
   NOT proof.
5. Ownership-mixing check. Before any VIABLE verdict, check
   allocation/free ownership across L3, L4 and L5 against the §6.2
   rule (C6 above). If the candidate substitution mixes owners, the
   verdict is downgraded: VIABLE → VIABLE WITH OWNERSHIP DEFECT,
   which fails the gate.
6. Close against the gate. The analysis closes only when the §3.3
   exit gate is met, verbatim: 'someone else can reproduce the
   failure and can explain why the selected integration point can or
   cannot change it'. If substitution is not viable at the selected
   point, the fallback applies verbatim: If replacing tcmalloc is not viable at that point, continue MimirAlloc as an
   independent project but do not claim it fixes Mojo.

Verdict vocabulary for this framework: VIABLE / VIABLE WITH OWNERSHIP
DEFECT (fails gate) / NOT VIABLE AT THIS POINT / UNDETERMINED
(Blocked).


## 11. Linkage diagram

Pre-packet diagram. Every edge that the §12 passes have not yet
measured is marked `?`. F marks the failure point, which is unknown
because there is no packet. Allocator provider states are shown in
brackets exactly as known on Mimir today.

```text
                         SUBSTITUTION POINTS S1..S5
                         (one per layer L1..L5)

  +------------------- L1 BUILD TOOLS --------------------+
  |  bazel (absent on Mimir today) / compiler driver /    |
  |  linker / build dependencies                          |
  +------------------------+------------------------------+
                           |  invokes / links with   [S1 ?]
                           v
  +------------------- L2 COMPILER -----------------------+
  |  Mojo compiler executable                             |
  |  (prebuilt pixi/conda package on Mimir; see A7-7:     |
  |   PATH wrapper + two different ELFs disagree)         |
  +------------------------+------------------------------+
                           |  links / embeds       [S2 ?]
                           v
  +------------------- L3 LIBRARIES ----------------------+
  |  Mojo standard / runtime libraries linked into        |
  |  generated applications                               |
  +------------------------+------------------------------+
                           |  linked into          [S3 ?]
                           v
  +------------------- L4 GENERATED EXECUTABLE -----------+
  |  generated Mojo executable + its application-level    |
  |  allocator calls                                      |
  +------------------------+------------------------------+
                           |  hosted by / run under [S4 ?]
                           v
  +------------------- L5 HOST ----------------------------+
  |  Aesir host process (Linux)  /  Android process       |
  |  (Android = separate port, M5 only)          [S5 ?]   |
  +-------------------------------------------------------+

  ALLOCATOR PROVIDERS (state on Mimir today)
    [glibc malloc: observed provider for installed toolchain]
    [tcmalloc: NOT PRESENT on Mimir]
    [MimirAlloc: DOES NOT EXIST YET]

  EDGES
    L1 -> L2 -> L3 -> L4 -> L5 : consumer chain, all edges `?`
      (linkage finding per layer is UNDETERMINED until §12 runs)
    S1..S5 : candidate substitution points, all UNDETERMINED

  F = failure point (unknown — no packet)
      F is NOT placed on any layer: placing it without an A-005
      packet would be inventing the blocker §3.3 exists to find.
```

## 12. Procedure sequence

Run in order. Each step states input → action → output. No step may
be skipped silently: a step that cannot run records why, in its
output, and the run's status becomes Blocked at that point.

**Step 0 — Gate check.**
Input: a candidate A-005 failure packet directory
(`evidence/runs/<run_id>/`).
Action: run `sha256sum -c SHA256SUMS` inside the packet and the
A-005 harness `verify` subcommand; read `manifest.json` and require:
`claim_scope` is NOT `harness-evidence-only`; a `classification`
value is present (not left as the capture-time default with no
classification work recorded); the case binaries named by the packet
are present and hashable.
Output: PASS (proceed to Step 1) or Blocked — record Blocked in the
ledgers with the failed check named, and stop.
Note: the synthetic smoke packet `20261106T114752Z-smoke-fail-0f4a3c`
fails this gate by design — it is harness evidence for a synthetic
case (`failing_process` = `other`, `stage` = `other`), not the Mojo
failure, and must never be mapped as if it were.

**Step 1 — Environment manifest.**
Input: the passed packet (Step 0) and the live Mimir host.
Action: record the §3.1 environment manifest: repository, commit or
release, branch, build instructions, clean build directory, exact
command; CPU architecture and ABI (`aarch64` vs Android `arm64-v8a`),
OS, libc, toolchain, linker, Bazel and NDK versions (recording
"absent" where absent); environment class (ordinary ARM64 Linux,
proot, Termux, Android app process, cross-build host); page size
(`getconf PAGESIZE`); the exact tcmalloc revision, build options and
local modifications if any (today: tcmalloc is NOT PRESENT on Mimir).
Output: environment manifest section of the mapping record.

**Step 2 — Name the failing process/stage; layer concordance.**
Input: packet fields `failing_process` and `stage` (A-005 case
vocabulary, commit c4298b3) plus the §3.2 classification.
Action: map `failing_process` to a layer with this concordance table
(a workaround, not a resolution — see A7-6), and sanity-check `stage`
against the layer (`fetch`, `compile`, `link`, `tool-startup`,
`mojo-compile`, `generated-startup`, `runtime`, `other`):

| `failing_process` (A-005) | Layer (§3.3) |
|---|---|
| `build-tool` | L1 |
| `mojo-compiler` | L2 |
| `mojo-runtime` | L3 libraries |
| `generated-binary` | L4 |
| `aesir` | L5 |
| `other` | unmapped — escalate to §3.2 classification before any layer work |

Output: named failing process, stage, primary layer, and any
secondary layers to inspect; an `other` value stops layer work until
§3.2 classification assigns it.

**Step 3 — Inventory with wrapper resolution and SHA-256.**
Input: every executable/library named by the packet and the
concordance (Step 2).
Action: resolve each name through PATH and any wrapper scripts to
the real file (record the full chain: wrapper → target); record
exact absolute path, file type, size, and SHA-256 (`sha256sum`) for
each artifact; record BuildID where present (see A7-7: exact
path + BuildID pinning is required, never a bare command name).
Output: inventory table (path, wrapper chain, SHA-256, BuildID) —
labelled inventory evidence only; an inventory fact is not a linkage
finding.

**Step 4 — ELF pass.**
Input: inventoried artifacts (Step 3).
Action: `readelf -h -l -d -sW` per artifact (Appendix B): ELF type,
machine, interpreter (PT_INTERP), DT_NEEDED / DT_RPATH / DT_RUNPATH,
and whether the file is statically linked; repeat for each library
in the dependency closure that can carry allocation symbols.
Output: per-layer raw ELF evidence; preliminary Static / Dynamic
classification per layer, still Proposed until Step 8.

**Step 5 — Symbol pass.**
Input: artifacts + ELF evidence (Step 4).
Action: collect exported symbols (`nm -D --defined-only`,
`objdump -T`) and unresolved symbols (`nm -u`) for each artifact;
check the allocation watch-list (the §6.1 candidate surface:
`malloc`, `free`, `calloc`, `realloc`, `aligned_alloc`,
`posix_memalign`, `reallocarray`, `malloc_usable_size`, and C++
`operator new`/`delete` variants where present); record symbol
visibility (default vs hidden/protected) and any -Bsymbolic binding;
write each observation as a row destined for
`evidence/symbol-contract.md` (location dispute recorded as A7-1 /
A7-9: rows are prepared in that format wherever the file is finally
housed).
Output: `evidence/symbol-contract.md` rows (exported + unresolved,
per artifact, with visibility).

**Step 6 — Build-system pass.**
Input: the build that produced the failing layer (Step 2).
Action: capture the exact link line and link map (`-Wl,-Map`) for
the failing target; where Bazel is the build system, capture
`bazel query` (target graph / `malloc` attribute) and
`bazel aquery` (the executed link action) output. Record honestly:
Bazel is absent on Mimir today, and the Mojo on Mimir is a prebuilt
pixi/conda package, so for L1/L2 this pass normally records "no
build system available to query on this device" rather than query
output (see A7-4).
Output: link line / map / query evidence, or an explicit
not-available record per layer.

**Step 7 — Android pass.**
Input: any layer claimed to be an Android target (L5, or an
Android-target L2/L4 build).
Action: inspect only with NDK tools — `llvm-readelf`, `llvm-nm`,
`llvm-objdump` — never host binutils output presented as Android
evidence; record API level, ABI (`arm64-v8a`), page-size policy, and
the namespace / sandbox facts of §10.4 C5. If no NDK is present
(today: no NDK on Mimir), record the pass as not runnable on this
device and keep every Android claim Blocked.
Output: Android evidence section, or a Blocked record for Android.

**Step 8 — Runtime binding pass.**
Input: the failing command (packet) and a runnable host.
Action: run the failing process under loader diagnostics
(`LD_DEBUG=libs,bindings`), capture `/proc/<pid>/maps` while it
runs, and record which library actually binds each watch-list
symbol at runtime; where a comparator allocator is trialled, add its
allocation counter / audit hook so servicing is proven, not inferred
from names.
Output: runtime binding evidence per layer; this is the evidence
that promotes a Step 4 preliminary classification to a linkage
finding (Static / Dynamic / Explicit / Interposed / Absent).

**Step 9 — Fill layer tables, summary and diagram.**
Input: outputs of Steps 3–8.
Action: fill one layer table per consumer L1..L5 (linkage finding +
forcing evidence per layer), the all-layers summary table, and
update the §11 linkage diagram (replace `?` only where Step 8
evidence forces it; place F only on the layer the packet and binding
evidence name).
Output: completed layer tables + summary + diagram for this run.

**Step 10 — Substitution analysis.**
Input: completed layer tables (Step 9).
Action: apply the §10 framework to each failing layer: mechanism
table rows, cannot-effect conditions C1..C6, and the §10.6 decision
rule in order, including the ownership-mixing check across L3/L4/L5
and, for any viable candidate, the comparator-allocator decisive
test.
Output: per-layer verdict (VIABLE / VIABLE WITH OWNERSHIP DEFECT /
NOT VIABLE AT THIS POINT / UNDETERMINED) with the rule number that
decided it.

**Step 11 — One-page conclusion.**
Input: Steps 9–10 outputs.
Action: complete the §13 template, one page, every field filled or
explicitly marked UNDETERMINED with its resumption point.
Output: the run's one-page conclusion.

**Step 12 — Ledger close-out.**
Input: all outputs above, including Blocked records.
Action: append (append-only, per the A-002 ledger rules) an
EVIDENCE_LEDGER entry naming the packet run_id, every evidence
artifact produced and its location, and a DECISION_LEDGER entry
naming the verdict(s), the deciding rule(s), and the next slice
proposed. Never edit or remove earlier entries; corrections are new
entries that cite the old ones.
Output: ledger entries written; the mapping run is closed.


## 13. One-page conclusion (template)

Fixed fields. One page maximum when completed for a real run.
Prefilled below with today's honest state: every field UNDETERMINED —
no packet, no blocker named yet.

| Field | Value |
|---|---|
| Exact blocker (one sentence) | UNDETERMINED — no A-005 packet has passed the §12 Step 0 gate, so no blocker has been named yet |
| Failing layer(s) | UNDETERMINED |
| Reproduction pointer (packet run_id + SHA) | UNDETERMINED — no packet exists; the synthetic smoke packet `20261106T114752Z-smoke-fail-0f4a3c` is NOT this pointer (it fails Step 0 by design) |
| Linkage finding per failing layer + forcing evidence | UNDETERMINED — no layer has a finding; no binding evidence exists |
| Candidate substitution points ranked | UNDETERMINED — ranking before a linkage finding would be guessing |
| Why the selected point can/cannot change the failure (≤10 lines) | UNDETERMINED — no point is selected. Selection waits for: packet → Step 2 layer → Steps 4/8 finding → §10.6 rule. |
| Ownership-mixing result | UNDETERMINED — the §6.2 cross-layer check (§10.6 rule 5) has not run |
| Verdict | UNDETERMINED (Blocked). Resumption point: a real A-005 packet for the actual Mojo/tcmalloc failure passing §12 Step 0 |
| Next slice proposed | UNDETERMINED — to be named in the DECISION_LEDGER entry of the first run that passes Step 0 |

## 14. Inconsistencies recorded

Numbered A7-1..A7-10. Each entry states both sides and the resolution
owner. Recording an inconsistency here is not resolving it; §12 is
written to work around them without silently choosing a side.

**A7-1 — Deliverable locations: `evidence/` vs `docs/`.**
Side A: §3.3 lists its deliverables under `evidence/`
(`evidence/mojo-arm-repro.md`, `evidence/symbol-contract.md`).
Side B: the Wave-1 precedent houses documents under `docs/` (this
document's line, A-001..A-006), the §13 repository layout shows both
`docs/` and a top-level `evidence/` directory, and the actual main
tree has no `evidence/` directory (the A-002 N-5/N-6 notes on this
dispute are themselves unresolved). This slice delivers under
`docs/`; `evidence/integration/` is reserved for the raw artifacts
(§12 outputs) when a real run produces them.
Resolution owner: roadmap owner, on the A-002 N-5/N-6 ruling.

**A7-2 — The repro deliverable is listed under the wrong section and
does not exist.**
Side A: `evidence/mojo-arm-repro.md` is a §3.1 "Capture the failure"
deliverable. Side B: §3.3 lists it among its own deliverables, and
§3.3's exit gate presupposes reproduction ("someone else can
reproduce the failure"), yet no such file exists and reproduction is
A-005's line, not A-007's. §12 Step 0 therefore gates on an A-005
packet, not on the missing markdown file.
Resolution owner: A-005 line owner, with the roadmap owner for the
§3.1/§3.3 listing.

**A7-3 — Three unreconciled Phase-1 exit gates.**
The roadmap states three different bars: (a) §3.3's exit gate —
reproduction plus an explainable integration point, with the
independent-project fallback; (b) §8 Phase 1's exit — "documented
causal hypothesis and a repeatable validation command; include
conflicting evidence and exact unknowns"; (c) §17 "Initial definition
of done" items 1 + 5 — the real issue has "a reproducible, documented
cause" with the integration boundary mapped, and the original failure
is "either demonstrably fixed with verified MimirAlloc bindings or
accurately marked blocked with evidence". No text reconciles them;
this document closes against (a) only (§10.6 rule 6).
Resolution owner: roadmap owner.

**A7-4 — §3.3 presumes a Bazel/source-build consumer; Mimir has
neither.**
Side A: §3.3 directs review via "the build system's query/aquery
facilities" and frames L2 as "a prebuilt compiler or a compiler
being built from source". Side B: Mimir has no Bazel binary today,
and the Mojo on Mimir is a prebuilt pixi/conda package — there is no
source build to query. §12 Step 6 records not-available instead of
fabricating query output.
Resolution owner: roadmap owner (scope of L1/L2 on this device).

**A7-5 — §3.3 presumes Android NDK inspection; there is no NDK here.**
Side A: §3.3 says "On Android, use NDK-compatible tools and inspect
namespace and app sandbox restrictions". Side B: no Android NDK is
installed on Mimir, and per §5 / the A-003 target work Android is a
separate port with its own support claim, not a host inspection.
§12 Step 7 keeps all Android findings Blocked on this device.
Resolution owner: A-003 line owner, with the roadmap owner.

**A7-6 — Layer taxonomy mismatch: §3.3 layers vs A-005 enum.**
Side A: §3.3 has five layers (L1..L5 in this document), with Mojo
standard/runtime libraries as their own layer L3. Side B: A-005's
`failing_process` enum (commit c4298b3) has six values — `build-tool`,
`mojo-compiler`, `mojo-runtime`, `generated-binary`, `aesir`,
`other` — in which `mojo-runtime` folds L3 into a process name and
`other` has no layer at all. The §12 Step 2 concordance table is a
workaround, not a resolution: neither taxonomy has been amended.
Resolution owner: A-005 line owner and roadmap owner, jointly.

**A7-7 — "The Mojo executable" is ambiguous on Mimir.**
One name resolves to three different artifacts: the PATH entry is a
99-byte bash wrapper script (`/usr/local/bin/mojo`), while the pixi
installation holds two different ELFs — a statically linked,
stripped executable (`~/.pixi/bin/mojo`) and a dynamically linked
environment compiler (under the pixi env) — which give opposite
linkage classifications (Static vs Dynamic, hence opposite §10.6
rule-3 outcomes). Any L2 finding stated for "mojo" without an exact
absolute path and BuildID is meaningless; §12 Step 3 therefore pins
path + SHA-256 + BuildID for every artifact.
Resolution owner: A-007 (procedure, applied in §12); underlying
installation layout is the device owner's.

**A7-8 — "A one-page conclusion" has no filename.**
§3.3 requires "a one-page conclusion naming the exact blocker and
candidate substitution points" but names no file for it, unlike the
two `evidence/` deliverables it does name. It is housed as §13 of
this document pending a ruling; a real run's completed conclusion
may still need its own file.
Resolution owner: roadmap owner.

**A7-9 — One symbol contract or two?**
Side A: §3.3's deliverable `evidence/symbol-contract.md` is an
evidence artifact: the observed exported/unresolved symbols of the
consumers (§12 Step 5). Side B: §6 is "Contract-first API and ABI
work" (the A-008 line): a designed contract for what MimirAlloc
will provide. The roadmap never states whether these are one
contract seen from two sides or two separate documents, and the
shared name invites conflation. This document treats them as
separate until ruled otherwise: observed (A-007) vs designed (A-008).
Resolution owner: A-008 line owner, with the roadmap owner.

**A7-10 — Cross-references point at commits, not in-tree files.**
A-002 (ledgers), A-003 (targets) and A-005 (failure capture) exist
only on their team branches; none of them is present in team-07's
base tree. Every reference in this document to their rules,
vocabularies or gates therefore cites a commit (for A-005:
c4298b3) rather than a file a reader of this tree can open. If a
cited branch changes, the citation — not the tree — is the record.
Resolution owner: Wave-1 integration owner (merge order for the
team branches).

## 15. Acceptance checklist

Each §3.3 requirement, the section of this document (with Parts A/B
of INTEGRATION_LAYER_MAP.md) that carries it, and the pass condition.
A box may be checked only when the pass condition is met; mapping
claims stay at Proposed/Blocked until §12 evidence promotes them.

| ✓ | §3.3 requirement | Carried in | Pass condition |
|---|---|---|---|
| ☐ | All 5 consumers identified | §10.1, §12 Step 2, layer sections for L1..L5 | Each of L1..L5 has its own section/table naming its consumer, substitution point and linkage finding vocabulary |
| ☐ | Linkage classification per consumer: statically linked, dynamically linked, explicitly called, interposed, or absent | §10.1, §12 Steps 4 + 8 | All 5 linkage findings are defined, and the procedure states which evidence (Steps 4/8) is required before one may be used for a real layer |
| ☐ | Exact exported and unresolved symbols reviewed | §12 Step 5, Appendix B | Procedure specifies exported (`nm -D --defined-only`, `objdump -T`) and unresolved (`nm -u`) collection per artifact, with the §6.1 watch-list and visibility recorded |
| ☐ | Inspection via readelf / nm / objdump, compiler link maps, build-system query/aquery | §12 Steps 4–6, Appendix B | readelf/nm/objdump passes, `-Wl,-Map` link maps, and `bazel query` / `bazel aquery` forms are all specified, with Bazel's absence on Mimir recorded rather than papered over |
| ☐ | Android: NDK-compatible tools, namespace and app sandbox restrictions | §10.2 M5, §10.4 C5, §12 Step 7 | NDK llvm-* tooling specified; zygote namespaces, SELinux and the no-LD_PRELOAD rule for installed apps addressed; Android kept a separate port |
| ☐ | Dynamic preloading limits listed | §10.3 M1 row, §10.4 C1..C5 | Statically linked executable, private/hidden symbols / -Bsymbolic, secure execution (AT_SECURE/setuid), internal allocators / direct mmap bypass, and Android app processes are each listed as cannot-effect conditions |
| ☐ | Same names are not proof | §10.3, §10.4, §10.6 rule 4 | Stated explicitly: a library with the same function names alone is not proof it is used; only the binding-audit decisive test proves substitution |
| ☐ | Ownership rule honoured | §10.4 C6, §10.6 rule 5 | §6.2 cross-allocator rule stated; partial-layer substitution across L3/L4/L5 classified as a defect; VIABLE → VIABLE WITH OWNERSHIP DEFECT downgrade in the decision rule |
| ☐ | Deliverable: linkage diagram | §11 | Diagram present, showing L1→L5, S1..S5, the three allocator provider states as known on Mimir, `?` on undetermined edges, and F unplaced (unknown — no packet) |
| ☐ | Deliverable: one-page conclusion | §13 | Template present with all fixed fields; prefilled honestly as UNDETERMINED throughout; filename dispute recorded (A7-8), not silently relocated |
| ☐ | Deliverables: symbol-contract + repro locations | §12 Step 5, §14 A7-1/A7-2 | Locations resolved via §14 — recorded as disputed with an owner — not silently relocated; this slice delivers under `docs/`, `evidence/integration/` reserved for raw artifacts |
| ☐ | Exit gate + fallback sentence | §10.6 rule 6 | §3.3's gate sentence quoted verbatim and the fallback (continue MimirAlloc as an independent project, do not claim it fixes Mojo) present |
| ☐ | Evidence honesty | §10 header, §11 header, §13 | No label above Proposed/Blocked for any mapping claim; device-observed inventory facts labelled as inventory only, never as linkage findings |


## Appendix A. Symbol watch-list

Symbols to record — exported and unresolved, separately — for every binary examined under Section 3. Source: Roadmap §6.1.

| Group | Symbols |
|---|---|
| Core allocation | `malloc`, `free`, `calloc`, `realloc`, `aligned_alloc`, `posix_memalign`, `reallocarray` |
| Usable size | `malloc_usable_size` — only when required per target |
| C++ allocation | C++ `operator new` / `operator delete` variants: plain, array, nothrow, aligned, and sized — demangled via `nm -C` |
| tcmalloc-specific extensions | Observed in actual binaries on Mimir (Section 7): `TCMallocGlobals::tc_new` / `tc_delete` (UND in libKGENCompilerRTShared.so), weak UND `tcmalloc_internal::*` and defined `TCMallocInternal*` / `TcmallocSlab_Internal_PopBatch` / `PushBatch` in libAsyncRTRuntimeGlobals.so. Any further name enters this row only when observed in an actual binary symbol table — never invented. |

> Caution: glibc behaviour is not universal Android/POSIX behaviour. A symbol's presence, binding, or semantics observed against glibc on this device must not be stated as the behaviour on Android or on POSIX systems in general.

## Appendix B. Command cookbook

The exact command set used by §12. Host binutils forms are for
ARM64 Linux on Mimir; Android artifacts are inspected ONLY with the
NDK `llvm-*` forms at the end. `ldd` is host-only (see its warning).

| Command | Purpose (one line) |
|---|---|
| `file <path>` | Identify wrapper script vs ELF vs archive before any ELF tool runs (§12 Step 3) |
| `sha256sum <path>` | Pin every inventoried artifact by hash (§12 Step 3); also verifies packet files via the packet's own `SHA256SUMS` (§12 Step 0) |
| `getconf PAGESIZE` | Record the runtime page size for the §3.1 environment manifest (§12 Step 1) |
| `readelf -h <file>` | ELF header: type (EXEC/DYN), machine, entry — first static/dynamic signal (§12 Step 4) |
| `readelf -l <file>` | Program headers: PT_INTERP presence (dynamic) or absence (static), PT_LOAD layout (§12 Step 4) |
| `readelf -d <file>` | Dynamic section: DT_NEEDED, DT_RPATH, DT_RUNPATH, DT_SYMBOLIC — the M2 evidence (§12 Step 4) |
| `readelf -sW <file>` | Wide symbol table: binding, visibility (hidden/protected), defined vs UND (§12 Steps 4–5) |
| `readelf -n <file>` | Notes, including the GNU BuildID used to pin exact artifacts (A7-7, §12 Step 3) |
| `nm -D --defined-only <file>` | Exported (defined dynamic) symbols — the supply side of the symbol contract (§12 Step 5) |
| `nm -u <file>` | Unresolved (undefined) symbols — the demand side of the symbol contract (§12 Step 5) |
| `nm -C <file>` | Symbol table with C++ names demangled, for `operator new`/`delete` watch-list rows (§12 Step 5) |
| `objdump -T <file>` | Dynamic symbol table with version references, cross-checking `nm -D` (§12 Step 5) |
| `objdump -R <file>` | Dynamic relocation records: which symbols the loader must resolve at load time (§12 Step 5) |
| `objdump -p <file>` | Private headers: NEEDED entries and search-path fields in one view (§12 Step 4) |
| `ldd <file>` | Host-only: shows which libraries the host loader resolves — WARNING: never run on an untrusted or Android binary, and never present its output as Android or binding evidence (§12 Step 4, host artifacts only) |
| `-Wl,-Map=<name>.map` (at link time) | Produce the compiler link map showing archive/library selection and symbol placement (§12 Step 6) |
| `bazel query 'deps(<target>)'` | Bazel target graph for the failing target, where Bazel exists (absent on Mimir today) (§12 Step 6) |
| `bazel query 'attr(malloc, ., <target>)'` | Read the Bazel `malloc` attribute — the M3 link-time selection point (§12 Steps 6, §10.2 M3) |
| `bazel aquery 'mnemonic(CppLink, <target>)'` | The executed link action: the real link line Bazel ran, where Bazel exists (§12 Step 6) |
| `LD_DEBUG=libs <cmd>` | Loader diagnostics: which libraries were searched and loaded (§12 Step 8) |
| `LD_DEBUG=bindings <cmd>` | Loader diagnostics: which library each symbol actually bound to — core binding-audit evidence (§12 Step 8) |
| `cat /proc/<pid>/maps` | Live memory map of the running failing process: proves a preload/replacement library is actually mapped (§12 Step 8) |
| `llvm-readelf -h -l -d -sW <file>` | NDK form of the readelf pass for Android binaries (§12 Step 7) |
| `llvm-nm -D --defined-only <file>` / `llvm-nm -u <file>` | NDK form of the exported/unresolved symbol pass for Android binaries (§12 Step 7) |
| `llvm-objdump -T -R -p <file>` | NDK form of the dynamic-symbol / relocation / private-header pass for Android binaries (§12 Step 7) |

End of Part C. Labelling reminder: everything in sections 10–15 is
Proposed procedure and Blocked analysis until a real A-005 packet
passes §12 Step 0; Appendix B commands are specified, not yet run
against the real failure.
