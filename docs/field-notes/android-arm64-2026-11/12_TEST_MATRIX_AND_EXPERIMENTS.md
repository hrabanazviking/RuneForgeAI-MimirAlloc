# 12 — Test matrix and experiments

These tests are proposed. No MimirAlloc implementation or test suite is present in this repository as of this note. Tests must exercise behavioral contracts and independent expected results, not merely restate implementation details.

| Dimension | Representative cases | Evidence needed |
|---|---|---|
| Address space | 39-bit ARM64 and a standard larger-address host; more devices where available | Actual mappings, configuration identity and allocator startup |
| Page size | 4 KiB and other supported sizes | Runtime page size, alignment, guard and mapping results |
| Basic operations | Allocate, write, read, free; zero-size; varied sizes | Data integrity, contract-compliant results and error codes |
| Alignment | Supported power-of-two sizes, invalid alignment, overflow | Address modulus, rejection rules and no metadata corruption |
| Resize | Grow, shrink, failure, edge sizes, API-specific zero behavior | Preserved prefix and ownership after failure |
| Arithmetic | Size multiplication, addition and rounding near limits | Fail safely without wraparound |
| Threads | Concurrent allocation/free, ownership transfer, thread exit | Correctness under stress and bounded metadata |
| Fragmentation | Mixed lifetimes and sizes; reuse after release | Live bytes, reserved/resident pages and repeatable traces |
| Pressure | Admission failure, reclaim, allocation failure, cache reduction | Explicit errors, bounded retries and UI observations |
| Lifecycle | Startup, library loading, shutdown, repeated creation | No cross-allocator frees or use after teardown |
| Integration | Explicit API, dynamic library, actual Mojo binary | Proof of actual selected allocator and symbol paths |
| Android | App sandbox, process death, lifecycle and supported API levels | Physical-device logs and OS behavior |
| Accelerator | GLES/Vulkan and vendor NNAPI buffers, where supported | Ownership, fences, actual dispatch and numerical checks |

## Small allocator workload

A first real 39-bit test can run one short, low-priority process with a strict allocation cap. Allocate varied blocks, fill with deterministic patterns, check them, resize selected blocks, transfer ownership across a small number of threads, recheck, and free through the same allocator. Run the unmodified baseline and the candidate as distinct artifacts. Log exit status, high-water RSS/PSS if accessible, elapsed time, memory available before and after, revision, hash and configuration.

A header assertion is valuable but cannot replace this workload. Sanitizers and platform debugging tools should complement it where supported; they may alter the virtual address layout and cannot be assumed compatible with every allocator configuration. Inject allocation failures rather than exhausting the tablet. Include boundary and failure tests before performance tests.

## Compiler integration gate

A meaningful Mojo test needs actual startup (`--version`), compilation and execution of small programs, error paths, repeated runs, thread behavior and a link audit proving which allocator the tested process uses. Then test the A.E.S.I.R. workload only after model identity, memory admission and an Android target are established. A successful glibc Linux compiler is an intermediate result, not a native Android app.

## Comparison and responsiveness

Use matched workloads, devices, threads and environmental conditions. Report cold/warm runs and multiple repetitions, throughput and latency distributions, memory high-water marks and failed runs. Capture UI frame-time or another repeatable interactive workload, thermals and energy when available. State unavailable metrics and avoid treating a single synthetic result as a general performance win.
