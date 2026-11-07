# Phase 9 — Soft-Budget Design + Process Telemetry Definitions (Slice I-002)

Status: **DESIGN — Proposed.** Nothing in this document is implemented by this slice.
This slice makes **no** change to `src/`, `include/mimir_alloc.h`, the Makefile, or
`README.md`. Every proposal below is labelled **Proposed** where it proposes something
new; every statement about what the code does today is grounded in the banked base
`81c0474` (Wave 8 complete, remote main) and in greps run on the Pi in this slice.

Governing texts: roadmap Phase 9 ("Memory-pressure behavior") and Section 9
(Reliability engineering and failure handling, esp. 9.1 Invariants 7–8 and 9.3
Controlled failure tests); roadmap Phase 10 (counter meanings, for the quantities
this design names but does **not** implement); `docs/PHASE_5_AUDIT_HOOK_E005.md`
(E-005 audit counters); `docs/PHASE_7_RECLAIM_POLICY_G005.md` (G-005 RP-1..RP-5);
`docs/PHASE_7_TUNING_PARAMETERS_G007.md` (G-007 internal tuning pair);
`docs/PHASE_7_UNLOAD_DRAIN_H001.md` (H-001 drain); `include/mimir_alloc.h`
(the exactly-4 public symbols: `mimir_malloc`, `mimir_free`, `mimir_calloc`,
`mimir_realloc`); `docs/contracts.md` (core-symbol rows, by analogy only — the
namespaced surface is [UNRESOLVED: U-03]).

Wave 9 queue: `/home/pi/mimiralloc-forge/WAVE_PLAN.md` line 125. I-001 (team-01,
Phase 9 foundation plan + pressure-state inventory) is built in parallel from the
same base; this slice does not depend on I-001's file existing, and no I-001
statement is relied on here.

## 0. What was actually verified on the Pi in this slice

- Base HEAD `81c04746aa5a3ea497a22ba7f04d39f0629095ec` on branch `yrsa/mimiralloc-02`,
  verified by `git rev-parse HEAD` before drafting. `getconf PAGESIZE` = **16384**
  (read on the Pi in this slice); `uname -m` = **aarch64**.
- `free -g` at slice start: 15 GiB total, 13 GiB available — the Pi is lightly loaded;
  Samtal on port 8787 answered HTTP **200** before drafting.
- No allocation, mapping, or pressure run is claimed in this slice except the
  `make test` baseline recorded in Section 8 (real counts, if run) and the greps
  cited below. Every other number in this document is either a **derived** value
  (arithmetic shown, from a banked bound) or a **Proposed** definition, never a
  measurement.

## 1. The allocator this budget is for (as-built baseline, base 81c0474)

The budget design only makes sense against the real allocation paths, so they are
stated first, exactly as banked — no path is invented for the budget's convenience.

- **Two paths, one global lock.** All allocation state lives under a single global
  lock (B-006 S9 discipline). There is no per-thread cache and no thread-local
  state (G-005 RP-4). A budget check therefore has exactly one place to live:
  under that lock, at the decision point where a new mapping or span would be
  acquired — never in a caller, never after the fact.
- **Mapping path (requests > 1024 bytes).** One fresh anonymous mapping per
  allocation. Layout: `[ reserve page (one system page, holds the 96-byte
  descriptor) | usable region ]`. `mapping_length = page_round_up(page_size +
  requested)`; on this Pi that is `16384 + round_up(requested, 16384)`. Example
  (derived, not measured): `mimir_malloc(64)` on the mapping path would map
  32,768 B — but 64 B is in fact served by the small path; a mapping-path example
  is `mimir_malloc(2048)` → 32,768 B mapped, 16,384 B usable extent, 2,048 B
  requested. The mapping is released **whole** at the free of its single block
  (Phase 3 baseline); retained-after-free on this path is **0** by construction.
  A realloc grow always moves: the new mapping is published, then the old one is
  retired (E-005 S2) — so during a grow both mappings are live at once.
- **Small-object path (requests 1..1024 bytes).** 20 size classes
  (16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512,
  640, 768, 896, 1024), each span exactly **one system page** with a 96-byte span
  header at offset 0; a span serves exactly one class; capacity of a span =
  `(16384 − 96) / class_size` (e.g. 1018 objects at class 16, 15 at class 1024).
  A freed small object returns to its span's free list; a span whose `live_count`
  returns to 0 is **EMPTY** and is retained or released under G-005 RP-1..RP-5.
- **Retained spans (the only cache).** At most `mimir_retention_bound` EMPTY spans
  per class are retained (G-007: default **2**, valid range 0..16, internal
  setter/getter pair, Proposed-internal shape (b) — the proven pattern this design
  copies). Retained-after-free ceiling at default: 20 classes × 2 spans × 16,384 B
  = **655,360 B (640 KiB)** on this host; at bound 16: **5,242,880 B**. The empty
  span that would exceed its class bound is released whole, synchronously, inside
  the same locked free that emptied it (RP-3) — reclaim is never deferred, there
  is no background reaper.
- **Unload drain (H-001).** `mimir_core_drain_retained()` releases every retained
  EMPTY span whole through `span_release_locked`, under the global lock, at image
  unload (destructor) and on explicit call. It never touches a live span, a live
  small object, or a live mapping; it is non-terminal and idempotent. It is the
  only existing bulk-release operation, and I-003's on-demand trim is designed as
  its on-demand twin (Section 5), not as a second mechanism.

## 2. Existing counters — what the budget can already measure (and what it cannot)

