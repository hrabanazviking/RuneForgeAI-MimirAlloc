# Phase 7 — Size-Class Table Design (Slice G-002)

Slice: **G-002 (team-02)**. Wave 7, Phase 7 (Mobile-friendly
small objects). Deliverables: this design document, the
bounded probe `tests/core/probe_g002_sizes.py`, and the
recorded run `tests/core/recorded_run_g002.txt`. Base:
`0372d94` (Wave 6 COMPLETE) on branch `yrsa/mimiralloc-02`,
worktree `/home/pi/mimiralloc-team-02` on Raspberry Pi
`purpleviking1`. Governing text: Roadmap Phase 7 ("Add
measured size classes, bounded local reuse, central
fallback, reclaim policies and thread exit handling"),
Roadmap Sections 7.1 (layers), 7.3 (threading and slow
paths), 7.5 (large allocations), 9.1 (invariants); the
banked Phase 3 design `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md`
(B-006, with B-007 arithmetic and B-008 calloc/realloc);
`docs/ARCHITECTURE_LAYERS.md` (L3/L4/L5 responsibilities);
banked evidence D-003 (page-size portability) and D-008
(libc/alignment compatibility). `README.md` was not
touched; the public header and the 4-symbol ABI are
unchanged; the Makefile is unchanged (the probe is a
standalone script, run directly with `python3`).

> **VERDICT (plain first).** This slice designs — and does
> not implement — a 20-class size-class table for the
> small-object path: classes of 16..1024 bytes (every class
> a multiple of the 16-byte alignment floor measured by
> D-008), served from one-system-page spans carrying a
> 96-byte header (the B-006 S7.3 descriptor size, reused),
> with every request above 1024 bytes continuing to take
> the existing mapping-per-allocation path completely
> unchanged. The table is justified three ways, all from
> runs actually executed on the Pi in this slice
> (recorded run, probe Parts A–C): against the banked test
> suites' own request sizes the table's weighted mean
> internal fragmentation is 4.62 bytes per small request
> (Part A1 census); its worst case at any request size is
> bounded by the class-step arithmetic in Section 5; and a
> full span costs between 16.4 and 1365.3 mapped bytes per
> object where the current fallback maps 8192..32768 bytes
> for the same requests (Part C arithmetic). Implementation
> is slice G-003; nothing in this document is implemented,
> timed, or claimed as measured performance. The synthetic
> workload in probe Part B is an invented hypothesis,
> labelled SYNTHETIC everywhere it is used, and no design
> decision rests on it alone.

## 1. Scope and non-scope

In scope (this slice):

- The size-class table itself: class sizes, the band each
  class serves, and the small/large cutoff (Sections 4–6).
- The span form the table assumes (Section 7), stated at
  design level only, in the layer language of the banked
  architecture: L3 serves, L4 owns classes, L5 owns spans.
- How the class path and the existing mapping-per-allocation
  fallback divide requests and share the free path
  (Section 8).
- The evidence for every choice, with its label:
  MEASURED FROM SOURCES (probe Part A), SYNTHETIC
  (probe Part B), or ARITHMETIC (probe Part C and the
  formulas in Sections 5–7). Banked facts carried in from
  earlier slices are cited to their slice records.

Explicitly not in scope (later Phase 7 slices own these):

- Any allocator implementation. G-003 implements this
  design; this slice changes no file under `src/` or
  `include/` and no Makefile target.
- The bounded local reuse policy (L3 thread-local lists
  or caches), thread-exit handling, and reclaim policy
  tuning. Section 7 fixes only the span mechanics the
  table arithmetic depends on; how many spans a class may
  retain, and when an empty span is returned, are policy
  decisions for the reuse slice, bounded by Roadmap
  Section 7.3 (bounded caches, low-RAM posture).
- Latency, retained-memory, and responsiveness numbers
  for the Phase 7 exit. Those compare a built allocator
  against the Phase 3 baseline and the platform allocator;
  no built class path exists yet, so no such number is
  stated or implied anywhere in this document.
- Any change to the contract surface: no new public
  symbol, no alignment entry point, no usable-size
  query. The four public symbols and their banked
  postures (including U-09 zero-size and U-12
  realloc-to-zero, both still UNRESOLVED) are untouched.

## 2. Constraints a size class must satisfy

Each constraint is banked before this slice; the table in
Section 4 is designed against exactly this list.

- **C-1 Alignment floor 16.** Every returned pointer must
  satisfy the contract's non-numeric "suitably aligned"
  claim (public header, by analogy to MA-6.1-MALLOC-01;
  U-15/V-07 still open at contract level). The operational
  floor is 16 bytes: D-008 measured
  `_Alignof(max_align_t)` = 16 on this Pi (glibc 2.36,
  AArch64) and verified 33/33 malloc sizes, 5/5 calloc
  cases, and 4/4 realloc steps aligned to it (PROVEN on
  the Pi, `docs/LIBC_ALIGNMENT_COMPATIBILITY.md` Section
  2). Design consequence: every class size is a multiple
  of 16, and the object area of a span starts at a
  16-aligned offset, so every object in every span is
  16-aligned by construction (proof sketch, Section 9).
  A class of 8, or any non-multiple of 16 (e.g. 24), is
  therefore inadmissible as a uniform stride: alternate
  objects would land at 8-mod-16 offsets. This is why the
  smallest band (requests 17..32) cannot have a class at
  24 and carries the table's worst percentage waste
  (Section 5) — a structural cost, recorded, not hidden.
- **C-2 Page sizes are host facts, never constants.**
  Admitted system page sizes are 4096 and 16384 bytes,
  discovered at runtime via L6 on every call (D-003,
  PROVEN for the allocator; this Pi measures 16384 via
  `getconf PAGESIZE`, recorded run Section 0). Design
  consequence: the span is exactly one *system* page —
  whichever size the host reports — and every packing
  figure in this document is computed at both sizes.
  No class, offset, or span quantity may be tuned to
  16384 alone.
- **C-3 Descriptor cost discipline.** The banked
  per-allocation metadata is a 96-byte descriptor
  (B-006 S7.3 layout, `_Static_assert`-checked in
  `src/core/mimir_core.c`) carried in a whole reserve
  system page per allocation: the fallback's true
  per-allocation metadata cost today is 96 used bytes
  plus (page − 96) unused reserve bytes, inside a
  mapping of page + round_up(request, page) bytes
  (probe Part C fallback table). Design consequence: a
  span carries ONE 96-byte header per page of objects,
  amortised over the objects it holds (Section 5,
  header-per-object column: 0.09 bytes at class 16 on a
  16 KiB span, worst 6.40 bytes at class 1024). The span
  header reuses the 96-byte size and, per Section 7, the
  descriptor's layout discipline; it does not grow it.
- **C-4 The fallback stays correct and stays the
  fallback.** The Phase 3 core is a mapping-per-allocation
  correctness baseline with deliberately no reuse
  (B-006 S8.5). Phase 7 adds classes *in front of* it;
  it does not repair, replace, or re-tune it. Every
  behaviour the banked suites prove against the fallback
  (checked arithmetic B-007, calloc/realloc semantics
  B-008, free-path validation order in
  `src/core/mimir_core.c`) must survive the addition,
  and any request the class path declines must reach
  the fallback with ownership preserved or fail exactly
  as the fallback fails today (Invariant 10).
- **C-5 Layer law.** L3 (small-object fast path) serves
  admitted small requests and falls through to L4 on any
  miss; L4 owns size classes and free-span accounting;
  L5 owns spans and is L4's only downstream; no layer
  skips to L6 (`docs/ARCHITECTURE_LAYERS.md` Sections
  6–8). The initial honest form of L3 may be a
  pass-through (ARCH A9-6): this design therefore places
  class selection in L4 and requires nothing of L3 beyond
  forwarding, so G-003 can ship the table with L3 still
  pass-through and add caching later only on measurement.
- **C-6 One coarse lock, for now.** The banked core runs
  under the one L6-provided global lock (B-006 S9), and
  Roadmap Section 7.3 directs beginning coarse-locked,
  with finer structure only on benchmark evidence. The
  span design in Section 7 is therefore specified under
  the existing lock; no lock-free or per-class locking
  is designed, claimed, or needed by the table.
- **C-7 Failure is exactly NULL, as today.** A class-path
  failure (no span available and the backing mapping
  request fails) is the banked defined failure: NULL, no
  partially constructed block, nothing published
  (MA-6.1-MALLOC-02 / MA-6.2-020 / MA-6.2-024 by
  analogy). A span allocation that cannot complete must
  roll back to no-span-published, exactly as
  `alloc_locked` rolls back a failed mapping today.
- **C-8 Free-list link storage.** A free object must be
  able to hold one pointer (8 bytes on every admitted
  64-bit target) for the free list threaded through free
  objects (Section 7). The smallest class, 16, satisfies
  this with room to spare; no out-of-line link storage
  is needed and none is designed.

## 3. Evidence: the G-002 probe

Method (exact, reproducible; recorded run Sections 0–1).
`tests/core/probe_g002_sizes.py` is a read-only census
plus arithmetic: it allocates nothing through MimirAlloc
and modifies nothing. It ran on the Pi in this slice and
exited 0. Its three parts:

- **Part A1 — literal call-site census (MEASURED FROM
  SOURCES).** Regular-expression count, over 35 source
  files (`tests/**/*.c`, `tests/**/*.mojo`, `src/**/*.c`),
  of integer-literal request sizes at `mimir_malloc`
  sites, `mimir_calloc` literal products, and
  `mimir_realloc` literal new sizes. Result: **274
  literal occurrences**, of which 228 are small
  (1..1024), 18 are large (>1024), and 28 are zero-size
  (the deliberate U-09 posture probes). A further **161
  call sites take non-literal (dynamic) sizes** and are
  counted as dynamic — their sizes are computed at run
  time (many are page-derived: page−1 / page / page+1
  families) and are *not* guessed anywhere in this
  design (see INC-G002-02). The most frequent literal
  sizes are 64 (54 sites), 128 (31), 32 (19), 96 (15),
  256 (14), 512 (12), 48 (9), and 1024 (5); the most
  frequent large literal is 4096 (8).
- **Part A2 — size-ladder census (MEASURED FROM
  SOURCES).** Elements of array initialisers whose
  declarator name contains "size" (the exercised
  boundary ladders): **25 elements**, 15 small and 10
  large. This sub-census follows its stated pattern
  exactly; ladders written as expressions (page−1 and
  kin) are dynamic by construction and outside it
  (INC-G002-02).
- **Part B — SYNTHETIC mix (SYNTHETIC, invented).** A
  stated 10,000-request small-object-heavy mix (900
  requests of 8 bytes, 1400 of 16, 1100 of 24, 1200 of
  32, tapering to 80 of 1024, with 70 requests above the
  cutoff), invented by this slice as a workload
  *hypothesis* because Part A is a correctness
  distribution — test suites deliberately probe edges,
  zeros, and page boundaries, which no real workload
  resembles. Part B is used only to check that the table
  does not disgrace itself on a plausible small-object
  mix; every figure drawn from it is labelled SYNTHETIC.
- **Part C — table arithmetic (ARITHMETIC).** Per-class
  worst-case fragmentation, span packing at 4096 and
  16384, amortised header cost, and the fallback/span
  mapped-bytes contrast. Formula-level only: no timing,
  no allocator run, no performance claim.

Census honesty limits are recorded as INC-G002-01 and
INC-G002-02 and summarised in Section 10. Headline
results used below:

| Distribution | Small obs. | Weighted mean internal frag. |
|---|---|---|
| A1 literal call sites | 228 of 274 | **4.62 bytes/request** |
| A2 ladder elements | 15 of 25 | 6.93 bytes/request |
| A1+A2 combined | 243 of 299 | 4.76 bytes/request |
| B (SYNTHETIC) | 9930 of 10000 | 1.61 bytes/request (SYNTHETIC) |

The Part B mean is the rosiest of the four and is *not*
the headline: several stated Part B sizes sit exactly on
class sizes (zero fragmentation by construction of the
hypothesis). The headline is A1's 4.62, earned against
sizes the project itself actually requests, including
deliberately awkward ones (13, 21, 37, 100, 333, 407,
777, 1000 all appear in the census and all round up
inside the bounds of Section 5).

## 4. The proposed table

Twenty classes. A request of `r` bytes with
`1 <= r <= 1024` is served by the smallest class
`>= r`; `r > 1024` takes the large path (Section 6);
`r == 0` is not a class-path request at all — it keeps
the banked U-09 posture (NULL, unresolved) before any
class logic runs, exactly as L2 admission treats it
today. Band counts below are Part A1 census occurrences
falling in each band (they sum to the 228 small A1
observations).

| Class | Band served | Step | A1 in band | Worst frag in band | Worst frag % |
|---|---|---|---|---|---|
| 16 | 1–16 | — | 13 | 15 B (at r=1) | 93.8% |
| 32 | 17–32 | 16 | 23 | 15 B (at r=17) | 46.9% |
| 48 | 33–48 | 16 | 19 | 15 B | 31.2% |
| 64 | 49–64 | 16 | 58 | 15 B | 23.4% |
| 80 | 65–80 | 16 | 5 | 15 B | 18.8% |
| 96 | 81–96 | 16 | 15 | 15 B | 15.6% |
| 112 | 97–112 | 16 | 5 | 15 B | 13.4% |
| 128 | 113–128 | 16 | 31 | 15 B | 11.7% |
| 160 | 129–160 | 32 | 1 | 31 B | 19.4% |
| 192 | 161–192 | 32 | 6 | 31 B | 16.1% |
| 224 | 193–224 | 32 | 6 | 31 B | 13.8% |
| 256 | 225–256 | 32 | 14 | 31 B | 12.1% |
| 320 | 257–320 | 64 | 4 | 63 B | 19.7% |
| 384 | 321–384 | 64 | 1 | 63 B | 16.4% |
| 448 | 385–448 | 64 | 2 | 63 B | 14.1% |
| 512 | 449–512 | 64 | 12 | 63 B | 12.3% |
| 640 | 513–640 | 128 | 2 | 127 B | 19.8% |
| 768 | 641–768 | 128 | 0 | 127 B | 16.5% |
| 896 | 769–896 | 128 | 2 | 127 B | 14.2% |
| 1024 | 897–1024 | 128 | 9 | 127 B | 12.4% |
| LARGE | >1024 | — | 18 A1 obs. | fallback rules | — |

Step structure: 16-byte steps to 128, 32-byte steps to
256, 64-byte steps to 512, 128-byte steps to 1024.
Each step doubling keeps the worst-case percentage
bounded at ≈20% above the smallest band while holding
the class count — and with it the per-class span-list
state L4 must carry — to 20.

## 5. Per-class justification and fragmentation

**Bands 16–128 (step 16) — where the census lives.**
169 of the 228 small A1 observations (74%) request
128 bytes or fewer, and the single most requested size
in the whole census is 64 (54 sites), followed by 128
(31) and 32 (19). At step 16 the absolute waste in this
range can never exceed 15 bytes per request, which is
also the alignment quantum: no finer table is possible
under C-1, so this range is *provably* as tight as the
alignment floor allows. The ugly percentages at the
bottom (93.8% at r=1, 46.9% at r=17) are percentages of
tiny numbers: the waste is at most 15 bytes, once, per
allocation, against a fallback that maps at least 8192
bytes for the same request (Part C). The r=17 case is
the C-1 structural cost — a 24-byte class would
misalign alternate objects — and it is bounded at
15 wasted bytes, forever, by the step.

**Bands 160–256 (step 32).** 27 A1 observations. Worst
waste 31 bytes; worst percentage 19.4% (r=129 → 160).
A step of 16 here would add four more classes (144,
176, 208, 240) to save at most 16 bytes on requests
the census shows are uncommon, at the price of four
more per-class span lists for L4 to keep warm —
retained-memory pressure Phase 7 is explicitly told
to avoid. The SYNTHETIC mix agrees in shape (400
requests at 160, 300 at 192, 400 at 256 — all exact
class hits), but the decision rests on A1 plus the
waste bound, not on Part B.

**Bands 320–512 (step 64).** 19 A1 observations,
including the census's awkward middle (300 → 320,
waste 20; 333 → 384, waste 51; 400/407 → 448). Worst
waste 63 bytes, worst percentage 19.7% (r=257 → 320).
512 itself is the census's sixth most frequent small
size (12 sites) and a Part B point (200 requests,
SYNTHETIC); it earns its class on A1 alone.

