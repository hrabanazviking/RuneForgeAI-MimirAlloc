# Neural Computers and LLM-Embedded Virtual Machines

A research collection for extending MimirAlloc to model-contained programmable computers, their physical hosts, and future distributed computing chains. **Research only:** no allocator, model integration, physical device support or network service is implemented by these documents. Third-party results are attributed and require reproduction.

| Guide | Focus |
|---|---|
| [Language models containing computers: research map](01_FIELD_MAP_AND_TERMINOLOGY.md) | Architecture and terminology |
| [Qwenputer: RISC-V in Qwen model computation](02_QWENPUTER_DEEP_DIVE.md) | Qwenputer RISC-V and recurrent state |
| [Transformer VM: compiled WebAssembly execution](03_TRANSFORMER_VM_DEEP_DIVE.md) | Percepta transformer-based WASM |
| [Designing a guest allocator for neural computers](04_GUEST_ALLOCATOR_PORT.md) | Portable bounded guest allocator design |
| [Host memory, model state and Project A.E.S.I.R.](05_HOST_STATE_AND_AEISIR.md) | Host resources and A.E.S.I.R. compatibility |
| [Neural computers in distributed computing chains](06_DISTRIBUTED_NEURAL_COMPUTE.md) | Heterogeneous networked worker design |
| [Verification, experiments and benchmarking protocol](07_VERIFICATION_AND_BENCHMARKS.md) | Experiments and correctness evaluation |
| [Reliability, isolation, security and research provenance](08_SAFETY_AND_PROVENANCE.md) | Security, reliability and source provenance |
| [Neural-computer roadmap for MimirAlloc](09_ROADMAP_AND_OPEN_QUESTIONS.md) | Staged roadmap and decisions |
| [Primary sources and evidence catalog](10_PRIMARY_SOURCES.md) | Primary sources and standards |
| [Proposed neural-computer worker protocol](11_WORKER_PROTOCOL_DESIGN.md) | Capability negotiation, retries, fencing and message examples |
| [Allocation strategies and case studies](12_ALLOCATION_PATTERNS_AND_CASE_STUDIES.md) | Concrete guest, host, inference and distributed scenarios |

## Key distinction

The guest allocator manages addresses in the virtual computer; the host runtime manages the physical memory that holds its model and state; the network adapter manages explicit messages and handles between workers. These are three separate boundaries. An in-model machine has no physical network interface or free host memory merely because it is represented in a model.

Start with the [field map](01_FIELD_MAP_AND_TERMINOLOGY.md), then choose the [Qwenputer](02_QWENPUTER_DEEP_DIVE.md) or [Transformer VM](03_TRANSFORMER_VM_DEEP_DIVE.md) case study. For implementation work, read the [guest port](04_GUEST_ALLOCATOR_PORT.md), [test protocol](07_VERIFICATION_AND_BENCHMARKS.md), [safety guide](08_SAFETY_AND_PROVENANCE.md), and [roadmap](09_ROADMAP_AND_OPEN_QUESTIONS.md). For the broader project, see the [memory systems library](../README.md) and [original roadmap](../../MimirAlloc_Research_and_Development_Roadmap.md).
