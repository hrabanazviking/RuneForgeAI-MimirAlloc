# Agent memory, provenance and changing facts

## What persistent agent memory does

A memory-enabled agent may store interactions, tasks, decisions, observations, tool results and derived summaries, then retrieve a relevant subset for later work. [Generative Agents](https://research.google/pubs/generative-agents-interactive-simulacra-of-human-behavior/) studies recorded experiences, reflection and retrieval in a simulated environment. [MemGPT](https://arxiv.org/abs/2310.08560) investigates an OS-inspired approach for moving information among model context and external storage. Both are research architectures; neither establishes that stored assertions are automatically accurate or that an agent has unlimited context.

## Separate records by role

| Record type | Example | Handling |
|---|---|---|
| Observation | A test emitted an error | Preserve source, time and environment |
| User statement or preference | A chosen output format | Preserve who stated it and whether it was superseded |
| Tool result | A device reports an available memory budget | Attach measurement time, scope and known uncertainty |
| Inference | A suspected cause of allocation pressure | Mark as hypothesis, link observations |
| Decision | A cache cap was chosen | Record actor, authorization, bounds and result |
| Summary | A compressed project history | Link back to source records and mark omissions possible |

A derived summary should not overwrite the original evidence simply because it is shorter. Contradictions can indicate updates, different scopes or mistakes; keep effective time and record time separate where needed. A memory query should be able to ask “what was believed then?” and “what is currently supported?”

## Proposed record schema

Fields may include `record_id`, `type`, `subject`, `claim`, `source_id`, `source_version`, `observed_at`, `effective_from`, `effective_until`, `scope`, `confidence_or_evidence`, `visibility`, `supersedes`, `created_by`, `retention` and `schema_version`. This is a research schema, not a deployed API. Use structured identifiers and allow unknown values rather than invented precision. An author-generated confidence score is not evidence that a claim is true.

## Write path

1. Receive an observation from an authorized source and preserve its provenance.
2. Validate format, bounds, tenant scope and retention requirements.
3. Detect candidate duplicates or contradictions without automatically merging incompatible claims.
4. Optionally create a summary or graph edge, explicitly marking it as derived.
5. Index accessible material and track index revision and derived artifacts.
6. Allow correction, supersession and deletion according to the actual product contract.

## Read path

A query specifies purpose, scope, time and permissions. Filter by access *before* exposing text to the model, retrieve multiple candidates, rank by relevance and valid time, and include a concise provenance trail. If evidence is absent or contradictory, the system should be able to say so. Semantic proximity is not factual identity; two similar GPU models or allocator revisions can have very different behavior.

## Implications for MimirAlloc

An optional project advisor can retrieve previous experiments and learn which policy helped a similar workload, but a past decision is not a current capability check. The deterministic resource manager still checks real device availability, quotas and ownership. A semantic memory store should be outside the allocator fast path and independently replaceable. MimirAlloc need not become a general companion-memory application to benefit from versioned experiment records.

## Evaluation

[LongMemEval](https://proceedings.iclr.cc/paper_files/paper/2025/hash/d813d324dbf0598bbdc9c8e79740ed01-Abstract-Conference.html) examines extraction, multi-session reasoning, temporal reasoning, updates and abstention. [LoCoMo](https://aclanthology.org/2024.acl-long.747/) studies long conversational histories. These test semantic/agent memory, not pointer safety, cache synchronization or allocator fragmentation. Test MimirAlloc's use case with additional synthetic device-version changes, stale benchmark data, incorrect summaries and denied access. Measure recall, source correctness, update latency and cost; inspect failures rather than relying only on a single score.

## Sources

- [Generative Agents](https://research.google/pubs/generative-agents-interactive-simulacra-of-human-behavior/)
- [MemGPT](https://arxiv.org/abs/2310.08560)
- [LongMemEval](https://proceedings.iclr.cc/paper_files/paper/2025/hash/d813d324dbf0598bbdc9c8e79740ed01-Abstract-Conference.html)
- [LoCoMo](https://aclanthology.org/2024.acl-long.747/)
