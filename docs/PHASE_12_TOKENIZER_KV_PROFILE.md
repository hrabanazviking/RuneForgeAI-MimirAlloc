# Phase 12 - Tokenizer + KV Cache Allocation Profile (L-005)

Slice: **L-005 (team-05)**. Wave 12, Phase 12 (Aesir
model-workload integration). Base:
`2f2d8fe8800009a56c317ae32007de9af82487b4` (Wave 12,
3 of 9 banked; L-004 banking in parallel, files
disjoint) on branch `yrsa/mimiralloc-05`, worktree
`/home/pi/mimiralloc-team-05` on Raspberry Pi
`purpleviking1`. Deliverables:
`tests/perf/probe_l005.c` (573 lines),
`tests/perf/run_l005.sh` (48 lines),
`tests/perf/recorded_run_l005.txt` (290 lines), and
this doc. No Makefile, README, header, or C-source
change. Pi load stayed light (13 GiB available; Samtal
HTTP 200 before and after; temp 57.6-58.2 C,
throttled=0x0).

Governing text: roadmap Phase 12 - track tokenizer,
KV cache, prompt prefill, decode and teardown; the
L-002 taxonomy binds (one class per byte: W/C1/C2/G/R,
route decides). L-004 explicitly left KV/prefill/
decode to this slice (INC-L004-04).

> **HEADLINE 1 - TOKENIZER, MEASURED (two distinct
> legs, not to be conflated):** (a) the REAL Aesir
> tokenizer code - `RuneWeaver` BPE inside Aesir's own
> `tests/test_memory_refinements.mojo`, rebuilt and
> run natively as in L-003/L-004 - PASSes both its
> subtests, child peak RSS 10,960 KiB; its heap is the
> Mojo stdlib route, class **C2** (its tiny synthetic
> 3-token vocab is the component's own fixture, NOT
> the model vocab). (b) Over REAL model data: the
> TinyLlama-1.1B Q4_0 GGUF (sha256 da3087fb..., the
> L-004 file) was mmap'd (W) and its tokenizer.ggml.*
> metadata parsed through the mapping: vocab=32,000,
> token strings 210,919 B, scores 32,000 x f32, types
> 32,000 x i32. The parsed structures a tokenizer
> holds (string blob + ids + scores + types =
> 626,919 B requested) were built in a **C1** leg
> (mimir_*: counter delta EXACTLY 626,919, gate ok)
> and a **C2** leg (malloc: C1 counters invariant,
> checksums equal 33,285). Parsing itself moved no
> allocator counter (views stay W).
>
> **HEADLINE 2 - KV CACHE, MODELLED SHAPE ONLY:**
> Aesir's engine cannot run here (INC-L003-01/02), so
> no KV figure in this slice is Aesir running. The
> harness reproduces the allocation shape Aesir's
> source describes (Section 2, cited) at the real
> TinyLlama dims (22 layers, hidden 2048, ctx 2048):
> K pool = V pool = 92,274,688 B, paged metadata
> 3,256 B, total 184,552,632 B for 64 physical blocks
> of 16 tokens (1,024 physical tokens). Prefill 256
> tokens (16 blocks) + 64 decode steps (20 blocks)
> were touched in logical order in both routes: C2
> (malloc; C1 counters invariant; ANON Rss rose to
> 57,200 KiB under the touch policy) and C1 (mimir_*;
> counter delta EXACTLY the shape total, gate ok).
> This shows what MimirAlloc WOULD carry under
> explicit adoption - a modelled-shape harness
> measurement, labelled as such everywhere.
>
> **HEADLINE 3 - PHASE 12 EXIT STILL OWED:** this
> slice cannot meet the Phase 12 exit conditions
> (no real engine, no Qwen3-1.7B Q4_K_M, no baseline
> vs comparison run). Section 5 records the owed
> items honestly.

## 1. Verdict per element

