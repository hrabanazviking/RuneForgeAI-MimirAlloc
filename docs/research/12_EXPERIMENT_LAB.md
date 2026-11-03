# Experimental lab and measurement recipes

This is a research plan; none of these experiments has been executed by the documentation authors. Commands below inspect common Linux information and may not exist or be permitted on a particular target. They do not configure the system or require elevated access.

## Lab notebook

Record date, operator, hypothesis, machine model, CPU and memory, OS/kernel, ABI/libc, page size, compiler/linker, allocator revision/build flags, workload revision and all non-default settings. Record power mode, thermal conditions, competing processes, data/model checksum and scope of private logs. Preserve command lines and result schema. Identify physical hardware, emulator or cross-build explicitly. Do not publish tokens, addresses or serial numbers.

```sh
uname -a
getconf PAGESIZE
getconf LONG_BIT
cat /proc/meminfo | head -n 15
cat /proc/pressure/memory 2>/dev/null || true
```

These are snapshots and do not establish peak available memory; `/proc` can be unavailable in restricted environments. Device or container limits must be measured separately.

## Experiment A — size-class waste

**Question:** which size-class scheme is appropriate for a real workload? Capture a sampled distribution of requested sizes and allocation counts, plus phase and lifetime if available. Replay the same trace through candidate mappings. For each, compute rounding waste and estimate per-class occupied slabs, but label this as an estimate because placement, lifetime and concurrency change actual retention. Run actual prototypes later, measuring metadata and RSS. Report worst, median and phase-specific results; a global average may hide pathological classes.

## Experiment B — remote free and cache caps

**Setup:** one or more producers allocate objects; consumers free them across threads. Include same-thread control, 1:1 and many:many, varied object sizes, burst/idle, thread exit, and queue saturation. Repeat with bounded local-cache and remote-queue sizes. **Measurements:** application operations/s, p99 allocation/free time, live/held bytes, peak RSS, post-idle RSS, context switches and correctness failures. A queue reaching its cap must have a safe fallback. Compare the same work and thread placement across runs.

## Experiment C — constrained-memory lifecycle

Use a disposable test process or isolated environment with known memory limits and a clearly recorded baseline. Allocate and touch pages; free half in controlled patterns, request trim and repeat. Observe process survival, faults, pressure, live vs held memory and recovery. A mapping can succeed before later page fault; OOM may kill a process instead of returning a conventional error. Do not run a destructive pressure test on a machine with unsaved work. Distinguish global host, cgroup and per-process signals.

## Experiment D — inference lifecycle

Select one model and backend that actually run on the device. Fix model checksum, format, prompt, output length, batch, context, runtime and output validation. Measure idle → load → prefill → decode → idle → unload, repeatedly. Capture time to first token, generation latency, throughput, physical-memory estimates, device memory when observable, page faults and peak. Compare system allocator to an explicitly integrated candidate and prove allocator binding. Include session cancellation and reload.

## Experiment E — accelerator pool

Pick an available GPU API and runtime; first query capabilities. Allocate buffers, submit kernels and transfer results with proper events; compare bounded pooling against direct allocation. Validate each output and test interleaving multiple queues. Delay completions intentionally to detect premature reuse. Report host and device memory separately, and note shared physical backing on UMA. Measure API call overhead, total operation time, memory and actual data movement. Do not claim zero-copy without evidence.

## Experiment F — networked buffer service

Use two isolated test machines and synthetic non-sensitive data. Define object generation, byte order, checksum, timeout, quotas, auth and completion first. Compare a local-buffer control to a message-based remote-buffer prototype. Inject timeout after request submission, dropped responses, restart, duplicate requests and stale-handle reuse. Record recovery and integrity before bandwidth; report p50/p95/p99, throughput, transferred bytes, CPU and cost. Do not expose an unauthenticated service.

## Statistical discipline

Warm up where appropriate but also report cold start separately. Repeat complete runs, not just inner operations, and retain per-run observations. Randomize or alternate candidate order to reduce thermal/time drift. Show median and quantiles with sample counts, plus variation or confidence intervals. State exclusions, failures, background load and measurement resolution. A 1% change smaller than observed noise is not an established win.

## Reproducibility deliverable

Each experiment should eventually produce a committed protocol, machine-readable environment metadata, permitted workload or retrieval instructions, result tables, raw logs with privacy review, plotting script, analysis, negative results and exact version pins. Keep large traces or proprietary models out of git unless explicitly licensed and intended for publication.
