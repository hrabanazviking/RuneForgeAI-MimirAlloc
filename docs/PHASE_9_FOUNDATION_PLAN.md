# Phase 9 - Foundation Plan + Pressure-State Inventory (Slice I-001)

Slice: **I-001 (team-01)**. Wave 9, first slice. Phase 9
foundation: this plan originates no design and no
implementation; it restates Phase 9 in the roadmap's own
terms, inventories the banked allocator's pressure state
from the sources as they actually are at this base, maps
the Wave 9 queue I-002..I-009 onto the roadmap clauses
with each slice's evidence obligation, breaks the Phase 9
exit sentence into gradeable items, and carries every
still-open debt forward by name.
Base: `81c04746aa5a3ea497a22ba7f04d39f0629095ec`
(Wave 8 complete, 9 of 9 banked; remote main) on branch
`yrsa/mimiralloc-01`, worktree
`/home/pi/mimiralloc-team-01` on Raspberry Pi
`purpleviking1` (aarch64, 4 cores, system page size
16384, measured by `getconf PAGESIZE` in this slice).
Host state at muster: `free -g` 15 GiB total, 13 GiB
available; Samtal HTTP 200 on port 8787 before and after
this slice's one run. Nothing was stopped, configured,
or installed. All workloads cited from earlier slices
are **synthetic** unless a row says otherwise.
Governing texts: Roadmap Phase 9 (Section 8 of
`docs/MimirAlloc_Research_and_Development_Roadmap.md`)
with Sections 9.1, 9.3, 10 and 11;
`WAVE_PLAN.md` in the coordinator checkout (Wave 9 queue,
derived 2026-11-07 05:3x EST); the banked slice docs and
recorded runs named in each section. Fresh evidence in
this slice: the source reads, greps and `nm` checks of
Section 2 (commands and results stated where used) and
one `make test` run (Section 7). No other run is claimed.
`README.md`, `include/mimir_alloc.h`, the Makefile and
every `src/` file are byte-identical to base: this slice
adds exactly this one document.

> **BOTTOM LINE, STATED FIRST.** The banked allocator at
> this base has **no memory-pressure behaviour in the
> roadmap's Phase 9 sense**: no soft budget is defined or
> enforced anywhere, no telemetry surface exists (the
> internal audit counters count events, not bytes, and
> are readable only by test-linked binaries), no
> caller-invokable trim or cache-release entry point
> exists on any surface a shared-library consumer can
> reach, and no fault-injection instrument exists with
> which the exit's fault-injection half could even be
> run. What exists today is a correct two-path allocator
> whose failure shape is already clean and bounded
> (Section 2.5), one bounded retention mechanism (empty
> size-class spans, at most 2 per class by default, one
> system page each), and one internal unload drain
> (H-001) that releases those retained spans at
> `dlclose` / process exit. Wave 9 therefore starts
> from definitions and instruments, not from tuning:
> I-002 must define budgets and telemetry, I-003 must
> build on-demand release, and I-004 must build the
> Nth-mapping fault-injection shim that Phase 3 already
> owed (INC-C007-01) before any exit evidence can exist.
> The Phase 9 exit is **not** gradeable at this base on
> any item except the failure-contract reading of the
> code; Section 4 grades every item OWED or PARTIAL
> accordingly, and no item is graded MET on intention.

## 1. Phase 9 in the roadmap's own terms

Roadmap Phase 9, verbatim:

> *Design explicit soft budgets, process telemetry,
> trimming and cache release. Handle genuine allocation
> failure cleanly where the caller contract permits it.
> Measure reclaim overhead and fragmentation. Separate
> allocator-managed bytes from process-wide memory,
> model weights, mmap-backed files and device-driver
> memory. Android may kill a process under pressure
> before an allocation can be gracefully recovered.*
>
> *Exit: fault injection and constrained-memory runs
> show documented, bounded behavior; no claim to
> prevent Android low-memory kills.*

Clause by clause, with the owning Wave 9 slice:

| # | Phase 9 clause (roadmap wording) | Owning slice(s) | Position at this base |
|---|---|---|---|
| P9-1 | Explicit soft budgets | **I-002** (design + definitions) | Nothing exists: no budget value, variable, comparison or setter anywhere in `src/` or `include/` (Section 2.6). Enforcement is chartered to no slice by name - INC-I001-14 |
| P9-2 | Process telemetry | **I-002** (definitions), **I-007** (separation proof) | Event counters only, test-linked only, no bytes (Section 2.4, 2.7). The term itself is undefined in the roadmap - INC-I001-08 |
| P9-3 | Trimming and cache release | **I-003** (implementation + public-surface decision) | Free-time excess-span release exists (automatic); on-demand release exists only as the internal H-001 drain, unreachable by `.so` consumers (Section 2.3, 2.6) |
| P9-4 | Genuine allocation failure handled cleanly where the caller contract permits | **I-004** (Nth-mapping injection, NULL contract, realloc failure-atomicity under pressure) | The code's failure shape already reads clean and failure-atomic (Section 2.5); it has never been evidenced under injected mapping failure, because no shim exists (INC-I001-01) |
| P9-5 | Reclaim overhead measured | **I-006** | No reclaim-cost measurement exists. H-001 measured drain *effect* (0 B growth per cycle), never drain *cost* (Section 2.8) |
| P9-6 | Fragmentation measured | **I-006** | No fragmentation measurement exists, and exact small-path internal fragmentation is not computable from allocator state today (INC-I001-04) |
| P9-7 | Allocator-managed bytes separated from process-wide memory | **I-002** (definitions), **I-007** (measured proof) | Separable in principle (the allocator maps only anonymous private memory, Section 2.7); not separable exactly in practice today, because no byte state is observable (INC-I001-02) |
| P9-8 | Separated from model weights, mmap-backed files and device-driver memory | **I-002**, **I-007** | The allocator never maps files and never touches GPU/NPU memory (L6 acquire form, Section 2.7); a measured separation proof is still owed - construction alone is not the exit evidence |
| P9-9 | Android may kill a process under pressure before graceful recovery | **I-009** (caveat carried into the exit grade) | A limit on every Phase 9 claim, not a task: no Android run of any kind exists (Phase 4 exit UNMET, Section 5) |
| E-P9 | Exit: fault injection + constrained-memory runs show documented, bounded behavior | **I-004** (injection), **I-005** (constrained runs), **I-008** (repeated cycles), **I-009** (packet) | Gradeable breakdown in Section 4. At this base: OWED, except the code-reading half of the failure contract |
| E-P9-C | Exit caveat: no claim to prevent Android low-memory kills | **I-009**, and every Wave 9 slice doc | Standing wording law for the whole wave; repeated in Sections 4 and 5 |

