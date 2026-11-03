# Experimental program and evaluation framework

All experiments are proposed. No benchmark, model test or device compatibility result is claimed here. Preserve the deterministic allocator baseline and avoid destructive pressure tests on a machine with unsaved work.

## Common reproducibility record

For every run record source and dependency versions, allocator build and flags, OS/kernel, CPU/GPU/NPU, memory, actual page size, model ID and immutable revision, tokenizer, quantization, prompt/context, retrieval index version, backend, device power mode and workload hash. Identify physical device versus emulator and cold versus warm state. Store privacy-reviewed traces and random seeds; include failures and skips.

## Experiment A — Can a tiny LLM beat simpler controllers?

**Treatments:** fixed safe configuration; hand-written heuristic; small classifier/tree; small generative language model; LLM+RAG. Run a shadow phase first. Use identical workload arrivals, memory ceilings and hard constraints. Tasks: phase detection, lifetime-class suggestion and cache cap selection. Separate training, validation and held-out test workloads by run/device/version.

**Measurements:** total completed work, p50/p95/p99 latency, peak and time-series RSS/PSS where available, live and retained bytes, pressure stall, faults, advisor loading/steady/transient footprint, inference latency, energy and invalid/stale proposals. Measure per-op telemetry overhead. Count failures. **Success:** predeclared net gain after including controller overhead, no correctness regression, and stable behavior under shifts. A no-LLM result is valid and valuable.

## Experiment B — Lifetime-aware allocation

Run a reproducible sequence mixing short- and long-lived allocations, then a real application workload. Compare baseline, application-provided hints, simple site predictor and learned predictor. Collect lifetime buckets, predicted probabilities and misses; account for right-censored allocations. Report size-class waste, live vs retained bytes, reclaimable pages, page faults, throughput and p99. Test a phase reversal that makes historical predictions wrong. Validate that all objects remain valid irrespective of classification.

## Experiment C — RAG support

Create a small authorized corpus of versioned manuals and previous sanitized test results. Compare deterministic filtered lookup, keyword search and ANN retrieval, then advisor without and with retrieval. Use a known-answer set with conflicting, stale and poisoned documents. Measure retrieval recall, correct grounded proposals, invalid actions, source provenance, elapsed time and memory for document store/index/embedding model. Demonstrate that access filtering prevents cross-tenant retrieval. Evaluate answer quality separately from allocation performance.

## Experiment D — KV cache

Fix model revision, prompt tokens, adapter, position setup, runtime and generation parameters. Compare no reuse, exact prefix reuse, and supported offload/compression options. Sweep block size and session load. Measure TTFT, decode latency, throughput, cache hit, physical host/device peak, transfer bytes, recompute, error and output-quality measures. Test mismatched model/tokenizer/adapter/tenant. A false hit or premature reuse is a failure, even if throughput improves.

## Experiment E — Managed memory federation

Start with two local supported backends (CPU and a device buffer) and a single immutable managed object. Then test bounded transfer to a separate process or remote node. Report actual physical backing, pool capacity, copy count and time, in-flight memory, synchronization latency, bandwidth, failure and CPU use. Compare with manual application-managed transfers. For shared physical UMA pages, avoid adding duplicate counters. Test inaccessible or unsupported NPU buffers as an explicit rejection, not a fake capacity.

## Experiment F — distributed reliability

With authorized isolated test nodes, inject lost replies, retries, partitions, stale generations, long-running device operations, provider restarts, disk full and corrupted checkpoints. Verify access controls, bounded resource retention, idempotency, authoritative fencing and recovery. Distinguish transport completion, durable commit and verified result. Log sanitized identifiers, not private inputs.

## Experiment G — interaction and interference

Compare control settings with the advisor disabled, CPU-hosted and accelerator-hosted while the same inference job runs. Quantify resource contention, tail latency and energy. Repeat under changing model load and the simultaneous RAG/KV workload. Test hysteresis and rate limits during bursts; document whether the advisor can recover or stop safely under memory pressure.

## Statistical and reporting standard

Run complete trials multiple times, alternate treatment order and record background activity and temperature. Report sample size, distributions and uncertainty. Separate latency of the policy itself from total workload time. Evaluate real-device behavior separately for ARM Linux, Android and each accelerator. Cite source claims with their original scope and versions. A measured improvement on one task is not a universal gain.

## Sources

- [Learning-based allocation](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [PagedAttention](https://arxiv.org/abs/2309.06180)
- [CacheGen](https://doi.org/10.1145/3651890.3672274)
- [Linux PSI](https://docs.kernel.org/accounting/psi.html)
