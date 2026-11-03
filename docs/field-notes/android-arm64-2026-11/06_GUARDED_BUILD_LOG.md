# 06 — Guarded allocator build

## Objective and outcome

On 3 November 2026 the tablet attempted a complete build of the experimental 39-bit TCMalloc source. The goal was an archive for a later real allocation test. **It did not finish.** The final build log reached 264 of 300 Bazel actions, with allocator source files compiled; a watchdog terminated the build after `MemAvailable` fell below 850 MiB. No archive completion, allocator execution or Mojo rebuild was demonstrated.

The observed low reading was 847,196 KiB, sampled every two seconds. After termination, a sample showed 1,209,656 KiB available and approximately 3.14 GiB free storage. These values include other activity; they do not prove an exact per-process cost or tablet responsiveness.

## Toolchain and build-rule obstacles

- Installed Termux Bazel 9.3.1 and OpenJDK 21.
- The first attempt failed while resolving an older transitive rule expecting `local_config_platform`, before allocator compilation.
- An exploratory disposable copy updated `aspect_bazel_lib` to 2.22.5, `rules_go` to 0.65.0 and `aspect_rules_esbuild` to 0.27.1 to advance analysis. These changes were not accepted as a verified upstream dependency upgrade.
- Termux's generated C++ configuration supplied Clang-only options that glibc GCC rejected. A local wrapper selected Termux Clang's AArch64 GNU/Linux target and the installed glibc headers, libraries and linker. A small glibc C program compiled and ran with this setup.
- An Aspect telemetry extension printed a notice that collection would begin on a later invocation. Subsequent runs used `DO_NOT_TRACK=1` in the process and Bazel repository environments. Reproduction should review every tool's current telemetry settings rather than assume defaults.

## Guard

The experiment used batch mode, a 320 MiB Java heap, one Bazel job, low process priority, local execution, and both C and C++ flags defining the experimental address-width setting to 39. The watchdog sampled available memory and free disk every two seconds. It terminated the process group below 850 MiB available, below 2 GiB free storage, on measurement failure, or after 180 seconds. Two time-limited windows resumed from cached results; the next stopped at the memory floor.

The 850 MiB floor was an experimental operational cutoff, not a formally validated Android low-memory threshold. Sampling can miss brief spikes, `MemAvailable` changes independently, and a process-group termination cannot guarantee every child exits. Verify remaining processes and remeasure after any stop. Keep foreground activity and thermal conditions in view.

## Resumption and evidence

At recording time, the disposable build tree, wrapper, watchdog and Bazel cache remained on the original tablet. They are **not included in this MimirAlloc repository**. Temporary directories can be cleared by the operating system; preserve and independently review tools before relying on them. A subsequent build must use the exact source revision, a consistent width across all TCMalloc units, the dependency overrides and a guarded resource policy. A source-built allocation workload and a comparison against an unmodified supported baseline are next.

No claim about MimirAlloc's allocator, performance, a built Mojo compiler or Android-native application follows from this partial build.
