# Android ARM64 field notes: Mojo, allocation and local inference

**Evidence recorded:** 2–3 November 2026. **Project state:** MimirAlloc is research and planning; no MimirAlloc allocator, Mojo integration, native Android application or Qwen inference has been demonstrated.

This collection records a physical Samsung tablet investigation that helped motivate MimirAlloc. It is a research notebook and a reproducibility guide, not a claim that every ARM device behaves alike. It separates direct observations, inferences and proposals. Measurements are snapshots, not performance promises. Commands that depend on a particular Termux installation are illustrative; where a script lives outside this repository, its absence is stated explicitly.

| Note | Contents |
|---|---|
| [01 — Evidence ledger and timeline](01_EVIDENCE_AND_TIMELINE.md) | What was actually observed, versions and open gates |
| [02 — Physical tablet inventory](02_TABLET_INVENTORY.md) | Model, CPU, memory, graphics and access boundaries |
| [03 — Virtual addresses and the Mojo failure](03_VIRTUAL_ADDRESS_AND_MOJO.md) | 39-bit observations, TCMalloc startup error and interpretation |
| [04 — Android, Termux and glibc toolchains](04_ANDROID_AND_LINUX_TOOLCHAINS.md) | ABI, libc, executable loader and toolchain experiments |
| [05 — Allocator source experiment](05_TCMALLOC_SOURCE_EXPERIMENT.md) | Narrow compile-time candidate and limits of header tests |
| [06 — Guarded allocator build](06_GUARDED_BUILD_LOG.md) | Bazel failures, bounded compilation and interrupted build |
| [07 — Memory budgeting for inference](07_INFERENCE_MEMORY_BUDGET.md) | Model provenance, KV arithmetic and conservative admission |
| [08 — Mali GPU findings](08_MALI_GPU.md) | Vulkan inventory and an actual GLES compute test |
| [09 — Samsung NNAPI findings](09_SAMSUNG_NNAPI.md) | Explicit vendor-device operator execution and physical-dispatch uncertainty |
| [10 — MimirAlloc requirements and clean-room boundary](10_MIMIRALLOC_REQUIREMENTS.md) | Behavioral requirements, ABI ownership, independent implementation |
| [11 — Reproduction, monitoring and safety](11_REPRODUCTION_AND_RESOURCE_GUARDS.md) | Minimal checks, sampling, stop conditions and privacy |
| [12 — Test matrix and experiments](12_TEST_MATRIX_AND_EXPERIMENTS.md) | Correctness, concurrency, memory pressure and integration gates |
| [13 — Decisions, open questions and next steps](13_DECISIONS_AND_NEXT_STEPS.md) | Priorities and concrete resumption points |
| [14 — Source and artifact ledger](14_SOURCES_AND_ARTIFACTS.md) | Primary sources, local artifact provenance and limitations |

## Reading this notebook

**Observed** means a described operation ran or a value was read in the named environment. **Inferred** identifies an interpretation of evidence. **Proposed** identifies work that has not yet passed its acceptance gate. A library file, device node, manifest declaration, header, model listing or partial build is not equivalent to working model inference. A successful small operator is not a general accelerator capability claim.

The [engineering guide](../../ENGINEERING_GUIDE.md) and [project roadmap](../../MimirAlloc_Research_and_Development_Roadmap.md) set MimirAlloc's clean-room and evidence expectations. [Project A.E.S.I.R. PR #9](https://github.com/hrabanazviking/RuneForgeAI-Project-Aesir/pull/9) supplies the original tablet roadmap. A.E.S.I.R. and this independent allocator project have separate implementation and dependency policies.