**Bands 640–1024 (step 128).** 13 A1 observations
(640 ×2, 777 ×2 → 896 with 119 bytes waste = 13.3%,
1000 ×4 → 1024, 1024 ×5). Worst waste 127 bytes,
worst percentage 19.8% (r=513 → 640). These classes
exist because the alternative for 513..1024 is the
fallback: at this Pi's page size a 1000-byte request
maps 32768 bytes today; in a (hypothetical, G-003)
class-1024 span it costs 1092.3 mapped bytes per
object at 16 KiB pages, 1365.3 at 4 KiB pages
(ARITHMETIC, Part C). Class 768's band is empty in A1
— it is retained because removing it would push
641..768 requests into 896 (worst waste 255 bytes,
28.5%) to save one span list; the table's uniformity
(step doubling, no holes) is worth more than the one
list, and the SYNTHETIC tail (100 requests at 768)
is consistent with the band being real but rare.

**Span packing (ARITHMETIC, Part C).** Span = one
system page, 96-byte header, objects at class stride
from offset 96 (16-aligned, per C-1/C-3):

| Class | Objs/span @4K | Tail @4K | Objs/span @16K | Tail @16K | Header/obj @16K |
|---|---|---|---|---|---|
| 16 | 250 | 0 | 1018 | 0 | 0.09 B |
| 32 | 125 | 0 | 509 | 0 | 0.19 B |
| 128 | 31 | 32 | 127 | 32 | 0.76 B |
| 512 | 7 | 416 | 31 | 416 | 3.10 B |
| 1024 | 3 | 928 | 15 | 928 | 6.40 B |

