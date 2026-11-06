# MimirAlloc System Allocator Evaluation

Slice: B-002 (Phase 2, first route). Companion baseline: B-01 in BASELINE_INVENTORY.md. Status: research / evaluation. Phase 2 is not exited by this
slice. Citations: Roadmap = MimirAlloc_Research_and_Development_Roadmap.md; Baseline = BASELINE_INVENTORY.md; Targets = SUPPORTED_TARGETS.md;
Integration = INTEGRATION_LAYER_MAP.md; Field NN = docs/field-notes/android-arm64-2026-11/. Pi-observed facts only in the probe section (Section 7).

**B-001 statement: B-001 (Phase 2 evaluation plan, team-01) has NOT banked on base 43ae545ed797d300cbd09239a7d123ee2efb644d. No B-001 document exists
in this tree, its fields were NOT used, and this document uses the Roadmap Sec. 8 Phase 2 exit-table fields directly. Reconcile when B-001 banks. The
keeper banks B-002 after B-001.**

## 1. Honest headline

No measured baseline existed before this slice. Baseline Sec. 1 is explicit: there is no measured baseline in this repository, and B-01, the platform
libc allocator, was a candidate only, with no version pin, build record, or run log. This slice creates the first one: a tiny Pi micro-probe proving
that glibc is selected on this Pi and that basic contract patterns pass against it.

That is all it proves. It proves nothing about Mojo, nothing about Aesir, and nothing about performance. Phase 1 remains open: there is no real
failure packet. A-005 is synthetic only, and Integration records every relevant verdict as UNDETERMINED. Verdict for this route: Blocked for Mojo;
Device-tested for the probe only.

## 2. What the route means

"System allocator" is per-libc, not singular. There is no one system allocator to evaluate; there is one per C library, and a result on one libc is
evidence about that libc only.

| Target | System allocator | Evidence state |
|---|---|---|
| Linux AArch64 + glibc | glibc in-libc allocator | Pi-pinned: glibc 2.36 |
| Linux x86-64 + glibc | glibc in-libc allocator | Unevidenced |
| Linux + musl | musl allocator | TBD |
| Android + bionic | bionic allocator | TBD; contracts U-04 sources missing |

Results do not transfer across libcs. A glibc pass does not predict a musl or bionic result, and a bionic failure would not indict glibc.

The route itself: remove, or never introduce, tcmalloc, so that allocations fall back to the libc allocator. It is a diagnostic comparator per Roadmap
Phase 2. It does not cancel MimirAlloc, replace it, or pre-empt it.

The hypothesis is falsifiable both ways. If the failure signature changes when the allocator changes, the failure is allocator-specific. If the
signature is unchanged, the allocator is not causal for that failure, and the Roadmap 3.3 fallback applies.

What this route cannot do:

- It cannot remove a static tcmalloc linked inside a prebuilt binary; there is nothing to fall back from without a rebuild.
- It cannot fix internal allocators (Integration C4).
- It cannot fix address-width, loader, or driver faults.
- It cannot interpose inside Android apps (Integration C5).

Field 03 failure, stated as hypothesis only: on an SM-X400 tablet, in a Termux glibc environment, Mojo 1.2.0 plus a nightly aborted before `--version`
inside TCMalloc `MmapAligned()`, which assumes a 48-bit virtual address space. Probes: 2^38 OK; 2^39 and 2^47 ENOMEM; a 39-bit boundary. That the
allocator caused the abort is a hypothesis, not a finding.

Two opposing Integration facts frame everything below:

- In the Mimir environment, `mojo` is dynamically linked: DT_NEEDED `libc.so.6`, with UND `malloc@GLIBC_2.17`. For that consumer the system-allocator
  route is already in effect.
- The pixi `mojo` is statically linked and stripped, and its runtime library defines a TCMalloc-derived surface in-library. That consumer is
  unreachable by this route without an owner rebuild.

## 3. Selection and configuration

### 3.1 Selection is no action

Selection means doing nothing special: the consumer is dynamically linked to libc, and the competing allocator is absent. Absent means all of the
following:

- No LD_PRELOAD allocator, and no `/etc/ld.so.preload` entry (absent on this Pi; see Section 7).
- No allocator DT_NEEDED: no `-ltcmalloc`, `-ljemalloc`, or `-lmimalloc` on the link line, and no Bazel `malloc` attribute selecting one.
- No static allocator archive linked in.

