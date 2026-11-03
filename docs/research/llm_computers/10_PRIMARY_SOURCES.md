# Primary sources and evidence catalog

Research assembled 2026-11-02. These are primary project documents, official specifications and research publications. Live project pages may change; pin a Git revision and model artifact when executing an experiment. This list is not an exhaustive survey or independent verification of the authors' results.

## In-model and transformer computers

1. **Qwenputer, author-maintained repository:** [divagr18/qwenputer](https://github.com/divagr18/qwenputer). Describes the architecture, author-reported results, examples, drivers and experimental branches. Check branch and commit before drawing a conclusion.
2. **Qwenputer RV32 engineering notes:** [experimental/riscv/docs/rv32.md](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md). Includes a P2 32 KiB RAM map, instruction support, testing stages and a numeric read issue. Distinguish reference, core and integrated-model results.
3. **Qwenputer author overview:** [Qwenputer: a fully functioning, deterministic RISC-V CPU inside Qwen3.5](https://divagr.com/blog/qwenputer). Describes the design from its creator's perspective; its claims need configuration-specific verification.
4. **Qwenputer model card:** [Qwen3.5-4B-Qwenputer](https://huggingface.co/divagr1925/Qwen3.5-4B-Qwenputer). Artifact metadata and reported model usage; check an immutable revision, files and component licenses.
5. **Percepta Transformer VM:** [transformer-vm](https://github.com/Percepta-Core/transformer-vm). Describes an analytically constructed WASM-like transformer, supported operations, reference tools and optimized executor. A specialized model and a general conversational LLM are different things.
6. **Percepta project background:** [Can LLMs Be Computers?](https://www.percepta.ai/blog/can-llms-be-computers) and [Constructing an LLM-Computer](https://www.percepta.ai/blog/constructing-llm-computer). Researchers' descriptions of their construction.
7. **Related programmable transformer work:** [Looped Transformers as Programmable Computers](https://arxiv.org/abs/2301.13196). Research into programmable transformer constructions; read assumptions and model separately from any specific repository.

## Standards, guest toolchains and runtime

8. [RISC-V unprivileged ISA](https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html). Exact instruction behavior and optional extensions; implementation support needs verification.
9. [RISC-V ELF psABI](https://github.com/riscv-non-isa/riscv-elf-psabi-doc). Calling convention and executable interfaces; guest firmware may implement only a subset of a general environment.
10. [RISC-V tests](https://github.com/riscv-software-src/riscv-tests). Compliance-oriented tests, not proof of all possible program behavior.
11. [WebAssembly core specification](https://www.w3.org/TR/wasm-core-2/). Normative guest language reference; a research VM may support only a subset.
12. [QEMU user-mode documentation](https://www.qemu.org/docs/master/user/index.html). Conventional emulator reference; it is distinct from a neural forward-pass implementation.

## Memory and distributed systems

13. [Linux memory management](https://docs.kernel.org/admin-guide/mm/index.html). Host memory behavior, not a claim about a bare guest.
14. [Linux PSI](https://docs.kernel.org/accounting/psi.html) and [cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html). Available Linux pressure and control interfaces; availability depends on environment.
15. [CUDA stream-ordered allocator](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html). Accelerator lifetime semantics for a particular API.
16. [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html). Shared buffer synchronization; does not imply all guest memories are DMA-capable.
17. [Linux RDMA userspace verbs](https://docs.kernel.org/infiniband/user_verbs.html). Host-side network memory operations and limits.
18. [Linux CXL documentation](https://cxl.docs.kernel.org/). Platform-supported memory expansion; no implication that neural model state is CXL-coherent.

## Source-use ledger template

For each adopted fact or code contribution, record date, author, title, URL, immutable version, relevant section, direct observation versus inference, license, implementation-code access, related tests and unresolved questions. A research link is not permission to copy code, model weights or lengthy text into an MIT-licensed project.
