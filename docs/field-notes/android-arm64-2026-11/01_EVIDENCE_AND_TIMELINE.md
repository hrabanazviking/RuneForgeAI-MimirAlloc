# 01 — Evidence ledger and timeline

**Scope:** observations made on one physical SM-X400 in a Termux app context. Dates reflect the field notes. No benchmark, status or capability below should be generalized to every device.

## Evidence ledger

| Area | Observed result | What remains open |
|---|---|---|
| Tablet | SM-X400, Android 16/API 36, ARM64; kernel-visible RAM 5,841,464 KiB | Foreground app state, thermal, energy and ActivityManager low-memory threshold |
| Mojo stable | Linux ARM64 Mojo 1.2.0 installed in separate glibc environment; direct loader reached compiler then TCMalloc aborted before `--version` | A runnable compiler; Android/bionic output |
| Mojo nightly | 1.3.0.dev2026110205 produced the same allocator startup failure | Correspondence of that wheel to public source and a source-built compiler test |
| Address mapping | 4 KiB fixed-no-replace probe succeeded at 2^38; failed with ENOMEM at 2^39 and 2^47 | Exact kernel configuration; results on other devices |
| Candidate | Modified pinned TCMalloc header accepted default 48 and explicit 39; rejected 38 and 49; tag-shift compile assertions passed | Complete allocator build, allocation stress, compiler integration |
| Partial build | Guarded single-job build reached 264/300 actions; watchdog stopped when sampled MemAvailable was 847,196 KiB | Completed archive, runtime and threading tests |
| Mali | GLES 3.2 compute shader produced 16 expected values; Vulkan instance enumerated Mali-G68 | Vulkan dispatch, Q4 kernels, UI contention and Aesir backend |
| NNAPI | Explicit `enn` and reference devices each produced expected output for FP32 ADD | Physical NPU trace, quantized model support, Qwen compatibility |
| Qwen | Published candidate model metadata recorded; file not downloaded or loaded | Integrity verification, model admission, inference and accuracy |

## Sequence

1. Device inventory established the tablet identity, limited available memory and visible drivers. Android command execution and device nodes had app-sandbox restrictions.
2. An A.E.S.I.R. checkout at `3358973cfe542c052b7937bd9ff5c2d9bcd98a34` was used to preserve device diagnostics. Its PR #9 was planning, not an Android implementation.
3. Native Termux build tools and a separate glibc userspace were installed. A glibc C hello program ran; that result did not establish Android-native Mojo.
4. The Mojo packages failed at allocator initialization. An address-space probe gave evidence consistent with a 39-bit ceiling on this device.
5. A pinned TCMalloc configuration experiment was created outside MimirAlloc and exercised only through header-level checks. Public Modular source at `99ab7e90258db16bda9261b19bcb59d729ee3429` selected jemalloc for its Linux Mojo driver; the distributed nightly still failed with TCMalloc. Those observations do not establish which source revision built a particular wheel.
6. A low-priority Bazel build with a watchdog progressed through allocator compilation, then stopped at the memory floor. The archive and program were not tested.

## Evidence preservation

Local A.E.S.I.R. commits `026586d`, `a1c436e`, `82a0056` and `82558ef` recorded successive notes and diagnostics. They were local commits at the time of this notebook; do not assume they exist on a remote. The local TCMalloc experimental commit `3390b10` was based on upstream `12f255231938d30493186b0a037feedd70f5a1c1`. This collection independently states the observations and does not require access to those local commits.

Raw output should be preserved with a timestamp, command, exit status, environment, revision and privacy review. File paths and operating-system build strings are contextual; serial numbers, account data, credentials, personal prompts and model conversations do not belong in public evidence.
