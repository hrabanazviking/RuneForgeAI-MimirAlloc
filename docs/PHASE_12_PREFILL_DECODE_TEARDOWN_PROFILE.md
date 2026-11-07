# Phase 12 - Prompt Prefill / Decode / Teardown Allocation Profile (Slice L-006)

Status: **MEASURED (explicit-adoption harness first look,
L-002 S5.2 sense).** This slice extends the L-004 tracking
harness to the three remaining workload phases of the L-002
S2.3 table - prefill, decode and teardown - with quiescent
snapshots before, during and after each phase and the class
separation (W / C1 / C2 / G / R) reconciled at every
snapshot. Every figure below is read back from
`tests/perf/recorded_run_l006.txt` on this Pi. The harness
is an EXPLICIT-ADOPTION harness and is labelled exactly
that: every C1 byte is a `mimir_*` call made by the harness
itself. It is NOT Aesir prefill or decode - no Aesir
inference run exists in this slice, and what cannot run is
recorded OWED in Section 6 with its exact blocker, never
simulated or claimed.
Base: `27fa6129` (Wave 12, L-004 banked; L-005 banking in
parallel, files disjoint). Team-06 worktree, branch
`yrsa/mimiralloc-06`. Files: `tests/perf/probe_l006.c`
(harness, 512 lines / 22,213 B, sha256 `9b399cb1`),
`tests/perf/run_l006.sh` (driver, 57 lines / 2,450 B,
mode 100755, sha256 `ac3296ec`), `tests/perf/
recorded_run_l006.txt` (recorded run, 300 lines /
16,650 B, sha256 `278d343b`), this doc. No Makefile,
README or public-header change (README and
`include/mimir_alloc.h` verified byte-identical to base;
`nm -D` on the built library shows exactly the 4 public
symbols).

## 1. What was built

`probe_l006` is a single-threaded C harness that runs one
defined workload (Section 2) and, at each quiescent point
(no allocation in flight), takes a snapshot with the same
three independent accounts as L-004, kept separate exactly
as the banked Phase 9/10 method requires:

- **/proc account.** Its own `/proc/self/smaps` read raw
  into a static 2 MiB buffer (the reader allocates nothing
  on the measured heap, L-002 S5.3), classified mapping by
  mapping by the L-002 S2.2 decision procedure: pathname
  equal to the model path -> **W**; empty or bracket-label
  pathname -> the **ANON** bucket; any other file pathname
  -> **R (file)**. Size, Rss and Pss are summed only within
  their own column (L-002 S2.1). `/proc/self/
  smaps_rollup` Rss/Pss is printed as a cross-check only,
  never added.
- **Allocator account (C1).** `mimir_core_stats_snapshot`
  (the Phase 10 statistics surface, reached by linking the
  library objects directly - the J-002 pattern; the public
  header is untouched and the shared library's dynamic
  symbols are unchanged by construction).
- **Route evidence.** The ANON bucket is split into C1, C2
  and R only by route evidence (L-002 S2.2 step 3/4, S5.6):
  C1 by the counter block, C2 by counter-invariance across
  a step whose code path is known stdlib, the remainder
  stays R and is printed.

Snapshots (L-002 S5.3 numbering): **S0** baseline, **S1**
after mmap, **S2** after metadata walk + weight touch,
**S3** after KV allocation, **S4** after prefill (C1
route), **S4C2** after the prefill stdlib workspace (C2
route; an extra snapshot, labelled as such), **S5** steady
decode after a fixed 32 steps, **S6** after teardown.

`run_l006.sh` builds the harness with the repo CFLAGS
(`-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc
-O2 -DMIMIR_DEBUG=1`), runs it 3 times under
`timeout 120`, then attempts the Aesir KV-cache component
leg (Section 4). Every step is bounded; there is no soak.

## 2. Frozen exit conditions (L-002 S5.1, fixed BEFORE the run)

- **Model:** `/mnt/yggdrasil/models/tinyllama/tinyllama-1.1b-chat-v1.0.Q4_0.gguf`,
  637,699,456 bytes, sha256
  `da3087fb14aede55fde6eb81a0e55e886810e43509ec82ecdc7aa5d62a03b556`
  (the same file and hash as L-004). The Phase 12
  reference model (Qwen3-1.7B Q4_K_M) is NOT on this Pi
  (INC-L003-04); TinyLlama is the only real GGUF here and
  its hash is this run's identity, per S5.1.
