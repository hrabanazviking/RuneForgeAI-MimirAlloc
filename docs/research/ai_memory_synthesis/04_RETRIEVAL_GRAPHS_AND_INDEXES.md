# Retrieval, indexes, graphs and evidence

## Retrieval choices

A retrieval system selects external material to answer a question or inform a policy. Start with the user's actual problem: exact device and revision lookup often favors structured filters or keyword search; semantic embeddings can find related language; graph links can help multi-hop questions. Combining methods may improve coverage, but adds memory, indexing cost and new failure modes. [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) is a broad research family, not a guarantee that retrieved text is true.

## Three representations

**Documents and chunks:** preserve the original record, section, author, date and version. Chunking affects context and can split a qualifying caveat from its claim. **Embeddings and ANN:** numerical representations permit approximate nearest-neighbor search; the index can miss relevant items, and embedding model changes can invalidate prior vectors. **Graphs:** entities and edges can represent relationships and time, but extracted edges may be wrong, ambiguous or omit context. Indexes and graphs are derived structures, not necessarily authoritative sources.

[HippoRAG](https://papers.nips.cc/paper/2024/hash/6ddc001d07ca4f319af96a3024f6dbd1-Abstract-Conference.html) explores combining a knowledge graph with a retrieval method inspired by biological-memory concepts. [GraphRAG](https://www.microsoft.com/en-us/research/publication/from-local-to-global-a-graph-rag-approach-to-query-focused-summarization/) studies graph-based summaries for collection-level questions. Compare both to simpler baselines on the actual corpus; graph construction and maintenance may be expensive, and biological analogies do not establish biological equivalence.

## Proposed research-evidence retrieval for MimirAlloc

A result includes `source`, `version`, `claim_kind`, `target_arch`, `OS`, `runtime`, `date`, `license`, `experiment_id`, `result`, `limitations` and visibility scope. Queries first filter by authorization and compatible targets; then combine exact match, lexical retrieval and optional vector/graph ranking. The system should return conflicting and negative results when relevant. Model-generated summaries carry links to original measurements; they are not authoritative configuration.

A query such as “Does cap X reduce memory on device Y?” should retrieve: measured outcome for Y's exact configuration, adjacent but non-identical environments, failures, methodology and conditions. If no matching experiment exists, the correct output is an untested hypothesis and proposed measurement.

## Resource-aware index design

Budget source text, embeddings, metadata, index, caches and transient reranking. Raw vector bytes are `count × dimensions × bytes-per-dimension`; graph links and implementation overhead add more. For example, 100,000 × 384 × 4 bytes ≈ 146.5 MiB before index overhead. Disk-backed search may reduce resident memory but increase storage reads and tail latency. Compression can alter recall. The exact choice must be measured, not inferred from the name of an index.

Cache keys include tenant/security scope, corpus version, query and retrieval settings, embedding/reranker version and applicable deletion state. On source correction or deletion, propagate changes to derived chunks, embeddings, graphs, caches and replicas according to policy. A checksum detects changes but is not access control.

## Evidence security

Retrieved material may contain instructions, private information or fabricated claims. Mark it as untrusted data. A deterministic policy validator controls any proposed memory setting; no passage may authorize device access or relax quotas. Never send private data to an external embedding or language model without an authorized deployment policy.

## Experiments

- Compare filtered SQL/metadata, lexical, dense ANN and hybrid retrieval on a fixed, permission-scoped corpus.
- Score recall against exhaustive or human-reviewed references, temporal accuracy, contradiction handling and abstention.
- Measure index build/update time, resident memory, peak memory, disk/network use and p95 latency.
- Inject a stale source, wrong device, deleted record, cross-tenant item and malicious instruction. A correct system does not treat them as authority.

## Sources

- [RAG original paper](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html)
- [HippoRAG](https://papers.nips.cc/paper/2024/hash/6ddc001d07ca4f319af96a3024f6dbd1-Abstract-Conference.html)
- [GraphRAG research](https://www.microsoft.com/en-us/research/publication/from-local-to-global-a-graph-rag-approach-to-query-focused-summarization/)
- [FAISS](https://github.com/facebookresearch/faiss)
- [DiskANN](https://github.com/microsoft/DiskANN)