(Full 20-class table: recorded run Section 1, Part C.)
Tail waste is the span's unusable remainder; it is
paid once per span, not once per object, and it is
the honest worst figure in this design: class 1024
strands 928 bytes in every span at both page sizes
(22.7% of a 4 KiB span). Section 6 explains why the
cutoff sits there anyway; INC-G002-06 records the
two-page-span alternative for large classes as an
open evaluation for G-003, not a silent omission.

**Rejected table shapes (ARITHMETIC).**

- *Powers of two* (16, 32, 64, …, 1024; 7 classes):
  worst-case waste 49.6% at r=129 → 256 and 127 wasted
  bytes at r=129 — double the chosen table's worst
  percentage through the census's densest range, to
  save span lists the census says would be busy.
  Rejected on A1 density.
- *Step 16 throughout* (64 classes to 1024): caps
  waste at 15 bytes everywhere, but quadruples L4's
  per-class span state and strands more spans
  near-empty under any realistic reuse policy —
  exactly the idle-thread bloat Phase 7 forbids.
  Rejected on retained-memory discipline; the chosen
  table's extra waste above 128 is at most 112 bytes
  per request beyond the step-16 ideal (at r=897:
  127 vs 15), on 13 of 228 A1 observations.
- *Cutoff at 512:* would send 513..1024 (13 A1
  observations, incl. 1000 ×4 and 1024 ×5) to a
  fallback costing 8–32× the request in mapped bytes,
  to avoid the table's two worst tail figures.
  Rejected; Section 6 gives the full cutoff argument.