| # | Element | Verdict | Where |
|---|---|---|---|
| 1 | Real Aesir tokenizer component runs natively | **MEASURED** - both subtests PASS, exit 0, child peak RSS 10,960 KiB, route C2 | recorded run, component leg |
| 2 | Real GGUF tokenizer metadata parsed through W mapping | **MEASURED** - vocab 32,000; strings 210,919 B; scores/types 32,000 each; parse moves no counter | probe S1 |
| 3 | Parsed tokenizer structures, C1 route | **MEASURED** - requested delta exactly 626,919 B | probe S2 |
| 4 | Parsed tokenizer structures, C2 route | **MEASURED** - C1 invariant, checksum equal (33,285) | probe S3 |
| 5 | KV allocation shape from Aesir source | **SOURCE-PROVEN** (inspection, cited) | Section 2 |
| 6 | KV shape at TinyLlama dims, C2 + C1 legs, prefill/decode lifetimes | **MODELLED, harness-measured** - total 184,552,632 B; C1 delta exact; touch checksums equal (1,554,432) | probe S5-S7 |
| 7 | Aesir itself allocating KV through any route on this Pi | **NOT MEASURED, NOT CLAIMED** - engine cannot build/run (INC-L003-01/02) | Section 5 |
| 8 | Phase 12 exit (same-hash Qwen3 baseline vs comparison) | **OWED** | Section 5 |
| 9 | `make test` regression | **PASS** - exit 0, 4590 run / 0 failed | recorded run header |

## 2. The KV shape, as Aesir's source describes it

All citations: `/home/pi/c2/aesir_engine` (L-003's
canonical checkout).

- `core/mimir_well.mojo:824` `struct KVCache`: fixed-
  capacity; constructor (line ~838) computes
  `total_elements = num_layers * max_seq_len *
  hidden_dim` and makes ONE `well.allocate` call for
  K and ONE for V (f16 elements). Storage is drawn
  from `MimirWell` - Aesir's own stdlib arena (route
  C2 by L-002/L-003; the Well is NOT MimirAlloc,
  INC-L001-04 carried).
- `core/mimir_well.mojo:962` `struct PagedKVCache`:
  preallocated host K/V page pool over a base KVCache.
  Defaults: `block_size=16`, `max_sequences=1`,
  `physical_blocks=0` meaning all logical blocks.
  Metadata built at construction: `page_table`,
  `block_owner`, `block_logical_index` - each
  `List[Int]` of `max_sequences * ceil(max_seq_len /
  block_size)`; `sequence_lengths[max_sequences]`;
  `layer_lengths[max_sequences * num_layers]`.
  Physical pool tokens = `physical_blocks *
  block_size`, and the base KVCache is sized to the
  PHYSICAL token count. `core/README.md:7` notes
  model attention sessions do not use PagedKVCache
  yet - the paged path is library code, not the
  engine's live path; the live path is the plain
  KVCache inside the Well's one arena allocation
  (pool sizing at `aesir.mojo:29-50`, L-003 S2).
- Concrete instance, `tests/test_kv_cache.mojo:18`:
  `PagedKVCache(8, 4, well, 2, 2, 2, 3)` - max_seq 8,
  hidden 4, 2 layers, block 2, 2 sequences, 3
  physical blocks; append/release/reuse semantics
  exercised there (that test is a component test; it
  was NOT re-run by this slice - see INC-L005-03).
- Tokenizer fields, `loader/tokenizer.mojo:7-17`:
  `RuneWeaver { vocab: List[String], token_to_id:
  Dict[String, Int], scores: List[Float32],
  token_types: List[Int], vocab_size, ... }` - the
  parsed-structure set the harness's T legs mirror
  (the harness's id array is identity-ordered; a
  real Dict's bucket storage is stdlib-internal and
  is NOT modelled - INC-L005-02).

## 3. Tokenizer profile (measured)

Rep 1 of 3 (all reps PASS, all 14 gates ok each rep;
recorded run has all three):

- S1: mmap + parse. W Size = file (622,768 KiB);
  C1 counters at 0 before and after the parse -
  metadata views through the mapping allocate
  nothing (L-002 S1.1, trap DC-7 avoided).
- T-C1 (S2): four mimir_* allocations totalling
  exactly 626,919 B requested (blob 242,919 = 210,919
  string bytes + 32,000 NULs; ids 128,000; scores
  128,000; types 128,000). live_mappings 4.
- T-C2 (S3): same four via malloc; ANON Size rises
  688 KiB (glibc arena), C1 counters bit-identical,
  content checksum equal to C1 leg (33,285).
- S4: teardown returns C1 live bytes/objects to the
  S0 baseline (gate ok; released 704,512 mapped B).
- Component leg: Mojo 1.1.0 build exit 0; MimirWell
  telemetry PASS + BPE tokenizer PASS; child peak
  RSS 10,960 KiB. The component's tokenizer fixture
  is 3 tokens on a synthetic prompt - it proves the
  real RuneWeaver CODE runs and classifies C2; it
  says nothing about 32,000-token vocab cost, which
  is what leg (b) measures over the real metadata.

