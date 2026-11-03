# Source-to-engineering matrix and open questions

The following entries connect each primary source to a specific engineering question. The last column identifies an evidence gap; it is not a promise that the referenced method can be directly transplanted into MimirAlloc.

| Research | What the source investigates | Proposed MimirAlloc experiment | Important gap |
|---|---|---|---|
| [Neural Turing Machines](https://arxiv.org/abs/1410.5401) | Learned access to differentiable external memory | Compare learned ranking with explicit retrieval | Soft attention is not byte-exact ownership |
| [Transformer-XL](https://arxiv.org/abs/1901.02860) | Segment recurrence | Test a supported model-state snapshot | State format and positional compatibility |
| [Mamba](https://arxiv.org/abs/2312.00752) | Input-selective state-space model | Account recurrent state separately from KV | Architecture and kernel support |
| [Lost in the Middle](https://arxiv.org/abs/2307.03172) | Position effects in long contexts | Move relevant evidence through prompt | Results depend on model and task |
| [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) | Retrieval with generation | Retrieve versioned allocator experiments | Relevance is not correctness or authorization |
| [HippoRAG](https://papers.nips.cc/paper/2024/hash/6ddc001d07ca4f319af96a3024f6dbd1-Abstract-Conference.html) | Graph and associative retrieval | Evaluate multi-hop source links | Graph build cost and extraction errors |
| [MemGPT](https://arxiv.org/abs/2310.08560) | Context and external-store management | Compare summarized vs raw experiment history | Semantic memory differs from OS paging |
| [Generative Agents](https://research.google/pubs/generative-agents-interactive-simulacra-of-human-behavior/) | Event streams and reflections | Keep evidence and derived summaries separate | Reflection can introduce unsupported claims |
| [LongMemEval](https://proceedings.iclr.cc/paper_files/paper/2025/hash/d813d324dbf0598bbdc9c8e79740ed01-Abstract-Conference.html) | Long-term chat memory benchmark | Test updates, abstention and retrieval | Does not test native allocator safety |
| [LoCoMo](https://aclanthology.org/2024.acl-long.747/) | Long conversational history | Evaluate long-range evidence recall | Benchmark domain differs from device telemetry |
| [PagedAttention](https://arxiv.org/abs/2309.06180) | Block-based cache allocation | Integrate a cooperating inference runtime | Engine-specific layout and compatibility |
| [FlashAttention](https://arxiv.org/abs/2205.14135) | Attention IO | Measure whole inference workload and data movement | Kernel availability and backend compatibility |
| [H2O](https://arxiv.org/abs/2306.14048) | KV eviction policy | Compare memory reduction vs answer quality | Eviction may alter model behavior |
| [CacheGen](https://doi.org/10.1145/3651890.3672274) | KV compression and streaming | Compare offload vs recomputation | Precision, model and network differences |
| [LLAMA](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) | Lifetime prediction for heap placement | Compare size/site counter vs learned policy | Server assumptions may not fit edge |
| [MiniMalloc](https://research.google/pubs/minimalloc-a-lightweight-memory-allocator-for-hardware-accelerated-machine-learning/) | Accelerator static allocation | Evaluate tensor-lifetime planning | Graph scheduling is not generic malloc |
| [HALP](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/) | Learned/heuristic eviction | Compare ranking policies in shadow mode | CDN cache and AI memory differ |
| [Linux CXL](https://cxl.docs.kernel.org/) | Exposing CXL-backed capacity | Test explicitly managed tier on compatible hardware | Cross-host sharing/coherence is conditional |
| [DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html) | Shared device buffers/fences | Validate one import/export path | Not every device or driver supports it |
| [Qwenputer](https://github.com/divagr18/qwenputer) | Author-reported in-model RISC-V | Run a bounded guest arena with references | Modified model, host runtime and exactness conditions |

## Cross-cutting unknowns

1. Which model-state APIs does the exact intended A.E.S.I.R. runtime expose? A useful proposal depends on actual runtime interfaces.
2. What percentage of total device pressure is controlled by a replaceable CPU allocator, versus model/driver workspaces and other applications?
3. Does a small LLM outperform a site counter after including its weights, KV, retrieval index, power and interference?
4. Can an application express immutable, movable and recomputable objects sufficiently to justify a managed fabric?
5. Which device pairs on each target truly support import/export, visibility and completion semantics?
6. Which memory tier makes sense for a given workload once real transfer, recompute, energy and failure costs are included?
7. Can evidence retrieval remain accurate after version changes and deletions without excessive index overhead?
8. Can state snapshots be restored with the required numerical equivalence across the exact runtime, device and precision?
9. What does a user want persisted, forgotten, shared or kept local? Product policy and access control must be explicit.
10. Which findings transfer from server studies to small physical ARM devices? Only new experiments can answer.

## Minimum result packet

A project result should identify source, hypothesis, code/version, target, baseline, experiment steps, expected and actual behavior, state and memory metrics, quality/correctness, statistical variation, negative outcomes and privacy scope. Label it research, prototype, device-tested or integration-proven. Archive scripts and sanitized inputs under their own licenses; avoid attributing speculative conclusions to the original authors.