## 6. The small/large cutoff: 1024 bytes

The cutoff is `1024 = 4096 / 4`, one quarter of the
*smallest* admitted system page. Three legs, in order
of strength:

1. **Span viability at the smallest page (ARITHMETIC).**
   A class earns its span only if the smallest admitted
   span holds at least 3 objects — below 3, header +
   tail dominate and the span is a mapping with extra
   steps. Class 1024 holds exactly 3 at 4096 (tail 928).
   The next step-128 class, 1152, would hold 3 with
   tail 544 but breaks the step-doubling table; the
   next table-consistent class, 1280, holds 3 at 4 KiB
   (tail 160) but its band's worst waste rises to 255
   bytes (19.9% at r=1025) while its fallback
   alternative only maps 8192 bytes at 4 KiB pages —
   a 6.4× ratio, against 32× at this Pi's page size
   for a 1024 request. The gain from classing above
   1024 shrinks exactly where the waste bound grows.
2. **The census tail is page-scale, not class-scale
   (MEASURED FROM SOURCES).** Of the 18 large A1
   observations, 13 are >= 4096 (4096 ×8, 5000 ×2,
   8192 ×3, 20000, 70000 ×2 — the page-and-above
   correctness probes) and the remaining handful
   (2000, 2048) sit in realloc/boundary tests whose
   subject is the mapping path itself. Nothing in the
   banked suites asks for a 1025..4096 *small-object*
   regime; designing classes for it would be inventing
   a workload, which this campaign does not do.
