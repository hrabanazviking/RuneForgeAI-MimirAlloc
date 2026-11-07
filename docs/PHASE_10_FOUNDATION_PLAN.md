# Phase 10 - Foundation Plan + Diagnostics Inventory + Counter-Definition Contract (Slice J-001)

Slice: **J-001 (team-01)**. Wave 10, first slice. Phase 10
foundation: this plan originates no implementation; it
inventories every counter and quantity that exists at this
base from the sources as they actually are, marks each
Phase 10 counter EXISTS / PARTIAL / ABSENT against the
roadmap's list, fixes the exact counter-definition contract
that J-002 implements and J-003 proves, plans the
debug/production distinction for J-004 / J-005 / J-008,
breaks the Phase 10 exit sentence into gradeable items for
J-009, and carries every still-open debt forward by name.
Base: `0dd6f7a5e3741d7936186c2eecdca765c36c66db`
(Wave 9 COMPLETE, 9 of 9 banked; Phase 9 exit PARTIAL as
written in `docs/PHASE_9_EXIT_EVIDENCE.md`) on branch
`yrsa/mimiralloc-01`, worktree `/home/pi/mimiralloc-team-01`
on Raspberry Pi `purpleviking1` (aarch64, 4 cores, system
page size 16384, measured by `getconf PAGESIZE` in this
slice).
Host state at muster: `free -g` 15 GiB total, 13 GiB
available; Samtal HTTP 200 on port 8787 before and after
this slice's run. Nothing was stopped, configured, or
installed. All workloads cited from earlier slices are
**synthetic** unless a row says otherwise.
Governing texts: Roadmap Phase 10 (Section 8 of
`docs/MimirAlloc_Research_and_Development_Roadmap.md`)
with Sections 9.1, 9.2, 9.3, 10 and 11;
`WAVE_PLAN.md` in the coordinator checkout (Wave 10 queue,
derived 2026-11-07 07:00 EST); `docs/PHASE_9_FOUNDATION_PLAN.md`
(I-001) and `docs/PHASE_8_FOUNDATION_PLAN_H002.md` (H-002)
as the foundation-plan precedents for structure and honesty;
`docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md` (I-002) for
the Q-quantity definitions this contract adopts rather
than re-invents; the banked slice docs and recorded runs
named in each section. Fresh evidence in this slice: the
source reads, greps and `nm` checks of Section 2 (commands
and results stated where used) and one `make test` run
(Section 9). No other run is claimed.
`README.md`, `include/mimir_alloc.h`, the Makefile and
every `src/` file are byte-identical to base: this slice
adds exactly this one document.

> **BOTTOM LINE, STATED FIRST.** The banked allocator at
> this base has **event counters and exact live byte
> state, but no Phase 10 diagnostics surface**: ten
> internal audit counters count calls, mapping
> publications/retirements and span/object
> publications/retirements - events, never bytes - and
> are readable only by test-linked binaries. The core
> keeps exact live byte state in five `static` variables
> and, since I-003, exposes exactly two locked snapshot
> queries (Q-MAPPED and Q-RETAINED) on the same
> test-only terms. Of the roadmap's ten Phase 10
> quantities, exactly **2 exist** (mapped and cached,
> both as I-003 snapshot queries, not counters), **5 are
> partial** (requested, allocated, released, failures,
> span occupancy - the underlying state or counts exist
> but the byte totals, accessors or failure attribution
> do not), and **3 are absent** (peak bytes, cross-thread
> activity, lock contention - nothing records them in
> any form). There is no debug build, no red zone, no
> canary, no poisoning, no quarantine, and **no error
> reporting channel of any kind** in `src/` - the null
> return is the only failure signal the allocator has
> ever emitted. Wave 10 therefore starts, like Wave 9,
> from definitions and instruments: Section 4 fixes the
> contract, J-002 builds the counters, J-003 proves
> they balance, and J-007 builds reporting from zero.

## 1. Phase 10 in the roadmap's own terms

Roadmap Phase 10, verbatim:

> *Expose counters: requested, allocated, cached, mapped,
> released and peak bytes; allocation failures; span
> occupancy; cross-thread activity; optional lock
> contention. Define exactly what each counter means.
> Add debug-only red zones, canaries, poisoning,
> quarantine or metadata checks where appropriate, with
> overhead reported. Protect against integer overflow
> and corrupted metadata; fail clearly rather than
> pretending to repair arbitrary corruption.*
>
> *Exit: statistics balance under tests, error reports
> are useful and do not recursively allocate, and
> production/debug behavior is clearly distinguished.*

Clause by clause, with the owning Wave 10 slice:

| # | Phase 10 clause (roadmap wording) | Owning slice(s) | Position at this base |
|---|---|---|---|
| P10-1 | Expose counters: requested, allocated, cached, mapped, released and peak bytes | **J-002** (implementation), **J-001** (definitions, Section 4) | 2 of the 6 byte quantities exist as snapshot queries; the rest are partial or absent (Sections 2, 3) |
| P10-2 | Allocation failures (counter) | **J-002** | No failure counter exists in the allocator; failure attribution exists only in the I-004 test-only fault shim (Section 2.6) |
| P10-3 | Span occupancy | **J-002** | Per-span `live_count` / `carve_count` fields exist; no occupancy snapshot or accessor exists (Section 2.4) |
| P10-4 | Cross-thread activity | **J-002** | Nothing records any thread identity anywhere in `src/` (Section 2.7); the *behaviour* is proven (H-003), the *counting* does not exist |
| P10-5 | Optional lock contention | **J-002** (adopt/defer decision), **J-008** (overhead if adopted) | The one global lock is a plain `pthread_mutex_t`; no contention instrumentation of any kind (Section 2.7). The roadmap word is *optional* - a recorded deferral is a valid outcome |
| P10-6 | Define exactly what each counter means | **J-001** (this plan, Section 4) | I-002 defined the Q-quantities for Phase 9; no Phase 10 counter contract existed before this slice |
| P10-7 | Debug-only red zones, canaries | **J-004** | Nothing exists: no debug build flag, no red zone, no canary byte anywhere in `src/` (Section 2.8) |
| P10-8 | Debug-only poisoning, quarantine | **J-005** | Nothing exists (Section 2.8). Quarantine interacts with the cached-bytes definition - the contract rule is fixed in Section 5.3 |
| P10-9 | Metadata checks where appropriate | **J-006** (corrupted-metadata posture) | Always-on descriptor/span validation already exists at free/realloc lookup (Section 2.9); coverage beyond that is J-006's to map |
| P10-10 | Overhead reported (for every debug feature) | **J-004**, **J-005**, **J-008** (comparison) | No debug feature exists, so no overhead figure exists |
| P10-11 | Protect against integer overflow | **J-008** (audit) | Checked arithmetic is already the banked discipline on every size path (Section 2.10); the audit, incl. the new counters' own overflow posture, is owed |
| P10-12 | Corrupted metadata: fail clearly, never pretend to repair | **J-006** | The current posture is non-destructive refusal on invalid pointers (Section 2.9); *corrupted* (once-valid, now damaged) metadata has never been probed - J-006 owns disposable-child probes only |
| E-P10 | Exit: statistics balance under tests | **J-003** (proof), **J-009** (packet) | Gradeable breakdown in Section 6. At this base: OWED - the identities are fixed in Section 4.4, the counters mostly do not exist |
| E-P10-R | Exit: error reports are useful and do not recursively allocate | **J-007** (reporting), **J-009** | No report exists to grade; there is no reporting channel at all (Section 2.8) |
| E-P10-D | Exit: production/debug behavior is clearly distinguished | **J-004**, **J-005**, **J-008**, **J-009** | There is only one build today, so there is nothing to distinguish yet; the distinction plan is Section 5 |

### 1.1 The governing cross-sections