Proof, per Roadmap Sec. 11: `ldd`, `readelf -d`, `nm -D` / `nm -u`, and LD_DEBUG / maps evidence wherever interposition is claimed. A claim of
selection without that evidence is a name, not a result.

### 3.2 Configuration (Pi-verified, glibc 2.36)

glibc exposes configuration, but configuration is not selection. The knobs on this Pi, glibc 2.36:

- `GLIBC_TUNABLES`, with 13 `glibc.malloc.*` tunables: check, arena_max, arena_test, mmap_threshold, mmap_max, trim_threshold, top_pad, perturb,
  mxfast, tcache_max, tcache_count, tcache_unsorted_limit, hugetlb.
- Legacy `MALLOC_*` variables: `MALLOC_CHECK_`, `MALLOC_ARENA_MAX`, `MALLOC_PERTURB_`, `MALLOC_TRIM_THRESHOLD_`, `MALLOC_TOP_PAD_`, `MALLOC_TRACE`,
  and relatives.
- `mallopt(3)` and `mallinfo2(3)` in code.

Rules: the default run comes first and is the baseline row. Tuned runs are separate rows, never merged into the baseline. In the probe, tuned runs
with `MALLOC_CHECK_=3` and with `GLIBC_TUNABLES=glibc.malloc.arena_max=2` both exited 0 with identical usable sizes (Section 7).

Note: `/etc/malloc.conf` is NOT a glibc file. It belongs to jemalloc / BSD. Its absence on this Pi means nothing about glibc.

## 4. Licenses and linking

The project is MIT: LICENSE, copyright 2026 Volmarr Wyrd. There is no conflict in using the operating system libc. The Guide duties are only: preserve
MIT, and credit external material.

glibc, the library: LGPL-2.1-or-later. Pi-verified at `/usr/share/doc/libc6/copyright` and `/usr/share/common-licenses/LGPL-2.1`. The libc6 package
also ships GPL utilities and BSD / ISC portions; the library itself is the LGPL part.

Dynamic linking is the LGPL 6b shared-library case, on the standard reading: no application source disclosure is owed, and duties attach to
redistributing glibc itself, not to merely linking it.

Static glibc is different: LGPL 6a imposes a relink / source duty (or the 6c three-year written offer), and ld on this Pi warns verbatim that using
`getpwnam` and `gethostbyname` "in statically linked applications requires at runtime the shared libraries from the glibc version used for linking".
That is the NSS / dlopen exact-version trap: a static binary can still need the exact shared glibc at runtime. It is function-dependent; a trivial
static `main` links silently.

Clean-room rule D-001: never copy glibc source into MimirAlloc. Use it, link it, do not ingest it.

Bionic and musl: this document asserts NOTHING about their allocator licensing or internals. The sources are missing per contracts U-04. The research
library lists Scudo separately and never calls it bionic's allocator, so neither does this document.

## 5. Four modes for this route

Cross-reference: Integration modes M1-M5 and conditions C1-C6. Every Mojo verdict in this section is UNDETERMINED / Blocked; no mode below has been
exercised against a real Mojo consumer.

### 5.1 LD_PRELOAD (M1)

Paradoxical for this route. No separate system-allocator shared object exists: the allocator is inside `libc.so.6`, which is already a DT_NEEDED of
the consumer. Pi-verified: `LD_PRELOAD=libc.so.6` on the probe exited 0, `ldd` was unchanged, and it selected nothing new.

So for this route M1 means only two things: removal (`env -u LD_PRELOAD`, and stripping injected preloads, including a tablet inherited Android
preload per Field 04), and use as a null control. A forwarding shim would not be this route at all; it would be a new interposer, carrying the Sec.
6.3 risks.

Scope: dynamic, non-secure consumers only (C1-C3). No Android apps (C5). Evidence needed: preload list before and after, LD_DEBUG binding trace, maps,
and the failing command rerun under the cleaned environment.

### 5.2 Direct link (M2 + M3)

The natural mode. Relink the consumer omitting the competing allocator: no `-ltcmalloc` on the link line, Bazel `malloc=system` or the default, and
UND allocation symbols then resolve to `libc.so.6`.

Concrete Mojo instance: public Mojo source selects jemalloc for the Linux driver (Baseline B-04, Targets I-3). Removing that selection is a
direct-link change. Paradox: shipped wheels abort in TCMalloc, so fixing the source does not fix the wheel.

Applies to: L1 / L3 / L4 relinkable consumers, and L5, Aesir's own source, where it is most viable.

