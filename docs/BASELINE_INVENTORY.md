# MimirAlloc Baseline Inventory

Slice: A-003 (Phase 0 exit evidence E4). This inventory completes the baseline part of the Phase 0
exit (Roadmap Sec. 8, Phase 0: exact baseline documented) and prepares
Phase 2 (Roadmap Sec. 8, Phase 2: evaluate lower-cost compatibility
routes against controlled diagnostic comparators). It lists every
baseline the repository documents mention, what artifacts or logs exist
in this repository for each, and what is still missing before Phase 2
evaluation can use them. Citations: Roadmap = the Research and
Development Roadmap; Guide = ENGINEERING_GUIDE.md; Research NN =
docs/research/; Field NN = docs/field-notes/android-arm64-2026-11/.
Pi-observed facts from this team are in Section 5 only, kept separate
from document-derived claims.

## 1. The honest headline

There is no measured baseline in this repository. No benchmark result,
raw benchmark log, CSV, harness, or script is tracked: this team
verified the tracked file list (95 files at base b0b8374) and the only files with
benchmark in the name are research documents (Research synthesis
10_EXPERIMENTS_AND_BENCHMARKS.md and llm_computers
07_VERIFICATION_AND_BENCHMARKS.md), which are plans, not results.
Research 12 states the same of its own recipes: none of these
experiments has been executed by the documentation authors. Every
baseline below is therefore a named candidate with, at most, partial
environment evidence - never a comparison MimirAlloc or Phase 2 can
quote numbers from today.

## 2. Baseline inventory

| ID | Baseline | Named where | Artifacts / logs in this repo | Measurements on record | Usable for Phase 2 today |
|---|---|---|---|---|---|
| B-01 | Platform libc allocator (glibc on Linux; bionic on Android; musl where applicable) | Roadmap Sec. 11 principles (compare platform libc allocator first); Phase 2 (system allocator route) | None. No version pin, build record, or run log for any libc allocator as a baseline. Field 04 records glibc 2.44 present in a Termux glibc userspace and tiny glibc programs running; that is environment inventory, not a baseline run. | None | No - candidate only. Needs the Section 4 items. |
| B-02 | TCMalloc, upstream as documented | Roadmap Sec. 2 and sources S1, S2, S8; Research 11 | None in repo. The repo cites public TCMalloc platform and design documentation (Linux AArch64 listed as supported, S1) but holds no TCMalloc build, log, or result. | None | No - candidate only. |
| B-03 | TCMalloc field experiment (address-width candidate) | Field 05, Field 06, Field 14, Field 01 | Written summaries only: Field 05 (experiment description), Field 06 (guarded build summary), Field 14 (artifact ledger). The artifacts themselves are NOT in this repo - see Section 3. | Header-level only: default width 48 accepted, explicit 39 accepted, 38 and 49 rejected, tag-shift assertions passed (Field 05). Guarded build reached 264 of 300 Bazel actions and was stopped by its watchdog; no archive, no allocator execution (Field 06). No malloc / free, threading, or compiler-integration measurement exists. | No - explicitly incomplete. Field 05 lists acceptance gates 1 to 6 and all remain open. |
| B-04 | jemalloc | Field 03 (public Mojo source selects jemalloc for the Linux Mojo driver); Research 11; Roadmap Sec. 11 comparator set by family | None. No jemalloc build, configuration, or run is recorded. | None | No - candidate only. See inconsistency I-3 in SUPPORTED_TARGETS.md: source selects jemalloc while shipped Mojo wheels abort in TCMalloc, provenance unresolved. |
| B-05 | mimalloc (Microsoft) | Roadmap Phase 2 (established allocators such as mimalloc as controlled diagnostic comparators); Roadmap Sec. 11; source S4 | None. No build or run recorded. Also the naming-confusion subject: Roadmap title block and Research 11 warn MimirAlloc and mimalloc are distinct, confusable projects. | None | No - candidate only. |
| B-06 | Other landscape allocators: snmalloc, scalloc, TLSF, Scudo, StarMalloc | Research 11 (Allocator landscape) | None. Research 11 presents them as study subjects with questions to investigate, with primary sources. | None | No - study only, not baselines. Phase 2 does not name them; promoting any of them needs a recorded decision. |
| B-07 | MimirAlloc Phase 3 minimal allocator (future internal baseline) | Roadmap Phase 3 exit; Phase 7 exit (compare against Phase 3 and the platform allocator) | None. No allocator code exists in the repository. | None | No - does not exist yet. Recorded here because later phases use it as a comparison point. |
| B-08 | Workload baselines (not allocators): Mojo compiler build / invocation; A.E.S.I.R. model load, prefill, decode, unload | Roadmap Sec. 11 benchmark classes 6 and 7; Phase 6 and Phase 12 exits | Field notes record the Mojo failure side only: Mojo 1.2.0 and 1.3.0.dev2026110205 abort before --version on the SM-X400 (Field 03, Field 01). A Qwen3-1.7B GGUF candidate is recorded as metadata only; the file was not downloaded or loaded (Field 07, Field 14). | No successful Mojo build, compile, or inference run is recorded on any target. No model hash-verified run exists. | No - the workloads themselves are not yet runnable as baselines on the named targets. |