3. **Fallback cost ratio collapses with size
   (ARITHMETIC).** Mapping overhead per request is
   (P + round_up(r, P)) / r: at r=1024 it is 8× (4 KiB
   pages) to 32× (16 KiB pages); at r=4096 it is 2× to
   8×; at r=16384, 1.06× to 2×. Above the cutoff the
   fallback is merely wasteful; below it, it is the
   dominant cost of the whole allocator. The cutoff
   sits where the class path's advantage is
   overwhelming and stops where it becomes marginal.

Requests above the cutoff are not second-class
citizens: they take the banked path with its banked
semantics, unchanged, including B-008 realloc
grow-by-move into and out of the large regime
(Section 8).

## 7. Span form (design level, for G-003)

Only the mechanics the table arithmetic assumes are
fixed here; policy is deferred per Section 1.

- A **span** is one OS mapping of exactly one system
  page, obtained from L5 via L6 exactly as mappings are
  obtained today (runtime page-size discovery, checked
  round-up, all-or-nothing acquire; C-2, C-7).
- Layout: `[ 96-byte span header | objects at class
  stride ]`. The header occupies offset 0..95; the
  first object is at offset 96. The header follows the
  B-006 S7.3 descriptor discipline (magic, mapping
  base/length, state LIVE/RETIRED, registry links) and
  adds — inside the existing 96 bytes, using `flags`
  and `reserved`, never growing the header — the class
  index and the live-object count. Assigning those
  fields is G-003's work, constrained by INC-G002-04.
