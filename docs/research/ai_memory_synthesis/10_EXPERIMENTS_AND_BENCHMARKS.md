# Evaluation: measure memory, quality and correctness together

## Why separate benchmark families

A system can excel at conversational recall and still mishandle a raw pointer. It can reduce GPU KV usage and increase total host memory or damage answer quality. It can optimize allocator throughput while increasing tail latency and thermal pressure. Evaluate each domain separately, then measure combined application outcomes.

## Evidence matrix

| Domain | Baseline | Outcome | Failure cases |
|---|---|---|---|
| Heap | system allocator and deterministic prototype | contract correctness, live/retained bytes, p99, throughput | overflow, OOM, cross-thread frees |
| Advisor | no advisor, heuristic and small classifier | net workload benefit after model overhead | invalid output, drift, recursion |
| Retrieval | exact/filtered lookup and lexical search | recall, attribution, temporal accuracy, cost | stale data, denied ACL, prompt injection |
| Agent memory | no memory, raw history and hybrid | retrieval and answer accuracy, updates, abstention | conflicting evidence, wrong speaker |
| KV | native engine baseline | TTFT, decode, quality, hit and memory | false hit, active-block eviction |
| Device fabric | direct application buffer management | transfer/copy time, peak physical RAM | unsupported import, premature reuse |
| Network | local and conventional object service | latency, recovery, integrity and cost | lost reply, partition, stale generation |
| Neural guest | native/emulated reference | guest contract, free-running correctness, resources | divergence, timeout, out-of-arena |

## Semantic-memory benchmarks

[LongMemEval](https://proceedings.iclr.cc/paper_files/paper/2025/hash/d813d324dbf0598bbdc9c8e79740ed01-Abstract-Conference.html) tests extraction, multi-session reasoning, time, updates and abstention. [LoCoMo](https://aclanthology.org/2024.acl-long.747/) tests long conversational histories. Document benchmark version, prompts, model, evaluation method and license. Scores from different datasets, judges, prompts or subsets are not directly comparable. Add MimirAlloc-specific questions about versioned experiments, stale device measurements and invalid configuration; use held-out real or synthetic cases with source provenance.

## Allocator and policy experiments

A. Measure fixed-size and mixed-size traces, cross-thread frees, repeated load/unload and pressure. Verify content/non-overlap and audit actual integration. B. Predict lifetime class in shadow mode; compare a simple counter, specialized model and optional tiny LLM. Handle right-censoring and separate train/test by run/version/device. C. Enable one bounded reversible knob in a canary; test phase changes and rollback. A policy must be correct for mispredictions and beneficial after its own RAM, CPU/GPU and energy cost.

## KV and RAG experiments

Run the same prompt/document corpus with a fixed model revision and runtime. Compare no prefix reuse, runtime-native reuse, and any supported offload. Record TTFT, decode latency, exact compatibility, memory, transfer, recomputation and output quality. Test different adapters, positions, tenants and document versions as deliberate cache misses. For retrieval compare structured, lexical, ANN and graph methods using labeled evidence, record recall and access violations, plus indexing/embedding/reranking cost.

## Reproducibility protocol

Record date, hardware, OS/kernel, page size, compiler/build, model and tokenizer hashes, runtime, precision, datasets, seed, configuration, workload, warm/cold state, safety limits and process isolation. Alternate treatment order, repeat entire trials and provide sample counts, p50/p95/p99 and variability. Describe missing metrics and failed runs. When devices use shared physical memory, do not sum duplicate host/GPU views; disclose uncertainty.

Test under constrained memory only in disposable environments without unsaved work. Model/controller OOM, missing retrieval, reset and remote outage are required negative paths. A successful synthetic test is not proof of actual Mojo integration or production readiness.

## Sources

- [LongMemEval](https://proceedings.iclr.cc/paper_files/paper/2025/hash/d813d324dbf0598bbdc9c8e79740ed01-Abstract-Conference.html)
- [LoCoMo](https://aclanthology.org/2024.acl-long.747/)
- [Linux PSI](https://docs.kernel.org/accounting/psi.html)
- [PagedAttention](https://arxiv.org/abs/2309.06180)