## 3. Artifact availability in detail (Field 14 ledger, checked)

Field 14 (Source and artifact ledger) is the repository-own statement
of what exists where. Checked against the tracked file list by this
team, it holds:

- In this repository (summaries and ledgers only): Field 01 evidence
  ledger and timeline; Field 02 tablet inventory (SM-X400, Android 16 /
  API 36, Exynos 1380 association via SoC identifier s5e8835, eight
  cores, MemTotal 5,841,464 KiB, one MemAvailable snapshot of
  1,525,480 KiB, page size probe 4,096 bytes); Field 03 failure and
  address-probe summary (2^38 success, 2^39 and 2^47 ENOMEM); Field 05
  and Field 06 experiment summaries, including the guarded build
  numbers (264 / 300 actions, watchdog floor 850 MiB, lowest sampled
  MemAvailable 847,196 KiB); Field 14 itself.
- NOT in this repository: the experimental TCMalloc source (upstream
  12f255231938d30493186b0a037feedd70f5a1c1, local candidate commit
  3390b10 on experiment/aarch64-address-bits, local at time of writing);
  the guarded build raw log, Bazel cache, disposable build tree,
  wrapper, and watchdog (remained on the original tablet, Field 06);
  the GLES and NNAPI diagnostic scripts (summarized in Field 08 and 09,
  scripts not included, Field 14); the local A.E.S.I.R. note commits
  026586d, a1c436e, 82a0056, 82558ef (local at time of investigation,
  remote availability unconfirmed, Field 01 and Field 14).
- Identified but not obtained: the Qwen GGUF candidate file (metadata
  recorded at a pinned Hugging Face revision; not downloaded, not
  hash-verified locally, Field 14); Mojo stable wheel 1.2.0 was fetched
  and its SHA-256 matched the publisher digest
  cb54942fd99be346ef273cc9beae5f692c58195dcbe3798596b2e7368d4f719c,
  and the compiler startup aborted (Field 14, Field 03). The wheel
  itself is not in this repository.

## 4. What is missing before Phase 2 can use these baselines

Roadmap Phase 2 exit requires a table of options with commands,
measured results, and integration limitations, distinguishing LD_PRELOAD,
direct link, static replacement, and namespaced API. None of that
exists yet. Concretely missing, per baseline B-01 to B-05:

1. Pinned builds: exact upstream release or commit, build flags, and
   consistent configuration for each candidate on each target (the
   Field 05 warning: every TCMalloc translation unit and consumer must
   agree on the address-width setting).
2. Selection proof: evidence each allocator was actually selected by
   the test process - dynamic versus static linkage and exported symbol
   evidence (Roadmap Sec. 11 principles). No such proof exists for any
   candidate.
3. A harness: no benchmark or contract-test harness, script, or result
   schema is tracked in the repository (verified, Section 1). Research
   12 gives recipes and a lab-notebook field list, but no executable
   protocol has been committed.
4. Workloads: the Roadmap Sec. 11 benchmark classes 1 to 8 have never
   been run. Classes 6 (real Mojo build) and 7 (A.E.S.I.R. lifecycle)
   additionally need B-08 resolved.
5. Environment manifests per run: machine, CPU, memory, OS / kernel,
   ABI / libc, page size, compiler / linker, allocator revision, thermal
   state, competing processes (Research 12 lab notebook; Roadmap
   Sec. 11). Only the SM-X400 inventory (Field 02) and the new Pi
   manifest (Section 5) exist, and neither is attached to a baseline
   run.
6. Repetition and statistics protocol: fresh process runs, median and
   p95 / p99, repeat distributions (Roadmap Sec. 11). No run exists to
   apply it to.
7. For B-03 specifically: completion of Field 05 gates 1 to 6 - full
   allocator build and link, contract tests, multithread stress, both
   39-bit and larger-address verification, compiler startup and use,
   and separate native Android / bionic verification.
8. Upstream Phase 1 dependencies: the reproducible Mojo / TCMalloc
   failure packet and the mapped integration / symbol contract
   (Roadmap Sec. 3.1 and 3.3, owned by slices A-005 to A-007) are
   still open; without them Phase 2 cannot tell which process or
   linkage a comparator must unblock.
