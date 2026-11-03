# Qwenputer: RISC-V in Qwen model computation

**Evidence status:** author-reported design and tests. This document does not assert independent reproduction, general Qwen compatibility or MimirAlloc integration.

## What was constructed

The [Qwenputer author](https://divagr.com/blog/qwenputer) describes a modified Qwen3.5-4B with eleven additional transformer layers encoding CPU logic. The core is described as RV32IM with Zba plus custom arithmetic facilities. Its state is mapped to selected Gated DeltaNet heads: the author's explanation uses controlled gates, one-hot selection and updates so state rows act as addressable cells. Token records encode execution events, and a driver handles firmware and the boundary between processor traces and language generation. The processor logic is executed as model computation on ordinary hardware; the CPU is not a separate installed tool being called for each arithmetic operation.

A precise implementation is important: **adding a processor to a model changes the architecture/checkpoint and runtime behavior**. An unmodified Qwen model loaded by a generic inference server does not automatically acquire these capabilities. A runtime must support all layer shapes, recurrent state and execution semantics required by the specific artifact and driver.

## The documented guest memory map

The project's [RV32 design notes](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md) describe a P2 memory-map revision with:

| Region | Guest location and documented capacity | Meaning |
|---|---|---|
| RAM | 32 KiB at address `0x00000000` | Mutable guest `.data`, TLS, `.bss`, heap and stack share it |
| DROM | 128 KiB at `0x00020000` | Guest read-only data |
| Code ROM | 256 KiB at `0x00040000` | Guest code in the documented representation |

These numbers are for the described revision; confirm the pinned code and current artifact. Guest addresses are not host pointers. The 32 KiB figure includes all those mutable users; it is not a free 32 KiB heap. The host state representing that guest RAM may be far larger than 32 KiB because values are encoded in model tensors, with runtime and alignment overhead. Register state, model weights, recurrent state, logits and temporary tensors also consume host resources.

## ISA, ABI and environment

The published P2 notes identify `-march=rv32im_zba` builds using picolibc and dedicated linker scripts. This is a constrained bare-metal-like machine, not evidence of Linux, POSIX threads, virtual memory, general system calls or a network device inside the guest. A guest allocator should be configured with an explicit static arena supplied by the linker or application, and it must coexist with stack and other data. Need to verify supported code generation, calling convention, alignment, libc hooks and trap behavior against a pinned version; do not assume that all RV32 toolchain outputs work.

The author documents a software stack and separately reports which paths ran on a standalone core, reference model, GPU executor or merged Qwen bridge. Their P3 notes state that MicroPython had run on the standalone core/executor and that a merged-model bridge remained a later task at that point. Treat a later upstream claim as version-specific and confirm actual artifact and logs; do not collapse these execution modes into one proof.

## Reported tests, and what they establish

The upstream design documents particular RV32 test sets, QEMU/reference output comparisons, fuzz cases and free-running traces. Its README reports roughly 230,000 replayed output positions without mismatch for one measurement, and a separate trace rate on L4. These are evidence about their specific tested setup, not a universal proof for every model checkpoint, software version, language response, machine instruction, precision or device. Review the exact test sections: a reference evaluator, a trace forced from expected tokens, an optimized executor and an end-to-end merged-model run provide different evidence.

One documented read design produced floating-point residues when a multi-hot operation was used in a GPU kernel; the design changed the reads. This is a valuable lesson: mathematical intent must be tested on real inference kernels. Quantization, different batch sizes and kernels can change behavior; an ordinary Q4 conversion cannot be assumed to preserve an embedded digital circuit.

## Where allocation can help

### Inside the guest

A small static arena allocator could provide `alloc`, `free` and perhaps a restricted C interface for guest programs. It must work without OS mapping calls; cap metadata and stack growth and return deterministic failure when the arena is exhausted. Initial tests should avoid concurrency and atomics unless the machine exposes genuine concurrent execution and the ABI defines it.

### Outside the guest

The physical host must allocate model weights, activations, recurrent state, accelerator workspace, tokens, snapshots and any transport buffers. A host-side MimirAlloc could potentially help *some* CPU allocations if the runtime links to it, but GPU/NPU tensor storage is owned by the relevant framework/backend. Measure each category separately.

### Between guest and host

A runner can exchange bounded request/response messages, initialize state and save snapshots. Any network connection is provided by host-side software, not implied by the guest CPU. A guest allocator address must be translated into a checked guest offset plus length before copying; never dereference it as a host pointer.

## Minimal experiment

1. Pin upstream code commit and model revision; note artifact checksums, license and target hardware.
2. Build a tiny freestanding RV32 guest that uses a fixed buffer and an independent reference allocator, printing results through the documented guest output interface. Start with integer arithmetic and a tiny static footprint.
3. Verify linker map: `.data + .bss + TLS + arena + maximum stack + guard` fits the documented RAM, with non-overlap and alignment. The linker map is an estimate; instrument actual stack high-water mark if feasible.
4. Execute natively in a conventional supported reference/emulator, then upstream reference model, then actual free-running model path. Compare output, memory ownership trace and failure code for each operation.
5. Add free/reuse and exhaustion tests, then an independently built MimirAlloc guest prototype only after it exists. Record instruction and trace-token counts, wall time, model and host memory.
6. Report the exact execution mode and absent features. No successful guest-only test proves that the model's chat bridge can drive the same workload.

## Sources

- [Qwenputer repository and README](https://github.com/divagr18/qwenputer)
- [Qwenputer RV32 design and experiments](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Qwenputer author's overview](https://divagr.com/blog/qwenputer)
- [Published model card](https://huggingface.co/divagr1925/Qwen3.5-4B-Qwenputer)
- [RISC-V unprivileged ISA reference](https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html)
