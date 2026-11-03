# Memory allocation: a practical foundation

**Purpose:** understand what an allocator does, how common designs work, and which contracts MimirAlloc must make explicit. **Status:** research and design; no MimirAlloc implementation or benchmark is implied.

## From a program to physical memory

A program requests storage for objects whose size or lifetime may not be known at compile time. A user-space allocator locates an appropriate block, records enough information to manage it, returns an address with the promised alignment, and eventually reuses or releases storage. It normally obtains larger regions from the operating system and subdivides them. Virtual addresses, committed pages and resident physical memory are different quantities: reserving address space does not mean all of it is resident, and demand paging can defer physical costs until access. Operating-system overcommit can permit an apparently successful allocation followed by failure or process termination when memory is touched.

A toy example: requests of 19 bytes are placed into 32-byte slots. Each live request uses at least 13 bytes more than requested, plus amortized metadata; other blocks of that class may be empty. If a page contains one live object, its remaining free slots cannot necessarily be returned to the OS without moving the object. This is why fast `free` and low resident memory are related but different goals.

## Allocator layers

1. **API and contract:** C allocation functions, explicit allocator APIs, language runtime hooks, or device-buffer interfaces. Each has distinct failure and ownership rules.
2. **Small-object front end:** size-class lookup and local cache or slab free list. Good locality and few locks help common calls, but excessive caches retain memory.
3. **Central coordination:** distributes slabs and handles batches or objects freed by a different thread. Metadata correctness and synchronization dominate complexity.
4. **Page/extent backend:** manages larger regions, coalescing and release. It requests mappings from the OS, which controls virtual and physical memory.
5. **Observability and policy:** counters, pressure handling, security, configuration and diagnostics must avoid recursive allocation or unsafe initialization.

These are a useful decomposition, not a required tcmalloc replica. Define behavior independently and record provenance. The application, runtime, libc, allocator, kernel and hardware each own different parts of the path.

## Basic design families

| Family | How it works | Strength | Cost or constraint |
|---|---|---|---|
| Bump/arena | Advance a cursor; release whole region together | Very cheap, predictable for known lifetimes | Individual free absent or limited; bad lifetime grouping retains memory |
| Fixed pool/slab | Divide regions into same-size slots | Fast and compact metadata | Wrong size class wastes space; partially occupied slabs retain pages |
| Free list / segregated fits | Track available blocks by size | General use and reuse | Splitting, coalescing and metadata are hard under concurrency |
| Buddy | Split power-of-two blocks and merge buddies | Simple coalescing | Rounding can waste space; real-time bound depends on implementation |
| Direct OS mapping | Request a mapping for an allocation | Simple large-object isolation | System call, virtual-map and page overhead; not ideal for tiny objects |
| Moving collector / handle pool | Refer through handles; relocate objects | Can compact or migrate | Requires cooperation, barriers and explicit pinning; ordinary C pointers cannot be silently moved |

## Vocabulary and accounting

- **Requested live bytes:** sum requested by still-live allocations. Track separately from rounded slot sizes.
- **Internal fragmentation:** space inside allocated blocks that callers cannot use, including size-class rounding and possibly per-object metadata. State the exact definition used.
- **External fragmentation:** free storage exists but cannot satisfy a request because it is distributed or constrained by size, locality or other rules.
- **Retention:** memory held by allocator or runtime for reuse after free. Retention may reduce latency while increasing RSS.
- **Virtual size / RSS / PSS / commit:** separate OS measures with different semantics; shared pages and swap complicate comparison. Neither virtual size nor RSS equals live requested bytes.
- **Footprint amplification:** measured resident bytes divided by live requested bytes at a defined sampling point; avoid division by zero and include baseline process memory separately.
- **False sharing:** independent variables on the same cache line cause coherence traffic; avoiding it can require padding that increases memory usage.

## Contracts that cannot be hand-waved

For every supported OS and ABI, specify zero-size allocation, alignment, integer overflow, failure and `errno`, `free(NULL)`, `realloc` preservation and failure behavior, `realloc(p, 0)` platform/standard differences, aligned allocation constraints, thread safety and C++ operators if exposed. Do not assume a pointer from one allocator can be freed by another. Process startup, teardown, recursive calls, signals, `fork`, dynamic loading and static linking require separate analysis. Consult the target libc and applicable standards; the C language standard, POSIX, Android bionic and system ABI are not interchangeable.

## Independent design workflow

Describe the external behavior and test vectors first, log primary sources and applicable licenses, and keep design and implementation provenance. If a strict clean-room separation is claimed, document who researched and implemented what and the materials they accessed; a team that studies implementation details cannot simply assert independent-room separation. Build a namespaced correctness-first prototype before claiming global drop-in replacement.

## Questions for MimirAlloc

- Which observed Mojo process, library and symbol must change for the actual reported blocker?
- What minimum function and alignment contract does that exact boundary require?
- Are small allocation latency, retained pages, thread count or startup cost dominant on each target?
- How will ownership be proven when libraries use different runtimes?

## Primary reading

- [Linux memory management concepts](https://docs.kernel.org/admin-guide/mm/concepts.html)
- [Linux memory management documentation](https://docs.kernel.org/admin-guide/mm/index.html)
- [mimalloc: free-list sharding research](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/)
- [scalloc research](https://research.google/pubs/fast-multicore-scalable-low-fragmentation-memory-allocation-through-large-virtual-memory-and-global-data-structures/)
- [TCMalloc warehouse-scale study](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/)
