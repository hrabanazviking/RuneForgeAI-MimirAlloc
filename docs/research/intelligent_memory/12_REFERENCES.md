# Source catalog and evidence boundaries

Collected 2026-11-02. Primary research and official technical references are listed below. Their results have the scope of their own workloads, hardware and revisions. Check licenses and pin revisions before using code or artifacts; this catalog is not an exhaustive survey.

## Learned allocation and policy

1. [Learning-based Memory Allocation for C++ Server Workloads](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) — LLAMA uses a learned language model over allocation calling contexts to predict lifetimes and studies huge-page fragmentation. It is relevant precedent, not an instruction to run an LLM on every allocation.
2. [HALP: Heuristic Aided Learned Preference Eviction Policy](https://research.google/pubs/halp-heuristic-aided-learned-preference-eviction-policy-for-youtube-content-delivery-network/) — learned and heuristic CDN cache decisions; domain differs from an application heap.
3. [MiniMalloc](https://research.google/pubs/minimalloc-a-lightweight-memory-allocator-for-hardware-accelerated-machine-learning/) — static allocation planning for specialized accelerators; not a general dynamic CPU allocator.
4. [Characterizing a Memory Allocator at Warehouse Scale](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/) — workload evidence from TCMalloc deployment; not a mobile-device benchmark.
5. [SmolLM2-135M-Instruct](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct) and [Qwen3-0.6B](https://huggingface.co/Qwen/Qwen3-0.6B) — candidate general models for controlled advisor experiments, not validated allocator policies.

## KV, retrieval and inference

6. [PagedAttention and vLLM](https://arxiv.org/abs/2309.06180) — logical blocks and cache sharing in the studied serving design.
7. [vLLM automatic prefix caching](https://docs.vllm.ai/en/latest/features/automatic_prefix_caching/) and [prefix-caching design](https://docs.vllm.ai/en/latest/design/prefix_caching/) — engine-specific caching behavior and keys.
8. [CacheGen](https://doi.org/10.1145/3651890.3672274) — KV compression and transmission research; results depend on workload and quality metrics.
9. [LMCache](https://github.com/LMCache/LMCache) and [architecture](https://github.com/LMCache/LMCache/blob/dev/docs/source/developer_guide/architecture.rst) — a multi-tier KV management project with runtime integration.
10. [RAG original research](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) — retrieval combined with model generation; not operating-system paging.
11. [FAISS](https://github.com/facebookresearch/faiss) and [DiskANN](https://github.com/microsoft/DiskANN) — vector retrieval implementations and research; compare actual index memory, recall and licenses.
12. [Memory-R1](https://arxiv.org/abs/2508.19828) — learned management of agent memories; semantic agent memory differs from safe handling of raw pointers.

## Heterogeneous systems and OS

13. [Linux memory management](https://docs.kernel.org/admin-guide/mm/index.html), [PSI](https://docs.kernel.org/accounting/psi.html) and [cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html) — OS resource concepts and interfaces.
14. [Linux CXL](https://cxl.docs.kernel.org/) and [CXL device types](https://docs.kernel.org/driver-api/cxl/devices/device-types.html) — OS and hardware support dependent on the actual topology.
15. [Vulkan memory specification](https://docs.vulkan.org/spec/latest/chapters/memory.html) and [embedded programming](https://docs.vulkan.org/guide/latest/embedded_programming.html) — device memory types, synchronization and embedded considerations.
16. [CUDA unified and system memory](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html) and [stream-ordered allocation](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html) — platform-specific access and lifetime contracts.
17. [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html) and [RDMA userspace verbs](https://docs.kernel.org/infiniband/user_verbs.html) — cross-device and network interfaces; different semantics.
18. [Android page sizes](https://developer.android.com/guide/practices/page-sizes) — native-library compatibility on 16 KiB devices and related build/testing requirements.

## Evidence ledger template

For a claim record: source title, author, URL, publication date, immutable revision, relevant section, result and measurement conditions, direct observation versus inference, license, and how/when MimirAlloc will verify it. Numeric results belong to their original experiments until reproduced. When incorporating code, review the separate project/model/firmware/dependency licenses and preserve required notices; MimirAlloc's MIT license does not erase third-party obligations.
