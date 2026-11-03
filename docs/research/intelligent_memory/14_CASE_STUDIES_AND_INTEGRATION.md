# Case studies: connecting intelligent policies, RAG and shared memory

These scenarios are experiments for future development, not claims that MimirAlloc already implements any of the described features. Each compares a deterministic baseline and reports total system costs.

## 1. Low-memory ARM tablet during model loading

A tablet begins loading a model, temporarily increasing host memory and potentially consuming GPU memory from the same physical RAM. A deterministic controller measures available budget and prevents unbounded new cache admissions. An optional small advisor reads only coarse telemetry, retrieves a matching *device, runtime and workload version* report, and suggests lowering a free-block cache cap. A guard checks the policy epoch, hard limits and whether the cap is a supported reversible knob. Existing allocations remain valid.

**Evaluate:** input telemetry overhead, advisor memory/latency, peak physical memory, first-token time, page faults and system responsiveness. Compare no advisor, heuristic and LLM+RAG. **Failure:** missing or stale records, controller OOM or invalid output falls back to the deterministic cap. An advisor that costs more RAM than it saves may not be a good fit.

## 2. A.E.S.I.R. serving repeated document questions

A user queries the same authorized document several times. A retrieval service selects document passages, and the inference engine may reuse an exact token prefix's KV blocks if model revision, tokenizer, position configuration, adapters, multimodal data and access scope match. RAG result caching and KV prefix caching are distinct: a similar query can retrieve the same passage without producing an identical prefix, and the engine must decide whether a particular KV block is compatible.

**Proposed MimirAlloc role:** account for index, retrieved text buffers, KV block budgets, device state and offload storage through cooperating runtime adapters. Track physical backing to avoid counting shared UMA pages twice. **Evaluate:** answer quality, retrieval recall, prefix hit rate, TTFT, decode latency, peak memory, transfer/recompute cost and access-control violations. **Failure:** stale source, changed ACL or incompatible prefix must miss/retrieve again; never substitute a semantically similar prefix's KV as if exact.

## 3. Local laptop plus Raspberry Pi

A laptop and Pi each expose an authenticated host-side managed-object service. The laptop sends an immutable block to the Pi for a designated task, not an ordinary pointer. Each node reports actual CPU memory and supported device-access methods. A tiny advisor can suggest which worker gets a job based on measured capacity and network conditions; deterministic scheduler filtering ensures ISA, security, deadline and capability requirements are met.

**Evaluate:** end-to-end task completion, bytes transferred, latency, power, error handling and data privacy versus processing locally. **Failure:** disconnect, stale object or an unavailable device yields a recoverable explicit failure; retries use idempotency keys. This setup adds usable remote storage or work only for workloads and software that can tolerate transfers, latency and node failure.

## 4. GPU and NPU cooperating on tensor data

An inference pipeline would like to reuse an intermediate tensor across GPU and NPU. The resource manager checks whether each driver supports the required export/import handle, tensor layout, security domain and synchronization. If compatible, use the actual documented sharing and completion mechanisms; otherwise ask the application whether an explicit copy is acceptable or fail.

**Evaluate:** end-to-end latency, data movement, CPU overhead, device memory and energy. **Failure:** no shared import path means no zero-copy claim; a host staging copy can increase both memory and latency. Shared physical RAM alone does not settle cache visibility or access rights.

## 5. Tiering cold KV blocks

A serving engine has unreferenced exact-prefix blocks. The runtime marks certain blocks safe to offload or recompute; a controller compares measured expected reuse, transfer time, recompute cost and available capacity. A learned predictor may rank eligible blocks, but the engine controls compatibility and live references. A bandwidth drop may make recomputation preferable to a remote transfer.

**Evaluate:** hit rate, TTFT, retrieval latency, throughput, quality, storage wear, network traffic, host memory and device stalls. **Failure:** network unavailability or damaged cache should trigger a valid recompute or explicit error under the engine's contract. Do not assume all state is safely reconstructible or serializable.

## 6. In-model computer as a worker

A model-contained RISC-V guest owns a small fixed arena. Its host adapter handles networking and request validation. A job requests a small deterministic computation, the guest uses a bounded allocator, and the adapter checks the result against a reference. The coordinator accounts separately for guest bytes and large host model-state costs. No host dereferences a guest pointer.

**Evaluate:** correctness, guest allocation operations, trace tokens, total latency, host accelerator memory and cost versus a native or conventional-emulator worker. **Failure:** absent bridge, unsupported instructions, precision divergence or insufficient guest arena is reported as an experimental blocker. See the [neural computer collection](../llm_computers/README.md).

## Cross-scenario lesson

Use a common policy vocabulary—ownership, access, lifetime, placement, consistency, latency and budget—while preserving the actual mechanics of each backend. The generality comes from explicit contracts and adapters. Test a new connection in isolation before combining controller, retrieval, KV management, accelerator transfers and networking.

## Sources

- [PagedAttention](https://arxiv.org/abs/2309.06180)
- [vLLM prefix caching](https://docs.vllm.ai/en/latest/features/automatic_prefix_caching/)
- [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html)
- [Qwenputer design notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