### 1.1 The governing cross-sections

**Section 9.1 invariants that Phase 9 work must not
break** (roadmap numbering): Invariant 1 (every live
allocation has one valid owner and non-overlapping
storage) is what *bounded behavior* means under injected
failure; Invariant 5 (a failed `realloc` preserves the
original valid allocation where the contract requires
it) is I-004's failure-atomicity row; Invariant 7 (a
full cache or failed backing allocation has a bounded
and testable slow path) is the invariant form of P9-4;
Invariant 8 (statistics do not drive unsafe decisions
if they are approximate or stale) binds I-002/I-003:
a soft budget fed by telemetry must fail safe when the
telemetry is the event-counter approximation of
Section 2.4; Invariant 10 (every fallback either
preserves allocation ownership or explicitly fails)
forbids the classic pressure shortcut of silently
serving a request from a different allocator.

**Section 9.3 controlled failure tests** are the exit's
method list. The Phase 9-relevant bullets: *artificially
fail the Nth mapping or metadata allocation* (I-004's
shim; owed since Phase 3, INC-C007-01); *enforce low
process limits where supported; simulate fragmented
address space* (I-005; a first form already exists in
the C-008 evidence program, Section 2.8); *confirm
failures neither hang indefinitely nor return
overlapping valid blocks* (the bounded-behavior test
shape for I-004/I-005/I-008); *track results separately
for normal Linux, Android and proot* (on this Pi only
the Linux track can run; the Android track stays OWED
with the Phase 4 debt, Section 5).