The budget must be defined from counters that exist, not from counters wished for.
All counters below are **internal audit state**, INC-E001-07 shape (b): declared in
no installed header, readable only by a test-only static build, bound local in
`libmimiralloc.so`, `nm -D` still exactly 4 symbols. None of them is a byte counter
for requested bytes, and **no peak counter exists at this base** — that gap is
INC-I002-01 and shapes Section 3.

| Counter (existing) | Where | Counts, exactly |
|---|---|---|
| `mimir_audit_call_malloc` / `_calloc` / `_realloc` / `_free` | `src/api/mimir_alloc.c` (E-005 L1) | One per invocation of its entry point, whatever the outcome — success, defined failure, or defined no-op (`mimir_free(NULL)`). Call counts, **not** bytes. |
| `mimir_audit_alloc_published` | `src/core/mimir_core.c` (E-005) | One per mapping published LIVE at the single publication point in `alloc_locked` (covers malloc, calloc, realloc grow-by-move). |
| `mimir_audit_mapping_retired` | `src/core/mimir_core.c` (E-005) | One per mapping successfully released at the single release point in `retire_locked` (covers free and realloc's transfer of the old block). `published − retired` = number of live mapping-path mappings. |
| `mimir_audit_span_published` / `span_retired` | `src/core/mimir_core.c` (G-003) | Spans acquired / spans successfully released. `span_published − span_retired` = number of live spans (live + empty-retained). |
| `mimir_audit_small_alloc` / `small_free` | `src/core/mimir_core.c` (G-003) | Small objects handed out / retired to a span free list. `small_alloc − small_free` = number of live small objects. |
| `mimir_live_bytes` (internal `static size_t`) | `src/core/mimir_core.c` | Sum of `mapping_length` over **live mapping-path mappings only**. It does **not** include span bytes, requested bytes, or any libc allocation. |
| `mimir_live_span_bytes` / `mimir_live_spans` (internal `static`) | `src/core/mimir_core.c` | Sum of `mapping_length` (one page each) / count over **live spans** (live + empty-retained). |

What does **not** exist at this base: no requested-bytes total, no allocated-bytes
total, no cached/retained-bytes counter, no released-bytes total, no peak of any
quantity, no failure counter of any kind (budget or OS), no trim/reclaim counter.
Phase 10 names "requested, allocated, cached, mapped, released and peak bytes" as
quantities to expose — this design **uses Phase 10's names as the definition
target** (Section 4) but implements none of them; I-006/I-007 measure against the
definitions, and a later slice decides the Phase 10 exposure.

## 3. Proposed soft-budget design

### 3.1 What "soft" means here — and what it does not mean

A **soft budget** is an advisory, allocator-local threshold on the allocator's own
**mapped bytes** (Section 3.2, Q-MAPPED). At and over the budget, the allocator
first releases what it is already allowed to release (retained EMPTY spans) and
then, only if the new allocation still does not fit, **refuses to acquire new
backing** and fails that one allocation in the existing NULL way. It is soft in
four exact senses, and hard in none:

- It is **not an OS limit**: it changes no `RLIMIT_*`, no cgroup, no `mmap`
  behaviour of the process. The OS can still fail an allocation under the budget
  (genuine pressure), and an allocation can succeed above the budget if it is
  served without new backing (Section 3.3 step 1).
- It **never** unmaps, trims, or otherwise touches a live allocation or a span
  holding one (G-005 RP-1; H-001 posture; roadmap Invariant 1).
- It **never** blocks, retries, sleeps, or waits for memory: the at/over-budget path
  is bounded and single-pass (Section 3.3), honouring roadmap Section 9.3's
  "failures neither hang indefinitely…".
- It **never** prevents a free, a trim/drain, or a realloc failure from returning:
  frees and releases are never charged, never refused.
- It carries **no claim about Android low-memory kills** (roadmap Phase 9 exit):
  the kernel / lmkd may kill the process under pressure before, during, or after
  any allocation the budget refused. The budget governs this allocator's mapping
  decisions only.

### 3.2 Budget quantities — defined exactly, named from their counters

All quantities are in **bytes**, unsigned, and computed under the global lock.
"Existing-derivable" = computable today from counters/state named in Section 2,
without a code change, in a test-only static build. "Proposed-internal" = needs
the new counter/state named here; its definition is fixed by this design, its
implementation is owed to the slice that builds it (I-003 or a named later slice).

| Quantity | Symbol | Exact definition | Status |
|---|---|---|---|
| Live mapping bytes | Q-LIVEMAP | `mimir_live_bytes`: sum of `mapping_length` over live mapping-path mappings. Each term is `page_round_up(page_size + requested)`; the reserve page is included, because it is really mapped. | Existing-derivable |
| Live span bytes | Q-LIVESPAN | `mimir_live_span_bytes`: sum of `mapping_length` (= one page) over all spans on the span registry, live and empty-retained alike. | Existing-derivable |
| **Mapped bytes (the budgeted quantity)** | **Q-MAPPED** | `Q-LIVEMAP + Q-LIVESPAN`. Exactly the anonymous bytes this allocator instance currently holds mappings for. Nothing else is in it: no libc heap, no thread stacks, no file-backed pages, no descriptor bytes beyond the reserve page already inside `mapping_length`. | Existing-derivable (sum of the two above; no single counter holds it today) |
| Retained bytes | Q-RETAINED | Bytes of EMPTY spans: `(live_spans_with_live_count_0) × page_size`, counted by a registry walk under the lock (no counter holds the empty-span count today). Always `0 ≤ Q-RETAINED ≤ retention_bound × 20 × page_size` (640 KiB at default on this host). Mapping path contributes 0, always. | Proposed-internal (registry-walk helper) |
| Live requested bytes | Q-LIVEREQ | Sum of `requested` over all live blocks — for mapping-path blocks their descriptor `requested_size`; for small objects the original request size, which the span header does **not** retain today (it retains the class size). | **Proposed-internal — not derivable today** for small objects (see INC-I002-02); the mapping-path half is derivable by registry walk, the small-object half is not, so Q-LIVEREQ as a whole is Proposed |
| Released bytes (cumulative) | Q-RELEASED | Monotone cumulative sum of every successfully released `mapping_length`, both mapping-path retirements and span releases. Today only release **counts** exist (`mapping_retired`, `span_retired`); the byte total needs one new internal accumulator fed at the same two release points, or must be derived per-event (`count × length`, valid since span length is always one page and mapping lengths are per-descriptor). | Proposed-internal |
| Peak mapped bytes | Q-PEAKMAPPED | Maximum value of Q-MAPPED sampled at each publication point (after the mapping/span is added, under the lock). No high-water counter exists at this base. | Proposed-internal |
| Budget refusals | Q-REFUSED-BUDGET | Monotone count of allocation entry-point calls refused by the budget (Section 3.3 step 3). Deliberately separate from Q-REFUSED-OS. | Proposed-internal |
| OS failures | Q-FAILED-OS | Monotone count of allocation entry-point calls that returned NULL because backing acquisition (L6 `mimir_platform_map`) failed, an oversize/unrepresentable request was rejected, a calloc product overflowed, or lock acquisition failed — i.e. every NULL that is **not** a budget refusal and not the U-09/U-12 posture NULLs (those stay uncounted; see INC-I002-06). | Proposed-internal |

**The budget itself (Proposed):** one `size_t` internal scalar, `mimir_budget_mapped_bytes`,
with **0 meaning "no budget set"** (the default) — never zero bytes, because a real
0-byte budget would refuse every backed allocation, a meaning no caller can intend
silently; an operator who wants that refusal gets it from `Q = 1` and any non-zero
request (INC-I002-03 records this sentinel choice as owed-not-proven). The budget
compares against **Q-MAPPED only**. It deliberately does **not** compare against
requested bytes: requested bytes are not what pressure is made of — a 1-byte
request on the mapping path is impossible (small path), a 2,048-byte request maps
32,768 B, and Q-MAPPED is the quantity the OS and the process actually feel.

### 3.3 At / over budget — the decision procedure (Proposed)

Evaluated under the global lock, only at the point where backing would grow —
i.e. inside `alloc_locked` before mapping a new large block, and inside the small
path before acquiring a new span. Fork/join, free, and realloc-shrink never run it.

1. **Serve without new backing first.** If the request can be served from existing
   state — a free object in an existing span of its class, or (small path) an EMPTY
   retained span of its class, or (realloc) a shrink/same-size case that publishes
   nothing — it is served **regardless of the budget**. Reuse of retained spans is
   exactly what retention exists for, and retained bytes were already inside
   Q-MAPPED when they were mapped. (Realloc grow-by-move is **not** this case:
   it publishes a new mapping first, so it runs steps 2–3 with the new
   `mapping_length` — and, per Section 6, the old mapping is only ever retired
   after the new one succeeds.)
2. **Compute the projection.** `projected = Q-MAPPED + new_length`, where
   `new_length` is the exact `mapping_length` the acquisition would add (one page
   for a new span; the computed `mapping_length` for a large mapping). If no budget
   is set, or `projected ≤ budget`, acquire as today. Equality is admitted:
   "at budget" is allowed, "over budget" is what triggers step 3.
3. **Release retained spans, then re-judge once.** If `projected > budget`, release
   EMPTY retained spans whole, in the Section 5 order, through the same
   `span_release_locked` path as the H-001 drain, until `projected ≤ budget` or
   no EMPTY span remains. This is the budget spending its one reclaim tool first
   — reclaim before refusal, per roadmap Phase 9 ("trimming and cache release"
   beside "soft budgets"). The released bytes are added to Q-RELEASED and the
   spans are counted for I-006 (Section 6.2). Recompute `projected` once. If it
   now fits, acquire. The loop is bounded: each iteration releases a whole span,
   spans are finite, and no span is revisited.
4. **Fail cleanly.** If `projected` still exceeds the budget, acquire nothing,
   publish nothing, and return exactly the existing failure: `NULL` from
   `mimir_malloc` / `mimir_calloc`; `NULL` from `mimir_realloc` with the original
   block live, unchanged, caller-owned, and freeable exactly as before (header
   contract, by analogy to MA-6.1-REALLOC-02). No partial block, no smaller block,
   no pointer to unowned storage (Invariant: failed allocation never returns
   storage not allocated to the caller). Increment Q-REFUSED-BUDGET by one.
   `errno` is **not** promised (U-10 / V-02 stays open): a budget refusal is
   caller-indistinguishable from an OS failure — both are `NULL` — and only the
   internal counters tell them apart. That indistinguishability is a design
   consequence, recorded here so I-004 tests the right thing (Section 6.3).

What the procedure never does: never releases a live span or live mapping to make
room; never evicts another class's retained spans preferentially (the Section 5
order is class-fair); never defers the refusal to a later call (no debt, no
negative credit); never changes the retention bound itself (the budget and the
RP-2 bound are orthogonal knobs — the bound limits what **frees leave behind**,
the budget limits what **acquisitions may grow to** — and either can bind first;
see Section 3.5).

