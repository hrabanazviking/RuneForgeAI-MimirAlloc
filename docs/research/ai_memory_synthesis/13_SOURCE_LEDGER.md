# Primary-source ledger: AI and memory research

**Accessed for this synthesis:** November 2026. This is a curated, non-exhaustive map of primary research and official technical documentation, not a claim to have surveyed every online paper. Live pages and model repositories change; pin an immutable revision for implementation work. Claims below describe the source's scope and do not imply independent MimirAlloc reproduction.

| Source | Topic and contribution | Scope for MimirAlloc |
|---|---|---|
| [Neural Turing Machines](https://arxiv.org/abs/1410.5401) | Differentiable external memory access | Research inspiration; not an exact heap allocator |
| [Transformer-XL](https://arxiv.org/abs/1901.02860) | Segment recurrence and longer context | Model-specific state and positions |
| [Mamba](https://arxiv.org/abs/2312.00752) | Selective state-space sequence modeling | Recurrent state is not generic KV |
| [Lost in the Middle](https://arxiv.org/abs/2307.03172) | Position sensitivity in studied long-context tasks | Evaluate usable recall, not just context length |
| [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) | Retrieval plus language generation | External evidence and indexing; no guarantee of truth |
| [HippoRAG](https://papers.nips.cc/paper/2024/hash/6ddc001d07ca4f319af96a3024f6dbd1-Abstract-Conference.html) | Graph-oriented retrieval and associative questions | Evaluate graph cost and task-specific benefit |
| [GraphRAG](https://www.microsoft.com/en-us/research/publication/from-local-to-global-a-graph-rag-approach-to-query-focused-summarization/) | Graph extraction and collection-level summarization | Compare to simpler lexical/dense retrieval |
| [MemGPT](https://arxiv.org/abs/2310.08560) | OS-inspired context and external storage | Agent context management, not kernel virtual memory |
| [Generative Agents](https://research.google/pubs/generative-agents-interactive-simulacra-of-human-behavior/) | Experience storage, reflection and retrieval | Research on agent behavior in a simulated setting |
| [LongMemEval](https://proceedings.iclr.cc/paper_files/paper/2025/hash/d813d324dbf0598bbdc9c8e79740ed01-Abstract-Conference.html) | Long-term chat memory evaluation | Does not test memory allocator correctness |
| [LoCoMo](https://aclanthology.org/2024.acl-long.747/) | Very long-term conversational memory benchmark | Evaluate version, scoring and scope |
| [PagedAttention](https://arxiv.org/abs/2309.06180) | Block-based attention cache and sharing | Runtime-specific interface and compatibility |
| [FlashAttention](https://arxiv.org/abs/2205.14135) | IO-aware tiled attention computation | Not a general allocator or automatic KV elimination |
| [H2O](https://arxiv.org/abs/2306.14048) | Cache eviction research | Quality and workload sensitivity |
| [CacheGen](https://doi.org/10.1145/3651890.3672274) | KV compression and streaming | Measure total cost and quality |
| [LMCache](https://github.com/LMCache/LMCache) | Multi-tier inference cache project | Check supported engine/model/layout revisions |
| [LLAMA](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) | Learned lifetime prediction for C++ allocation | Study applicability on ARM and edge workloads |
| [MiniMalloc](https://research.google/pubs/minimalloc-a-lightweight-memory-allocator-for-hardware-accelerated-machine-learning/) | Static allocation planning for accelerators | Distinct from general dynamic heap |
| [HALP](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/) | Learned cache eviction with heuristics | Different domain; compare cost to simple policies |
| [Qwenputer](https://github.com/divagr18/qwenputer) | Author-reported model-contained RISC-V machine | Experimental target; verify exact revision and guest/host state |
| [Transformer VM](https://github.com/Percepta-Core/transformer-vm) | Analytically constructed transformer executing a WASM-like machine | Specific opcodes and implementation limits |
| [Linux CXL](https://cxl.docs.kernel.org/) | CXL memory resource support | Platform-dependent capability, not universal coherence |
| [Vulkan memory](https://docs.vulkan.org/spec/latest/chapters/memory.html) | GPU memory types and operations | Query target and obey API synchronization |
| [CUDA unified/system memory](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html) | Hardware-dependent access and migration | Do not extrapolate to all devices |
| [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html) | Buffer sharing and synchronization | Driver support and permissions required |
| [Linux PSI](https://docs.kernel.org/accounting/psi.html) | Pressure and stall metrics | Availability and interpretation vary |

## How to use this ledger

For every adopted design, append author, date, title, immutable commit or DOI, relevant sections, scope, direct evidence, inference, license and planned reproduction. Do not copy source code, model weights, diagrams or extended passages without checking applicable rights and notices. Read full papers where implementation details or numerical claims are important; abstracts and repository READMEs are entry points, not a complete survey.
