# 14 — Source and artifact ledger

## Field artifacts and their availability

| Item | Identity | Availability and limitation |
|---|---|---|
| A.E.S.I.R. tablet plan | [PR #9](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir/pull/9) | Public roadmap; check current state and revision |
| A.E.S.I.R. source baseline | `3358973cfe542c052b7937bd9ff5c2d9bcd98a34` | Baseline recorded in field work |
| Local A.E.S.I.R. notes | `026586d`, `a1c436e`, `82a0056`, `82558ef` | Local at time of investigation; remote availability unconfirmed |
| Experimental TCMalloc source | upstream `12f255231938d30493186b0a037feedd70f5a1c1`, local candidate `3390b10` | Local experimental branch; header tests only and incomplete allocator build |
| Public Modular source inspected | `99ab7e90258db16bda9261b19bcb59d729ee3429` | [Source repository](https://github.com/modular/modular); source selection is not proof of wheel provenance |
| Mojo stable wheel | 1.2.0, Linux ARM64; published SHA-256 `cb54942fd99be346ef273cc9beae5f692c58195dcbe3798596b2e7368d4f719c` | A separately fetched wheel matched the publisher's digest; compiler startup aborted |
| Mojo nightly | 1.3.0.dev2026110205, Linux ARM64 | Tested package also aborted; exact relation to inspected source not established |
| Qwen GGUF candidate | [repository at pinned revision](https://huggingface.co/bartowski/Qwen_Qwen3-1.7B-GGUF/tree/dcb19155b962dbb6389f4691a982043a8e651022) | Metadata only; the GGUF file was not downloaded or verified locally |
| Guarded allocator build | Local Bazel build log and cache | Stopped at 264/300 actions; not a finished build; raw log not included here |
| GLES and NNAPI probes | Local A.E.S.I.R. diagnostic scripts | Results are summarized in this collection; scripts are not included here |

Hashes identify intended artifacts; always verify bytes actually obtained. A Git commit or package revision does not by itself establish that a binary was built from that source. Avoid creating links to local-only commits that may not be reachable.

## Primary sources for further work

- [Mojo system requirements](https://mojolang.org/docs/requirements/) and [compilation targets](https://mojolang.org/docs/tools/compilation/): supported environments and target selection; check current version.
- [Modular source repository](https://github.com/modular/modular): compiler and build configuration. Inspect the exact commit used.
- [Google TCMalloc](https://github.com/google/tcmalloc): allocator source, build and license; external to MimirAlloc.
- [Android NDK neural-network guide](https://developer.android.com/ndk/guides/neuralnetworks): public API context, subject to Android version and device support.
- [Android thermal guidance](https://developer.android.com/games/optimize/adpf/thermal): platform signals and limitations.
- [Samsung Exynos 1380](https://semiconductor.samsung.com/processor/mobile-processor/exynos-1380/): manufacturer processor background.
- [Qwen3-1.7B model card](https://huggingface.co/Qwen/Qwen3-1.7B): publisher architecture and use guidance.

Source consultation informs research and requirements; it does not grant permission to copy third-party code into an independently designed allocator. Preserve applicable notices and follow the project's [engineering guide](../../ENGINEERING_GUIDE.md).