- All objects in a span share the span's class. A span
  serves exactly one class for its whole lifetime;
  class change happens only by retiring the empty span
  and acquiring a fresh one (no in-place reclassing —
  the ownership bookkeeping of Invariant 1 stays
  trivially auditable).
- **Free objects** thread a singly-linked free list
  through their own first 8 bytes (C-8). A span is
  *full* when its free list is empty and all carved
  objects are live; carving is sequential from the
  header end until the tail (Section 5) is reached.
  The tail is never carved, lent to a neighbour class,
  or split (no splitting/coalescing in this design —
  the same deliberate absence as B-006 S8.5, one level
  down).
- **Ownership bookkeeping** stays L4's: spans sit on
  per-class span lists in the same registry discipline
  as today's mapping registry (insert at publication,
  remove at retirement, counters updated in the same
  locked section; Invariants 1, 2, 6). Whether the
  E-005 audit counters count a span publication as a
  mapping publication is a semantics question this
  design deliberately does not fudge — INC-G002-09.
- **Empty spans** (live-object count returns to 0) are
  the reclaim unit. This design fixes only that an
  empty span *can* be released whole via L5→L6 with its
  exact acquired pair, like any mapping today. How many
  empty spans a class may cache before release is the
  deferred reuse policy (Section 1), bounded by the
  Roadmap 7.3 low-RAM posture.

## 8. Interaction with the mapping-per-allocation fallback

The class path is a front end, not a fork. Routing and
shared paths, stated as rules G-003 must implement
verbatim unless it records a superseding design:

- **R-1 Allocation routing (L2 → L4).** After L2
  admission (checked arithmetic, zero-size posture —
  both unchanged), L4 routes: `1..1024` → class path;
  `>1024` → the existing `alloc_locked` mapping path,
  byte-for-byte the banked behaviour. No request is
  ever served by both, and a class-path backing failure
  does *not* silently re-route a small request to a
  private mapping: it returns the defined NULL failure
  (C-7). (Re-routing would publish a mapping where the
  free path expects a span object and would need the
  dual-registry free of R-4 anyway; the simple,
  auditable rule is fail, exactly as a failed mapping
  fails today. A future slice may revisit this with
  measurements; this design does not.)
- **R-2 calloc.** A calloc whose checked product is
  `1..1024` is class-served. Zeroing follows B-008
  S3.3's rule carried to the class form: the *class
  extent* (not merely the requested extent) is zeroed
  before publication, because the object's slack bytes
  are the next free-list link's home and must never
  carry a previous owner's data into a later state.
  Products above the cutoff take the banked mapping
  calloc unchanged. The wording reconciliation this
  needs against B-008's mapping-shaped text is
  INC-G002-08.