- **Model dims, parsed from the file itself by the
  harness:** version 3, **201 tensors, 23 metadata KVs**,
  layers 22, embedding 2048, heads 32, KV heads 4, head
  dim 64 (`llama.rope.dimension_count`, gated equal to
  embedding / heads), context 2048, vocab 32,000 (counted
  from the `tokenizer.ggml.tokens` array), metadata ends
  at byte 1,709,436, tensor data starts at 1,709,440.
- **KV sizing (harness convention, stated, not an Aesir
  fact):** KV bytes per token = layers x KV heads x head
  dim x 2 (K and V) x 4 (f32) = **45,056 B/token**,
  computed in code from the parsed dims and gated
  (`kv_per_token_formula`).
- **Workload (the harness's own, defined here, NOT
  Aesir's inference):** prompt = 32 tokens, decode = 32
  steps (both fixed constants of the harness).
  - *Load + metadata:* as L-004 - `mmap` the whole file
    `PROT_READ, MAP_SHARED` (class W), walk all metadata
    and tensor infos THROUGH the mapping (viewed bytes
    stay W, trap DC-7), fault residency by the same touch
    policy (every page of the first 32 MiB, one page per
    256 KiB beyond).
  - *KV allocation:* one `mimir_malloc(45,056)` slice per
    prompt token (32 slices), each fully touched - the KV
    cache the prefill phase fills (C1, explicit adoption).
  - *Prefill (C1):* prompt token-id buffer (32 x 4 B) +
    prompt embedding buffer (32 x 2048 x 2 B, f16) + a
    burst of 4 x 256 KiB activation workspace buffers,
    all via `mimir_*`, all touched.
  - *Prefill (C2):* one 512 KiB stdlib workspace + the
    token-id copy via plain `malloc` - the stdlib route,
    the route Aesir's own implicit heap takes (L-003).
  - *Decode:* 32 steps; each step appends ONE KV slice
    via `mimir_malloc` (KV growth, held to S5), plus
    transient churn freed inside the step: one 8 KiB
    `mimir_malloc` scratch and one logits buffer
    (vocab x 4 B = 128,000 B) via `mimir_*`, and one
    8 KiB stdlib scratch alloc/free (C2 churn).
  - *Teardown:* free every C1 and C2 block, `munmap`.
- **Threads:** 1 (single-threaded by design, so every
  snapshot is quiescent by construction).
- **Backend:** none. No GPU/device backend exists in this
  workload, so **G = 0 by absence of a backend**, stated,
  not assumed (no `/dev/dri` or dma-buf mapping appeared
  in any snapshot's R (file) line either).
- **Thermal state:** `temp=59.3'C`, `throttled=0x0` at the
  start; `temp=59.8'C`, `throttled=0x0` at the end. No run
  started throttled; no throttling began (S5.1 gate met).
- **Responsiveness gate (S5.5):** Samtal answered HTTP 200
  on port 8787 before AND after the recorded run.
- **Gates:** the harness exits 0 iff all 18 deterministic
  gates print ok (magic, walk counts, parsed dims, the
  exact counter delta of each phase, teardown returns).
  Rss movements are printed as evidence, never gated:
  page residency is the kernel's choice, not the
  allocator's.

## 3. Measured results - probe leg (3 reps)

Every counter figure and every snapshot figure below is
identical across the three reps (spread 0). The only
variance in the whole run is baseline R jitter: S0 R
(file) Rss is 1,248 kB in reps 1-2 and 1,232 kB in rep 3,
carrying a matching 16 kB through that rep's rollup -
exactly the pre-load jitter class L-004 recorded, inside
R, where S5.6 expects reader residue. The table shows
rep 1. `make test` gate for the slice: **exit 0, CHECKS:
4590 run, 0 failed, RESULT: PASS** (recorded in the run
header).

| Snapshot | W Size | W Rss | ANON Size | ANON Rss | R (file) Rss | rollup Rss (check) |
|---|---|---|---|---|---|---|
| S0 baseline | 0 | 0 | 3,040 kB | 128 kB | 1,248 kB | 1,392 kB |
| S1 after mmap | 622,768 kB | 64 kB | 3,040 kB | 144 kB | 1,312 kB | 1,520 kB |
| S2 metadata + touch | 622,768 kB | 180,288 kB | 3,040 kB | 144 kB | 1,312 kB | 181,744 kB |
| S3 after KV allocation | 622,768 kB | 180,288 kB | 5,088 kB | 2,192 kB | 1,312 kB | 183,792 kB |
| S4 after prefill (C1) | 622,768 kB | 180,288 kB | 6,336 kB | 3,440 kB | 1,312 kB | 185,040 kB |
| S4C2 after prefill (C2) | 622,768 kB | 180,288 kB | 6,864 kB | 3,968 kB | 1,312 kB | 185,568 kB |
| S5 steady decode (32 steps) | 622,768 kB | 180,288 kB | 8,912 kB | 6,016 kB | 1,312 kB | 187,616 kB |
| S6 after teardown | 0 | 0 | 3,056 kB | 160 kB | 1,312 kB | 1,472 kB |

Pss tracks Rss within the shared-page split printed in
the recorded run and is not repeated in this table.

C1 counter progression (identical in all reps):

| Point | requested | allocated | mapped | live_mappings | live_small | released |
|---|---|---|---|---|---|---|
| S0-S2 | 0 | 0 | 0 | 0 | 0 | 0 |
| S3 KV alloc | 1,441,792 | 1,572,864 | 2,097,152 | 32 | 0 | 0 |
| S4 prefill C1 | 2,621,568 | 2,752,640 | 3,375,104 | 37 | 1 | 0 |
| S4C2 prefill C2 | 2,621,568 | 2,752,640 | 3,375,104 | 37 | 1 | 0 |
| S5 decode x32 | 8,421,504 | 9,044,096 | 5,472,256 | 69 | 1 | 5,767,168 |
| S6 teardown | 8,421,504 | 9,044,096 | 16,384 | 0 | 0 | 11,223,040 |

The phase findings, each tied to its gate:

1. **The load and metadata phases are unchanged by the
   new phases.** W Size jumps to the whole file at S1
   (622,768 kB) and W Rss reaches 180,288 kB at S2 under
   the recorded touch policy (kernel readahead, as in
   L-004), while every C1 counter stays exactly 0 through
   S2 (`mmap_moves_no_allocator_counter`,
   `metadata_view_and_touch_move_no_counter`). W Rss then
   stays at exactly 180,288 kB through S3, S4, S4C2 and
   S5: the prefill and decode phases move NO weight
   residency, exactly as the L-002 S2.3 phase table
   requires of a correct profile.
2. **KV allocation is pure C1 growth, by the exact sum.**
   The S3 counter delta is `bytes_requested=1,441,792` =
   32 slices x 45,056 B (gate
   `KV_counter_delta_equals_prompt_slices`); each slice
   is a mapping-path publication (`live_mappings` 0 ->
   32, usable 49,152 B granted per slice,
   `bytes_allocated=1,572,864`). ANON Rss rises by
   2,048 kB in the same step - exactly 64 kB per slice
   mapping, its touched pages resident - the /proc
   shadow of the counter account, reconciled, not added
   to it (trap DC-5).
3. **Prefill is a burst, and the burst is exactly the
   harness's sum.** S4 delta `bytes_requested=1,179,776`
   = ids 128 + embedding 131,072 + workspace 4 x 262,144
   (gate `prefill_C1_delta_equals_harness_requested_sum`);
   `live_mappings` rises 32 -> 37 (4 workspace + 1
   embedding) and the 128 B id buffer is the run's one
   live small object (`live_small=1`, inside a 16,384 B
   span). Peak C1 mapped for the whole run is read at
   S4/S5: `bytes_mapped=5,472,256` at S5 with the decode
   KV growth included.
4. **The C2 prefill route is invisible to the counters,
   visible in /proc.** Across the C2 step every C1
   counter is byte-identical to S4 (gate
   `C2_prefill_step_moves_no_C1_counter`) while ANON Rss
   rises by exactly 528 kB - the touched 512 KiB stdlib
   workspace plus its id-copy page. Route known (this
   harness's own `malloc` calls), counters invariant:
   the growth is C2 by route evidence, per S2.2 step 3.
5. **Decode grows KV linearly and churns transiently,
   both exactly.** The S5 counter delta over the 32 steps
   is `bytes_requested=5,799,936` = 32 x (KV 45,056 +
   scratch 8,192 + logits 128,000) (gate
   `decode_C1_delta_equals_harness_requested_sum`). Of
   that, the held part is the KV growth: 32 new slices,
   so at S5 the cache holds 64 slices x 45,056 B =
   2,883,584 B requested (`live_mappings` 37 -> 69: the
   32 decode KV slices; the per-step scratch and logits
   publications were freed inside each step, which is
   why `bytes_released` grows by 5,767,168 during decode
   while live bytes grow only by the KV slices and
   their mapping slack). ANON Rss grows 3,968 ->
   6,016 kB across decode (+2,048 kB: the touched pages
   of the 32 new slices plus churn residue). Per-token
   KV growth is therefore a straight line at the parsed
   45,056 B/token in the counter account, with the /proc
   account tracking it page by page - the growth curve
   the L-001 site inventory asks for, for THIS harness's
   KV route (see INC-L006-03 for what it does not say
   about Aesir's route).
6. **Teardown separates cleanly and completely.** At
   S6: W = 0/0 (gate `W_returns_to_zero`); C1
   `live_small` and `live_mappings` are back to the S0
   baseline exactly (gate
   `C1_live_returns_to_baseline`); cumulative
   `bytes_released` = 11,223,040. What remains is named,
   not hidden (S5.6): `bytes_mapped=16,384` of retained
   empty span (`live_span_bytes=16,384`,
   `bytes_cached=0`) - one span page retained under the
   G-005 posture, reported, not gated (INC-L006-06) -
   and ANON Rss 160 kB vs the 128 kB baseline, the
   harness's own staging/heap residue (R).
7. **Reconciliation (S3.2).** In reps 1-2 the three
   classified Rss terms sum EXACTLY to the rollup Rss at
   every snapshot from S1 onward (e.g. S3 183,792 kB,
   S5 187,616 kB, S6 1,472 kB); rep 3 is exact from S2
   onward. Only the S0 baseline sum (and rep 3's S1) sit
   16 kB below their rollup - the snapshot reader's own
   transient pages at the first reads; the reader is
   this process, its residue is R, and the direction
   (classified <= rollup) is the safe one. No byte is
   counted twice in any snapshot: the class terms are
   pairwise disjoint by construction of the S2.2 rule.

## 4. Aesir component leg - BLOCKED, recorded as owed

The runner attempts the one Aesir component whose subject
is this slice's phases - the KV-cache test - as a native
aarch64 Mojo build, exactly as L-004 ran its component leg:

- Command: `cd /home/pi/c2/aesir_engine && timeout 260
  mojo build tests/test_kv_cache.mojo -o /tmp/l006_kv`
  (Mojo 1.1.0) - **build exit 1, no binary produced.**
  First error, verbatim from the recorded run:
  `/home/pi/c2/aesir_engine/core/cuda_gate.mojo:39:21:
  error: use of unknown declaration 'InlineArray'`,
  reached via `core/inference.mojo` -> `core/compute.mojo`
  -> `core/cuda_gate.mojo`, ending `failed to parse the
  provided Mojo source module`. This is the banked
  INC-L003-02 stdlib skew, met again on the inference
  import path.
- Additional in-slice checks (same session, same result,
  all build exit 1 at parse on the same skew class):
  `tests/test_inference.mojo`, `tests/
  test_turing_model_prefill.mojo`, `tests/
  test_small_attention_model.mojo`, `tests/
  test_generation_control.mojo`. No Aesir prefill, decode
  or KV component ran on this Pi in this slice, and none
  is claimed. The only Aesir component proven native on
  this Pi remains the L-003/L-004 memory-refinements leg
  (MimirWell + tokenizer), which is L-005's subject, not
  this slice's, and is not re-run here.
- Class reading, for the record: had any of these run,
  its bytes would be **C2** by the established L-003
  boundary (MimirWell is one stdlib arena allocation; the
  rest is implicit stdlib heap) - no byte of an Aesir
  component run is MimirAlloc (C1) without the explicit
  adoption this harness performs.

## 5. What this slice proves, and what it does not

Proven on this Pi, from the recorded run: around a real
model file, the prefill, decode and teardown phases are
mechanically trackable under the L-002 taxonomy with the
L-004 instrument extended, and their class behaviour
matches the L-002 S2.3 phase table - W frozen once
touched, C1 moving by exact phase sums (KV allocation
1,441,792 B; prefill burst 1,179,776 B; decode 5,799,936 B
over 32 steps, of which KV growth is a linear
45,056 B/token), the C2 route moving no counter while
moving /proc, and teardown returning every live C1
object and the whole W mapping to the quiescent baseline
with the one retained span named. With L-004 (load +
metadata) and L-005 (tokenizer + KV sizing), the S5.3
snapshot series S0-S6 is now exercised end to end by a
runnable harness on this Pi. This is a **first look** in
the L-002 S5.2 sense: 3 reps of ONE workload arm (the
explicit-adoption arm), not the alternating
baseline/comparison series - there is no allocator
comparison result here and none is implied.

Not proven, and not claimed: anything about Aesir's own
prefill, decode, KV or teardown allocations (Section 4,
INC-L006-01/03); anything about the Phase 12 reference
model (INC-L006-02); anything about GPU/device memory
(INC-L006-04).

## 6. Owed / incomplete (INC-L006 and carried blockers)

- **INC-L006-01 (owed, blocker INC-L003-01/02):** Aesir's
  real prefill/decode path did NOT run. The full engine
  cannot build on this Pi (MAX absent on aarch64; the
  inference import path also hits the InlineArray stdlib
  skew, re-verified this slice on five test modules,
  Section 4). The probe's phases are this harness's own
  C code over the same model file, with phase shapes from
  the L-001 S2.3 hypotheses and sizes from the file's own
  metadata - they are NOT Aesir prefill/decode, and no
  Aesir prefill, decode or teardown figure exists in
  this slice.
- **INC-L006-02 (owed, blocker INC-L003-04):** no Qwen3
  profile of any phase: Qwen3-1.7B Q4_K_M is absent from
  this Pi (only TinyLlama-1.1B quants are present). The
  Phase 12 reference-model profile remains owed wherever
  L-002 S7 already records it.
- **INC-L006-03 (route evidence gap, carried
  INC-L002-04):** whether Aesir's real KV cache, prefill
  workspace and per-token buffers are allocated through
  the CPU allocator (C1) or by explicit runtime routes
  (C2) is still not established by any banked evidence.
  This slice's KV is C1 BY CONSTRUCTION (the harness
  itself calls `mimir_*`); its 45,056 B/token growth
  line describes the harness's KV route only and must
  not be read as Aesir's KV behaviour.
- **INC-L006-04 (unmeasured class, carried
  INC-L002-03):** G stayed 0 because no device backend
  ran. Nothing in this slice measures, bounds or speaks
  for GPU/device memory in any form; Aesir's CUDA paths
  are source-described only (L-001 S3.1) and this Pi has
  no CUDA backend.
- **INC-L006-05 (convention, not a defect):** the KV
  sizing uses f32 K+V (4 B/element) because that is the
  harness's stated convention, computed from the parsed
  dims. A real runtime may hold KV in a quantized or
  f16 form; no evidence on this Pi decides Aesir's KV
  element size, so the 45,056 B/token figure is a
  harness figure, never an Aesir figure.
- **INC-L006-06 (observation, retained span):** teardown
  leaves `bytes_mapped=16,384` (one retained empty span,
  `bytes_cached=0`, live objects and mappings exactly
  0). This differs from L-004's retained 49,152 B only
  because the two workloads free different small-object
  mixes (this run's single small object is the 128 B
  prompt-id buffer); it is the banked G-005 retained-
  span posture, not a leak - the S6 live gates prove
  nothing is live - and it is reported here so the
  difference is on record rather than smoothed over.
- **Carried honesty:** the S0 (and one S1) reconciliation
  sits 16 kB under rollup in every rep - the snapshot
  reader's own transient pages, inside R, direction
  safe, identical in kind to L-004's S0 note. R (file)
  also carries a 16 kB baseline jitter between reps
  (1,232-1,312 kB); no conclusion in this slice rests on
  an R movement, per S5.6.

*End of L-006. Deliverables: this document,
tests/perf/probe_l006.c, tests/perf/run_l006.sh,
tests/perf/recorded_run_l006.txt. No code in src/, no
header, no Makefile, no README change. The owed Aesir
runs stand exactly where INC-L003-01/02/04 put them.*