**Section 9.1 invariants that Phase 10 work must not
break** (roadmap numbering): Invariant 8 (*statistics do
not drive unsafe decisions if they are approximate or
stale*) is the wave's centre of gravity - it is why
Section 4 requires every core quantity to be read as one
locked snapshot and restricts cross-counter identities to
quiescent points, and why no counter increment may ever
refuse, delay or reroute an allocation. Invariant 3
(*size and alignment calculations cannot overflow*)
binds the counters themselves: a statistics accumulator
that can wrap silently into a small live-byte figure
would be worse than no statistic. Invariant 2 (*metadata
describes actual allocated memory*) is what the balance
proof of J-003 actually tests. Invariant 1 (*one valid
owner, non-overlapping storage*) bounds J-004/J-005:
red zones and quarantine may change debug layout and
debug retention, never ownership.

**Section 9.2 (error detection versus correction)** is
J-006's charter and a wording law for the whole wave:
debug features *detect* some overruns, stale metadata,
repeated frees and corrupted canaries; they cannot
safely recover a corrupted heap, allocator checks must
not be called ECC, and the safe policy is to preserve
diagnostic evidence and stop or report per the host
contract. No Wave 10 document may claim repair of
arbitrary corruption.

**Section 9.3 controlled failure tests** give J-006 and
J-007 their method: artificially fail the Nth mapping
(the I-004 shim, banked), allocate near integer and
alignment boundaries, repeatedly create/destroy threads
while transferring objects, and confirm failures neither
hang nor return overlapping blocks. Corrupted-metadata
probes are destructive by nature and run only in
disposable child processes (Section 6, X5).

