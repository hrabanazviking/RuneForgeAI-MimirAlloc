# 04 — Android, Termux and glibc toolchains

ARM64 identifies an instruction architecture, not a complete binary interface. Android uses bionic and its own dynamic loader, linker namespaces, application sandbox and API-level availability. A GNU/Linux AArch64 executable can require glibc, a different loader, libraries and syscall behavior. A compiler that starts in a glibc environment has not thereby produced a native Android library.

## What was installed and exercised

Termux Clang 21.1.8 reported default target `aarch64-unknown-linux-android24`. CMake 4.4.5, Ninja 1.13.2, glslang 16.6.0, Vulkan headers 1.4.367, proot-distro 5.10.3 and patchelf 0.19.3 were installed. A tiny GLSL shader compiled to SPIR-V. The shader compilation did not dispatch work to a GPU.

A separate Termux glibc repository supplied glibc 2.44, GCC 14.2.1 and Python 3.12.9. A tiny glibc C program compiled and ran after putting the glibc `bin` directory first in PATH and removing Android's inherited `LD_PRELOAD` for that execution. In this context, leaving the preload in place caused glibc commands to fail with an invalid ELF header. Environment hygiene is part of a reproducible experiment.

The Android NNAPI C probe needed an explicit Android API 29 target because the default Android 24 target marks its APIs unavailable. This demonstrates one bionic-linked C diagnostic, not a Mojo bridge.

`proot-distro list` refused to run after detecting an already traced/proot-like session. A Linux environment inside a user-space translation layer is not equivalent to native Android packaging. Do not bypass security policy or represent one environment as another.

## Mojo status and target experiment

The A.E.S.I.R. `pixi.toml` at the observed baseline listed `linux-64`; that is an x86-64 environment and cannot simply be treated as Android ARM64. No local Mojo compiler could reach its version command. Modular documents Linux glibc requirements and describes cross-compilation support as under development: [requirements](https://mojolang.org/docs/requirements/) and [compilation targets](https://mojolang.org/docs/tools/compilation/).

For a future build record:

1. Pin the compiler source, toolchain, libc, Android NDK and target API.
2. Build a tiny exported function and record its target triple and linker invocation.
3. Inspect its ELF interpreter, dependencies, relocations and required symbols.
4. Load it from a minimal Android shell on the target and test success and error returns.
5. Test threads, exceptions/error propagation, file descriptors, mmap and lifecycle before integrating the runtime.

Keep Linux-hosted feasibility, cross-compilation and native Android results in separate evidence categories.