9. Raw logs with privacy review, preserved with timestamp, command,
   exit status, environment, and revision (Field 01, Evidence
   preservation). The repository holds summaries; raw logs for any
   baseline run are missing.

## 5. Pi-observed facts (Team-03, 2026-11-06, device purpleviking1)

Observed by this team on the Raspberry Pi itself, kept separate from
document-derived claims. No allocator was built or run. Load was
checked first (free -g and uptime, load average about 1.3, about
13 GiB available) and all commands below are read-only inventory.

    command: uname -m
    output:  aarch64

    command: uname -a
    output:  Linux purpleviking1 6.12.112-v8-16k-hailo+ #1 SMP PREEMPT
             Sat Oct 10 09:05:25 EDT 2026 aarch64 GNU/Linux
             (wrapped here; single line on the device)

    command: getconf PAGE_SIZE
    output:  16384

    command: getconf LONG_BIT
    output:  64

    command: nproc
    output:  4

    command: gcc --version (first line)
    output:  gcc (Debian 12.2.0-14+deb12u1) 12.2.0

    command: clang --version (first line)
    output:  Debian clang version 14.0.6

    command: ldd --version (first line)
    output:  ldd (Debian GLIBC 2.36-9+rpt2+deb12u14) 2.36

    command: head -n 4 /etc/os-release
    output:  PRETTY_NAME Debian GNU/Linux 12 (bookworm); VERSION_ID 12

    command: getconf PAGESIZE (spelling used in Roadmap Sec. 12)
    output:  16384 (agrees with getconf PAGE_SIZE above)

    command: ld --version (first line)
    output:  GNU ld (GNU Binutils for Debian) 2.40

    command: grep Model in /proc/cpuinfo (first match)
    output:  Raspberry Pi 5 Model B Rev 1.1

    command: grep MemTotal and MemAvailable in /proc/meminfo
    output:  MemTotal 16598576 kB; MemAvailable 14482144 kB
             (snapshot at sampling time; values move)

    command: ldconfig -p, filtered for tcmalloc, jemalloc, mimalloc
    output:  libjemalloc.so.2 (libc6, AArch64) present at
             /lib/aarch64-linux-gnu/libjemalloc.so.2;
             no tcmalloc or mimalloc shared library in the cache.
             Presence of a system library is inventory only: jemalloc
             was not run, configured, or measured by this team.

    command: git ls-files (in the team-03 worktree) counted, and
             filtered for bench, .log, results, .csv
    output:  95 tracked files at base b0b8374; the only matches were the two research
             documents named in Section 1. No raw log, CSV, results
             file, or harness is tracked.

## 6. Inconsistencies and gaps discovered (recorded, not resolved)

- J-1. Jemalloc versus TCMalloc provenance: public Mojo source selects
  jemalloc for the Linux Mojo driver, while the shipped wheels abort
  in TCMalloc (Field 03). Until the wheel provenance is established, it
  is unknown which allocator a source-built baseline represents.
  Full record: SUPPORTED_TARGETS.md, I-3.
- J-2. The strongest baseline-adjacent evidence in the repo (B-03) is
  also the least preserved: its source branch, raw build log, cache,
  and scripts all live outside this repository and, per Field 06, in
  temporary tablet storage that the OS can clear. Phase 2 cannot
  reproduce B-03 from this repository alone.
- J-3. Roadmap Section 11 names the comparison set as platform libc,
  a pinned tcmalloc build where supported, mimalloc, and MimirAlloc.
  Jemalloc - the allocator the public Mojo source actually selects -
  is absent from that sentence, though Research 11 and Field 03 treat
  it as central. Phase 2 scoping should record a decision on whether
  jemalloc is in the comparator set.
- J-4. No baseline exists for the musl target at all: no musl libc
  allocator record, no musl build of any candidate. The Roadmap Sec. 5
  musl compatibility row therefore has no baseline partner yet.
- J-5. Tablet memory figures are snapshots, not baselines: Field 02
  records one MemAvailable reading and warns the values change while
  applications run, and Field 01 notes no repeatable idle baseline was
  obtained. They must not be quoted as a device baseline.

## 7. Phase 2 readiness verdict

Not ready, and honestly so. Phase 2 can begin its scoping work (target
and comparator decisions, harness design per Research 12, pinned-build
selection), but it cannot evaluate options or record measured results
until at least items 1 to 5 of Section 4 exist for a first comparator
on a first target. The natural first pair, on this inventory, is B-01
(platform glibc allocator, already the Pi and Linux default) on Linux
AArch64 + glibc - the Roadmap first production candidate - because it
needs no new allocator build to start harness work. That is a
recommendation from the inventory, not a measured result.