Caveats: explicit `tc_new` / `tc_delete` calls survive dropping `-ltcmalloc` and will fail to link or bind; C++ `new` / `delete` are accounted
separately. A cross-allocator free (C6) is a failed gate, not a warning. Evidence: before / after link line and map, a clean pinned rebuild,
DT_NEEDED, a symbol pass, a binding audit, and the failing command rerun.

### 5.3 Static replacement (M3, static form)

The term is ambiguous and is recorded as such. It can mean (a) an archive swap at relink time, or (b) wholesale substitution of a rebuilt binary. It
never means an in-place patch of a stripped static binary; there is no such operation here.

This is the only mode that could touch C1, the pixi `mojo`, and it is blocked today: there is no Mojo source or Bazel on Mimir, the static premise
itself is not proven (it rests on static / absent-of-symbols evidence only), and an in-library surface is not an archive swap. The static glibc
embedding costs are in Section 4; static is not the recommended default.

Highest evidence burden of any mode: pre / post sha256 and BuildID, link map, a symbol pass on an unstripped rebuild, runtime proof, and the original
command succeeding.

### 5.4 Namespaced API (M4)

The worst fit. The system allocator has NO namespaced API distinct from the standard C ABI: the Roadmap 6.1 surface plus the Sec. 3.2 configuration is
all there is.

For this route M4 means only: our own code (Aesir L5, glue, harnesses) calls the libc surface directly, or via a thin wrapper, instead of an
allocator-specific surface. It does NOT mean `mimir_malloc`; that is the MimirAlloc Sec. 7.1 API, and conflating the two is a recorded confusion. This
mode cannot fix a compiler startup abort, because the abort happens before any adopting code runs. Evidence: call-site diff, symbol pass, contract
tests, and a scope statement saying adopting-code-only.

### 5.5 Summary

| Mode | Mojo compiler / wheel | Generated L4 | Aesir L5 |
|---|---|---|---|
| LD_PRELOAD | Vacuous; fails static | Candidate if dynamic | Linux only |
| Direct link | Source only | Viable if controlled | Most viable |
| Static replacement | Only mode, but blocked | Same | Same |
| Namespaced API | Cannot fix abort | Partial | Aesir-own only |

## 6. Phase 6 gate, restated for this route

The gate is: the original failing command succeeds, with proof that the replacement serviced the allocations, or a blocker is recorded. A hello-world
does not close it.

For this route, proof is the Sec. 3.1 selection evidence plus a changed signature, on the exact pinned artifact, in the exact failing environment: the
tablet, not the Pi. A Pi pass is not tablet evidence.

Negative-result next step: if the failure reproduces on an already-glibc consumer, the allocator hypothesis is dead for that consumer. Record it and
move on; do not re-tune and retry the same claim.

## 7. Pi-observed facts (Team-02, 2026-11-06)

This is the ONLY measured section of this document. Everything above is document-derived or reasoned; only what follows was observed on the Pi by this
team.

### 7.1 Gate and manifest

Gate: `free -g` showed 15 total / 13 available; MemAvailable 14,475,712 kB. Samtal on :8787 returned HTTP 200 before and after the probe. The probe
ran in `/tmp`, single-threaded, bounded.

Manifest, verbatim values:

- uname: "Linux purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64 GNU/Linux"
- ldd: "ldd (Debian GLIBC 2.36-9+rpt2+deb12u14) 2.36"
- PAGESIZE: 16384
- gcc: "gcc (Debian 12.2.0-14+deb12u1) 12.2.0"
- MemTotal: 16598576 kB
- MemAvailable: 14459616 kB, at manifest time 2026-11-06T08:08:08-05:00
- Machine: Pi 5, 4 cores, Debian 12
- Environment: no LD_PRELOAD, MALLOC_*, or GLIBC_* set
- `/etc/ld.so.preload` and `/etc/malloc.conf`: absent

This matches the Baseline Sec. 5 manifest.

### 7.2 Probe

Source: Appendix A. 2470 bytes, sha256 2ed8ac86c03ac7561ff5656019d751fc6316039fab860677f8e7509a8392f788. Compile: `gcc -O2 -Wall -o
/tmp/mimiralloc_probe /tmp/mimiralloc_probe.c`, exit 0, no warnings.

Three fresh runs, identical stdout, exit 0. Stdout, verbatim:

```
probe start
small alloc ok usable_first=40 usable_last=40
small verify+free ok count=10000 size=32
large alloc ok usable_first=1064944
large verify+free ok count=10 size=1048576
realloc before usable=72 content=hello-realloc
realloc after usable=4104 content=hello-realloc
realloc ok
calloc usable=1032
calloc zero check ok count=256 size=4 total=1024
usable req=1 usable=24
usable req=16 usable=24
usable req=32 usable=40
usable req=64 usable=72
usable req=128 usable=136
usable req=1024 usable=1032
usable req=65536 usable=65544
probe success
```

Wall times: real 0m0.007s / 0m0.006s / 0m0.006s. These are explicitly NOT a benchmark: no Sec. 11 protocol, no comparator, and no performance claim is
made from them. The configuration runs (Sec. 3.2) both exited 0 with identical output.

Selection proof: `ldd` on the probe lists only linux-vdso, `libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6`, and `/lib/ld-linux-aarch64.so.1`. `nm -D`
on libc shows `malloc@@GLIBC_2.17` as T at 0x92240, `free` and `realloc` as T, and `calloc` and `malloc_usable_size` as W. Conclusion: Device-tested,
for the probe only.

Also observed: `libjemalloc.so.2` is installed on this Pi; no tcmalloc and no mimalloc are installed. Those are other routes' slices, not this one.

### 7.3 NOT run

No Mojo. No Aesir. No multithreaded run. No fragmentation or pressure test. No RSS / PSS or latency distribution. No tablet, Termux, proot, bionic, or
musl run. None of those results exist, and none are claimed.

## 8. Phase 2 exit table

Roadmap Sec. 8, Phase 2 exit, quoted verbatim: "table of options with commands, measured results and integration limitations. No claims based on names
alone." This slice contributes one row:

| Field | Value |
|---|---|
| Option / route | System allocator (fall back to libc) |
| Target + libc | Linux AArch64, glibc 2.36 (Pi) |
| Consumer / layer | Probe only; no Mojo consumer named |
| Modes | M1 removal / null control; M2 + M3 direct link; M3 static (blocked); M4 own-code only |
| Exact command | `gcc -O2 -Wall -o /tmp/mimiralloc_probe /tmp/mimiralloc_probe.c && /tmp/mimiralloc_probe` |
| Selection proof | ldd: libc.so.6 only; nm -D: malloc@@GLIBC_2.17 T 0x92240 |
| Measured result | 3/3 runs exit 0, identical stdout; no performance claim |
| Integration limitation | No real failure packet; static pixi mojo unreachable without owner rebuild |
| Evidence level | Device-tested (probe) / Blocked (Mojo) |
| Artifact / log | This document Sec. 7 + Appendix A; probe in /tmp only, no tracked harness (I-B002-7) |

## 9. Verdict and resumption

The baseline is now pinned and probed: B-01 has moved from candidate to a recorded Pi baseline (glibc 2.36, this manifest, this probe). The Mojo route
remains UNDETERMINED / Blocked.

Resumption requires: a real Phase 1 packet passing Integration Sec. 12 Step 0 (path, sha256, and BuildID pinned); per-layer ELF and binding passes;
and the exact failing command rerun with the system allocator proven selected. Until then, this route is evaluated, not applied.

## 10. Inconsistencies recorded

- I-B002-1: The singular phrase "system allocator" hides at least three distinct allocators, and glibc itself differs: 2.36 on this Pi vs 2.44 in the
  Termux field environment.
- I-B002-2: Page-size split: 16384 on this Pi vs 4096 on the tablet. Allocation behaviour compared across that split needs the difference stated,
  every time.
- I-B002-3: The jemalloc paradox, plus an omission: Roadmap Sec. 11 omits jemalloc (Targets J-3) even though jemalloc is the only comparator actually
  installed on this Pi. A scoping decision is owed; see Q10.
- I-B002-4: "Static replacement" is ambiguous between an archive swap at relink and wholesale substitution of a rebuilt binary (Sec. 5.3).
- I-B002-5: Bionic and musl are unevidenced; contracts U-04 sources are missing. Nothing here covers them.
- I-B002-6: The name "ptmalloc" appears in no Pi file and no repository document. It is a general-knowledge label only; cite the glibc manual before
  using it as a term.
- I-B002-7: There is no tracked harness (Baseline Sec. 4 item 3). The probe lives only in /tmp plus Appendix A, and the Sec. 13 layout is unsettled
  (N-5).