**Section 10 test matrix** rows Phase 9 must satisfy:
*Memory* (repeated cycles, fragmentation, cache drain
and trim, large mapping pressure - *counters and OS
metrics align with their stated meanings*, which is
why I-002's definitions precede every measurement) and
*Robustness* (failing OS calls, OOM, interrupted
workloads - *defined failure or diagnostic behavior;
no false success*).

**Section 11 benchmarking principles** that bind the
wave's measurements: verify the allocator was actually
selected; use fresh process runs; report median,
p95/p99 and outliers, peak RSS/PSS, virtual size,
retained memory after freeing, mapping count and
startup time; store raw data, harness revision, scripts
and negative results; flag noise rather than invent
certainty. A lower allocation-time number that
increases memory pressure may not satisfy the mobile
objective - the sentence that makes P9-5/P9-6
measurements, not latencies, the wave's centre.

**Phase 10 boundary.** The roadmap assigns the counter
vocabulary (requested, allocated, cached, mapped,
released and peak bytes; allocation failures; span
occupancy; cross-thread activity) to **Phase 10 -
Diagnostics and hardened builds**, not Phase 9. Wave 9
defines only the telemetry its own exit needs
(I-002) and must not silently grow into the Phase 10
diagnostics surface; where a Phase 9 need and the
Phase 10 list overlap (peak bytes, failure counts),
I-002 records the definition once and names Phase 10
as the surface's future home.

## 2. Pressure-state inventory of the banked allocator

Every claim in this section was read from the sources
in this worktree at this base (`src/api/mimir_alloc.c`,
`src/core/mimir_core.c`, `src/core/mimir_core.h`,
`src/platform/mimir_platform.c`,
`src/platform/mimir_platform.h`,
`include/mimir_alloc.h`, `src/api/mimir_alloc.map`, and
the test-only headers under `tests/`). Line numbers are
this base's. Where a claim of absence is made, the grep
or `nm` command and its result, both from this slice's
session on the Pi, are stated with it.

### 2.1 Two paths, not one - the current construction

The Phase 3 baseline was mapping-per-allocation, and
that is still exactly the **large path**: a request
above 1024 bytes takes `alloc_locked`
(`mimir_core.c:354`), which acquires its own OS mapping
through L6 - one reserve system page holding the
96-byte descriptor, then the usable extent, the whole
mapping page-rounded (`mimir_platform.c:76`,
`mmap` at :109, `PROT_READ | PROT_WRITE`,
`MAP_PRIVATE | MAP_ANONYMOUS`). A large free unmaps
the whole mapping in the same locked operation
(`retire_locked`, :314). The large path retains
**nothing** after free: retained-after-free there is 0
by construction.

Phase 7 (G-003) added the **small path** in front of
it: requests of 1..1024 bytes are served from 20 size
classes (16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
224, 256, 320, 384, 448, 512, 640, 768, 896, 1024;
`mimir_class_sizes`, :580) carved out of spans of
exactly one system page each (96-byte span header at
offset 0). The dispatcher is `alloc_any_locked` (:1121
area): at or below the 1024-byte cutoff
(`MIMIR_SMALL_CUTOFF`, :554) the class path serves; above
it, the mapping path serves, byte-for-byte as banked.
A small request is **never** silently re-routed to a
private mapping when span acquisition fails (G-002
R-1) - the pressure-relevant consequence is in
Section 2.5. The one global L6 lock serialises both
paths; there is no per-thread cache, no shard and no
second lock (H-002 inventory, still true at this base).

### 2.2 Retained memory today: the span bound

The allocator's only retained-after-free memory is
empty small-path spans. When a span's `live_count`
returns to 0 it is EMPTY; at most
`mimir_retention_bound` empty spans per class are
retained, and the empty span that would exceed the
bound is released whole through L6 synchronously,
inside the same locked free that emptied it
(`small_free_locked`, :953; G-005 policy RP-1..RP-3).
The bound is a single runtime scalar: default **2**,
valid range **0..16**, settable only through the
internal/test-only pair
`mimir_tune_set_retention_bound` /
`mimir_tune_get_retention_bound` (:988 / :1001,
declared only in `tests/core/mimir_tune_g007.h`,
INC-E001-07 shape (b), bound local in the shared
library). Out-of-range is refused with -1 before any
state is touched; lock failure is -2; the bound applies
at free time only and never eagerly releases spans
already retained (G-007, RP-3).

Byte form, at this Pi's 16384-byte page: worst-case
retained-after-free is 20 classes x 2 spans x 16384 B
= **655,360 B (640 KiB)** at the default, and
20 x 16 x 16384 B = **5,242,880 B (5 MiB)** at the
maximum setting. Both figures are span counts times
the measured system page size - arithmetic from the
code's own constants, not a measurement, and both are
page-size-dependent (INC-I001-11): on a 4096-byte-page
host the same counts retain one quarter of the bytes.
Live spans are bounded by live demand, never by the
retention bound; the bound caps only the empty tail.

### 2.3 The H-001 destructor / unload drain

Slice H-001 (Wave 8, commissioned fix for INC-G008-02)
added the allocator's only release-everything
mechanism. `mimir_core_drain_retained` (:1067) takes
the one global lock, walks the span registry once, and
releases every span whose `live_count == 0` whole
through `span_release_locked` (with its F-06 rollback),
returning how many spans were actually released. It
never touches a span holding a live block and never
touches a large mapping (the mapping path retains
nothing, so there is nothing to drain there). The
drain is non-terminal and idempotent: registries,
counters and the lock are left fully valid, the
allocator stays usable, a call with nothing retained
returns 0, and a lock failure releases nothing and
returns 0. Exactly one image destructor,
`mimir_core_unload_drain` (:1107), calls it at
`dlclose` and at process exit, before the image
unmaps; there is deliberately no constructor. H-001's
recorded evidence: anonymous rw growth of exactly
**0 B per load/unload cycle** (test_h001 299/0),
discharging the G-008 finding of +49,152 B per cycle.

Surface status: the drain entry point is internal
shape (b) state - declared in `src/core/mimir_core.h`
and re-declared for tests only in
`tests/core/mimir_drain_h001.h`; in the shared library
it is bound local and no consumer of the `.so` can
link against it. **On-demand cache release therefore
exists today only where a test can link the objects.**
Giving a caller a release operation is I-003's
chartered decision, not an existing fact.

### 2.4 The audit counters, named exactly

The **E-005 internal audit counters**
(`docs/PHASE_5_AUDIT_HOOK_E005.md`, INC-E001-07 shape
(b)) are exactly six, and they count **events, never
bytes**:

| Counter | Defined in | Counts exactly |
|---|---|---|
| `mimir_audit_call_malloc` | `src/api/mimir_alloc.c:113` | Invocations of `mimir_malloc` at the L1 boundary, whatever the outcome (success, defined failure, unresolved-case posture) |
| `mimir_audit_call_calloc` | `src/api/mimir_alloc.c:114` | Invocations of `mimir_calloc`, same rule |
| `mimir_audit_call_realloc` | `src/api/mimir_alloc.c:115` | Invocations of `mimir_realloc`, same rule |
| `mimir_audit_call_free` | `src/api/mimir_alloc.c:116` | Invocations of `mimir_free`, same rule (including `mimir_free(NULL)`) |
| `mimir_audit_alloc_published` | `src/core/mimir_core.c:233` | Large-path mappings published LIVE (`alloc_locked`, the single publication point: malloc, calloc, realloc grow-by-move) |
| `mimir_audit_mapping_retired` | `src/core/mimir_core.c:234` | Large-path mappings successfully released (`retire_locked`: free of a live large block, realloc's transfer of the old block). Published minus retired = live large mappings |

Phase 7 added a second, separate set beside them
(G-003, G-002 R-5 - span-served allocations move **no**
mapping counter, so the sets never double-count):

| Counter | Defined in | Counts exactly |
|---|---|---|
| `mimir_audit_span_published` | `src/core/mimir_core.c:628` | Spans acquired and published LIVE (`span_acquire_locked`) |
| `mimir_audit_span_retired` | `src/core/mimir_core.c:629` | Spans successfully released (`span_release_locked`). Published minus retired = live spans |
| `mimir_audit_small_alloc` | `src/core/mimir_core.c:630` | Small objects handed out from spans (small malloc, small calloc, realloc constructions whose new size routes small) |
| `mimir_audit_small_free` | `src/core/mimir_core.c:631` | Small objects retired back to their spans (free of a span object, realloc's transfer of an old span block). Alloc minus free = live small objects |

All ten counters share one status: **internal audit
state only, not part of any contract** ([UNRESOLVED:
U-03] applies to the whole namespaced surface). They
are declared in no installed header - the test-only
declarations are `tests/abi/mimir_audit_e005.h` (the
six) and `tests/core/mimir_small_g003.h` (the four) -
readable only by linking the library **objects**
directly, and bound local in the shared library
(hidden visibility plus the version script's
`local: *`), so a `.so` consumer cannot link against
or read any of them.

What the counters do **not** provide, read as
carefully as what they do: no byte totals of any kind
(requested, usable, mapped, cached, released or peak);
no failure counter (a failed call moves its call
counter and no publication counter, but the two paths'
publication counters are separate, so a single exact
failure count for a mixed workload cannot be
reconstructed from outside the core - INC-I001-03);
no per-class occupancy beyond what span/small
differences imply; no timestamps and no peak tracking.
The core does keep exact live byte state -
`mimir_live_bytes` and `mimir_live_mappings` for the
mapping registry (:206-207), `mimir_live_spans`,
`mimir_live_span_bytes` and `mimir_live_small_objects`
for the span registry (:624-626) - but all five are
`static` to `mimir_core.c` with no accessor of any
kind: **the allocator knows its managed bytes exactly
and exposes them to no one, not even to its own
tests** (INC-I001-02). That gap, not any missing
bookkeeping, is what I-002's telemetry definitions
and I-007's separation proof have to close.

### 2.5 The current failure shape, read from the code

The caller contract today (public header, by analogy
under U-03) promises exactly one failure channel:
**a null return**. `errno` is not promised on any
path (U-10 / V-02); grep finds the word `errno` in
`src/` and `include/` only inside comments stating
that it is not promised, and no code sets it. What
each entry point actually does on failure, from the
core at this base:

| Entry point | Failure input / cause | Exact behaviour (source) |
|---|---|---|
| `mimir_malloc` (`mimir_core_alloc`, :1160) | size 0 | NULL, before any locking or mapping. Posture only; U-09 UNRESOLVED |
| | Global lock cannot be taken | NULL, nothing published, no ownership passes (F-07) |
| | Small path: page-size discovery fails, span cannot hold its class, `mimir_platform_map_acquire` fails, or the live-span counter checks fail | NULL from `small_alloc_locked` (:889) via `span_acquire_locked` (:702); any partially acquired mapping is released first; **never** re-routed to a private mapping (G-002 R-1) |
| | Large path: page-size discovery fails (`alloc_locked`, :354) | NULL, no mapping attempted, no fallback constant (F-11) |
| | Large path: `size > SIZE_MAX - usable_offset` | NULL before evaluation (checked arithmetic, B-007 A3/A4) |
| | Large path: mapping acquire fails (L6 `ERR_MAP` / `ERR_OVERFLOW` / `ERR_INVALID`) | NULL; L6 is all-or-nothing, nothing is mapped, no descriptor exists, registry and counters unchanged |
| | Large path: post-acquire checks fail (usable extent, pointer range, A10 live-counter overflow) | The acquired mapping is released, nothing is published, NULL; registry and counters unchanged |
| `mimir_calloc` (`mimir_core_calloc`, :1204) | Either factor 0 | NULL before any arithmetic. Posture only; U-09 UNRESOLVED |
| | Product overflow (`nmemb > SIZE_MAX / size`, checked before the multiplication is relied on) | NULL, pre-admission: no wrapped allocation, no mapping, nothing published |
| | Any other construction failure | Exactly the `mimir_malloc` failure of the checked product size: NULL, no ownership passes; on success the usable extent is zeroed before publication |
| `mimir_realloc` (`mimir_core_realloc`, :1294) | `ptr == NULL` | Behaves as `mimir_malloc(new_size)`, success and failure alike, including the zero-size posture |
| | `new_size == 0`, live `ptr` | NULL, original block left LIVE, unchanged and caller-owned, before any allocator state is touched. Posture only; U-12 UNRESOLVED, outside the supported list |
| | Invalid `ptr` (foreign, interior, already freed) | NULL; nothing allocated, unmapped, unlinked or counted on suspicion. The call itself is undefined behaviour with no promised outcome |
| | Same size (mapping block) / new size the old span class can serve (span block) | The same pointer, contents untouched; nothing allocated or released, so this case cannot enter the failure path |
| | Shrink (mapping block) | The same pointer; only the descriptor's `requested_size` record changes. The whole mapping is retained - **a shrink releases no system memory** (B-008 S4.4 design fact; pressure consequence INC-I001-06) |
| | Grow: new-block construction fails at any step above | NULL, and the original block remains valid, unchanged in contents and ownership, and freeable exactly as before (failure-atomic, MA-6.1-REALLOC-02 / MA-6.2-022 by analogy). The new block is constructed completely, the copy made, and only then is the old block retired - in that order, under the one lock |
| | Grow: the old block's release fails at the transfer step | `retire_locked` has already rolled the old block back LIVE (F-06); the new block is retired as unpublished partial work and NULL is returned with the original preserved. If that rollback release also fails, the new block stays LIVE and tracked rather than lost, and NULL is still returned. Neither branch is reachable through the public API on a healthy host - **no fault-injection hook exists**, so both rest on code inspection today (C-005 record, INC-C005-03) |
| | Global lock cannot be taken | NULL, original untouched (F-07) |
| `mimir_free` (`mimir_core_free`, :1406) | `ptr == NULL` | Defined no-op, returns normally before any locking |
| | Global lock cannot be taken | Returns with the block **not** freed - it stays LIVE (F-07). `free` returns void, so the caller cannot observe this |
| | Invalid `ptr` | Non-destructive return: nothing unmapped, unlinked or counted. Undefined behaviour, no defined error or recovery promised |
| | The mapping/span release itself fails | F-06 rollback in full: descriptor/span restored LIVE and re-linked, counters restored, the block remains recorded as owned - and `free` still returns void with no signal of any kind (INC-I001-05) |

Reading the table as Phase 9 evidence: the failure
shape is already the shape P9-4 asks for - null-only,
no partial or smaller block, no overlapping return,
realloc failure-atomic, no silent fallback to another
allocator (Invariant 10). What is missing is not the
shape but the **proof under genuine pressure**: every
mapping-failure row above except the unsatisfiable-
size and RLIMIT rows of the C-008 program
(Section 2.8) rests on code inspection, because the
Nth-mapping injection that would exercise it does not
exist. I-004 owns turning this reading into runs.

### 2.6 Absence proofs: budget, trim, telemetry

Proven in this slice's session on the Pi, over
`src/` and `include/` at this base - absence shown,
not assumed:

| Searched for | Command (over `src/` + `include/`) | Result |
|---|---|---|
| Soft budget | `grep -rni budget` | **0 hits.** No budget value, variable, comparison or setter exists; admission in both paths is only the zero-size gate plus checked arithmetic - no request is ever compared against any configured limit |
| Telemetry | `grep -rni telemetry` | **0 hits.** No telemetry type, snapshot, reader or callback exists on any surface |
| Trim | `grep -rni trim` | **1 hit**, and it is the absence record itself: `include/mimir_alloc.h` states the header declares no *usable-size, statistics, trim, or reallocarray entry point*, and that absent symbols are the documented unsupported behaviour - absence at link time, never a runtime refusal and never a silent fallback |
| Partial release / page advice | `grep -rn 'madvise\|MADV_'` | **0 hits.** L6 offers exactly acquire / release-whole / page-size / lock (`mimir_platform.h`); there is no `madvise`, no partial unmap, no sub-mapping release anywhere in the allocator |
| Process-memory reading | `grep -rn 'getrusage\|/proc'` (plus `VmRSS` / `smaps` terms) | **0 hits in `src/` + `include/`.** The allocator never reads its own process's memory figures; every RSS figure in the banked evidence was read by an external test harness, never by the allocator |
| Public surface | `nm -D --defined-only build/libmimiralloc.so` (library confirmed current by `make lib`, exit 0, in this slice) | **Exactly 4 defined dynamic symbols:** `mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc` - the version script's (`src/api/mimir_alloc.map`) four globals, everything else bound local. No budget, statistics, trim, release or telemetry symbol exists for a `.so` consumer to call, and the public header declares the same four and no others |

Internal/test surface, for completeness: the only
pressure-adjacent operations a test-linked binary can
reach are the retention-bound setter/getter (2.2) and
the drain (2.3). Neither is a budget, neither reports
telemetry, and neither is reachable through the
shared library. **Conclusion: P9-1, P9-2 and the
on-demand half of P9-3 describe surfaces that do not
exist at this base in any form.** Any new surface is
a decision for its owning later slice (I-002, I-003),
recorded there with its contract reasoning under
U-03 / INC-E001-07 shape (c) - never silently added,
and never added by this slice.

### 2.7 Telemetry separation: what can be told apart today

The Phase 9 separation sentence names five kinds of
bytes. Their standing at this base:

- **Allocator-managed bytes.** By construction these
  are exactly: live large mappings (`mapping_length`
  each: one reserve page plus the page-rounded usable
  extent), live spans (one system page each, including
  retained empty spans), and nothing else - L6 acquires
  only `MAP_PRIVATE | MAP_ANONYMOUS` mappings
  (`mimir_platform.c:109`), the allocator maps no file
  and reserves no other memory. The core's static byte
  state (2.4) tracks the first two kinds exactly but
  is unobservable, so today managed bytes can only be
  *derived* in test-linked processes from the event
  counters plus per-path geometry - exact only for
  uniform workloads (the H-008 memory leg's method),
  not for arbitrary ones (INC-I001-02).
- **Process-wide memory** (RSS / PSS / VmSize and the
  rest of `/proc`): includes the image, libc, stacks,
  every other mapping in the process and this
  allocator's. The allocator neither reads nor
  reports it (2.6); H-008's probe read VmRSS
  externally and reconciled it against the counters
  only at quiescent points with a deterministic held
  set. No standing separation exists.
- **Model weights.** Never allocator-managed here:
  weights in the Aesir workloads this project serves
  are `mmap`-backed file residency or runtime-owned
  buffers (Roadmap Phase 12's own separation), and
  this allocator's anonymous-only acquire form means
  none of its mappings can be a weight mapping. The
  separation is true by construction; I-007 still
  owes the measured proof, because the exit asks for
  shown separation, not argued separation.
- **Mmap-backed files.** Same standing: no file is
  ever mapped by this allocator (no `open`, no file
  descriptor in L6 - the acquire call passes fd -1),
  so file-backed pages in a process map are never
  MimirAlloc's. Proof owed by I-007 on a process that
  also carries real file mappings.
- **Device-driver memory** (GPU / NPU / DMA): outside
  this allocator entirely - it manages host-side
  anonymous memory only (Roadmap Section 12: CPU, GPU
  and NPU are separate systems with their own APIs).
  No counter here can see driver memory, and no
  Phase 9 claim may fold it into managed bytes.

### 2.8 Pressure-adjacent evidence that already exists

Not Phase 9 evidence, but the ground I-004/I-005
build on - all banked, all Pi-native Linux/glibc:

- **C-008 controlled-failure program**
  (`tests/core/test_c008.c`, in `make test`): S93-2
  lowers `RLIMIT_AS` in a forked child and allocates
  until the OS refuses - the refusal arrives as
  exactly a null return, no crash, no hang, no partial
  block, and a fresh allocation under the same limit
  succeeds after frees; S93-3 churns a fragmented
  address space and verifies survivors pairwise
  disjoint; S93-5 runs a 120-round failure storm with
  the same properties. Its S93-INJ cases (fail the
  Nth mapping, metadata-allocation injection,
  EMFILE/EAGAIN classes) are **SKIPPED/NOT-YET** in
  its own recorded output: the shim does not exist
  (INC-C008-01, from INC-C007-01).
- **G-005** measured the retention bound exactly:
  after churn forcing 3 spans in each of the 20
  classes, exactly 2 empty spans per class are
  retained (40 spans, 655,360 B at this page size),
  never exceeded; idle threads add zero retention
  (there is no thread-local cache to retain).
- **G-008 / H-001** measured the unload dimension:
  +49,152 B of orphaned anonymous memory per
  load/unload cycle before the drain, exactly 0 B
  after it (2.3).
- **H-008** reconciled held-set memory against the
  audit counters and VmRSS at quiescent points under
  contention - the closest existing approach to
  I-007's separation proof, and bounded to its
  uniform held set (2.7).

No banked artefact measures: reclaim cost in time
(P9-5), fragmentation as a quantity (P9-6), behaviour
under a budget (none exists), behaviour across
repeated pressure cycles as a designed experiment
(I-008), or anything at all on Android (Section 5).

## 3. The Wave 9 slice map (I-002..I-009)

The queue as derived in `WAVE_PLAN.md` (coordinator
checkout, 2026-11-07 05:3x EST), each slice with the
evidence obligation this plan fixes for it. Slices
bank in roadmap order; a slice's evidence obligation
is discharged only by its own recorded runs on the
Pi, never by another slice's summary.

| Slice | Team | Charter (queue wording, in substance) | Evidence obligation fixed by this plan |
|---|---|---|---|
| **I-002** | 02 | Soft-budget design + process telemetry definitions (allocator-managed bytes separated from process-wide, weights, file-backed, driver memory) | A definition document that gives every term a checkable meaning: *soft budget* (what is counted - requested, usable or mapped bytes; what happens at the boundary - refusal, trim-first, or advisory; per-process scope; who sets it; what a caller observes), each telemetry quantity (name, unit, exact source state, path split small/large, staleness under Invariant 8), and the five-way separation table of Section 2.7 turned into definitions I-007 can measure against. Must state which surface form any budget/telemetry takes (public, internal shape (b), or none) with the U-03 contract reasoning, and must record that enforcement implementation is chartered here as design only unless a later slice adopts it (INC-I001-14). No code claim without a run |
| **I-003** | 03 | Trimming and cache-release implementation (retained-span release on demand; public surface decision recorded, no silent ABI expansion) | A recorded run showing on-demand release actually returning retained bytes to the OS (span counters and an OS-side figure moving together), the retained bound of Section 2.2 re-verified after the change, live blocks demonstrably untouched by release, and drain behaviour (2.3) unregressed. The public-surface decision - expose, internal-only, or none - recorded with reasons; if the ABI moves at all, `nm -D` evidence and the contract decision are in the slice doc, and *no silent ABI expansion* means any count other than the decided one fails the slice |
| **I-004** | 04 | Genuine allocation-failure handling (Nth-mapping fault injection per Section 9.3, NULL contract, realloc failure-atomic under pressure) | The fault-injection shim itself (the instrument owed since INC-C007-01) plus recorded sweeps: fail the Nth mapping across both paths, at several N, showing exactly-null returns, no overlap among survivors, sentinel contents intact, recovery allocation succeeding after each injected failure, and realloc grow under injection returning NULL with the original byte-for-byte intact. Lock-failure and release-failure branches (2.5) either exercised by the shim or explicitly carried as still-inspection-only |
| **I-005** | 05 | Constrained-memory runs (address-space limits, bounded behavior, no hang, no overlap) | Recorded runs under stated limits (address-space limit at minimum, in the C-008 S93-2 child form or stronger): the limit value, the workload, where refusal began, that refusal was exactly-null with no hang (a wall-clock bound stated before the run, not after) and no overlap, allocator health after the limit is lifted or in a fresh process, and the page size recorded with every byte figure (INC-I001-11). Samtal HTTP 200 checked around any run large enough to matter |
| **I-006** | 06 | Fragmentation and reclaim-overhead measurement | A stated method before numbers: which fragmentation (internal small-path, page-rounding large-path, external/address-space) each figure measures, how reclaim overhead is timed (release cost per span / per byte, drain cost), workload and repetitions, median and tail per Section 11 - and an honest statement, per figure, of whether it is exact from allocator state or derived (INC-I001-04 bounds what is exact). Raw data filed with the slice |
| **I-007** | 07 | Telemetry separation proof (managed bytes vs RSS/mappings measured) | A measured reconciliation, in one process at quiescent points, of: allocator-managed bytes (by I-002's definitions, from whatever surface I-002/I-003 landed), process RSS/mappings from the OS, and at least one non-allocator resident kind (a real file-backed mapping; model-weight-shaped residency where the harness honestly labels it as such). The residual - process bytes that are neither managed nor the named other kinds - is reported, not rounded away. If exact managed bytes are still underivable, the slice grades that plainly instead of approximating silently (INC-I001-02) |
| **I-008** | 08 | Repeated pressure cycles + responsiveness under pressure (bounded, Samtal-safe) | Repeated fill / pressure / release cycles (count stated) showing retained and live figures returning to the same bound each cycle (no ratchet), failure behaviour stable across cycles, and a responsiveness reading under pressure in the G-008/H-008 house form (Samtal HTTP status and latency sampled during the cycles, no bound claimed from a shared home host). Any growth per cycle is reported as measured, never smoothed |
| **I-009** | 09 | Phase 9 exit evidence packet (exit graded honestly; Android kill caveat carried) | The packet grades Section 4's items one by one from the banked I-002..I-008 evidence plus one fresh recorded run, carries every debt of Section 5 forward by name, states the Android caveat in its bottom line (no claim to prevent Android low-memory kills; no Android evidence exists), and uses PARTIAL / UNMET where the evidence is partial - exactly as the H-009 packet did for Phase 8 |

Wave-internal dependency, stated once: I-003, I-004
and I-005 can proceed from this plan alone; I-006
and I-007 measure against I-002's definitions and
whatever surfaces I-003 landed, so their recorded
runs must name the base they measured at; I-008
presupposes I-003's release and I-005's limits;
I-009 presupposes all of them banked.

## 4. Phase 9 exit criteria, gradeable items

The exit sentence - *fault injection and
constrained-memory runs show documented, bounded
behavior; no claim to prevent Android low-memory
kills* - broken into items an I-009 packet can grade
without interpretation. Verdict vocabulary as in the
banked exit packets: **MET** / **PARTIAL** / **UNMET**
/ **OWED**. Grades at this base are this plan's, from
Section 2's inventory only.

| # | Gradeable item | Discharged by | Grade at this base |
|---|---|---|---|
| X1 | Soft budgets exist as explicit, defined objects: counted quantity, boundary behaviour, scope and setter are documented, and the documented behaviour is what a recorded run observes | I-002 (definitions); enforcement evidence from whichever slice adopts it (INC-I001-14) | **OWED** - no budget exists (2.6) |
| X2 | Telemetry definitions exist for every quantity the wave reports, each with unit, source state and path split, and Section 10's rule holds: counters and OS metrics align with their stated meanings in a recorded reconciliation | I-002, proven in I-007 | **OWED** - event counters only, no definitions, no byte state observable (2.4) |
| X3 | Trimming / cache release on demand: a caller-reachable (or explicitly internal-only, by recorded decision) operation releases retained empty spans to the OS, shown by counters and an OS figure moving together, with live blocks untouched and the retention bound re-verified | I-003 | **PARTIAL** - release exists only as the automatic free-time excess release and the internal unload drain (2.2, 2.3); the on-demand, decision-recorded form is owed |
| X4 | Fault injection: with the Nth mapping artificially failed (Section 9.3), every affected call returns exactly NULL, no live block is corrupted or overlapped, no call hangs, a recovery allocation succeeds, and realloc failure leaves the original byte-for-byte intact - across both allocation paths and several N | I-004 | **OWED** - the instrument does not exist (INC-I001-01); the code reading (2.5) is favourable and is not evidence |
| X5 | Constrained-memory runs: under a stated address-space (or stronger) limit, behaviour is documented and bounded - refusal is exactly-null, bounded in time, non-overlapping, and the allocator is healthy afterwards; results tracked for Linux, with the Android and proot tracks named as not run | I-005 (with C-008 S93-2 as prior partial form) | **PARTIAL** - the C-008 RLIMIT child run is a real first form at small scale (2.8); the designed, Phase 9-scoped runs are owed |
| X6 | Reclaim overhead is measured and archived: cost of free-time release and of on-demand release / drain, with method, repetitions and distribution, per Section 11 | I-006 | **OWED** - no reclaim-cost figure exists (2.8) |
| X7 | Fragmentation is measured and archived: internal, page-rounding and address-space fragmentation each either measured by a stated method or explicitly declared not measurable from allocator state, with the reason | I-006 | **OWED** - no fragmentation figure exists; one exact form is blocked by INC-I001-04 |
| X8 | Telemetry separation is proven by measurement: managed bytes reconciled against process-wide figures and at least one named non-allocator resident kind in one process, residual reported | I-007 | **OWED** - separable by construction only (2.7); H-008's quiescent reconciliation is a method precedent, not this proof |
| X9 | Repeated pressure cycles are bounded: across the stated cycle count, retained/live figures return to the same bound each cycle, failure behaviour is stable, and responsiveness under pressure is reported without a bound claim | I-008 | **OWED** - never run as a designed experiment (2.8) |
| X10 | The exit packet grades X1-X9 honestly from banked evidence, carries the Android caveat in its verdict, and claims no Android, x86-64, musl or proot result | I-009 | **OWED** - this plan is not the packet |
| XC | Caveat (not gradeable, standing): no Phase 9 document claims the allocator can prevent, survive-by-guarantee, or gracefully recover from an Android low-memory kill; Android pressure behaviour is unmeasured and unclaimed | All Wave 9 slices | **HELD** at this base - this plan makes no such claim, and Section 5 names the missing Android run |

**Exit reading at this base: Phase 9 exit is not
met on any item in full.** X3 and X5 stand PARTIAL on
real banked behaviour; X1, X2, X4, X6, X7, X8, X9
and X10 are OWED. That is the expected standing for
a foundation slice - and it is stated here so no
later summary can mistake the plan for progress on
the items themselves.

## 5. Carried debts, restated

Nothing in Wave 9 discharges any of these unless its
own row says so; I-009 carries them forward again.

- **Phase 3 exit UNMET - x86-64 suite run owed.**
  The portable suite has never run on x86-64 (no
  x86-64 libc sysroot on the Pi; the install is
  reserved for Volmarr's word). The Phase 3 packet's
  tally stands (MET 7 / UNMET 9 / OWED 4 of 20 at its
  base, since amended only where later packets say
  so). Consequence for Phase 9: every Wave 9 result
  is single-target Pi-native AArch64 Linux/glibc,
  16384-byte pages, until that run exists. The
  4096-page-host allocator run is owed with it
  (INC-D003-02) - and with it, every byte figure in
  this plan is a 16 KiB-page figure (INC-I001-11).
- **Phase 4 exit UNMET - physical Android run owed.**
  No MimirAlloc run on any Android device exists:
  no Android NDK on the Pi (install reserved for
  Volmarr's word), no NDK build, no device run; the
  ready test program in `tests/android/` has only
  ever run as a native logic check on the Pi.
  Consequence for Phase 9: the roadmap's Android
  sentence (P9-9 / XC) can only be carried as a
  caveat this wave. No Wave 9 slice may word a Linux
  pressure result as Android evidence, and the exit
  packet must grade the Android track OWED, exactly
  as the Section 9.3 separate-tracking rule requires.
- **Phase 8 exit PARTIAL - posture caveat S5.**
  The H-009 packet graded Phase 8 PARTIAL for two
  reasons: E2's H-008 banking (since discharged -
  H-008 banked as `af1f066` and H-009 itself as
  `81c0474`, the base of this slice, so the banking
  reason no longer stands) and **S5, which stands**:
  deferred cancellation and teardown are proven,
  while asynchronous cancellation and
  fork-while-busy are **measured unsupported** -
  decided H-006 contract postures (the unsupported
  shapes fail by hanging, never by corruption),
  not defects awaiting a fix. Consequence for
  Phase 9: pressure runs that involve cancellation
  or fork inherit those postures; I-005/I-008 must
  design around them (no async-cancellation or
  fork-while-busy leg) or name the hang risk
  explicitly, and no Wave 9 slice re-opens H-006's
  decision.
- **Phase 7 exit PARTIAL remainder (X5).** The
  exit-grade retained-memory comparison against the
  platform allocator still needs an
  allocator-attributable instrument for the platform
  side; RSS-only readings are what exist
  (INC-G001-06, G-009 O-14, H-009 R-09). I-007's
  separation proof works on the MimirAlloc side of
  that comparison and does not discharge the
  platform-side instrument debt; the natural home
  named by the packets is the Phase 10 counters.
- **Phase 1 exit unmet; Phase 6 Clause A UNMET /
  X1 BLOCKED** (blocker-record alternative only) -
  restated by every exit packet since F-009 and
  carried unchanged; nothing in Wave 9 touches
  either. Their resumption points are their own.
- **Open contract questions that bind Phase 9:**
  U-03 (the whole namespaced surface is by-analogy;
  every new Wave 9 surface needs its own contract
  decision - INC-I001-09), U-09 (zero-size) and
  U-12 (`realloc(p, 0)`) postures unchanged by
  pressure work, U-10 (errno never promised - the
  null return is the whole failure channel I-004
  evidences), and U-17..U-21 (fork), whose posture
  half is the S5 caveat above.
- **The fault-injection debt** (INC-C007-01 /
  INC-C008-01) is restated inside this plan as
  INC-I001-01 because Wave 9's exit depends on it
directly: it is the oldest instrument debt in the
  campaign and I-004 is its fourth chartered home
  (after the C-007, C-008 and Phase 3 packet
  records). If I-004 cannot land the shim, the
  Phase 9 exit cannot grade X4 above OWED and
  I-009 must say so in its bottom line.

## 6. Inconsistencies and unknowns (INC-I001)

- **INC-I001-01 (instrument missing; exit-critical).**
  No fault-injection shim exists anywhere in the
  banked tree (carried INC-C007-01 / INC-C008-01;
  this slice's C-008 re-run still prints its
  S93-INJ cases SKIPPED/NOT-YET for exactly this
  reason, Section 7). The Phase 9 exit's first half
  names fault injection, so the wave's central
  instrument is also its oldest debt. Owner: I-004.
- **INC-I001-02 (managed bytes unobservable).** The
  core's live byte state (`mimir_live_bytes`,
  `mimir_live_span_bytes`, Section 2.4) is `static`
  with no accessor; the audit counters count events,
  not bytes. Exact allocator-managed bytes for an
  arbitrary workload cannot be obtained today - only
  derived for uniform workloads. Owner: I-002
  (definitions / surface decision), proven or graded
  by I-007.
- **INC-I001-03 (no failure counter).** Allocation
  failures are counted nowhere. Call counters move
  on failure and publication counters do not, but
  the small and large paths publish through
  different counters, so one exact failure count for
  a mixed workload cannot be reconstructed outside
  the core. The Phase 10 counter list names
  *allocation failures* explicitly; I-002 records
  the definition, and no Wave 9 slice may report a
  failure count it derived by subtraction without
  naming the derivation. Owner: I-002.
- **INC-I001-04 (small-path requested size not
  recorded).** A span object carries no per-object
  requested-size record - the span's class size is
  the only size the allocator knows for it
  (INC-G003-03). Exact internal fragmentation and
  exact requested-vs-usable bytes for the small
  path are therefore not computable from allocator
  state; only the bound (waste strictly less than
  the class stride, plus the 96-byte header per
  span) is. Owner: I-006, which must grade its
  small-path figure as bounded/derived, not exact.
- **INC-I001-05 (release failure is caller-
  invisible).** On an L6 release failure the F-06
  rollback correctly keeps the block recorded as
  owned - and `mimir_free` returns void with no
  signal, so a caller cannot learn that memory it
  freed was not returned to the OS. Under sustained
  unmap failure, unreleasable memory would grow
  with no observable channel except the (internal,
  test-only) counters. No fix is proposed here;
  whether Phase 9 needs a diagnostic for this is a
  design question. Owners: I-003 (release design),
  I-004 (injection is the only way to exercise it).
- **INC-I001-06 (shrink releases nothing).** On
  both paths a shrink retains the whole mapping /
  class extent (2.5); a workload that grows then
  shrinks holds its peak bytes until free. This is
  a banked design fact (B-008 S4.4), not a defect -
  but its pressure consequence has never been
  measured or even stated in a slice doc before
  this one. Owners: I-005 (hold-peak behaviour
  under limits), I-006 (fragmentation accounting
  must count retained shrink extents honestly).
- **INC-I001-07 (the only pressure knob is
  internal, non-eager).** The retention bound is
  test-surface only and applies at free time;
  lowering it releases nothing until classes next
  empty, and no caller of the shared library can
  reach it at all. A process under pressure today
  has **no action it can take** through this
  allocator to shed retained memory on demand.
  Owner: I-003 (the on-demand decision).
- **INC-I001-08 (roadmap term undefined).** Phase 9
  says *process telemetry* without defining whether
  it means allocator-reported quantities,
  OS-reported process figures, or the reconciliation
  of both. This plan adopts the reconciliation
  reading - it is the only reading under which the
  phase's own separation sentence and Section 10's
  *counters and OS metrics align* rule make sense -
  and records the adoption here rather than
  presenting it as the roadmap's plain words.
  Owner: I-002, which pins the definitions.
- **INC-I001-09 (no contract home for new
  surfaces).** U-03 stands: the namespaced API has
  no direct contract rows, so a budget setter, a
  telemetry reader and a trim entry point each
  need their own contract decision (the
  INC-E001-07 shape (c) pattern) before any of them
  can be a public symbol. Implementation slices
  that add a public surface without that decision
  violate this plan. Owners: I-002 / I-003.
- **INC-I001-10 (Android pressure behaviour wholly
  unknown).** No Android run of any kind exists
  (Phase 4 debt, Section 5); Android's low-memory
  killer behaviour against this allocator - when it
  kills, what state the process is in, whether any
  graceful path exists - is not merely unmeasured
  but unobserved. Every Wave 9 Android sentence is
  a caveat, never a finding. Owner: I-009 (grading
  discipline); the run itself remains Phase 4's.
- **INC-I001-11 (byte figures are page-size
  figures).** Every retention and mapping byte
  figure in this plan is computed at this Pi's
  measured 16384-byte system page. The same span
  counts on a 4096-byte host retain one quarter of
  the bytes and every large mapping carries one
  quarter of the reserve/rounding overhead. Wave 9
  recorded runs must state the page size beside
  every byte figure, as the banked runs do.
  Owners: all Wave 9 slices.
- **INC-I001-12 (stale growth check in the tree).**
  `tests/core/test_g008.c` still carries its
  pre-H-001 PASS-check that expects retained spans
  to orphan on unload; at the H-009 base it ran
  214/1 for exactly that check (INC-H001-01,
  `test_g008.c` unedited, superseded by test_h001).
  It is not part of `make test`, so this slice's
  `make test` result (Section 7) neither covers nor
  contradicts it - recorded here so no Wave 9
  reader mistakes a future test-g008 failure for a
  pressure regression.
- **INC-I001-13 (stale base description in the
  campaign record).** The keeper tasking text and
  the head of `WAVE_PLAN.md` still describe the
  campaign base as `b0b8374`, *documentation only,
  no code exists yet*. That was true of that base
  and is false of the tree at `81c0474`, which
  carries the full `src/`, `include/` and `tests/`
  inventory of Section 2. Foundation work must be
  read against the banked tree and the verified
  HEAD, never against the stale paragraph - this
  plan does so, and records the discrepancy so the
  next reader does not have to rediscover it.
- **INC-I001-14 (budget enforcement unchartered).**
  The Wave 9 queue charters budget *design and
  definitions* (I-002) but names no slice to
  *implement* enforcement. Unless I-003 or I-004
  adopts it by recorded decision, X1 can grade no
  higher at I-009 than *defined, not enforced* -
  and this plan fixes that expectation now so the
  gap is a recorded queue property, not an exit-
  day surprise. Owner: the keeper, at I-003/I-004
  muster, or I-009's honest grade.

## 7. Method, evidence and this slice's one run

Method: the roadmap Phase 9 text and Sections 9.1 /
9.3 / 10 / 11 were read in full; the allocator
inventory was read from the six source/header files
and three test-only headers named in Section 2, at
this base, in this worktree; the Wave 9 queue was
read from the tail of `WAVE_PLAN.md` in the
coordinator checkout; carried debts were read from
the banked exit packets named in Section 5. Absence
claims rest only on the greps and the `nm` check
tabulated in Section 2.6, run in this slice's
session. No measurement, reproduction or device
result is claimed in this plan that was not run and
logged on the Pi in this slice or in the banked
slice it cites.

**This slice's run** (permitted, light, in this
worktree at base `81c0474`, Samtal HTTP 200 before
and after): `make test`, exit **0**. Per-binary
counts exactly as logged: test_smoke 18/18;
test_core 29 run, 0 failed; test_b007 69 run,
0 failed; test_c005 95 run, 0 failed; test_c006
23 run, 0 failed; test_c007 122 run, 0 failed
(its fault-injection cases SKIPPED/NOT-YET for
INC-C007-01, as in every banked run); test_c008
42 run, 0 failed, 7 skipped-not-yet (its S93-INJ
and environment cases, Section 2.8); test_d004
4590 run, 0 failed (2088 allocations, 2088 frees).
Total across the eight binaries: **4988 checks run,
0 failed**. Also in this session: `make lib`, exit
0 (library already current), feeding the `nm -D`
check of Section 2.6. No other run was made, and
none of these counts is Phase 9 evidence - they
confirm only that the foundation was written
against a green banked base.

Deliverable of I-001: this document only.
`docs/PHASE_9_FOUNDATION_PLAN.md`, one commit on
`yrsa/mimiralloc-01` whose parent is exactly
`81c0474`; no `src/`, header, Makefile, README or
`WAVE_PLAN.md` change; public ABI unchanged at
exactly the four banked symbols.
