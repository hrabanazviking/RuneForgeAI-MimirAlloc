# Cross-domain design studies: useful ideas and experiments

These designs synthesize ideas from multiple sources in original language. They are hypotheses, not implementations or claims that the source authors proposed this exact architecture. Each needs a deterministic baseline, compatible interfaces and reproducible tests.

## Study A — Jointly budgeting retrieval and inference caches

**Problem:** a local inference application has a fixed memory budget shared by a retrieval index, embedding model, main model, KV cache and system. Managing each independently can produce total pressure even though every component stays within its own cap.

**Proposed design:** the application registers each domain's required minimum, preferred capacity, actual accounting and explicitly reclaimable bytes. A deterministic budget manager first guarantees required memory and system reserve, then apportions surplus. The retrieval adapter can evict *rebuildable cache entries* while preserving authoritative documents and ACLs. The inference runtime can choose among unreferenced or reconstructible KV blocks. A controller may suggest shares, but each domain validates eligibility. No component may delete live state simply to satisfy a global score.

**Why it follows from the research:** [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) and [PagedAttention](https://arxiv.org/abs/2309.06180) show separate retrieval and cache mechanisms. Coordinating their budgets is an integration proposal, not a result established by either source.

**Experiment:** compare independent fixed quotas, a deterministic shared quota and a guarded learned suggestion under repeated-document and diverse-document workloads. Measure answer quality, retrieval recall, TTFT, token latency, cache churn, page faults, actual peak physical memory and controller overhead. Include a workload phase change; a policy that reduces bytes but loses critical evidence may not be useful.

## Study B — Provenance-aware policy advisor

**Problem:** a model asked to optimize an allocator may confidently apply a benchmark from a different platform or version.

**Proposed design:** a structured evidence store holds target OS, CPU, page size, runtime, build hash, workload, measurements and uncertainty. A query filters access and target constraints before retrieval. A small advisor produces a typed, bounded proposal along with evidence IDs. A deterministic validator rejects unsupported targets, stale telemetry and changes outside policy. The advisor can also return `no_change` or `insufficient_evidence`.

**Experiment:** compare fixed rules, structured lookup plus heuristic, a small predictor, an LLM without retrieval and the same LLM with retrieval. Use held-out device/revision combinations and deliberately conflicting sources. Measure valid actions, evidence correctness, net workload result, latency, memory and energy. Retrieved text is untrusted; an injected instruction must not change the action allowlist.

## Study C — Lifecycle-aware memory records

**Problem:** model states, semantic records and heap objects all have lifetimes, but their valid operations differ.

**Proposed design:** a common *descriptor* records identity, owner, expiry, access scope, size and provenance. Domain-specific extensions define whether a resource is movable, reconstructible, durable, active in a device queue or subject to deletion. The descriptor coordinates accounting; it does not replace each domain's own semantics. A KV runtime remains authoritative about cache compatibility; a document store remains authoritative about versions and ACL; a CPU allocator remains authoritative about pointer lifetime.

**Experiment:** implement a small schema and simulated adapters for immutable records and reclaimable buffers. Inject stale leases, incompatible model versions, privacy deletion and in-flight use. Evaluate how often the schema can correctly express each domain and where extra fields are required. Only connect real backends after each adapter has tests.

## Study D — Lifetime predictor informed by app phases

**Problem:** short- and long-lived heap objects sharing pages can cause retention. A per-site predictor may become stale when the application shifts from model load to interactive generation.

**Proposed design:** train or calculate lifetime classes using privacy-reviewed allocation-site statistics and coarse application phase, with an `unknown` class. The allocation mechanism is correct for any class. Cap the number and total size of arenas, and decay or invalidate statistics on workload/version shifts. Compare a counter, tree, small sequence model and optional general LLM; do not assume the largest model wins.

**Experiment:** collect and replay separate runs, including phase reversal, then run real workloads. Handle right-censored observations. Measure lifetime prediction, retained memory, size-class waste, page faults, p99 and predictor cost. The research precedent is [LLAMA](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/); this particular multi-phase controller is a proposal.

## Study E — Verified state checkpoints across tiers

**Problem:** resuming an LLM or neural-computer guest may require more than restoring one byte buffer.

**Proposed design:** a checkpoint manifest records model/firmware hashes, state schema, tokenizer or input cursor, KV/recurrent state, guest memory, allocator metadata, dtype, backend and pending I/O. Capture at a validated safe point. A restore validates every required component and compatibility before resuming; otherwise recompute from permitted source inputs or fail.

**Experiment:** compare exact resume against uninterrupted execution on a named runtime. Change one factor at a time—runtime version, precision, device or corrupted segment—and require correct rejection or validated conversion. Measure snapshot latency, peak duplicate memory, storage traffic and privacy controls. A manifest/checksum does not create compatibility by itself.

## Study F — Federated inference on constrained devices

**Problem:** multiple devices offer capacity and compute but have different support, speed and ownership.

**Proposed design:** register capabilities and quotas for an ARM host, a supported GPU backend and an authorized remote worker. A scheduler filters hard requirements before ranking measured transfer and compute costs. Share immutable model or retrieval blocks only through supported interfaces; avoid raw cross-host pointers. An optional tiny advisor predicts demand on a slow timescale, while deterministic code handles reservation and fallback.

**Experiment:** compare local execution, explicit application-managed transfer and fabric-managed transfer. Report physical memory, network traffic, latency, cost, energy and failure behavior. Test node loss and tenant isolation. No assumption of pooled coherent RAM or universal device access is allowed.

## Study G — A neural-computer allocator test

**Problem:** a model-contained processor may have a tiny address space whose host representation is much larger.

**Proposed design:** implement a freestanding fixed-arena allocator for the supported guest ISA and separately budget host weights, state, trace and snapshots. Exchange bounded operations through a real host bridge; compare actual free-running output with conventional emulation and an upstream reference.

**Experiment:** allocate, write, free, reuse, exhaust, snapshot and restore under the documented memory map. Track guest instructions, trace tokens, host memory and wall time. The creator's [Qwenputer notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md) give a concrete starting point; compatibility must be demonstrated for a pinned version.
