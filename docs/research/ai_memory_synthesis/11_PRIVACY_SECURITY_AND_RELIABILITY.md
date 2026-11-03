# Privacy, safety, reliability and provenance

## A shared fabric needs explicit boundaries

AI memory can contain private conversations, source documents, experimental logs, model state and derived representations. A unified management interface must not flatten permissions or turn retrieval relevance into authorization. Different users and devices may share infrastructure without sharing data.

## Core invariants

- Deterministic code validates address bounds, ownership, alignment, lifetime and device completion.
- A model can propose a permitted policy but cannot authorize access, free live objects, change hard limits or run arbitrary commands.
- RAG results are untrusted evidence, subject to source, version, scope and deletion rules.
- KV/state reuse requires exact runtime compatibility and security scope; false sharing is a correctness and privacy failure.
- A remote object's generation, rights, idempotency and fencing are enforced by the authority and any direct-access mechanism.
- A checkpoint captures a consistent safe point and is bound to compatible model/runtime/state versions.
- Missing LLM, RAG, network or accelerator support must not prevent the deterministic core from operating within its declared scope.

## Data lifecycle

Define authorized collection, purpose, retention, access, update, export and deletion. Track derived chunks, embeddings, index entries, summaries, caches and replicas; deleting one source does not automatically erase all derivatives. Distinguish a deletion request from guarantees about backups, persistent logs, training or third-party services. Describe the actual implementation rather than promising universal erasure.

Hashes are useful for identity and integrity; they are not encryption or authorization and can reveal information through guessable inputs. Cross-tenant caches need scope isolation and suitable collision/side-channel analysis. Avoid logging prompts, embeddings or raw memory when aggregate metrics suffice.

## Prompt-injection and policy validation

A retrieved paper, log, device label or user-provided document can contain instructions that conflict with system policy. Treat it as data. An advisor emits only a typed allowlisted proposal with a current epoch and bounded parameters. Independent deterministic code checks permission, physical capability and invariants. Out-of-scope fields, stale context or model failure cause rejection or default behavior.

## Reliability across tiers

Do not use an LLM to determine whether an async GPU operation is complete: use the actual backend's synchronization contract. Network timeout is an unknown outcome until reconciled. Lease expiry alone cannot stop stale writes unless the actual authority or direct-access mechanism enforces fencing/revocation. A replicated volatile object is not automatically durable. Device loss and corrupted checkpoint must lead to defined errors or validated recovery.

## Source and implementation provenance

Original-language synthesis means these documents paraphrase and cite ideas rather than copying project code. It is not evidence that two independent teams were used or that nobody saw upstream implementation details. A strict clean-room claim requires separately documented roles, materials, access and review. Maintain a source ledger and review the licenses of code, models, firmware, datasets and dependencies before reuse; MIT project licensing does not supersede third-party terms.

## Sources

- [vLLM security](https://docs.vllm.ai/en/latest/usage/security/)
- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
- [Linux RDMA](https://docs.kernel.org/infiniband/user_verbs.html)
- [Qwenputer RV32 notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