- I-B002-8: The libc6 copyright file header is stale: it says "version 2.23" against an installed 2.36. Pin versions from `ldd` and the changelog, not
  that header.
- I-B002-9: Sec. 11 comparator wording vs Phase 2 routes: are the Sec. 11 comparators flags / patch comparators, or interventions? Treated here as
  separate route slices, per the wave plan.

## Appendix A. Probe C source

Verbatim from `/tmp/mimiralloc_probe.c` on the Pi, sha256 2ed8ac86c03ac7561ff5656019d751fc6316039fab860677f8e7509a8392f788, 2470 bytes:

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
int main(void){
    printf("probe start\n");
    // small 32B x10000
    void *ptrs[10000];
    for(int i=0;i<10000;i++){
        ptrs[i]=malloc(32);
        if(!ptrs[i]){printf("FAIL small malloc %d\n",i); return 1;}
        memset(ptrs[i], 0xAB, 32);
    }
    printf("small alloc ok usable_first=%zu usable_last=%zu\n", malloc_usable_size(ptrs[0]), malloc_usable_size(ptrs[9999]));
    for(int i=0;i<10000;i++){
        unsigned char *p=(unsigned char*)ptrs[i];
        for(int j=0;j<32;j++) if(p[j]!=0xAB){printf("FAIL pattern small %d %d\n",i,j); return 1;}
    }
    for(int i=0;i<10000;i++) free(ptrs[i]);
    printf("small verify+free ok count=10000 size=32\n");
    // large 1MiB x10
    void *big[10];
    for(int i=0;i<10;i++){
        big[i]=malloc(1024*1024);
        if(!big[i]){printf("FAIL large malloc %d\n",i); return 1;}
        memset(big[i], 0xCD, 1024*1024);
    }
    printf("large alloc ok usable_first=%zu\n", malloc_usable_size(big[0]));
    for(int i=0;i<10;i++){
        unsigned char *p=(unsigned char*)big[i];
        if(p[0]!=0xCD || p[524288]!=0xCD || p[1048575]!=0xCD){printf("FAIL pattern large %d\n",i); return 1;}
    }
    for(int i=0;i<10;i++) free(big[i]);
    printf("large verify+free ok count=10 size=1048576\n");
    // realloc
    char *r=malloc(64);
    if(!r){printf("FAIL realloc init\n"); return 1;}
    strcpy(r,"hello-realloc");
    printf("realloc before usable=%zu content=%s\n", malloc_usable_size(r), r);
    r=realloc(r, 4096);
    if(!r){printf("FAIL realloc grow\n"); return 1;}
    printf("realloc after usable=%zu content=%s\n", malloc_usable_size(r), r);
    if(strcmp(r,"hello-realloc")!=0){printf("FAIL realloc content\n"); return 1;}
    memset(r+13, 0xEF, 100);
    free(r);
    printf("realloc ok\n");
    // calloc zero check
    unsigned char *c=calloc(256,4);
    if(!c){printf("FAIL calloc\n"); return 1;}
    printf("calloc usable=%zu\n", malloc_usable_size(c));
    for(int i=0;i<1024;i++) if(c[i]!=0){printf("FAIL calloc zero %d val=%d\n",i,c[i]); return 1;}
    printf("calloc zero check ok count=256 size=4 total=1024\n");
    free(c);
    // malloc_usable_size samples
    size_t reqs[]={1,16,32,64,128,1024,65536};
    for(int i=0;i<7;i++){ void *p=malloc(reqs[i]); printf("usable req=%zu usable=%zu\n", reqs[i], malloc_usable_size(p)); free(p); }
    printf("probe success\n");
    return 0;
}
```

## Appendix B. Acceptance mapping

Phase 2 exit (Roadmap Sec. 8): this slice contributes one evaluated route, with command, measured result, and integration limitation, in Sec. 8. It
does not exit Phase 2; the remaining routes are separate slices.

Sec. 11 principles: compare the platform libc allocator first (done, Sec. 7); selection proven with ldd, readelf, and nm, never by name (Sec. 3.1,
Sec. 7.2); no benchmark claim without the Sec. 11 protocol (Sec. 7.2 disclaimer).

Sec. 15 open questions: Q3 is touched by Sec. 5.3 (the static consumer cannot be re-routed without its owner); Q10 is raised by I-B002-3 (jemalloc is
the only comparator installed on the Pi, yet Sec. 11 omits it; a scoping decision is owed). Neither question is closed here.
