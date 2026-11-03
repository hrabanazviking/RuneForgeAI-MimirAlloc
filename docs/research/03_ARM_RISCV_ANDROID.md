# ARM, RISC-V, Android and constrained CPU targets

## Architectural starting point

ARM64/AArch64 and RISC-V are instruction-set architecture families, not a single device, operating system, memory hierarchy or ABI. RISC is not a guarantee of low power, small caches or fast allocation. MimirAlloc should be correct portable systems code with explicitly tested target adaptations, rather than a collection of assumptions named after an ISA.

## Target matrix

| Target | Distinguish | First proof |
|---|---|---|
| x86-64 Linux | libc, kernel, page size, compiler | Correctness baseline and sanitizer builds |
| AArch64 Linux | distribution, libc, kernel VA width/page size, SoC | Native physical-device test and symbol audit |
| Android ARM64 app | bionic, API level, app sandbox, NDK, page and ELF alignment | Native APK test on actual device |
| Termux | Android userland and its supported package environment | Explicit executable and dynamic-library test |
| Linux under Android proot | userspace ABI, emulation/container behavior | Treat as separate target; no native APK claim |
| RISC-V Linux | RV64 ISA extensions, libc, compiler, atomics, kernel | Native/emulated tests clearly labelled |
| Microcontroller / RTOS | RAM map, MMU/MPU, ISR rules, no virtual memory | Separate design profile; OS-mapping assumptions invalid |

Cross-compilation proves code generation, not execution or hardware correctness. Emulation is useful but is not a substitute for physical testing of coherence, timing, firmware and energy. Verify the actual tablet's architecture, memory page size and exact error rather than copying a model-specific assumption.

## Page size, address space and alignment

Query page size at runtime with an appropriate API such as `sysconf(_SC_PAGESIZE)` and check failure; avoid hardcoded 4096-byte page alignment. A 16 KiB Android system changes ELF/load alignment and mapping assumptions, not just a constant. Android's guidance covers NDK versions, APK packaging and testing; all bundled native dependencies need examination. Real devices may support other configurations. Use checked add/multiply/round-up operations and assert the necessary alignment properties at API boundaries.

Virtual-address width depends on hardware, kernel configuration and actual OS behavior. Do not reserve a giant fixed region or steal pointer bits on the assumption that all ARM64 machines expose the same usable address bits. Android tagged pointers, PAC, MTE and ABI rules constrain what can be done with high pointer bits. Capture real mapping behavior and error codes; never overwrite fixed mappings just to test an assumption.

## Weak memory and atomics

Portable C/C++ atomics still need a correct happens-before design. For a producer publishing a prepared free-list node, use a release operation and a matching acquire by the consumer as the algorithm requires; this alone does not solve reclamation or ABA. Document exactly who owns the node before and after publication and prove unmapping cannot race a reader. Avoid assuming x86-like ordering. RISC-V's RVWMO and optional atomic extensions make actual target capability relevant. Check compiler atomic support and whether a type is lock-free; a library fallback may be acceptable only when its initialization and dependency behavior are safe.

## Heterogeneous cores and mobile power

Large and small cores can have different speed and cache behavior. A thread can migrate; per-thread or per-CPU locality may change mid-workload. Benchmark affinity-controlled experiments and normal scheduling separately. Measure awake time, background cleanup, frame-time impact and thermal conditions as well as operations per second. Avoid setting affinity globally or changing device thermal policies without explicit experimental scope.

## Android-specific practice

- Treat Android app, Termux and proot as three environments; symbol interposition and loaders have restrictions that vary with context and platform version.
- App memory pressure, process lifecycle and low-memory termination are system concerns; userspace allocators cannot promise survival.
- Query supported NDK APIs and API levels; test 4 KiB and 16 KiB page environments as available, and check ELF alignment and packaged libraries.
- For MTE-capable systems, follow OS/ABI rules for tag generation and memory tagging. Optional tagging is neither universal nor a replacement for testing.
- Record device model only with user consent; never publish serial numbers, account identifiers or private paths.

## Safe inventory commands

On Linux or an appropriate shell, these are read-only and may be unavailable depending on permissions:

```sh
uname -a
getconf PAGESIZE
getconf LONG_BIT
cat /proc/self/maps | head -n 20
```

Record compiler and libc versions through the actual toolchain. `/proc/self/maps` describes the process reading it and can contain addresses and paths; redact before sharing. For a binary you own, `readelf -hW`, `readelf -lW`, `readelf -dW` and `readelf -Ws` can help inspect ELF machine, load segments, dynamic dependencies and symbol exports. They do not prove a runtime allocation was intercepted.

## Device acceptance gate

Require native build plus allocate/fill/verify/free, alignment, overflow/failure, multiple threads, cross-thread free, repeated startup/exit, pressure recovery and physical-device logs. Record build hash, page size, OS, ABI and exact commands. A successful cross-build or emulator test is a different evidence tier.

## Sources

- [Android 16 KB page-size guidance](https://developer.android.com/guide/practices/page-sizes)
- [Android Arm MTE](https://developer.android.com/ndk/guides/arm-mte)
- [Linux AArch64 memory tagging](https://docs.kernel.org/arch/arm64/memory-tagging-extension.html)
- [RISC-V RVWMO specification](https://docs.riscv.org/reference/isa/unpriv/rvwmo.html)
- [RISC-V atomic extension](https://docs.riscv.org/reference/isa/unpriv/a-st-ext.html)
