# MimirAlloc Project Charter

**Status:** Research and planning. MimirAlloc does not yet exist as a verified implementation.
**Working name:** MimirAlloc is a working name; see Section 10, Naming status. This charter is not a legal clearance.
**Charter basis:** Roadmap sections 1, 2, 4 and 5 (research date November 3, 2026); LICENSE; README project status.

---

## 1. Purpose and vision

MimirAlloc is a proposed independently engineered, general-purpose memory allocator, focused first on 64-bit ARM devices. Its intended qualities are correct memory ownership, dependable behaviour under pressure, useful diagnostics, low overhead, responsible behaviour on mobile devices, and competitive performance that is measured rather than asserted.

The practical motivation comes from Project A.E.S.I.R. and the Mojo toolchain: an obstacle related to tcmalloc is suspected when building or running Mojo on an ARM device. The first job is to identify that exact obstacle and to show whether changing the allocator can affect it at all. The project is still useful as a standalone open-source allocator even if the Mojo problem is later shown to sit in a different subsystem.

A limit applies from the start. An allocator manages dynamic memory for other software; it is not an inference engine and not a GPU or NPU runtime. It may help with startup, temporary allocations, fragmentation and stability, but it cannot by itself make an unsupported compiler, driver, operating system or model architecture work.

Open point, stated without resolution: the roadmap treats the Mojo blocker as uncharacterised, meaning the exact error, affected component and cause are not yet established and a reproducible evidence packet is still required. The field notes record partial, device-scoped findings, including an allocator initialisation failure during compiler startup on one tablet in one environment, together with address-space probes and a header-level experiment. Those findings are limited to the device and context in which they were gathered; they do not settle the general cause, and they do not show that an allocator change fixes Mojo.

## 2. Owner and governance

The project owner is Volmarr Wyrd, as recorded in the copyright line of the LICENSE file: Copyright (c) 2026 Volmarr Wyrd. The repository is hrabanazviking/RuneForgeAI-MimirAlloc.

Governance at this stage is intentionally simple: decisions rest with the owner, and the project intends to preserve a clean provenance record and meaningful open-source governance, in line with project goal 7 in Section 4 below. No wider governance structure is claimed in this charter, because none is recorded in the sources it relies on.

## 3. License

MimirAlloc is released under the MIT License (see LICENSE). That is a decided fact, recorded by the LICENSE file held in this repository.

Source note: the roadmap still phrases licence selection as an open matter in places, including its clean-room discussion in section 4 and question 12 of its decision register in section 15. Where the roadmap wording and the LICENSE file differ, the LICENSE file governs: it records the decision as MIT.

## 4. Scope: project goals

The project has seven goals, taken in order from section 1 of the roadmap:

1. Determine the exact allocator symbols and behaviour that the specific Mojo build and runtime need.
2. Build a standalone, original allocator for ARM64 Linux, with a clear C API and strong correctness tests.
3. Establish an explicit, tested integration path for the Mojo compiler or build, or for a generated application, whichever actually carries the blocker.
4. Extend support to native Android ARM64 and to the user's Samsung tablet, where feasible.
5. Provide measured performance and memory behaviour on realistic compiler and local-AI workloads.
6. Handle resource limits and failures predictably, without claiming to eliminate all crashes or all corruption.
7. Preserve a clean provenance record and meaningful open-source governance.

## 5. Non-goals for the first usable version

The following are non-goals for the first usable version, taken from section 1 of the roadmap:

- Reproducing every internal tcmalloc algorithm, C++ class, API extension, profiling protocol or performance characteristic.
- Claiming compatibility before the actual linkage has been confirmed and the real target exercised.
- Claiming Android support merely because an ARM64 Linux binary works under proot.
- Promising universal interposition, cross-allocator freeing, zero fragmentation, or guaranteed protection from out-of-memory termination.
- Making the allocator responsible for detecting every invalid pointer or repairing arbitrary application corruption.
- Assuming that tcmalloc has no ARM support. Its published Linux platforms include AArch64; the open question concerns the specific revision, target ABI, build flags, address-space assumptions and environment in use.

## 6. Supported targets

ARM64 Linux with glibc is the first production candidate, x86-64 with glibc serves as the development and reference platform, and Android ARM64 with bionic is a separate native port, following roadmap section 5; one open point is flagged here without resolution, namely that roadmap section 2 states the tablet page size must be measured, since Android covers more than one page-size configuration, while field-notes 02 records a single probe reading of 4,096 bytes on the tablet, a result limited to that one probe and context and not a settled property of the device. The supported-target matrix is maintained as slice A-003's artifact; this charter does not duplicate it.

## 7. Release and evidence terminology

Status claims in this project use only the following labels, adopted from roadmap section 2 with their meanings unchanged in substance:

- **Proposed:** an idea that has not been implemented.
- **Implemented:** code exists, but has not passed the full test gate.
- **Host-tested:** passed named tests in a specified host environment.
- **Device-tested:** ran on the named physical ARM or Android device.
- **Integration-proven:** the exact Mojo or A.E.S.I.R. workflow succeeded with MimirAlloc demonstrably handling the relevant allocations.
- **Performance-measured:** results come with complete scripts, workload, baseline, versions and hardware.
- **Blocked:** a specific prerequisite or test is unmet, with an actionable resumption point.

One promotion rule governs all use of these labels: never promote an emulator result to physical-device proof, a synthetic test to full application compatibility, or a successful compile to proof of runtime interposition.

Plainly stated: no release exists. The current status language for the project is Research and planning, and, for this repository, documentation and planning only.

## 8. Evidence-before-claims principle

Evidence comes before claims. This principle appears in the README engineering principles and in the verification guidance of the Engineering Guide: a status claim is only as strong as the check behind it, and every status claim must trace to a file or to a logged, reproducible check. Results are reported with their environment and their limits, and unsuccessful experiments are recorded alongside successes rather than hidden.

## 9. Clean-room and provenance principle

MimirAlloc is independently engineered from public documentation, standards and observed behaviour, following roadmap section 4 and its practical process in section 4.1. In summary: consulted sources are kept in a dated ledger; behaviour is specified in original words before implementation; the design is created from that specification and general allocator concepts; third-party implementation material is kept out of the implementation; and commit history, provenance notes and dependency notices are preserved so the origin of the work can be audited.

An honesty limit applies to this principle. Where strict separation between an investigating team and an implementing team is unavailable, that limitation is recorded rather than disguised. A solo or AI-assisted process must not be labelled a legally verified two-team clean room.

## 10. Naming status

MimirAlloc is a working name. Microsoft already publishes the prominently named mimalloc, so confusion is plausible. Repository, package, domain and trademark availability are UNVERIFIED: no check is recorded in this repository. The concern is recorded, not resolved. This is not a legal clearance.

## 11. Current status and what does not exist yet

This repository currently holds documentation, artwork and the MIT licence, and nothing more that can be run or released. No allocator implementation exists yet. No release exists. No build system exists. No runnable test suite exists. Any document in this repository that describes behaviour, targets or performance is describing proposed work, and must be read with the evidence labels in Section 7.

## 12. Records this charter relies on

This charter points to the following records and copies no content from them:

- The decision register and open research questions in section 15 of the research and development roadmap.
- Field-notes 01_EVIDENCE_AND_TIMELINE.md and 14_SOURCES_AND_ARTIFACTS.md in the Android ARM64 field-notes collection.
- The source ledgers in the research collection under docs/research, for example ai_memory_synthesis/13_SOURCE_LEDGER.md.
