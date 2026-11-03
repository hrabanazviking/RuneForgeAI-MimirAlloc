# RAG and retrieval-aware memory management

## Two complementary uses

**RAG for the advisor:** retrieve vetted documentation, past experiments and device-specific outcomes to help an optional language model explain or propose policy. **Memory management for RAG:** budget and place the documents, embeddings, indexes, candidate buffers and caches used by an application. Neither changes the semantics of raw `malloc` by itself.

The original [RAG research](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) combines parametric model memory with retrieved external passages. Retrieval can return outdated, irrelevant or malicious content; a citation helps trace a claim but does not prove it true. Keep allocator invariants and safety policy in deterministic trusted code, not a document selected by similarity.

## Suggested knowledge hierarchy

1. **Authoritative policy and API schemas:** versioned configuration and code reviewed by maintainers; retrieval does not override them.
2. **Hardware and runtime facts:** version-specific capability measurements and vendor/OS documentation, with timestamps and a source reference.
3. **Experiment records:** workload, environment, commands, result, uncertainty, failure and version pins. Treat observations as scoped evidence.
4. **Research notes:** proposed techniques, known limitations and literature references, explicitly labelled.
5. **Operational history:** anonymized pressure events and prior actions, subject to retention and access rules.

The advisor receives a compact, permission-filtered bundle rather than arbitrary user content. A deterministic retrieval layer checks metadata such as runtime version, device family, date, privacy scope and trust level. Contradictory or absent evidence must remain visible to the advisor and the user.

## Retrieval pipeline

`event summary → select authorized corpus → metadata filter → candidate retrieval → optional rerank → provenance and freshness check → bounded context → advisor proposal → deterministic validator`.

A keyword or structured SQL filter may beat a vector index on a small, exact telemetry corpus. ANN similarity can miss relevant items; measure recall against exact search. Never treat one nearest neighbor as proof that the same policy works on a different hardware generation or workload.

## Memory categories and budgets

- Original documents and immutable versions: authoritative source for the index, subject to explicit retention.
- Chunk text and metadata: may be cached, but ACLs and provenance must travel with results.
- Embeddings: derived from a particular embedding model/version and preprocessing configuration; embedding-model changes can require re-embedding.
- ANN index: graph, inverted lists or other structure with separate memory/storage costs. Whether it is rebuildable depends on the actual data pipeline; don't assume it can be deleted safely.
- Retrieval caches: queries, document IDs and result sets with security scope, corpus/version, ranking settings and expiry in the key.
- Reranking and context buffers: short-lived but can peak alongside inference and model loads.

For a rough raw-vector estimate, `number_of_vectors × dimensions × bytes_per_dimension`, plus metadata, ANN structure, copies, padding and database overhead. Example: 100,000 vectors × 384 dimensions × 4 bytes = 153,600,000 bytes ≈ 146.5 MiB for uncompressed raw floats alone. Quantization or disk-backed indexes change the tradeoff and retrieval quality; measure actual RSS, index size, load time and recall.

## Placement and invalidation

An explicit budget might keep frequently accessed index partitions in host memory while storing source documents on local SSD, with optional remote replicas. This is only feasible if the chosen retrieval engine exposes such controls. On document update or deletion, invalidate/rebuild affected chunks, embeddings, indexes and query caches according to version and privacy policy. A cached result from a different tenant, document version, embedding model or permissions scope is not reusable. Retrieval does not grant permission to move a document to another device.

## Protecting decisions

Retrieved text is untrusted data and may contain instructions to alter policies or reveal secrets. Mark source boundaries and apply strict output schemas; the deterministic guard only allows authorized, validated actions. Do not transmit workload data or private documents to a remote model without an explicit configured data policy. Give the user a model-free and retrieval-free mode.

## Experiments

- Compare structured lookup, keyword search and vector search on real sanitized diagnostics; use exact-match ground truth and report recall, latency, index memory and update cost.
- Compare static advisor prompt vs RAG-assisted advisor on held-out incidents. Measure correct recommendations, rejected actions, hallucinated sources, token/input cost and total system overhead.
- Test outdated docs, conflicting facts, malformed chunks, deleted documents, cross-tenant queries and prompt-injection text. A model should not gain authority from a retrieved document.
- For application RAG, compare all-memory vs selected disk-backed index under repeated and cold queries; report answer quality, throughput, p95 latency, peak memory and storage reads.

## Sources

- [Original RAG research](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html)
- [FAISS library and research](https://github.com/facebookresearch/faiss)
- [DiskANN project](https://github.com/microsoft/DiskANN)
- [Memory-R1 agent-memory research](https://arxiv.org/abs/2508.19828) — semantic agent memory, not proof of raw allocator correctness.