**Section 10 test matrix** rows Phase 10 must satisfy:
*Memory* (*counters and OS metrics align with their
stated meanings* - the sentence that makes Section 4's
definitions precede every measurement, exactly as in
Wave 9) and *Robustness* (*defined failure or diagnostic
behavior; no false success* - J-007's exit shape).

**Section 11 benchmarking principles** bind J-004/J-005/
J-008 overhead claims: verify the allocator (and the
build mode) was actually selected; fresh process runs;
report distributions, not single numbers; store raw
data and negative results; timing is never a correctness
oracle (the banked discipline of every perf slice).

**Phase 9 boundary.** Wave 9 defined the telemetry its
own exit needed (I-002's Q-quantities) and expressly
left the Phase 10 counter vocabulary to Phase 10
(I-001 Section 1, Phase 10 boundary paragraph). This
plan adopts I-002's definitions verbatim where they
exist (Q-MAPPED, Q-RETAINED, Q-PEAKMAPPED, Q-FAILED-OS,
Q-REFUSED-BUDGET) and names Phase 10 counters as the
surface that finally implements them. Nothing in Wave 9
graded a Phase 10 item, and nothing here re-grades a
Phase 9 item.

**Phase 11 boundary.** Performance research (fast paths,
SIMD, sizing) is Phase 11. Wave 10 measures debug
overhead because the roadmap orders it reported; it
does not optimise anything, and no Wave 10 slice may
trade a counter's exactness for speed.

## 2. Diagnostics inventory of the banked allocator

Every claim in this section was read from the sources in
this worktree at this base (`src/api/mimir_alloc.c`,
`src/core/mimir_core.c`, `src/core/mimir_core.h`,
`src/platform/mimir_platform.c`,
`src/platform/mimir_platform.h`, `include/mimir_alloc.h`,
`src/api/mimir_alloc.map`, the Makefile, and the
test-only headers under `tests/`). Line numbers are this
base's. Where a claim of absence is made, the grep
command and its result, both from this slice's session
on the Pi, are stated with it.

### 2.1 Two paths, one lock - the construction the counters must describe

Requests of 1..1024 bytes are served by the **small
path**: 20 size classes (16, 32, 48, 64, 80, 96, 112,
128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768,
896, 1024; `mimir_class_sizes`, `mimir_core.c:580`)
carved out of spans of exactly one system page each
(96-byte span header at offset 0). Requests above 1024
take the **large path**: one fresh anonymous mapping per
allocation (one reserve page holding the 96-byte
descriptor, then the usable extent, page-rounded whole).
All allocator state on both paths sits under **one
global lock** (a single `pthread_mutex_t`, L6,
`mimir_platform.c:140`). There is no per-thread cache,
no thread-local allocator state, and no second lock.
Every counter definition in Section 4 is stated against
exactly this construction; a definition that names a
structure this allocator does not have (a thread cache,
a central bin, a size-segregated free byte total) is a
defect in the definition, not a goal for the code.

### 2.2 The ten audit counters, named exactly

The **E-005 internal audit counters**
(`docs/PHASE_5_AUDIT_HOOK_E005.md`, INC-E001-07 shape
(b)) are exactly six, and they count **events, never
bytes**:

| Counter | Defined in | Increment point | Counts exactly |
|---|---|---|---|
| `mimir_audit_call_malloc` | `src/api/mimir_alloc.c:113` | L1 entry, `__atomic_fetch_add` relaxed, before forwarding | Invocations of `mimir_malloc`, whatever the outcome (success, defined failure, unresolved-case posture) |
| `mimir_audit_call_calloc` | `src/api/mimir_alloc.c:114` | L1 entry, same form | Invocations of `mimir_calloc`, same rule |
| `mimir_audit_call_realloc` | `src/api/mimir_alloc.c:115` | L1 entry, same form | Invocations of `mimir_realloc`, same rule (including `realloc(NULL, n)`, which does **not** move `call_malloc` - INC-E005-06) |
| `mimir_audit_call_free` | `src/api/mimir_alloc.c:116` | L1 entry, same form | Invocations of `mimir_free`, same rule (including the defined no-op `mimir_free(NULL)`) |
| `mimir_audit_alloc_published` | `src/core/mimir_core.c:233` | `alloc_locked`, the single large-path publication point, lock held | Large-path mappings published LIVE (malloc, calloc, realloc grow-by-move constructions). A span-served allocation moves it **never** |
| `mimir_audit_mapping_retired` | `src/core/mimir_core.c:234` | `retire_locked`, on successful L6 release only, lock held | Large-path mappings successfully released (free of a live large block, realloc's transfer of the old block). Published minus retired = live large mappings |

Phase 7 added a second, separate set beside them
(G-003, G-002 R-5 - the sets never double-count, because
a span-served allocation is not a mapping):

| Counter | Defined in | Increment point | Counts exactly |
|---|---|---|---|
| `mimir_audit_span_published` | `src/core/mimir_core.c:628` | `span_acquire_locked`, lock held | Spans acquired and published LIVE |
| `mimir_audit_span_retired` | `src/core/mimir_core.c:629` | `span_release_locked`, on successful release only | Spans successfully released. Published minus retired = live spans |
| `mimir_audit_small_alloc` | `src/core/mimir_core.c:630` | `small_alloc_locked`, lock held | Small objects handed out from spans (small malloc, small calloc, realloc constructions whose new size routes small) |
| `mimir_audit_small_free` | `src/core/mimir_core.c:631` | `small_free_locked`, lock held | Small objects retired back to their spans (free of a span object, realloc's transfer of an old span block). Alloc minus free = live small objects |

All ten share one status and one access rule:
**internal audit state only, not part of any contract**
([UNRESOLVED: U-03] applies to the whole namespaced
surface). The test-only declarations are
`tests/abi/mimir_audit_e005.h` (the six) and
`tests/core/mimir_small_g003.h` (the four); the counters
are readable only by linking the library **objects**
directly, and are bound local in the shared library
(hidden visibility plus the version script's
`local: *`), so a `.so` consumer cannot link against or
read any of them. The four L1 counters are incremented
with relaxed atomics **without** the allocator lock; the
six core counters are incremented with the lock held
(also via relaxed atomic adds, for uniformity). That
asymmetry is load-bearing for Section 4.3: any one
counter is read exactly, but a *set* of counters read
while other threads allocate can tear against each
other, so cross-counter identities are quiescent-point
statements only (INC-J001-03).

What the ten counters do **not** provide: no byte total
of any kind; no failure count (a failed call moves its
call counter and no publication counter, and because the
two paths' publication counters are separate, an exact
mixed-workload failure count cannot be reconstructed
from outside the core - INC-I001-03, carried); no peak
of anything; no per-class split; no thread attribution;
no timestamps.

### 2.3 The exact live state, and the two snapshot queries that exist

The core keeps exact live byte state in five `static`
variables, all updated only under the global lock in
the same locked section that inserts or removes the
registry entry they describe:

| State | Defined in | Meaning exactly |
|---|---|---|
| `mimir_live_mappings` | `src/core/mimir_core.c:206` | Count of LIVE large-path mappings (= length of the descriptor registry) |
| `mimir_live_bytes` | `src/core/mimir_core.c:207` | Sum of `mapping_length` over LIVE large-path mappings (Q-LIVEMAP of I-002 S3.2) |
| `mimir_live_spans` | `src/core/mimir_core.c:624` | Count of LIVE spans on the span registry (live and EMPTY-retained alike) |
| `mimir_live_span_bytes` | `src/core/mimir_core.c:625` | Sum of `mapping_length` (= one page each) over all registered spans (Q-LIVESPAN) |
| `mimir_live_small_objects` | `src/core/mimir_core.c:626` | Count of small objects currently handed out |

plus one tuning scalar, `mimir_retention_bound`
(`:574`, default 2, valid range 0..16), the per-class
EMPTY-span retention bound of G-005 RP-2 / G-007.

Before Wave 9 none of the five state variables had any
accessor. I-003 added exactly three locked snapshot
queries, declared in `src/core/mimir_core.h` and
re-declared for tests only in
`tests/core/mimir_trim_i003.h` (INC-E001-07 shape (b),
bound local in the `.so`, `nm -D` stays exactly 4):

| Query | Returns exactly |
|---|---|
| `mimir_core_mapped_bytes()` | **Q-MAPPED** = `mimir_live_bytes + mimir_live_span_bytes`, read as ONE locked snapshot. `(size_t)-1` only if the lock cannot be taken |
| `mimir_core_retained_bytes()` | **Q-RETAINED** bytes: sum of `mapping_length` over EMPTY spans (`live_count == 0`), one locked registry walk |
| `mimir_core_retained_spans()` | Q-RETAINED span count, the same walk's count |

The same slice's two trim entry points and the H-001
drain return *counts of spans actually released* by
that call (`mimir_core_trim_retained`,
`mimir_core_trim_retained_some`,
`mimir_core_drain_retained`) - return values, not
counters: nothing accumulates them, so cumulative
released spans/bytes exist only if a test sums the
returns itself. The G-007 tuning pair
(`mimir_tune_set_retention_bound` /
`mimir_tune_get_retention_bound`,
`tests/core/mimir_tune_g007.h`) is the same shape (b)
pattern. **That is the entire observable surface at
this base**: ten event counters, three snapshot queries,
three release-count returns, one tuning pair. Everything
else the allocator knows, it knows privately.

### 2.4 Per-object and per-span records

Quantities that exist as *records* (readable by the
core, exposed by no surface) - J-002's raw material:

- **Large descriptor** (`struct mimir_descriptor`,
  96 bytes at the base of the reserve page):
  `mapping_length`, `usable_offset`, `requested_size`
  (the caller's exact request), `usable_size`, `state`,
  and 24 `reserved` bytes, zeroed on publication
  (B-006 S7.3). A registry walk under the lock can
  therefore compute live requested bytes and live
  usable bytes for the large path **exactly today**;
  no code performs that walk for statistics.
- **Span header** (`struct mimir_span_header`, 96 bytes
  at the span base): `class_size`, `class_index`,
  `live_count` (objects currently handed out),
  `carve_count` (objects ever carved from this span),
  `free_head`. Span capacity is derived:
  `span_capacity()` = `(mapping_length - 96) /
  class_size` (`mimir_core.c:651`). Free slots in a
  span are exactly `carve_count - live_count` (every
  carved object is either live or on the free list -
  the I-006 F-3 derivation). Per-span occupancy
  therefore exists as fields; **aggregate or per-class
  occupancy exists nowhere** - no walk computes it and
  no counter accumulates it.
- **Small objects record nothing.** A small object's
  original request size is not stored anywhere - the
  span header keeps the *class* size only
  (INC-I002-02, carried). Any exact live *requested*
  byte total for the small path is impossible from
  allocator state at this base, in principle, not
  merely unimplemented (Section 4.2, D-REQ rule).

### 2.5 The I-002 Q-definitions: defined, partly implemented

I-002 (`docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md`
S3.2, S4.1) already fixed exact definitions for the
quantities Phase 10 now needs. Their standing at this
base, verified against Sections 2.2-2.4:

| Q-quantity (I-002) | Definition (adopted verbatim) | Standing at this base |
|---|---|---|
| Q-LIVEMAP | `mimir_live_bytes` | State exists; no direct accessor (readable only inside Q-MAPPED) |
| Q-LIVESPAN | `mimir_live_span_bytes` | State exists; no direct accessor |
| Q-MAPPED | Q-LIVEMAP + Q-LIVESPAN | **Implemented** as the I-003 locked query (S2.3) |
| Q-RETAINED | EMPTY spans x page, locked registry walk | **Implemented** as the I-003 locked queries (S2.3) |
| Q-LIVEREQ | Sum of original request sizes over live blocks | **Not derivable** for small objects (INC-I002-02); large half derivable by walk, never walked |
| Q-RELEASED | Monotone cumulative sum of every successfully released `mapping_length`, both paths | **Not implemented**: only release *counts* exist; mapping lengths vary per mapping, so the byte sum is not reconstructible from any counter pair after the fact (INC-J001-02) |
| Q-PEAKMAPPED | Maximum of Q-MAPPED sampled at each publication point, under the lock | **Not implemented**: no high-water state of any kind exists |
| Q-REFUSED-BUDGET | Monotone count of budget refusals | **Zero by absence**: no budget exists (INC-I001-14); I-004's FC-4 proved the 0 in its run |
| Q-FAILED-OS | Monotone count of allocation calls returning NULL for backing/oversize/overflow/lock reasons, excluding posture NULLs | **Not implemented** in the allocator; proven only via the I-004 shim's counters in that test binary (S2.6) |

I-007 then *proved* the separation of the implemented
quantities (`docs/PHASE_9_TELEMETRY_SEPARATION.md`,
63/0): Q-MAPPED and Q-RETAINED move exactly with
allocator action and not at all with non-allocator
process action, reconciled against `/proc` VM figures
at 11 quiescent points, with one honestly reported
resident residual (INC-I007-02: residency is
kernel-managed and is not an exact account - a limit
J-003's proof inherits: it balances *allocator*
quantities and VM byte totals, never RSS).

### 2.6 The I-004 fault shim: failure attribution that lives in a test binary

`tests/core/mimir_fault_i004.h` declares twelve
counters in four channels (acquire, release, lock,
page-size; each split attempts / injected / real-failed)
plus four single-shot arming functions. They count
calls the core makes to L6, separating injected
failures from real ones. Three facts bound what they
are: (1) the shim exists only in the `make test-i004`
binary, interposed through the linker's `--wrap`; (2)
**no production object carries any injection or
counting code** - the shared library is built exactly
as banked; (3) the counts describe L6 calls, not
allocation outcomes - I-004 derived its FC-4 failure
accounting from them for its scripted run, but no
allocator-resident failure counter results. They are
inventory here because J-002's failure counter must
agree with them wherever both can see the same event
(Section 4.4, R9 note), and because they prove the
failure *taxonomy* (injected vs real vs posture) is
already exact - what is missing is residence, not
definition.

### 2.7 Cross-thread activity and lock contention: the empty shelf

Proven absences, this slice's greps over `src/`:

| Searched for | Result |
|---|---|
| `pthread_self`, any thread-id type or field | **0 hits in allocator code.** No descriptor, span header or counter records which thread allocated or freed anything. Ownership in this allocator is process-global (the two registries), never per-thread - which is exactly why H-003's cross-thread frees work with no handoff machinery, and exactly why nothing can count them |
| `trylock`, contention, wait counting | **0 hits.** L6 offers exactly `mimir_platform_global_lock` / `_unlock` over one `pthread_mutex_t` (`mimir_platform.c:140-157`); a lock call either blocks in the kernel or succeeds, and nothing observes which |
| Per-thread caches, thread-local state | **None** (G-005 RP-4, restated in H-002 S2). There is no thread-local anything to count activity *in* |

What exists instead is *behavioural* evidence: H-003
verified cross-thread frees counter-exactly after the
fact (12,180 cross-thread frees, every audit counter
reconciled per phase) and H-004 measured contention
cost externally (probe timings around the single lock).
Both are test-side observations of an allocator that
itself records neither quantity. Phase 10 asks for the
allocator-side counters; Section 4.2 defines them and
states the metadata price honestly (INC-J001-01).

### 2.8 Debug features and error reporting: nothing exists

Proven absences, this slice's greps over `src/`,
`include/` and the Makefile:

| Searched for | Result |
|---|---|
| `MIMIR_DEBUG`, `DEBUG`, any build-mode flag | **No debug/production distinction exists.** The Makefile has one `CFLAGS` line (`-std=c11 -Wall -Wextra -Werror -pthread`) used for every object; there is no `DEBUG=` variable, no `-DNDEBUG`, no second build flavour of any kind |
| Red zone, canary, poison, quarantine (names and mechanisms) | **0 hits in `src/` / `include/`.** No guard bytes are written or checked anywhere; a freed small object's first 8 bytes become the free-list link (G-002 S7) - allocator metadata, not poisoning; freed large mappings are unmapped whole, so no freed-byte state exists to poison on that path |
| Error reporting (`stderr`, `write`, `fputs`, `fprintf`, `perror`, `syslog`, `abort`) | **0 hits in `src/`.** The allocator never writes, logs or aborts on any path. The null return is the only failure channel (public header, U-10: `errno` is not promised either). The H-001 unload destructor *deliberately discards* the drain count because no reporting channel exists at unload |
| Assert / internal consistency abort | The only compile-time checks are `_Static_assert`s on the two 96-byte layouts. No runtime assertion exists |

So J-004 and J-005 design their features against a
clean field (no legacy debug code to compat with), and
J-007 designs reporting from zero - including the
constraint the exit sentence names: a report path that
allocates can recursively fail inside the failure it is
reporting, so J-007's channel must be allocation-free
by construction and *proven* so, not merely careful.

### 2.9 Metadata validation that already exists (always on, not debug-only)

One part of P10-9/P10-12 predates Phase 10 and must not
be re-invented as a "debug feature": the free/realloc
lookup already validates, in production, without ever
dereferencing memory derived from the unvalidated
pointer - `descriptor_is_live` (magic, `state == LIVE`,
`mapping_base` == the descriptor's own address) and
`span_header_is_live` (magic, state, self-base,
`usable_offset`, class index/size agreement), plus
registry-membership by construction of the walks. An
invalid pointer (foreign, interior, already freed)
takes the non-destructive course: nothing unmapped,
unlinked or counted on suspicion, plain return / NULL.
The small path additionally re-validates the active
span's header before serving from it. What this does
**not** cover, and J-006 owns mapping precisely:
corruption of a *live* descriptor's or span's fields
after publication (detected only when a later lookup
happens to trip a checked field), corruption of
free-list links, corruption the registry walk itself
must traverse, and any corruption of user-adjacent
bytes. The posture for detected corruption today is
silent refusal (S2.8: no channel exists to fail *loudly*
with) - the gap between "fail clearly" (roadmap) and
"refuse quietly" (code) is J-006/J-007's joint work,
recorded as INC-J001-04's second half.

### 2.10 Overflow protection that already exists

J-008 audits; it does not start from zero. The banked
discipline (B-007 Section A, check-before-evaluate) is
in code at every size step, verified by reading in
this slice: the large path's `size > SIZE_MAX -
usable_offset` gate (`mimir_core.c:377`), the A10 live-
counter checks before publication (`:428-429`:
`mapping_length > SIZE_MAX - mimir_live_bytes`,
`mimir_live_mappings == SIZE_MAX`), the same pair for
spans (`:733-734`), L6's checked page round-up in both
addition and division forms (`mimir_platform.c:65-69`),
and calloc's product check before the multiplication
is relied on (`mimir_core.c:1394`). The counters' own
overflow posture is the unaudited remainder: the ten
audit counters are `unsigned long` (64-bit on this
LP64 target) incremented by unchecked atomic adds -
wraparound is unreachable at any test scale but is
nowhere *stated*; Section 4.3 states it, and J-008
verifies no new J-002 accumulator weakens the live-
state checks above (a statistics add must never become
a reason to refuse a sound allocation - Invariant 8).

## 3. The gap table: Phase 10 quantities at this base

One row per roadmap Phase 10 quantity. EXISTS means a
caller on the established test-only terms can read the
exact quantity today. PARTIAL means underlying state or
counts exist but the quantity as defined cannot be read
exactly (wrong unit, no accessor, or one path missing).
ABSENT means nothing records it in any form. The owning
slices are Wave 10's: J-002 implements, J-003 proves
balance, J-007 owns the reporting consumer; a row's
owner is the slice that closes the gap.

| Phase 10 quantity | Grade | What exists today | Exact gap | Owner |
|---|---|---|---|---|
| Requested bytes | **PARTIAL** | Large-path per-block `requested_size` records; L1 call *counts* | No live total is computed or readable; no cumulative total exists; the small-path live half is **impossible** from allocator state (request size not retained, INC-I002-02) | J-002 (cumulative + live-large per S4.2 D-REQ; small live half stays a named impossibility, not a silent estimate) |
| Allocated bytes | **PARTIAL** | Large `usable_size` records; span `class_size` + `live_count` fields | Derivable exactly by a locked registry walk; no walk, snapshot or accessor performs it | J-002 |
| Cached bytes | **EXISTS** | Q-RETAINED via `mimir_core_retained_bytes()` / `_spans()`, one locked snapshot (I-003) | None for the quantity as defined (S4.2 D-CACHED adopts it). Surface is test-only shape (b), like every quantity here | J-002 (adopts into the unified snapshot; no new mechanism) |
| Mapped bytes | **EXISTS** | Q-MAPPED via `mimir_core_mapped_bytes()`, one locked snapshot (I-003) | None for the quantity as defined (D-MAPPED) | J-002 (adopts; adds the cumulative/peak companions) |
| Released bytes | **PARTIAL** | Release *counts* (`mapping_retired`, `span_retired`); per-call span counts returned by trim/drain | Cumulative *bytes* not reconstructible after the fact - mapping lengths vary and are gone at release (INC-J001-02). Needs accumulators fed at the two release points | J-002 |
| Peak bytes | **ABSENT** | Nothing: no high-water state for any quantity | Q-PEAKMAPPED defined by I-002, never implemented. Needs a publication-point maximum under the lock | J-002 |
| Allocation failures | **PARTIAL** | Failure *taxonomy* proven by the I-004 shim in its test binary; call counters move on failure | No allocator-resident failure counter; from the ten counters alone an exact mixed-workload failure count cannot be derived (INC-I001-03) | J-002 (counter), J-007 (reporting consumer) |
| Span occupancy | **PARTIAL** | `live_count`, `carve_count`, capacity derivation per span | No aggregate or per-class occupancy snapshot exists; nothing walks the span registry for statistics | J-002 |
| Cross-thread activity | **ABSENT** | Behaviour proven externally (H-003); zero thread attribution in the allocator | No thread identity is recorded anywhere; exact counting needs a per-block allocating-thread record that exists for neither path (INC-J001-01) | J-002 (with the S4.2 D-XTHREAD two-tier rule) |
| Lock contention (optional) | **ABSENT** | One plain mutex; external timing probes (H-004) | No wait is observable to the allocator. Roadmap marks this counter optional; adoption requires instrumenting the lock, which perturbs the thing measured | J-002 (recorded adopt/defer decision); J-008 reports overhead if adopted |

**Tally: EXISTS 2 / PARTIAL 5 / ABSENT 3**, of 10.
Read the tally honestly: the two EXISTS rows exist as
*queries over live state*, not as the counters the
roadmap sentence pictures, and every row - existing or
owed - lives on the internal/test-only surface. Whether
any Phase 10 quantity is ever promoted to the public
ABI is a shape-(c) contract decision (INC-E001-07)
that **no Wave 10 slice is chartered to take**; J-002
records the question as OPEN exactly as I-002 did for
the budget surface, and `nm -D` stays at exactly 4 for
the whole wave unless a future contract slice rules
otherwise.

## 4. The counter-definition contract

This section is the contract J-002 implements and
J-003 proves. Definitions use the roadmap's words with
one rule: **where a roadmap word admits two readings,
the definition below picks one, says so, and names the
other reading as not meant.** All quantities describe
**this allocator instance in this process image**
(I-002 S3.4 scope rule, inherited): two loaded images
are two instances with independent counters, and no
quantity includes any other allocator's memory, the
libc heap, stacks, file-backed pages or driver memory
(the I-007 separation, proven for the quantities that
existed then and binding on the new ones by the same
construction).

### 4.1 Common rules (every counter below)

- **Units.** Byte quantities are bytes, held in
  `size_t`. Event counts are events, held in
  `unsigned long` (64-bit on this target). No quantity
  is a float, a ratio stored as a float, or an
  approximation: where a ratio is wanted (occupancy),
  the contract exposes the exact numerator and
  denominator and lets the consumer divide.
- **Epoch and reset.** Every counter's epoch is the
  process image's lifetime: static zero initialisation
  at load, **monotone non-decreasing thereafter for
  cumulative counters, and no reset operation exists
  or is defined**. A test that needs a zero point
  takes a snapshot and works in deltas. (A reset would
  void every cumulative identity in S4.4 mid-flight;
  no slice may add one silently.)
- **No decision may consume them.** Counters are
  written on paths that have already decided
  (Invariant 8). An increment never refuses, delays,
  reroutes or resizes an allocation; the live-state
  overflow *refusals* of S2.10 are properties of the
  live state itself, banked before counters existed,
  and are unchanged.
- **Overflow posture.** Cumulative counters increment
  modulo 2^64 (the existing audit counters' de facto
  posture, stated here for the first time). At any
  physically runnable scale on this host the wrap is
  unreachable; a J-003 proof states its deltas, never
  an assumption that a counter started at 0. Live byte
  state keeps its banked checked-refusal discipline
  (S2.10) - a different posture for a different kind
  of quantity, and the difference is deliberate.
- **Surface.** Internal/test-only, INC-E001-07 shape
  (b), exactly as Sections 2.2-2.3: declared in the
  internal core header and a test-only header under
  `tests/`, bound local in the shared library, `nm -D`
  exactly 4. One **snapshot operation** (S4.3) is part
  of the contract: J-002 delivers the S4.2 quantities
  as one struct filled under a single lock acquisition,
  alongside - never replacing - the existing per-
  counter and per-query reads.

### 4.2 The definitions, counter by counter

**D-REQ-CUM - requested bytes, cumulative.**
Event: a construction is published LIVE (large mapping
in `alloc_locked`; small object in
`small_alloc_locked`). Increment: by the caller's
request size for that construction (for realloc
grow-by-move, the *new* size; the old block's request
is never subtracted - this is a cumulative flow, not a
balance). Units: bytes. Calloc counts its checked
product once. Failed constructions, posture NULLs,
same-size/shrink/stay reallocs and frees increment it
never. *This* requested quantity is exact for both
paths, which is why the contract's headline "requested"
is the cumulative form.

**D-REQ-LIVE - requested bytes, live (large path only,
labelled).** Value at a snapshot: sum of
`requested_size` over LIVE large descriptors, computed
by registry walk inside the snapshot. The small-path
contribution is **not estimated and not folded in**:
the quantity is named and reported as
requested-live-large wherever it appears, and any
consumer wanting a whole-allocator live requested
figure is refused by the definition itself
(INC-I002-02). J-002 may additionally expose the
small path's *class-charged* live bytes - under
D-ALLOC, never under a requested name.

**D-ALLOC-LIVE - allocated bytes, live.** Value at a
snapshot: sum over live blocks of the extent charged
to that block - for large blocks the descriptor's
`usable_size`; for small objects their span's
`class_size` times the span's `live_count`, summed
over spans. This is the bytes currently handed out,
counted at the granularity the allocator actually
hands them out; it excludes reserve pages, span
headers and uncarved span tails (those live in
D-MAPPED's account, and S4.4 R5 keeps the two accounts
from double-counting). "Allocated" in any Wave 10
document means this quantity; the alternative reading
(allocated = mapped) is Q-MAPPED's, already named.

**D-CACHED - cached bytes.** Adopted verbatim from
I-002/I-003: Q-RETAINED - sum of `mapping_length` over
EMPTY spans (`live_count == 0`), one locked walk. The
small path's retained spans are the allocator's only
cache (the large path retains nothing; no thread cache
exists). Debug-build quarantine changes this
definition's *population*, not its form - the rule is
fixed in S5.3 so J-005 cannot silently redefine it.

**D-MAPPED - mapped bytes.** Adopted verbatim:
Q-MAPPED = Q-LIVEMAP + Q-LIVESPAN =
`mimir_live_bytes + mimir_live_span_bytes`, one locked
snapshot, exactly the anonymous bytes this instance
holds mappings for.

**D-ACQ-CUM / D-REL-CUM - acquired and released
bytes, cumulative.** Events: a mapping or span is
published (D-ACQ-CUM += its `mapping_length`) and a
mapping or span is *successfully* released through
`retire_locked` / `span_release_locked` (D-REL-CUM +=
its recorded `mapping_length`, captured before release
per the F-06 discipline). A rolled-back release
increments D-REL-CUM never - the bytes were not
released. Trim/drain releases are ordinary
`span_release_locked` events and count identically;
there is one release path and one account. D-ACQ-CUM
exists because D-REL-CUM alone cannot be reconciled
after the fact (INC-J001-02); together they close
identity R6. Roadmap's "released" is D-REL-CUM.

**D-PEAK-MAPPED - peak bytes.** The maximum value of
D-MAPPED observed at publication points: after any
publication updates the live state under the lock, if
the new D-MAPPED exceeds the stored peak, the peak
takes the new value - inside the same locked section,
so the peak is exact, not sampled. Release points
never lower it; nothing resets it (S4.1). I-002's
Q-PEAKMAPPED, implemented. Peaks of other quantities
(requested-live, allocated-live) are **not** defined
by this contract and J-002 does not invent them.

**D-FAIL-OS - allocation failures.** Event: an
allocation entry point returns NULL for a backing
reason, counted once per caller-visible failure at the
core boundary: L6 acquire failure on either path,
page-size discovery failure, the checked-arithmetic
refusals (oversize/unrepresentable, calloc product
overflow), lock-acquisition failure, and realloc
grow construction failure (counted once, at the
realloc, with the original preserved - the internal
construction NULL is not double-counted). Excluded by
definition: the posture NULLs - zero-size malloc /
zero-factor calloc (U-09) and `realloc(ptr, 0)`
(U-12) - which move no failure counter (I-002 S3.2,
I-004 FC-6, inherited exactly). `free` has no failure
counter: it returns void, and its F-06 rollback is a
release event that did not happen, visible only as
D-REL-CUM *not* moving. Where the I-004 shim is
linked, its real-failed acquire count and D-FAIL-OS
differ by construction (the shim counts L6 calls;
D-FAIL-OS counts caller-visible failures, including
failures that never reach L6) - J-003 states both
figures and their exact difference rather than forcing
an equality that does not hold.

**D-FAIL-BUDGET - budget refusals.** Defined by I-002
as the count of budget refusals. **No budget exists**
(INC-I001-14, carried in Section 7): D-FAIL-BUDGET is
therefore 0 by absence for the whole of Wave 10
unless a slice adopts enforcement by recorded
decision. J-002 does not create the counter as a
dangling zero - it records the absence, and I-004's
FC-4 precedent ("0 by construction") is the standing
evidence form.

**D-OCC - span occupancy.** Snapshot values, per class
and aggregate: live objects (sum of `live_count`),
carved objects (sum of `carve_count`), capacity
objects (sum of `span_capacity()`), live spans, EMPTY
spans - each an exact registry walk inside the
snapshot. Free slots are carved minus live (exact,
S2.4). Occupancy as a *ratio* is live/carved,
presented by consumers as the pair, never stored.
Per-class rows are 20 (one per class size); the
aggregate row is their sum, and R10 requires the two
to agree exactly.

**D-XTHREAD - cross-thread activity.** Two tiers,
because the metadata price differs by path
(INC-J001-01):
- *Tier 1 (exact, large path).* Counter
  XFREE-LARGE: a free (or a realloc transfer-retire)
  of a large block executed by a thread other than
  the thread that allocated the block. Requires
  J-002 to record the allocating thread's identity in
  the descriptor at publication (the 24 `reserved`
  bytes exist; using 8 of them changes the B-006 S7.3
  "reserved (zero on publication)" record, so the
  change is J-002's to make explicitly in its slice
  doc, with the layout static asserts re-stated - or
  to store the identity in a side record and say why).
  Comparison happens at retire, under the lock, before
  release.
- *Tier 2 (small path).* A small object has no
  per-object record, so an exact per-object allocating
  thread **cannot be recovered** at free time. J-002
  chooses, and records, one of: (a) add per-object
  owner storage on the small path (a layout change
  whose byte cost J-002 reports under P10-10); or
  (b) define XFREE-SMALL as unmeasurable and ship
  Tier 1 alone, with every Wave 10 document naming
  the counter XFREE-LARGE and never "cross-thread
  frees" unqualified. Silent estimation (e.g. by
  span's last-allocating thread) is forbidden - it
  would be wrong exactly in the mixed-thread span
  case the counter exists to describe.
Single-threaded runs must read XFREE = 0 exactly;
that, and XFREE <= total frees, are J-003 identities
(R11), not hopes.

**D-LOCK - lock contention (OPTIONAL).** If adopted:
the count of `mimir_platform_global_lock` acquisitions
that could not be satisfied immediately. The plain
mutex cannot report this; adoption therefore means an
instrumented acquisition (trylock-then-lock) in an
L6 variant used **only** where J-002's recorded
decision puts it (debug build or test-linked build -
Section 5), never silently in the production path,
because the instrumentation changes the timing it
measures. If deferred, J-002's doc records the
deferral and its reason, and no Wave 10 document
claims a contention *count* (H-004's timing evidence
remains what it is: external measurement, not a
counter).

### 4.3 Snapshot and locking semantics

- **Core quantities** (D-REQ-LIVE, D-ALLOC-LIVE,
  D-CACHED, D-MAPPED, D-OCC, the live-state inputs of
  every identity) are read **only** through the S4.1
  snapshot: one lock acquisition, every field filled
  from the registries and state variables under that
  hold, then unlock. A consumer that sums two separate
  query calls has a torn figure by construction and
  may not reconcile with it (the I-002 S3.6 rule that
  created `mimir_core_mapped_bytes`, generalised).
- **Cumulative counters** (the ten audit counters,
  D-REQ-CUM, D-ACQ-CUM, D-REL-CUM, D-FAIL-OS,
  D-XTHREAD) are read individually and exactly
  (atomic or lock-held increments, S2.2), but a *set*
  of them is mutually consistent **only at a
  quiescent point** - no allocator call in flight on
  any thread (I-002 S5.3, the rule every banked
  reconciliation already obeys). J-003's proofs are
  stated and run at quiescent points, single-threaded
  or after joins/barriers, never sampled mid-flight.
- **D-PEAK-MAPPED** is read under the lock (it is
  core state, written at publication points).
- **Lock failure** on a snapshot read follows the
  banked getter posture: byte queries return
  `(size_t)-1` rather than a torn or invented figure
  (G-007 precedent); a snapshot operation reports
  failure as a whole and fills nothing.
- Reading statistics **never allocates** and never
  blocks on anything but the one global lock briefly
  held - a property J-007's reporting design inherits
  for its own channel.

### 4.4 The reconciliation identities J-003 will prove

At any quiescent point, in a process whose counters
started at load (deltas stated where a test shares a
process across phases), with P = page size as measured
at runtime:

- **R1** `alloc_published - mapping_retired ==
  mimir_live_mappings ==` descriptor-registry length.
- **R2** `span_published - span_retired ==
  mimir_live_spans ==` span-registry length.
- **R3** `small_alloc - small_free ==
  mimir_live_small_objects ==` D-OCC live objects.
- **R4** D-MAPPED == `mimir_live_bytes +
  mimir_live_span_bytes` == (sum of `mapping_length`
  over both registries, walked independently).
- **R5** D-MAPPED == D-ALLOC-LIVE's backing +
  overhead, stated without remainder: large mappings
  contribute `mapping_length - usable_size` each
  (reserve page + rounding), spans contribute
  `mapping_length - class_size * live_count` each
  (header + uncarved tail + free slots' extents) -
  the identity is the two registry walks agreeing
  term by term, which is what "no double-count and no
  orphan bytes" means in arithmetic.
- **R6** D-ACQ-CUM - D-REL-CUM == D-MAPPED, exactly,
  at every quiescent point from load. (The identity
  the pair of accumulators exists to close.)
- **R7** D-CACHED == EMPTY spans x P ==
  D-MAPPED - (live spans x P + Q-LIVEMAP), and
  `0 <= D-CACHED <= retention_bound x 20 x P`.
- **R8** D-PEAK-MAPPED >= D-MAPPED at every snapshot;
  D-PEAK-MAPPED is unchanged by any sequence that
  only frees/trims; after free-all + full trim,
  D-MAPPED == 0 while D-PEAK-MAPPED retains the
  run's high-water value exactly.
- **R9** Failure accounting per entry point: for each
  of malloc/calloc, `calls == successes + D-FAIL-OS
  delta + posture NULLs`, where successes are
  publications + span-served constructions + (for
  realloc) stay/shrink completions, each term
  computed by the test from its own script - the
  I-004 FC-4/FC-6 accounting generalised to the
  allocator-resident counter. Realloc's line carries
  its stay/shrink/grow split explicitly.
- **R10** D-OCC aggregate == sum of the 20 per-class
  rows, field by field; free slots == carved - live
  per class and in aggregate.
- **R11** D-XTHREAD: single-threaded workloads read
  exactly 0; for the H-003-shaped cross-thread
  workloads, XFREE-LARGE equals the test's own count
  of large blocks freed by a non-allocating thread
  (Tier 2 per J-002's recorded choice).
- **R12** Full-cycle closure: after any workload that
  frees everything and trims/drains, R1-R3 read 0,
  D-MAPPED == 0, D-CACHED == 0, and D-ACQ-CUM ==
  D-REL-CUM - statistics return to baseline exactly,
  the E-005/I-007 closure property extended to every
  Phase 10 quantity.

J-003 proves these across workloads that include the
Wave 9 pressure shapes (I-005 constrained runs,
I-008 repeated cycles) - balance under *pressure*, not
only under tidy scripts, is the exit's word "under
tests" cashed out. A quantity J-002 did not implement
(e.g. D-FAIL-BUDGET, D-LOCK if deferred) is graded in
J-003/J-009 as its recorded status, never silently
dropped from the list.

## 5. The debug/production distinction plan (J-004, J-005, J-008)

There is one build today (S2.8). This section fixes
the shape the distinction takes so three slices build
one distinction, not three.

### 5.1 Build-flag shape

- **One compile-time macro:** `MIMIR_DEBUG`, defined
  as 1 for a debug build, absent (or 0) for
  production. Selected through the Makefile as
  `make DEBUG=1`, which adds `-DMIMIR_DEBUG=1` and
  directs objects to a **separate build directory**
  (e.g. `build-debug/`), so a debug object can never
  be linked into a production artifact by stale-file
  accident - the two flavours are distinguished by
  *directory and macro*, never by a runtime switch.
- **Default is production.** Plain `make`, `make test`
  and `make lib` remain byte-for-byte the banked
  build; J-004 introduces the flag without changing a
  single default command's output artifacts.
- **No runtime flag.** There is no environment
  variable, setter or mode word that turns debug
  features on in a production binary. A production
  binary does not contain the debug code compiled out
  behind a branch; it does not contain it at all.

### 5.2 What is debug-only

| Feature | Owning slice | Debug-build behaviour | Production |
|---|---|---|---|
| Red zones around allocations | J-004 | Guard extents written and checked on free/realloc | Absent: layouts exactly as banked |
| Canaries in metadata / block tails | J-004 | Written at publication, verified at retire and on demand | Absent |
| Poisoning of freed bytes | J-005 | Freed extents filled with a stated pattern before reuse/release | Absent: freed large mappings unmapped as today; freed small slots carry only the free-list link |
| Quarantine of freed blocks | J-005 | Freed blocks held in a bounded quarantine before reuse | Absent: immediate reuse as banked |
| Extra metadata consistency walks | J-006 (debug half) | Registry/span invariant walks beyond the banked lookup validation | The banked S2.9 validation stays - it was never debug-only and must not become so |
| Lock-contention instrumentation (D-LOCK, if adopted) | J-002 / J-008 | Permitted home: debug or test-linked build | Absent from the production lock path |
| Verbose diagnostic detail | J-007 | Report *detail* may be richer | Report *existence* is **not** debug-only: S5.4 |

Debug features may change debug-build **layout**
(class extents, mapping sizes, span capacity) - that
is what red zones are - and every such change is
reported as overhead (bytes per block, capacity per
span, time per operation, distributions per S11).
Debug features may never change which calls succeed:
for any defined call sequence, a debug build returns
the same success/NULL outcome as production, except
where a debug check detects actual corruption and
fails clearly per Section 9.2 - and that exception is
itself reported, never silent.

### 5.3 The quarantine / cached-bytes rule

Quarantine holds freed blocks out of reuse; if it
holds whole spans EMPTY, those bytes are
allocator-retained and would inflate D-CACHED without
being the retention cache I-002 defined. Rule,
binding on J-005: **in a debug build, D-CACHED keeps
its production definition (EMPTY spans), and
quarantined bytes are reported as their own quantity
with their own name** - or quarantine is implemented
inside spans without holding EMPTY spans, and J-005
proves D-CACHED unmoved in its runs. Redefining
D-CACHED in a debug build, in either direction,
without a recorded contract amendment in J-005's
doc, is a defect. Production D-CACHED is untouched
by construction (no quarantine code exists there).

### 5.4 What production keeps, unchanged

- **Byte behaviour:** for every defined call sequence,
  production returns, contents and counter values are
  exactly the banked ones plus the S4 counters' own
  increments. J-008 proves this by running the full
  suite against both flavours and diffing the
  *observable* results (counts and outcomes, not
  timings or addresses).
- **Symbols:** `nm -D` exactly 4 in both flavours.
  A debug build exports nothing extra.
- **Error reporting (J-007) ships in production.**
  The exit sentence distinguishes debug *features*;
  it does not make reporting debug-only. J-007's
  channel (allocation-free, recursion-proof) exists
  in the production build, with debug builds allowed
  richer detail per S5.2. The failure it reports in
  production is the same failure, in fewer words if
  need be - never no words.
- **Overhead accounting (J-008):** the comparison
  reports, per debug feature and combined: bytes of
  layout overhead, per-operation time distributions
  (median/p95 per Section 11), retained-bytes effect,
  and the production column reading exactly zero /
  absent for each - the table that makes "clearly
  distinguished" a measurement, not a claim.

## 6. Phase 10 exit items (for J-009 to grade)

The roadmap exit sentence - *statistics balance under
tests, error reports are useful and do not recursively
allocate, and production/debug behavior is clearly
distinguished* - broken into gradeable items. No item
grades MET on intention, on a definition alone, or on
evidence from another target: every figure behind a
grade is a Pi-native AArch64 Linux/glibc figure on
16 KiB pages unless its row says otherwise.

| Item | Statement | Owning evidence |
|---|---|---|
| X1 | Every S4.2 counter is implemented on the S4.1 surface, with the definitions of S4.2 verbatim - or the item for that counter is graded by its recorded exception (D-REQ-LIVE small half: named impossibility; D-FAIL-BUDGET: absence recorded; D-XTHREAD Tier 2 and D-LOCK: recorded choice/deferral) | J-002 |
| X2 | Statistics balance: identities R1-R12 hold exactly at quiescent points across scripted, cross-thread and pressure workloads | J-003 |
| X3 | Debug-only red zones + canaries exist in the debug build only, detect their stated overrun/corruption classes in tests, and carry reported overhead | J-004 |
| X4 | Debug-only poisoning + quarantine exist in the debug build only, behave per S5.3, and carry measured overhead | J-005 |
| X5 | Metadata integrity: the corruption classes J-006 enumerates are probed in disposable child processes only; each detected class fails clearly (report + defined stop); no run claims repair of arbitrary corruption; undetectable classes are named as undetectable | J-006 |
| X6 | Error reports are useful and allocation-free: a report names the failing operation and cause class in plain terms, is emitted on a channel that allocates nothing and cannot recursively allocate (proven, incl. under injected backing failure), and production emits it too (S5.4) | J-007 |
| X7 | Hardened build configuration: the production build's flags/warnings posture is stated and gated, and the integer-overflow audit covers every size path incl. the S4 counters' own arithmetic, with the S2.10 banked checks re-verified, not assumed | J-008 |
| X8 | Debug vs production overhead comparison is published with the S5.4 table complete - every debug feature's byte/time/retention cost, production column zero/absent | J-008 (from J-004/J-005 figures) |
| X9 | Production/debug clearly distinguished as a whole: one flag (S5.1), separate build directories, `nm -D` exactly 4 in both, identical observable behaviour on the full suite in both flavours except reported debug detections | J-004, J-005, J-008 |
| X10 | This wave's exit packet grades X1-X9 from banked evidence, carries every unclosed INC by name, and claims no Android / x86-64 / musl / proot result | J-009 |
| XC | Standing caveat: no Wave 10 surface expands the public ABI (shape-(c) decision untaken, S3); statistics drive no allocator decision (Invariant 8); no document claims heap-corruption *repair* (S9.2) | All slices |

**Grading rule inherited from I-001/J-009 precedent:**
an item whose evidence is a definition without a run
grades at most PARTIAL; an item resting on another
slice's banking-parallel evidence is named as such;
X-items are graded on the *exit sentence's* terms,
not on the wave's effort.

## 7. Carried debts, restated (nothing below is discharged by this plan)

- **INC-I001-14 - budget enforcement was adopted by
  NO slice, and Phase 10 does not change that.**
  Stated plainly, as this slice was charged to state
  it: the Wave 10 queue contains no budget-enforcement
  slice; J-002 implements failure *counting*, and a
  budget refusal cannot be counted before a budget
  exists to refuse. D-FAIL-BUDGET stays 0-by-absence
  (S4.2), I-002's budget design stays a design, and
  Phase 9's X1 stays PARTIAL for exactly this reason.
  Phase 10 neither adopts enforcement nor quietly
  redefines the term to make the gap disappear; if
  Volmarr wants enforcement, it is a future slice's
  recorded decision, not a Wave 10 implication.
- **INC-I009-01 - `docs/SUPPORTED_TARGETS.md` is
  stale**: it still states the repository contains
  documentation only and no allocator exists - true
  at its Phase 0 slice, false since Phase 3 and
  falser with every wave. **Does a docs-freshness fix
  belong in Wave 10?** This plan's recorded answer:
  **not inside J-001..J-009 as chartered** - every
  Wave 10 slice owns a Phase 10 deliverable, and
  folding an unrelated rewrite into one of them would
  blur its evidence. It belongs in a small named
  docs-freshness slice of its own (or a keeper
  banking slot between waves), and until then it
  stays carried here and in J-009's register, owed
  and visible - not silently edited by this slice
  (outside J-001's file scope) and not forgotten.
- **INC-I002-02** - small-object original request
  sizes are not retained; D-REQ-LIVE's two-tier form
  (S4.2) is this debt's Phase 10 shape. Closing it
  outright would need a per-object record whose cost
  no slice has measured; the contract refuses to
  estimate around it.
- **INC-I001-03** - failure counts were not
  reconstructible from the audit counters; closed by
  J-002's D-FAIL-OS if and only if J-003's R9 holds.
  Until then it stands.
- **INC-I006-02** - F-6's cross-slice leg (Q-MAPPED
  sampling across I-005's constrained runs) still
  owed; J-003's pressure workloads are its natural
  home but J-003 is not chartered to close Phase 9
  items - recorded, not assumed.
- **Phase 3 exit UNMET** (x86-64 suite run owed),
  **Phase 4 exit UNMET** (physical Android run owed),
  **Phase 9 exit PARTIAL** (X1 above; X7's F-1(a)
  allocator-state form, F-5, F-6) - all carried
  unchanged; every Wave 10 figure is a single-target
  figure, as every Wave 9 figure was.
- **U-03 / U-09 / U-10 / U-12** remain open exactly as
  the public header states them; no Wave 10 counter
  or report settles an unresolved case by posture.

## 8. INC-J001 register (found by this slice)

- **INC-J001-01 (cross-thread metadata price).**
  Exact cross-thread counting needs an allocating-
  thread record per block. The large descriptor has
  24 reserved bytes (usable, with the S7.3 record
  amended on purpose by J-002); a small object has
  no record at all. The two-tier D-XTHREAD contract
  (S4.2) exists because of this item; Tier 2's choice
  is J-002's recorded decision, and any document that
  later says "cross-thread frees" without the tier
  qualifier overclaims against this record.
- **INC-J001-02 (released bytes are not derivable
  retrospectively).** Release counts exist; mapping
  lengths are per-mapping and are destroyed at
  release, so cumulative released bytes cannot be
  computed from banked state - the accumulator pair
  D-ACQ-CUM / D-REL-CUM (S4.2) is a forward-looking
  fix, and no historical released-byte figure for
  Waves 1-9 exists or may be reconstructed.
- **INC-J001-03 (counter-set tearing).** The four L1
  counters increment without the allocator lock
  (relaxed atomics at the boundary, by E-005 design);
  the six core counters increment under it. Any one
  counter is exact; a *set* read mid-flight can tear.
  S4.3 confines identities to quiescent points and
  core quantities to the locked snapshot; a future
  consumer reading the raw counters concurrently and
  reconciling them live would be building on this
  recorded limit, not on a guarantee.
- **INC-J001-04 (no reporting channel; refusal is
  silent).** `src/` contains no write/log/abort path
  at all (S2.8), and the H-001 destructor discards
  its count for exactly that reason. The roadmap's
  "fail clearly" therefore exceeds the code's current
  "refuse quietly" on every invalid/corrupt path;
  J-006 maps the detection side, J-007 builds the
  channel, and until J-007 banks, no Phase 10
  document may describe current behaviour as
  "reporting" anything.
- **INC-J001-05 (one build flavour).** No debug/
  production distinction, flag or second build
  directory exists (S2.8); the Makefile's single
  `CFLAGS` serves every target. S5.1's flag shape is
  new infrastructure J-004 must land without moving
  the default build - if J-004 finds the separate-
  directory rule unworkable in this Makefile's
  structure, that is an inconsistency against this
  plan to record, not a silent variation.
- **INC-J001-06 (carried, restated as J-owned).**
  INC-I001-14 (budget enforcement unadopted) and
  INC-I009-01 (SUPPORTED_TARGETS stale) are carried
  in Section 7 with this slice's explicit rulings:
  Phase 10 does not adopt enforcement, and the
  docs-freshness fix is owed to its own small slice,
  not folded into Wave 10. Both are repeated here so
  J-009's register inherits them from two directions
  and neither can drop out of the chain.
- **INC-J001-07 (span free-list length is derived,
  never stored).** Occupancy's free-slot term
  (carved - live) is exact only because every carved
  object is provably live or on the free list; if a
  future slice adds any third object state
  (quarantine inside spans is the live candidate,
  S5.3), R10's derivation must be re-stated in that
  slice or it silently breaks. Flagged forward to
  J-005 explicitly.
- **INC-J001-08 (peak is mapped-only by contract).**
  "Peak bytes" in the roadmap list sits beside five
  byte nouns; this contract scopes the implemented
  peak to mapped bytes (I-002's Q-PEAKMAPPED) and
  defines no requested/allocated peak. A reader who
  needs a live-allocated high-water mark will not
  find one in Wave 10; that is a recorded scoping
  decision of this slice, open to a future slice's
  amendment, not an oversight to discover later.
- **INC-J001-09 (contention measurement perturbs).**
  D-LOCK's only honest implementations instrument
  the lock and thereby change the timing they count
  waits on; H-004's external timings remain the
  untainted contention evidence. Recorded so J-002's
  adopt/defer decision is made with the perturbation
  named, and so no later slice presents an
  instrumented count and a probe timing as the same
  kind of fact.
- **No allocator defect was found by this slice.**
  The inventory's surprises are all *absences the
  roadmap already schedules* (counters, debug
  features, reporting) plus the carried items above;
  the banked code's counters, state and validation
  agree with their slice docs at every point this
  slice checked. Per the slice rule, absences are
  recorded here and owned in Sections 3-6 - nothing
  was fixed out of scope, because a plan slice that
  quietly patched the allocator would falsify the
  inventory it exists to write.

## 9. Method, evidence and this slice's one run

- **Method.** Read the roadmap Phase 10 text and the
  Wave 10 queue; read every counter-bearing source
  named in Section 2 in full at this base; grep-proved
  each absence in Sections 2.7-2.8 over `src/`,
  `include/` and the Makefile; adopted I-002's
  Q-definitions where they exist instead of drafting
  rivals; wrote the S4 contract so every definition
  names its event, unit, epoch and snapshot rule.
- **This slice's one run** (on the Pi, this worktree,
  this base, after the inventory reads): `rm -rf
  build && make test` exit **0** - all suites PASS,
  final suite (test_d004) **4590 checks, 0 failed**;
  `make lib` exit 0; `nm -D --defined-only` on the
  built `libmimiralloc.so` = exactly **4**
  (`mimir_malloc`, `mimir_free`, `mimir_calloc`,
  `mimir_realloc`). `README.md` sha256
  `bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`
  and `include/mimir_alloc.h` sha256
  `55107f05b3eb8c120230f76b2193fdc953af800af57a91caa405c84971af8772`,
  both byte-identical to `git show HEAD:` at base.
  Samtal HTTP **200** (port 8787) before and after;
  `free -g` 13 GiB available before and after.
  No other measurement is claimed by this slice;
  every other number herein is banked evidence cited
  to its slice, a definition, or arithmetic shown
  where it stands.
- **Files.** This slice adds exactly one file:
  `docs/PHASE_10_FOUNDATION_PLAN.md`. No `src/`,
  header, Makefile or README change; no push, no
  merge - the keeper banks in roadmap order.
