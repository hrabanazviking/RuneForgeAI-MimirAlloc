# AI and Memory: Integrated Research Synthesis

A curated, original-language collection connecting neural memory, long-term agent memory, RAG, KV caches, allocators, small-model policies, heterogeneous devices and distributed objects. **Status:** research and proposals. Nothing here implements an allocator, model, knowledge base or memory fabric.

The online literature is vast and continually changing; this collection is a substantial, cited starting point rather than a claim to cover every paper. Source-reported findings are distinguished from proposals and have not been independently reproduced by this project. Original-language synthesis is not a claim that a formal two-team clean-room process was performed.

| Document | Focus |
|---|---|
| [Atlas of AI and computer memory](01_ATLAS_OF_AI_MEMORY.md) | Taxonomy and shared concepts |
| [Neural, recurrent and model-state memory](02_NEURAL_AND_MODEL_MEMORY.md) | Learned and recurrent model state |
| [Agent memory, provenance and changing facts](03_AGENT_MEMORY_AND_TEMPORAL_TRUTH.md) | Agent memory and temporal evidence |
| [Retrieval, indexes, graphs and evidence](04_RETRIEVAL_GRAPHS_AND_INDEXES.md) | RAG, indexes and graph retrieval |
| [Attention caches, IO and model inference](05_KV_CACHES_AND_ATTENTION.md) | Attention KV caches and inference |
| [Allocators, learned predictions and safe policy](06_ALLOCATOR_AND_LEARNED_POLICIES.md) | Allocation and optional learned policies |
| [A heterogeneous memory fabric: common interface, explicit semantics](07_HETEROGENEOUS_MEMORY_FABRIC.md) | Device resource federation |
| [Distributed memory, execution and failure](08_DISTRIBUTED_MEMORY_AND_COMPUTE.md) | Remote objects and neural workers |
| [Proposed MimirAlloc architecture: integrating the research](09_PROPOSED_MIMIRALLOC_ARCHITECTURE.md) | Cross-domain architecture and hypotheses |
| [Evaluation: measure memory, quality and correctness together](10_EXPERIMENTS_AND_BENCHMARKS.md) | Benchmarks and reproducibility |
| [Privacy, safety, reliability and provenance](11_PRIVACY_SECURITY_AND_RELIABILITY.md) | Privacy, safety and provenance |
| [Integrated research roadmap](12_RESEARCH_ROADMAP.md) | Incremental research roadmap |
| [Primary-source ledger: AI and memory research](13_SOURCE_LEDGER.md) | Curated primary-source ledger |
| [Cross-domain design studies](14_CROSS_DOMAIN_DESIGN_STUDIES.md) | Experiments connecting allocator, RAG, KV and fabric |
| [Source-to-engineering matrix](15_SOURCE_TO_ENGINEERING_MATRIX.md) | Evidence, applications and open questions |
| [Implementation work packages](16_IMPLEMENTATION_WORK_PACKAGES.md) | Concrete milestones and acceptance gates |

## Reading routes

- **A complete overview:** [atlas](01_ATLAS_OF_AI_MEMORY.md) → [architecture](09_PROPOSED_MIMIRALLOC_ARCHITECTURE.md) → [roadmap](12_RESEARCH_ROADMAP.md).
- **AI memory and retrieval:** [model state](02_NEURAL_AND_MODEL_MEMORY.md) → [agent memory](03_AGENT_MEMORY_AND_TEMPORAL_TRUTH.md) → [retrieval](04_RETRIEVAL_GRAPHS_AND_INDEXES.md) → [KV cache](05_KV_CACHES_AND_ATTENTION.md).
- **Systems development:** [allocator policies](06_ALLOCATOR_AND_LEARNED_POLICIES.md) → [heterogeneous memory](07_HETEROGENEOUS_MEMORY_FABRIC.md) → [distributed design](08_DISTRIBUTED_MEMORY_AND_COMPUTE.md).
- **Evidence and responsible implementation:** [benchmarks](10_EXPERIMENTS_AND_BENCHMARKS.md) → [privacy and provenance](11_PRIVACY_SECURITY_AND_RELIABILITY.md) → [source ledger](13_SOURCE_LEDGER.md).

Return to the [research library](../README.md), [intelligent-memory collection](../intelligent_memory/README.md), [neural-computer collection](../llm_computers/README.md), or [original roadmap](../../MimirAlloc_Research_and_Development_Roadmap.md).
