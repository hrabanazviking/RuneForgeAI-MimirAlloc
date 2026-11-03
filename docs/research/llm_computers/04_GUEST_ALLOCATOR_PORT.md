# Designing a guest allocator for neural computers

**Status:** conceptual design and acceptance criteria. There is no existing guest build of MimirAlloc. The proposed API below is illustrative pseudocode, not a working header.

## Start with a freestanding contract

Separate allocator logic from its source of memory. A hosted Linux backend may ask the kernel for pages; a small RV32 or WASM guest may have only a static array or a fixed linear-memory region. The portable core must not assume `mmap`, page faults, threads, signals, virtual address space, C++ runtime, dynamic linking or an OS.

```text
arena_init(base_offset, usable_length, alignment, callbacks?) -> status
arena_alloc(arena, size, alignment) -> guest_offset or failure
arena_free(arena, guest_offset) -> status
arena_stats(arena) -> used, free, metadata, largest_free_block
```

An offset is meaningful only relative to the owning guest arena and its generation. Choose a sentinel that cannot be confused with a valid offset; zero may be a valid guest address. An implementation must specify alignment constraints, invalid request behavior, zero size, overflow handling, allocation ownership and whether invalid frees are detected. Guest code may expose actual guest pointers through a C wrapper after converting within the same guest address space; the host sees an opaque offset, not a usable pointer.

## Model the entire 32 KiB machine

For the documented Qwenputer P2 layout, RAM is 32 KiB shared by `.data`, `.bss`, TLS, stack and heap. Let `A = RAM - static_data - bss - TLS - max_stack - runtime - guard - allocator_metadata`. `A` is the maximum candidate arena only if all regions are disjoint and aligned. For an illustrative design, not a measured target: 32,768 - 4,096 static - 8,192 stack - 2,048 runtime/guard - 1,024 allocator overhead leaves 17,408 bytes for the arena. Actual numbers must come from the linker map and runtime tests. If metadata resides inside the arena, count it once, not twice.

Use fixed compile-time linker boundaries and check the addresses at startup if the environment permits. The stack may grow down while the heap grows up; choose explicit non-overlapping regions rather than letting them collide. Initialize globals and `.bss` according to the guest startup ABI. Define out-of-memory without relying on Linux `errno` unless the selected libc and environment implement it.

## Design candidates

| Design | Advantages | Concerns | First experiment |
|---|---|---|---|
| Monotonic arena | Extremely small, simple | No per-object free; phase lifetimes needed | Create/clear per request |
| Bitmap + fixed slots | Predictable small-object reuse | Wasted space and fixed maximum object size | Model trace access and metadata size |
| Segregated small pools | Different object sizes | Multiple partially empty pools | Measure occupancy against traces |
| Address-ordered free blocks | General variable sizes and coalescing | Search and metadata access cost; fragmentation | Bounded small trace with coalescing |
| Buddy | Simple merges | Power-of-two waste, metadata | Compare peak usable bytes |

Start with monotonic and fixed-slot baselines. Add variable-size free only if the program needs it. For a transformer machine, a simple host-CPU-efficient algorithm may be expensive in guest trace tokens: compare guest instructions, memory reads/writes and emitted tokens, not just traditional instruction count.

## Conservative data layout

- Use fixed-width integers and checked addition/round-up; do not assume `size_t` is 64-bit on RV32.
- Store offsets within a bounded arena rather than serializing native host pointers. Reject `offset + size` overflow before range checks.
- Document minimum and requested alignment; validate alignment is supported and avoid unaligned multi-byte accesses if target disallows them.
- Keep metadata small and consistently encoded; exact layout is internal unless a versioned snapshot format commits to it.
- Enforce a per-operation work bound if advertised; a variable free-list scan is not constant-time merely because the arena is small.
- Do not add locks, TLS caches or remote frees until the guest actually provides suitable concurrency and atomics.
- Do not use pointer tagging or top-bit assumptions without target and ABI evidence.

## C integration paths

1. **Namespaced API:** explicit arena and offset/pointer management; safest first experiment.
2. **Guest libc integration:** use the libc/toolchain's documented allocation hooks or wrappers, if available; verify that C library calls bind correctly.
3. **Guest global `malloc`:** only after exact ABI and all linked symbols are established. Never claim replacing host malloc from guest code.
4. **WASM module import/export:** a separate target with linear-memory semantics. Module memory offsets and host views must be validated; memory growth can invalidate host views depending on runtime API.

## Contract tests

- Initialize an empty, aligned arena; verify arena too small and invalid alignment are rejected.
- Allocate zero, minimum, boundary, over-capacity and overflow sizes according to the selected documented policy.
- Fill blocks with distinct bytes, free a subset, ensure remaining blocks preserve content; check reuse and non-overlap.
- Exercise adjacent-free coalescing if supported, fragmentation, repeated exhaustion/recovery and double/invalid-free behavior only if explicitly promised.
- Run sequences from saved seeds; compare guest program output with a checked reference implementation and actual model traces.
- Test snapshot/restore with the arena half full; ensure state and generation rules remain consistent. Simulated concurrent access is not evidence of real multi-thread safety.

## State checkpoints

A heap snapshot must include arena bytes, allocator configuration, metadata version, alignment, live allocation ownership and guest execution state needed to resume. Checkpoint only at a safe point where no operation or async mutation is in flight. A guest snapshot and a model-recurrent-state snapshot are related but distinct; preserving only one may produce an inconsistent restore. Use integrity checks and schema/version validation, and do not assume a snapshot is portable across model revisions, dtypes or executors.

## Sources

- [Qwenputer RV32 design](https://github.com/divagr18/qwenputer/blob/main/experimental/riscv/docs/rv32.md)
- [Percepta Transformer VM](https://github.com/Percepta-Core/transformer-vm)
- [RISC-V ABI repository](https://github.com/riscv-non-isa/riscv-elf-psabi-doc)
- [WebAssembly specification](https://www.w3.org/TR/wasm-core-2/)