## 4. KV profile (modelled shape, harness-measured)

Shape at parsed dims (layers 22, hidden 2048, ctx
2048, block 16, 1 sequence, 64 physical blocks):
pool K = pool V = 22 x 1,024 x 2,048 x 2 =
92,274,688 B; metadata = 3 x 128 x 8 + 8 + 22 x 8 =
3,256 B; total 184,552,632 B. (Full-ctx residency
for one sequence would be 128 physical blocks =
369,098,752 B (~352 MiB) for K+V at f16; the 64-block pool is
the bounded harness choice, stated, not hidden.)

- K-C2 (S5-S6): malloc pools; C1 counters invariant
  (gate ok). Touch policy: prefill positions 0-255
  then decode positions 256-319, one byte per page
  of every (layer, position) row, logical order -
  the append() access shape. ANON Rss 912 -> 57,200
  KiB (residency is the kernel's, printed not gated).
- K-C1 (S7): same shape via mimir_*; bytes_requested
  delta EXACTLY 184,552,632 (gate ok); 3 live
  mappings; touch checksum 1,554,432 equals the C2
  legs' prefill+decode sum (1,436,160 + 118,272).
- S8: teardown returns to baseline (gate ok).
- G = 0 in all snapshots: no device backend ran.

What this DOES show: the byte counts MimirAlloc
would carry for Aesir's paged-KV shape at TinyLlama
dims under explicit adoption, with binding counters,
and that the C2 stdlib route carries the identical
shape invisibly to those counters. What it does NOT
show: Aesir allocating anything, attention compute,
or any timing/latency claim (none is made).

## 5. Owed / incomplete (INC-L005)

- **INC-L005-01 (owed, blockers INC-L003-01/02):**
  Aesir's real tokenizer load (RuneWeaver populated
  from a GGUF via `loader/`) and its real KV usage
  (KVCache/PagedKVCache inside an engine run) did
  NOT run: MAX is absent on aarch64 and the stdlib
  InlineArray skew blocks the engine build. Every
  engine-scale tokenizer/KV figure remains owed to
  the slice that first runs the engine.
- **INC-L005-02 (modelled, bounded):** the harness's
  parsed tokenizer structures mirror RuneWeaver's
  field SET and byte payloads, but not Mojo stdlib
  container internals (Dict buckets, List capacity
  growth). C2 byte-exactness for a real RuneWeaver
  load is therefore not claimed; the C2 leg here is
  the harness's own malloc structures, classified
  by route.
- **INC-L005-03 (scope):** Aesir's own
  `tests/test_kv_cache.mojo` component test was
  cited for the concrete paged instance but not
  re-run here; the runnable-component leg used the
  L-003/L-004-proven test_memory_refinements binary
  shape. Re-running test_kv_cache natively is owed
  to any slice needing paged-semantics evidence.
- **INC-L005-04 (owed, blocker INC-L003-04):** no
  Qwen3-1.7B Q4_K_M exists on this Pi; the Phase 12
  exit comparison (same model hash, prompt, context,
  threads, backend, thermal state; baseline vs
  MimirAlloc) remains entirely owed. Prefill/decode
  TIMING is likewise owed - this slice profiles
  allocation only, by design.
- **INC-L005-05 (carried):** PagedKVCache is not
  Aesir's live attention path (core/README.md:7);
  the live KV route is the plain KVCache inside the
  MimirWell arena's single stdlib allocation. The
  modelled C1/C2 comparison therefore speaks for
  both shapes' byte totals (identical pool maths)
  but for the paged METADATA only as library shape.
- **Carried honesty:** G stayed 0; nothing here
  measures or speaks for GPU/device memory.

## 6. Files and verification

| File | Lines | sha256 (first 8) |
|---|---|---|
| tests/perf/probe_l005.c | 573 | ef4eeb6d |
| tests/perf/run_l005.sh | 48 | 6ebfb7a6 |
| tests/perf/recorded_run_l005.txt | 290 | 1e2c4253 |
| docs/PHASE_12_TOKENIZER_KV_PROFILE.md | (this file) | - |

Recorded run: 3/3 probe reps PASS (14/14 gates
each), component leg PASS, make test exit 0
(4590 run / 0 failed), Samtal HTTP 200 before and
after, 13 GiB available throughout, no throttling.
README.md and the public header are untouched by
this slice (verified by git diff at commit time).