- **R-3 realloc.** Same-path cases stay in-path: a
  small block realloc'd within its own class band
  returns the same pointer (the B-008 same-size rule
  generalised to same-class, recorded here as a design
  decision for G-003 to implement or explicitly
  supersede); a small block realloc'd to another small
  size, or across the cutoff in either direction, or a
  large block realloc'd at all, follows the banked
  B-008 sequence — construct the new block completely
  (through whichever path the *new* size routes to),
  copy min(old, new), retire the old exactly once,
  original preserved on any failure. Shrink across a
  class boundary by *staying* (retaining the larger
  class, updating only the requested-size record, the
  B-008 shrink posture carried down) is the default;
  migrate-on-shrink is rejected for now and recorded
  as INC-G002-07.
- **R-4 Free path (the delicate one).** Today's free
  validates by registry walk over mapping descriptors
  and never dereferences memory derived from the
  unvalidated pointer (the C-003 order, preserved in
  `mimir_core_free`). With spans there are two
  registries to consult — mapping descriptors and span
  headers — and a span object's user pointer is *not*
  at mapping_base + page, so the B-006 S7.1 derivation
  (descriptor = pointer − usable_offset) does not
  apply to it. G-003 must therefore determine ownership
  by registry membership over *both* lists (span
  membership test: pointer lies inside a LIVE span's
  object area at its class stride — computed from the
  trusted span header's own fields, never from bytes
  at the candidate pointer), validate the span header's
  magic/state exactly as descriptors are validated
  today, and only then touch the object's free-list
  link. Foreign, interior, and double frees remain UB
  with the same best-effort debug detection limits as
  B-006 S8.3; an interior pointer that lands exactly on
  a span's header is the sharpest new case and is
  recorded as INC-G002-05. No behaviour is promised for
  any invalid free.
- **R-5 Counters and observability.** Live-mapping and
  live-byte counters keep their banked meanings for
  the mapping path. Span-served objects are not
  mappings; G-003 must add span/object counters beside
  them rather than folding objects into mapping counts
  (Invariant 8: statistics describe what they name).
  Exact counter set is Phase 10's to ratify;
  INC-G002-09 carries the E-005 interaction.
- **R-6 Startup and lock.** Nothing in the class path
  allocates at startup, adds a constructor, or takes a
  second lock (C-6; B-006 F-08 clean-by-construction
  posture preserved: per-class span lists are static
  storage in the empty state, exactly as the registry
  anchors are today).

## 9. Alignment proof sketch (design level)

Claim: every pointer the class path returns is aligned
to 16 (C-1). (1) A span base is system-page aligned by
the OS mapping (banked L6 behaviour). (2) The first
object sits at span_base + 96, and 96 is a multiple of
16. (3) Every later object sits at span_base + 96 +
k × class, and every class is a multiple of 16
(Section 4). Hence every object offset is a multiple
of 16 from a page-aligned base. ∎ (Design-level: it
rests on the banked mapping behaviour, not on a new
measurement; G-003's tests must verify it by execution,
as D-008 verified the fallback.)

Explicitly *not* preserved: the fallback's accidental
page alignment of every user pointer (D-008 Section 2:
a construction accident of mapping-per-allocation,
already flagged there as INC-D008-08). Class-path
pointers are 16-aligned, full stop; any caller or test
relying on a page modulus was relying on an accident
the banked record already disowns.

## 10. Inconsistencies and open items (INC-G002)

- **INC-G002-01 (census method limit).** Part A counts
  literal call-*shaped* occurrences in raw source text,
  including any inside comments; no comment stripping
  or parsing is performed. Counts are therefore exact
  for the stated method and are an upper bound on true
  executable call sites. The design uses A1 for density
  shape and band populations, never for exact runtime
  frequencies, so the limit does not reach any decision;
  recorded so no later slice cites A1 as a runtime
  profile.
- **INC-G002-02 (the runtime distribution is still
  unmeasured).** 161 dynamic call sites (88 malloc, 42
  calloc, 31 realloc) and all expression ladders
  (page−1 / page / page+1 families) carry sizes this
  probe deliberately does not guess. A runtime size
  histogram — allocator instrumentation under the
  banked suites — is OWED (natural homes: the G-003
  test plan, or Phase 10 counters) before any future
  table *revision* claims workload evidence stronger
  than this slice's.
- **INC-G002-03 (synthetic mix is invented).** Part B
  is a hypothesis with stated numbers, used only as a
  sanity stress. The real small-object workload for
  this allocator's stated purpose is the Aesir trace
  family, which Roadmap Phase 12 owns. If Phase 12
  evidence contradicts the table's shape, the table —
  not the evidence — is what changes, via a superseding
  design slice.
