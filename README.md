<div align="center">

<img src="assets/mimir-well.png" alt="Mímir’s well beneath Yggdrasil, lit by circuitry and Norse knotwork" width="100%">

# ᛗ MimirAlloc

### Memory forged for the machines at the edge

**An independent, open-source memory allocator project for ARM, Android, Mojo, and resource-conscious computing.**

[Explore the roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) · [Engineering guide](docs/ENGINEERING_GUIDE.md) · [Memory systems research library](docs/research/README.md) · [Android ARM64 field notes](docs/field-notes/android-arm64-2026-11/README.md) · [Project A.E.S.I.R.](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir) · [MIT license](LICENSE)

`Research & planning`　 •　 `ARM64 first`　 •　 `Open source`　 •　 `Evidence before claims`

</div>

---

> **Project status — the forge is just being lit.** This repository currently contains documentation, artwork, and an MIT license. The allocator, installation package, benchmarks, Android support, and Mojo integration are **planned**, not implemented or verified yet. There is no build or install command for MimirAlloc at this stage.

## 🌌 The idea

MimirAlloc is a proposed clean-room memory allocator: a piece of foundational software that manages the memory a program requests and releases. The goal is to build a small, dependable foundation for devices where memory, power, and responsiveness matter—from ARM Linux machines to Android tablets and local AI systems.

The project began with an obstacle encountered while exploring how to build Mojo for an ARM-based tablet. A tcmalloc-related issue inspired the idea of an independently engineered allocator that could help open another path for Mojo and [Project A.E.S.I.R.](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir)—and potentially be useful to other projects, too.

The exact build failure and replacement point still need to be reproduced and verified. Google’s public TCMalloc documentation lists Linux AArch64 support; the question here is how the **specific build, OS, ABI, and device** interact, not whether ARM exists upstream. See the [research roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) for sources and the investigation plan.

<img src="assets/runic-forge.png" alt="A Nordic forge with a glowing processor on an anvil and luminous circuit-like Norse knotwork" width="100%">

## 🧠 What does a memory allocator do?

When software needs temporary space for objects, it requests memory. An allocator finds available space, tracks it, and makes freed space available for reuse.

A good allocator must do more than make allocations quickly: it needs correct ownership, reliable handling of errors, safe behavior across threads, and reasonable memory use. On a tablet, it should also consider the other apps sharing the device.

**MimirAlloc’s mission:** explore those challenges with an original design, ARM-focused testing, and transparent results.

## ⚒️ What we want to build

These are **goals**, not current features:

| Focus | Planned work |
|---|---|
| **ARM64 foundations** | Build and test a portable allocator on real ARM64 Linux hardware. |
| **Android and Termux** | Investigate native Android/bionic support, device constraints, and different system page sizes. |
| **Mojo compatibility** | Identify the actual build blocker, required interfaces, and a demonstrable integration path. |
| **A.E.S.I.R. and local AI** | Measure allocator behavior in real model-loading and inference workloads. |
| **Correctness and resilience** | Test allocation contracts, alignment, concurrency, failures, and memory pressure. |
| **Measured performance** | Benchmark latency, throughput, retained memory, and device responsiveness against documented baselines. |

A memory allocator is one part of a larger system. It does not by itself provide GPU or NPU inference, and replacing an allocator does not automatically make a compiler or model run on Android.

## 🗺️ The journey ahead

1. **Find the actual blocker.** Capture the exact Mojo/ARM failure, toolchain versions, linker details, and reproducible commands.
2. **Define the contract.** Document the required interfaces and ownership rules before implementation.
3. **Build a small, correct foundation.** Start with a namespaced allocator, checked arithmetic, and meaningful tests.
4. **Prove the ARM and Android paths.** Test native environments separately from Linux running under Android’s `proot`.
5. **Demonstrate integration.** Verify that the intended Mojo or A.E.S.I.R. process actually uses MimirAlloc.
6. **Refine and measure.** Improve performance and memory use with benchmarks, stress tests, and physical-device evidence.

The full [research and development roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md) expands these into detailed engineering phases, acceptance gates, test matrices, and source references.

## 🚀 Getting started

### Explore the project

You can inspect the documentation now:

```bash
git clone https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc.git
cd RuneForgeAI-MimirAlloc
```

- Read the [research and development roadmap](docs/MimirAlloc_Research_and_Development_Roadmap.md).
- Read the [engineering guide](docs/ENGINEERING_GUIDE.md) for the first investigations and contribution workflow.
- Explore the [memory systems research library](docs/research/README.md) for allocator design, ARM and RISC-V, edge AI, accelerators, CXL, distributed memory, experiments, and primary sources.
- Discover the [neural computer research collection](docs/research/llm_computers/README.md), including guest allocators and distributed computing proposals.
- Explore [intelligent and federated memory research](docs/research/intelligent_memory/README.md), covering optional small-model policies, RAG, KV caches and multi-device resources.
- Read the [integrated AI and memory research synthesis](docs/research/ai_memory_synthesis/README.md) for cross-domain designs, primary sources, experiments and implementation work packages.
- Check the repository’s [issues](https://github.com/hrabanazviking/RuneForgeAI-MimirAlloc/issues) for current work.

**There is no allocator to install or run yet.** Build, integration, and benchmark instructions will be added when the corresponding implementation exists and has been tested.

### Help investigate

If you have a reproducible Mojo/ARM issue that may involve an allocator:

1. Record the exact command and the first relevant error.
2. Note the Mojo revision, operating system, architecture, compiler, linker, and whether the environment is native Android, Termux, `proot`, or Linux.
3. Remove passwords, tokens, personal data, and unnecessary device identifiers.
4. Open an issue with the minimal reproduction and relevant logs.

A reproducible failure is more useful than assuming every ARM or Android environment has the same cause.

## 🛡️ Engineering principles

- **Independent design:** define behavior and document source provenance; do not present copied implementation as clean-room work.
- **Correctness first:** check overflow, alignment, ownership, concurrency, and allocation failure.
- **Physical evidence:** identify what ran on a real device and what was only cross-compiled or simulated.
- **Mobile awareness:** watch memory pressure, thermal behavior, and responsiveness to other apps.
- **Honest results:** publish limitations and unsuccessful experiments alongside successes.
- **Open collaboration:** make the work understandable and useful to developers and curious newcomers.

## 📜 The story so far

MimirAlloc grew out of Volmarr’s work on **Project A.E.S.I.R.**, an independent local AI inference engine, and his effort to explore bringing that work to an Android tablet. Investigating Mojo on ARM led to a larger idea: create a separate open-source allocator project that could research the build obstacle while pursuing reliable, efficient memory management for ARM and edge devices.

The project is beginning with research and a staged roadmap. Its future implementation and integration claims will be earned through real tests, reproducible builds, and documented results.

## 🤝 Join the forge

Thoughtful contributions are welcome: reproductions, documentation improvements, ABI research, test design, ARM/Android expertise, and eventually code. Start with the [engineering guide](docs/ENGINEERING_GUIDE.md), then open an issue or pull request with the context and evidence behind your proposed change.

Please keep examples reproducible, avoid committing credentials or private device data, and distinguish tested results from ideas.

## 📜 License

MimirAlloc is released under the [MIT License](LICENSE).

<div align="center">

**Forged in curiosity. Grounded in evidence. Built in the open.**

ᚠ ᚢ ᚦ ᚨ ᚱ ᚲ

</div>