### 3.4 Scope — process-wide for the allocator instance, exactly

The budget's scope is **the one allocator instance in this process**: the single
global state (one global lock, one mapping registry, one span registry) that the
banked design has. It is therefore process-wide in effect: every thread, every
caller of the four entry points in this process draws on the same Q-MAPPED and the
same budget, and one thread's pressure is visible to every other thread's step-2
projection — that is the point of a soft budget under memory pressure, and it is
also a stated risk (a greedy consumer can cause refusals for others; see
INC-I002-04). Two scope edges are named, not glossed:

- **Per loaded image, not per abstract process.** The state is `static` globals in
  the image. If a process loads two copies of the library as distinct images, it
  has two instances and two budgets, and neither sees the other's Q-MAPPED
  (exactly as the H-001 drain's posture is per-image). No design element here
  coordinates budgets across images, across processes, or with any other
  allocator in the process (the process's libc allocations are outside scope by
  construction — see Section 4.2).
- **No per-thread, per-class, or per-caller budgets in this design.** One scalar,
  one quantity. Finer scopes are not Proposed here; adding them without evidence
  that I-005/I-006 need them would be unmeasured complexity (Possibility measured
  against evidence, roadmap Phase 11 discipline).

Setting the budget **below** current Q-MAPPED is legal and defined: nothing is
released at set time (setting is not a trim), existing live and retained bytes stay
mapped, and the budget binds only future acquisitions via Section 3.3 — which will
then run step 3 on the next backed acquisition until retained spans are exhausted.
An operator who wants immediate release pairs the set with the I-003 trim.

### 3.5 Interaction with the retention bound — either knob can bind first

Derived example (arithmetic from banked bounds, not a measurement): with the
default retention bound (2) and no budget set, steady-state Q-MAPPED is bounded
below by live demand and above only by live demand + 640 KiB retained. With a
budget of, say, 1 MiB and live demand of 900 KiB, a new 32,768 B mapping projects
932,768 B ≤ budget and is admitted; a new one after that projects above budget only
once live + retained actually cross 1 MiB — and step 3 can then recover at most the
retained part (≤ 640 KiB). If live bytes alone exceed the budget, every further
backed acquisition is refused after step 3 exhausts the (possibly zero) retained
spans, **while all frees still succeed** and the process drains normally as blocks
are freed. There is no deadlock state: the allocator never needs an acquisition to
complete a free.

### 3.6 Setting and querying — no silent ABI expansion (Proposed)

The public surface stays at **exactly 4 symbols**. This design proposes the G-007
pattern verbatim, because it already passed exactly this gate for the retention
bound (INC-G007-01: the proven tuning pair stays internal; promotion is a future
contract slice's business):

- **Proposed-internal** setter/getter pair, declared only in a test-only header
  under `tests/` and in the internal core header, defined with hidden visibility
  and bound local in the shared library by the existing version script — so
  `nm -D --defined-only` remains exactly the four public names and a consumer
  referencing the pair against the `.so` fails to link, exactly as the E-005
  counters behave today.
  - `int mimir_budget_set_mapped_bytes(size_t value)` — Proposed-internal. Stores
    `value` under the global lock; returns 0 on success. `0` unsets the budget
    (Section 3.2 sentinel). No release is performed by the setter (Section 3.4).
  - `size_t mimir_budget_get_mapped_bytes(void)` — Proposed-internal. Reads the
    scalar under the global lock; 0 = unset.
  - `size_t mimir_budget_mapped_now(void)` — Proposed-internal query of Q-MAPPED,
    computed under the global lock from `mimir_live_bytes + mimir_live_span_bytes`.
    It exists so a test (or I-007) reads the budgeted quantity as **one locked
    snapshot**, never as two unlocked reads that a concurrent free could tear
    (Invariant 8: statistics must not drive unsafe decisions if approximate or
    stale — a torn sum is worse than either).
- **Not proposed here:** any public symbol, any environment-variable interface,
  any per-process config file, any setter callable from a constructor. An
  environment variable (tempting for operations) would be a behaviour-changing
  input a consumer never opted into, and choosing one silently in a design slice
  would pre-decide what belongs to a contract slice — recorded as INC-I002-05.
- **The ABI decision is explicitly owed, not taken.** Whether any budget or
  telemetry surface is ever promoted to public is a **shape-(c) contract decision**
  (INC-E001-07 shape (c); cf. E-005 Section 1.3) owed to a later slice, with the
  re-proof obligations that promotion carries (header, contracts, version script,
  nm proof). This design slice records the question as OPEN and proposes nothing
  public. Zero-size (U-09), `realloc(ptr, 0)` (U-12), and errno (U-10) postures
  are unchanged by every proposal in this document.

## 4. Process telemetry definitions

### 4.1 Allocator-managed bytes — the exact definition table

Each row is defined so it can be **reconciled against the audit counters** of
Section 2 at a quiescent point (Section 5.3): an I-007 test must be able to assert
each identity exactly, not approximately. "Managed" means: bytes whose mapping
this allocator instance acquired and has not yet released.

| Name | Exact definition | Reconciliation identity (quiescent point) |
|---|---|---|
| Live (requested) | Q-LIVEREQ (Section 3.2): sum of original request sizes over live blocks | No identity exists today for small objects (INC-I002-02); mapping-path half = sum of descriptor `requested_size` over the mapping registry |
| Live (class-charged, small path) | Sum over live small objects of their span's class size — the space they actually occupy inside spans | `= (span bytes in live spans attributed by live_count × class_size)`; per span, `live_count × class_size`; derivable by registry walk under the lock, Proposed-internal |
| Cached / retained | Q-RETAINED (Section 3.2): EMPTY spans × page size. The only cache in the allocator (mapping path retains nothing; no thread-local cache exists) | `Q-RETAINED = Q-LIVESPAN − (live spans × page_size)` where "live spans" = registry spans with `live_count > 0`; also `Q-RETAINED ≤ 20 × retention_bound × page_size` |
| Mapped | Q-MAPPED = Q-LIVEMAP + Q-LIVESPAN | `Q-LIVEMAP = mimir_live_bytes` and `mimir_live_bytes` equals the sum the E-005 dynamic leg proved from `/proc/self/maps` for the mapping path (E-005 S3: baseline returned to exactly); count identity: live mappings `= published − retired`, live spans `= span_published − span_retired` |
| Live objects (count) | Live mapping-path blocks + live small objects | `= (published − retired) + (small_alloc − small_free)` |
| Released (cumulative) | Q-RELEASED (Section 3.2): cumulative bytes successfully released | Mapping-path term `= Σ mapping_length` over `mapping_retired` events; span term `= span_retired × page_size` exactly (spans are always one page) |
| Peak | Q-PEAKMAPPED (Section 3.2): high-water mark of Q-MAPPED, sampled at publication points under the lock | No reconciliation against any existing counter is possible — the counter does not exist (INC-I002-01); once implemented, `Q-PEAKMAPPED ≥ Q-MAPPED` at every snapshot, with equality iff the peak is now |
| Failures | Q-REFUSED-BUDGET and Q-FAILED-OS (Section 3.2): disjoint, monotone | `call_malloc + call_calloc + call_realloc` accounting: every entry-point call either publishes/retires, succeeds without backing, returns a posture NULL (U-09/U-12, uncounted by both), or increments exactly one of the two failure counters — this identity is Proposed and is I-004's to prove (Section 6.3) |

**Worked reconciliation (derived from banked values, not measured in this slice):**
E-005's recorded dynamic leg ended with total mapped bytes back at its baseline
exactly, with `published = retired = 10` and `live = 0` — in this table's terms:
Q-MAPPED-returned-to-baseline, Q-RELEASED (mapping term) = sum of the same 10
mappings' lengths, Q-RETAINED = 0 (no span was warmed). G-005's recorded end state
was 40 retained spans = 655,360 B — in this table's terms: Q-RETAINED at its
default ceiling, Q-LIVESPAN = 655,360 B, live spans = 0. Both banked results are
expressible without remainder in these definitions; that expressibility, not a new
run, is this slice's reconciliation evidence.

### 4.2 Process-wide quantities — what the allocator does NOT own

These quantities describe the **whole process** (or the whole machine). The
allocator neither controls nor may claim them. Q-MAPPED is expected to be a
**subset** of the anonymous part of them, and the difference is other actors'
memory: the C library's own heap and arenas, thread stacks, the dynamic loader,
the application itself, libraries loaded beside MimirAlloc. Telemetry that
reports any row of this table as "allocator memory" is wrong by definition.

| Process-wide quantity | Definition | Linux /proc source (per process, `self` or the target PID) |
|---|---|---|
| Resident set size (RSS) | Pages of the process currently resident in RAM, all mapping kinds summed | `/proc/self/status` `VmRSS`; also `/proc/self/statm` field 2 (pages); per-mapping detail in `/proc/self/smaps` field `Rss`, rolled up in `/proc/self/smaps_rollup` |
| Peak RSS | High-water mark of RSS since exec | `/proc/self/status` `VmHWM` |
| Virtual size (VSZ) | Total virtual address space reserved, mapped or not, resident or not | `/proc/self/status` `VmSize`; `/proc/self/statm` field 1 (pages) |
| Anonymous resident bytes | Resident pages with no file backing — the population Q-MAPPED lives in, together with libc heap, stacks, and every other anonymous user | `/proc/self/smaps` / `smaps_rollup` field `Anonymous`, and per-mapping `Rss` on entries whose pathname is empty or a heap/stack annotation |
| File-backed resident bytes | Resident pages backed by files: shared libraries' text, data files mapped by the application | `/proc/self/smaps` pathname-bearing entries and the rollup's file-backed portion (`Rss − Anonymous − AnonHugePages` style decomposition, never a single field) |
| Model weights residency | The file-backed (or page-cache-backed) pages of a model file (e.g. a GGUF) mapped by the application/framework — roadmap Phase 12's concern, **not** this allocator's: MimirAlloc has no model-file path and maps nothing file-backed at all | `/proc/self/maps` / `smaps` entries whose pathname is the model file; residency per such entry is its `Rss`; page-cache sharing makes several processes' `Rss` for the same file overlap (PSS exists precisely to apportion this) |
| Proportional set size (PSS) | RSS with shared pages divided among their sharers — the fair-share view of the two rows above | `/proc/self/smaps` / `smaps_rollup` field `Pss` |
| Swap used by the process | Resident-exchanged pages currently in swap | `/proc/self/status` `VmSwap`; per-mapping `Swap` / `SwapPss` in `smaps` |
| Device-driver / accelerator memory | Memory held by kernel drivers and accelerator runtimes (on this Pi: e.g. the Hailo device, GPU/video stacks): allocated in kernel or device space, **absent from the process's maps entirely** | **Not in `/proc/self/*`.** System-level views only: `/proc/meminfo` (totals, incl. driver-consumed kernel memory indirectly), device-specific nodes under `/sys` and `/proc` per driver. No per-process attribution exists on this route (INC-I002-07) |
| Machine free / available | Whole-machine headroom, all processes | `/proc/meminfo` `MemAvailable` / `MemFree` — context for a pressure run, never a process or allocator quantity |

**Known attribution limits (binding on I-005/I-007 readings):**

1. **Anonymous mappings are indistinguishable by origin in /proc.** A MimirAlloc
   mapping appears in `/proc/self/maps` as an anonymous private region like any
   other. Q-MAPPED can be attributed exactly only from the allocator's own
   counters/registry (Sections 2–3); /proc can corroborate the **total** (the
   E-005 dynamic-leg method: sum `Size` over the process's mappings before/after
   a scripted sequence and require the delta to equal the internal sum) but can
   never split the anonymous total by allocator. Any claim of the form "RSS minus
   X = MimirAlloc" is invalid.
2. **Mapping size ≠ residency.** Q-MAPPED counts mapped bytes whether or not any
   page was ever touched; a freshly mapped 32,768 B block may contribute ~0 to
   RSS until written. Conversely RSS can fall under kernel reclaim without any
   unmapping. I-005/I-007 must report mapped and resident quantities as separate
   series and never reconcile one against the other as if they were the same
   quantity with noise.
3. **Sampling races.** `/proc/self/*` is read while the process runs; a snapshot
   mid-allocation can catch a half-published state that the allocator's own
   locked snapshot (Section 3.6) would not. /proc evidence is therefore stated
   at quiescent points, or labelled as sampled-under-load with the workload
   named — the same quiescent-point rule as Section 5.3.
4. **Page size is host state, not a constant.** All per-page arithmetic in this
   document uses 16,384 B because that is this Pi's `getconf PAGESIZE`, read in
   this slice. On a 4 KiB-page host the same formulas hold with 4,096 substituted
   (roadmap: PAGE_SIZE_PORTABILITY); a design number that silently assumed
   either value would be a defect.
5. **Android is out of this table's evidence reach.** The /proc sources above are
   Linux-on-Pi evidence. Android's per-process accounting (and lmkd behaviour)
   is a Phase 4 target question; nothing here claims the same fields exist or
   mean the same thing there (INC-I002-08).

## 5. Trimming / cache-release semantics this design assumes for I-003

I-003 implements trimming and cache release. This design fixes the **semantics**
I-003 must implement against, so I-003 is an implementation slice, not a second
design round. Where I-003 finds a semantic here unimplementable, that is an
inconsistency against this document, to be recorded, not silently reinterpreted.

### 5.1 What trimming is (assumed semantics)

- **Trim = release EMPTY retained spans, whole, to L6** (`span_release_locked` →
  `mimir_platform_map_release`), exactly the H-001 drain's operation, invoked
  on demand instead of at unload. Proposed trigger surfaces are the same
  Proposed-internal shape as Section 3.6 (an internal `mimir_trim_retained()`-
  style entry point); any public surface is owed to the same later contract
  decision — I-003 records its surface decision in its own document, and this
  design binds it to **no silent ABI expansion** (the WAVE_PLAN I-003 line says
  the same).
- **Trim is the budget's step-3 tool and a standalone operation** with identical
  semantics in both call sites — there is one release path, not a budget-flavoured
  variant.
- **Trim returns a count**: number of spans released (and hence bytes =
  count × page_size, added to Q-RELEASED), so callers and tests can reconcile.
  A trim that releases nothing returns 0 and is a successful no-op (idempotent,
  exactly as the H-001 drain is).

### 5.2 Release order (assumed, deterministic)

Because all spans are one page, order cannot change **how many bytes** a full trim
releases; it matters for partial trims (budget step 3) and for repeatability:

1. **EMPTY spans only, ever.** A span with `live_count > 0` is skipped, never
   partially released, never waited on. Live allocations — mapping-path or
   small-object — are **never released by trim, drain, or budget, under any
   circumstances** (G-005 RP-1; roadmap Invariant 1). Also never released: the
   reserve page of a live mapping (it is part of that mapping, not a cache), and
   any span that is the class's current active span while it holds live objects
   (same rule, restated for the active-span pointer).
2. **Class-fair order:** release round-robin across classes that hold EMPTY spans
   (one span per class per pass), so a partial trim does not strip one hot class
   of its whole cache while another class's retained spans survive untouched.
   Within a class, release the span that became EMPTY **earliest** first (oldest
   coldest cache first); the most recently emptied span of a class is released
   last, as it is the likeliest to be reused.
3. **Stop exactly at need:** a budget-driven partial trim stops as soon as the
   Section 3.3 projection fits — releasing more than the projection requires
   would destroy reuse the budget did not ask to spend — while a caller-driven
   full trim releases all EMPTY spans. Both are the same walk with different
   stop conditions, recorded in I-003's evidence.
4. **Failure posture (F-06 inheritance):** if L6 release of a span fails, that
   span stays retained and LIVE in the registry (the banked rollback), the trim
   continues with the next span, and the returned count reflects only spans
   actually released. Trim never aborts midway into a half-described state.

### 5.3 Quiescent-point reconciliation rule (assumed for all telemetry)

A telemetry reading is **exact** only if it is a single locked snapshot
(Section 3.6 query) or is taken at a **quiescent point** — a moment when no
allocation operation is in flight in any thread of the process (e.g. all worker
threads joined, or the reading thread holds the only possible path to the
allocator). At a quiescent point the Section 4.1 identities must hold **exactly**;
a failed identity at a quiescent point is a defect, not measurement noise.
Outside quiescent points, individual counters are still each monotone and each
single read is valid, but sums across counters may tear (Invariant 8 governs:
statistics may inform, they must never drive an unsafe decision on torn data —
which is why the budget's step-2 projection is computed from locked state, not
from separately sampled counter values). I-007's telemetry-separation proof is
written against this rule.

## 6. What the later Phase 9 slices must measure / prove

### 6.1 Reserved

(Section numbering kept parallel to the WAVE_PLAN queue: I-003's semantics are
Section 5; I-004's contract is Section 6.3; I-006's quantities are Section 6.2.)

### 6.2 Quantities I-006 must measure (fragmentation + reclaim overhead)

I-006 (fragmentation and reclaim-overhead measurement) measures against these
definitions; a quantity I-006 cannot compute from then-existing counters is
reported as such by I-006, not approximated silently.

- **F-1 Internal fragmentation, small path:** `Σ_live small objects (class_size −
  requested_size)`, plus per-span slack `(span capacity × class_size − mapping_length
  + header)` expressed as the span tail: `mapping_length − 96 − capacity × class_size`
  (e.g. class 16: 16384 − 96 − 1018×16 = 0 B; class 1024: 16384 − 96 − 15×1024 = 928 B —
  derived arithmetic). First term needs Q-LIVEREQ's small-object half
  (INC-I002-02); until that exists, I-006 reports the span-tail term and the
  class-charged vs live-count terms separately.
- **F-2 Internal fragmentation, mapping path:** per live mapping,
  `mapping_length − requested_size` (reserve page + round-up tail); total and
  per-request distribution. Worst case per allocation is just under two pages
  of overhead on a page+1 request at 16 KiB pages (derived).
- **F-3 Free-but-unusable bytes:** bytes held in spans that cannot serve a new
  request of a **different** class: `(live spans' free slots × their class_size)
  + Q-RETAINED`. This is the fragmentation a pressure run actually feels: bytes
  mapped, not live, and not releasable without emptying their span.
- **F-4 Reclaim overhead per release:** wall time of a trim/drain that releases
  `k` spans, reported as total and per span, with the locked hold time distinguished
  where the harness can measure it; count of `mimir_platform_map_release` calls;
  and the follow-on cost — the next allocation of a trimmed class must acquire a
  fresh span, so I-006 also reports first-allocation-after-trim latency against
  the retained-hit latency (the reuse the cache exists to provide).
- **F-5 Budget-step-3 cost:** spans released per budget refusal event and per
  admitted-after-trim event; refusal rate under a stated pressure workload
  (I-005's), as counts (Q-REFUSED-BUDGET, admissions-after-trim), never as a
  bare percentage without its denominators.
- **F-6 Q-MAPPED trajectory:** Q-MAPPED (and Q-RETAINED within it) sampled across
  I-005's constrained runs, so fragmentation claims are tied to the mapped total
  that produced them.

### 6.3 The failure-handling contract I-004 must prove

I-004 (genuine allocation-failure handling) proves, by fault injection per
roadmap Section 9.3 ("artificially fail the Nth mapping"):

- **FC-1 NULL-only failure.** Every failure — OS backing failure, budget refusal
  (Section 3.3 step 4), oversize/unrepresentable request, calloc product
  overflow — surfaces as exactly `NULL` from malloc/calloc/realloc. No other
  value, no partial block, no smaller substitute block, no pointer into a span or
  mapping the caller does not own (Invariant: no overlapping storage, ever).
- **FC-2 Realloc failure-atomicity.** On any realloc failure the original block
  remains live, byte-identical in its preserved range, caller-owned, and freeable
  exactly once afterwards — including the grow-under-budget-refusal case, where
  the new mapping was never published and therefore nothing may be retired.
- **FC-3 No hang, bounded path.** The Nth-mapping failure and the budget path
  both complete in bounded time: no retry loop on backing failure, no waiting on
  memory, no recursive allocation inside any error path or counter update
  (statistics/error paths allocate nothing — cf. Phase 10 exit's "error reports…
  do not recursively allocate").
- **FC-4 Counter separation.** After an injected run, Q-FAILED-OS and
  Q-REFUSED-BUDGET account exactly for the NULLs of their kinds (Section 4.1
  failures identity), and the E-005 identities still hold: `published − retired`
  = live mappings, books balance to live 0 after all frees.
- **FC-5 Frees are unconditionally safe after failures.** `mimir_free(NULL)`
  remains a no-op; freeing every block allocated before, during, and after an
  injected failure sequence returns Q-MAPPED to its pre-run value at a quiescent
  point — failures leak nothing.
- **FC-6 Posture cases stay postures.** U-09/U-12 NULLs are not reclassified as
  failures or budget refusals by any counter; errno remains unpromised (U-10).

## 7. Inconsistencies and unknowns (INC-I002-NN)

- **INC-I002-01 — No peak counter exists.** Phase 10 names peak bytes; the base
  has no high-water state of any kind. Q-PEAKMAPPED is Proposed-internal
  (Section 3.2); until implemented, no peak statement about this allocator is
  evidence, and I-005/I-006 must not report one.
- **INC-I002-02 — Small-object requested sizes are not retained.** A span header
  keeps the class size, not each object's original request, so Q-LIVEREQ and
  F-1's first term are not derivable today. Options (per-object size storage vs
  accepting class-charged accounting as the definition) change either memory
  overhead or the meaning of "live requested" — owed to the slice that first
  needs the quantity (expected: I-006), not decided here.
- **INC-I002-03 — Budget sentinel choice is a design decision, not a proof.**
  0 = unset is Proposed (Section 3.2). The alternative (a separate set/unset flag,
  a real 0-byte budget) is implementable; the contract slice that promotes any
  surface must re-examine it, because a sentinel is caller-visible semantics.
- **INC-I002-04 — One process-wide budget can starve a well-behaved consumer.**
  A single scalar scope (Section 3.4) means one consumer's live bytes can cause
  another's refusals. Accepted for this design (matching the single-lock,
  single-registry architecture); per-consumer accounting is an evidence-owed
  future question, not a Proposed feature.
- **INC-I002-05 — No operational set-route is decided.** No environment variable,
  config file, or constructor-time read is Proposed (Section 3.6). Until a
  contract slice decides, the only route is the Proposed-internal setter, i.e.
  in practice a test or an integrating application that links the static
  objects. A dynamically-linked consumer cannot set the budget at all under
  this design — stated plainly so I-003/I-005 do not assume otherwise.
- **INC-I002-06 — Posture NULLs vs failure accounting.** U-09/U-12 posture NULLs
  are excluded from both failure counters (Sections 3.2, 6.3 FC-6) because the
  contract leaves those cases unresolved; if U-09/U-12 are ever settled to a
  defined failure, the accounting identity must be amended in the same slice.
- **INC-I002-07 — Driver/accelerator memory has no per-process attribution.**
  Section 4.2's last-but-one row: on this Pi, device memory is invisible in
  `/proc/self/*`. Telemetry separation (I-007) can therefore prove "managed vs
  process-wide user memory" only; a claim separating driver memory per process
  is not achievable on the named sources and must not be made.
- **INC-I002-08 — Android telemetry and kill behaviour are unexamined here.**
  The /proc definitions are Linux/Pi evidence (Section 4.2 limit 5); Android
  field evidence belongs to the Phase 4 line and I-009's exit packet must carry
  the roadmap's caveat verbatim in substance: no claim to prevent Android
  low-memory kills.
- **INC-I002-09 — Fork behaviour of budget and counters is open.** U-17..U-21
  (fork safety) remain open in the base; this design inherits that openness:
  whether a child process inherits the budget value and counter state exactly
  as memory-copy suggests is **not** stated here and must not be assumed by
  I-005's constrained runs (which should avoid fork-based designs or record
  the posture they used).
- **INC-I002-10 — I-001's inventory is not yet reconciled with this design.**
  I-001 is built in parallel from the same base; if its pressure-state inventory
  names a state or quantity differently from Sections 2–4, the reconciliation
  is owed at Wave 9 banking time by the keeper, and the later-banked document
  must carry the note. No conflict is known at writing time — none could be
  checked, which is the point of recording it.

## 8. Evidence and cross-references

Recorded on the Pi in this slice (real, bounded, and the only runs claimed):

- `git rev-parse HEAD` = `81c04746aa5a3ea497a22ba7f04d39f0629095ec`;
  branch `yrsa/mimiralloc-02`; pre-existing untracked `build/` and
  `hidden_files/` only (the assignment record lives in `hidden_files/`;
  neither is part of this slice's commit).
- `getconf PAGESIZE` = 16384; `uname -m` = aarch64; `free -g`: 13 GiB available.
- Samtal `http://127.0.0.1:8787/` answered HTTP 200 before drafting and after
  the recorded test run (final status in the keeper-facing run record).
- Source greps for every Section 2 counter and Section 1 structure named above
  (files and line anchors as cited in the run record): E-005 counters in
  `src/api/mimir_alloc.c` and `src/core/mimir_core.c`; G-003 span counters;
  `mimir_live_bytes`, `mimir_live_span_bytes`; retention bound default 2 /
  max 16 (`MIMIR_RETENTION_DEFAULT`, `MIMIR_RETENTION_MAX`); 20 classes,
  cutoff 1024; `mimir_core_drain_retained` in `src/core/mimir_core.c` /
  `mimir_core.h`. The public header declares exactly the four entry points
  (read in full in this slice).
- `make test` baseline: run once in this worktree at this base after drafting;
  the real result (exit code and the suite's own output) is recorded in the
  keeper-facing run record for this slice, **not** restated as a number here,
  because this document was finalised before that run — any number printed
  here would be a claim about a run the reader cannot order against the text.
  (If the run record shows a failure, it is a discovered defect: it is reported
  with this slice and commissioned, never silently absorbed — standing rule.)

Cross-references: roadmap Phase 9 + Sections 9.1/9.3/Phase 10;
`docs/PHASE_5_AUDIT_HOOK_E005.md`; `docs/PHASE_7_RECLAIM_POLICY_G005.md`
(RP-1..RP-5); `docs/PHASE_7_TUNING_PARAMETERS_G007.md` (TP-1, INC-G007-01);
`docs/PHASE_7_UNLOAD_DRAIN_H001.md`; `docs/PHASE_5_FOUNDATION_PLAN.md`
(INC-E001-07 shapes (a)/(b)/(c)); `include/mimir_alloc.h` (U-03, U-09, U-10,
U-12 postures); WAVE_PLAN.md Wave 9 queue (I-001..I-009).

— End of I-002 design. Implementation slices: I-003 (trimming/cache release),
I-004 (failure handling), I-005 (constrained runs), I-006 (fragmentation +
reclaim overhead), I-007 (telemetry separation proof). This document proposes;
it implements nothing and banks no Phase 9 exit claim.