- **INC-G002-04 (span header field assignment OWED).**
  Section 7 requires class index + live-object count
  inside the existing 96-byte header (`flags` /
  `reserved`), but does not assign the exact fields.
  G-003 must assign them without growing the header,
  without moving the S7.3 offsets the mapping path's
  validation relies on, and must static-assert the
  result as the core asserts today's layout.
- **INC-G002-05 (free-path discrimination hazard).**
  R-4's two-registry walk is specified, but the
  interior-pointer-onto-span-header case and the
  stride-membership test's exact arithmetic are
  G-003's to specify and adversarially test (the
  C-008 controlled-failure suite is the model). Until
  then the class path's invalid-free detection is a
  design obligation, not a proven property.
- **INC-G002-06 (class-1024 tail waste, alternative
  OWED).** 928 stranded bytes per 1024-span at both
  page sizes (22.7% of a 4 KiB span) is accepted here
  against a fallback costing 8–32× per request. A
  two-page span for the largest classes would cut the
  4 KiB tail to 928 of 8192 (11.3%) at the price of a
  second span size and coarser reclaim. Unmeasured and
  unevaluated; G-003 may evaluate it with evidence,
  but may not adopt it silently — it changes Section
  7's one-page span rule.
- **INC-G002-07 (shrink-across-classes posture).**
  R-3 keeps a shrunk block in its original class
  (slack retained, mapping-path B-008 posture carried
  down). The retained-slack cost under churn is
  unmeasured; if G-003's or a later slice's evidence
  shows it dominating, migrate-on-shrink returns as a
  design question with that evidence attached.
- **INC-G002-08 (B-008 zeroing wording is
  mapping-shaped).** B-008 S3.3 zeroes the *usable
  extent* of a mapping; R-2 carries the rule to class
  extents. The contract-analogy wording (contracts are
  by analogy only, U-03) should be reconciled by the
  next B-008-touching slice so "extent" names both
  forms; this slice records the gap rather than
  editing a banked design outside its scope.
- **INC-G002-09 (audit counters count mappings).**
  The E-005 audit counters increment at mapping
  publication/retirement. Span-served allocations
  publish no mapping, so Phase 7's retained-memory
  comparisons and any E-005-style proof rerun against
  a class-path build will undercount unless R-5's
  span/object counters exist and the proofs are
  taught to read them. OWED to G-003 (counters) and
  to whichever slice first re-runs E-005 proofs on a
  Phase 7 build.
- **INC-G002-10 (alignment floor rests on one libc).**
  The 16-byte floor is PROVEN for glibc/AArch64
  (D-008) and expected by-analogy for musl and bionic;
  D-008's musl leg is BLOCKED and its bionic leg OWED.
  The table is safe under any floor <= 16 and would
  need redesign only if a target's fundamental
  alignment exceeded 16 — no admitted target is known
  to, and the claim is recorded so the dependency is
  visible rather than assumed away. U-15 remains open
  at contract level; this design adopts 16 as an
  engineering floor per the D-008 record, it does not
  close U-15.
- **INC-G002-11 (G-001 reconciliation at banking).**
  G-001 (Phase 7 foundation plan) banks before this
  slice. This design was written against the banked
  tree at 0372d94 and the roadmap only; if G-001's
  plan fixes Phase 7 slice boundaries or terminology
  that differ from Sections 1/7 here, the keeper
  reconciles at banking and the difference is recorded
  in the bank commit — this slice does not guess
  G-001's contents.

## 11. What G-003 receives from this design

1. The 20-class table and 1024 cutoff (Section 4, 6),
   with the probe (`tests/core/probe_g002_sizes.py`)
   as its reproducible evidence base.
2. The span form and its invariants (Section 7), the
   routing rules R-1..R-6 (Section 8), and the
   alignment obligation (Section 9) as testable
   requirements, in the banked suites' idiom.
3. The open obligations INC-G002-04, -05, -06, -09 as
   named work, not surprises.

*End of G-002 design. Design only; Proposed. Nothing in
this document is implemented or measured as allocator
behaviour; every measurement cited was executed and
logged on the Pi in the recorded run named above or in
the banked slice records it cites.*
