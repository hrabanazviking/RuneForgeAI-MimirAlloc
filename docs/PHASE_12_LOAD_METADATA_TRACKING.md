# Phase 12 - Model-Loading + GGUF Metadata Allocation Tracking (Slice L-004)

Status: **MEASURED (harness first look, L-002 S5.2 sense).**
This slice builds the tracking harness the L-002 protocol
binds (`docs/PHASE_12_MEMORY_SEPARATION_TAXONOMY.md` S5) and
records its first runs: around a runnable workload, the
class separation (W / C1 / C2 / G / R) is tracked at the
load + metadata phases with quiescent snapshots before,
during and after, and every figure below is read back from
`tests/perf/recorded_run_l004.txt` on this Pi. What cannot
run on this Pi is recorded OWED in Section 6 with its exact
blocker, never simulated or claimed.
Base: `62cbb6fb` (Wave 12, L-002 banked; L-003 banking in
parallel, files disjoint). Team-04 worktree, branch
`yrsa/mimiralloc-04`. Files: `tests/perf/probe_l004.c`
(harness, 549 lines / 19,425 B, sha256 `78ffa301`),
`tests/perf/run_l004.sh` (driver, 56 lines / 2,447 B,
sha256 `cfc103fd`), `tests/perf/recorded_run_l004.txt`
(recorded run, 218 lines / 12,070 B, sha256 `36b4b316`),
this doc. No Makefile, README or public-header change.

## 1. What was built

`probe_l004` is a single-threaded C harness that runs one
defined workload (Section 2) and, at each quiescent point
(no allocation in flight), takes a snapshot with three
independent accounts, kept separate exactly as the banked
Phase 9/10 method requires:

- **/proc account.** Its own `/proc/self/smaps` read raw
  into a static 2 MiB buffer (the reader allocates nothing
  on the measured heap, L-002 S5.3), classified mapping by
  mapping by the L-002 S2.2 decision procedure: pathname
  equal to the model path -> **W**; empty or bracket-label
  pathname -> the **ANON** bucket; any other file pathname
  -> **R (file)**. Size, Rss and Pss are summed only within
  their own column (L-002 S2.1). `/proc/self/smaps_rollup`
  Rss/Pss is printed as a cross-check only, never added.
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

Snapshots: **S0** baseline, **S1** after mmap, **S2** after
metadata walk + weight touch, **S3** after the C1 step,
**S4** after the C2 step, **S6** after teardown. (S5 steady
decode does not exist in this workload: nothing here
generates tokens. That is L-005+ scope, Section 6.)

`run_l004.sh` builds the harness with the repo CFLAGS
(`-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc
-O2 -DMIMIR_DEBUG=1`), runs it 3 times under
`timeout 120`, then rebuilds and reruns the Aesir component
leg (Section 4). Every step is bounded; there is no soak.

## 2. Frozen exit conditions (L-002 S5.1, fixed BEFORE the run)

- **Model:** `/mnt/yggdrasil/models/tinyllama/tinyllama-1.1b-chat-v1.0.Q4_0.gguf`,
  637,699,456 bytes, sha256
  `da3087fb14aede55fde6eb81a0e55e886810e43509ec82ecdc7aa5d62a03b556`.
  The Phase 12 reference model (Qwen3-1.7B Q4_K_M) is NOT
  on this Pi (INC-L003-04); TinyLlama is the only real GGUF
  here and its hash is this run's identity, per S5.1's
  "whatever model is used, its hash is the identity".
- **Workload (the harness's own, defined here, NOT Aesir's
  loader):**
  - *Load phase:* `mmap` the whole file `PROT_READ,
    MAP_SHARED` - the loader route for weights (class W) -
    then fault residency by a recorded touch policy: every
    page of the first 32 MiB, one page per 256 KiB beyond.
  - *Metadata phase:* walk all GGUF metadata KVs and all
    tensor infos THROUGH the mapping (viewed bytes stay W,
    L-002 S1.1 / trap DC-7), then build the parsed records
    twice: once via `mimir_malloc`/`mimir_calloc` (C1 step:
    one 64 B record per KV, one key copy per KV, one 1 MiB
    scratch) and once via plain `malloc` (C2 step: the same
    key copies + one 1 MiB scratch) - the stdlib route is
    the route Aesir's own metadata/tokenizer heap takes
    (established, L-003), so the C2 step exercises the
    tracking of that route, in this harness's own process.
  - *Teardown:* free both routes, `munmap`.
- **Prompt / context / decode:** none - no generation is
  claimed anywhere in this slice.
- **Threads:** 1 (the harness is single-threaded by
  design, so every snapshot is quiescent by construction).
- **Backend:** none. No GPU/device backend exists in either
  workload leg, so **G = 0 by absence of a backend**,
  stated, not assumed (no `/dev/dri` or dma-buf mapping
  appeared in any snapshot's R (file) line either).
- **Thermal state:** `temp=58.2'C`, `throttled=0x0` at the
  start; `temp=59.8'C`, `throttled=0x0` at the end. No run
  started throttled; no throttling began (S5.1 gate met).
- **Responsiveness gate (S5.5):** Samtal answered HTTP 200
  on port 8787 before AND after the recorded run.
- **Gates:** the harness exits 0 iff all 11 deterministic
  gates print ok (magic, walk counts, the counter
  identities of Section 3, teardown returns). Rss movements
  are printed as evidence, never gated: page residency is
  the kernel's choice, not the allocator's.

## 3. Measured results - probe leg (3 reps)

Every figure from S1 onward is identical across the
three reps (spread 0, min = max = the value shown). The
only variance in the whole run is the S0 baseline ANON
Rss: 112 kB in rep 1, 128 kB in reps 2-3 - one heap page
set of pre-load jitter, inside R, exactly where S5.6
expects reader residue. The table shows rep 1. `make
test` gate for the slice: **exit 0, CHECKS: 4590 run,
0 failed, RESULT: PASS** (recorded in the run header).

| Snapshot | W Size | W Rss/Pss | ANON Rss | R (file) Rss | rollup Rss (check) |
|---|---|---|---|---|---|
| S0 baseline | 0 | 0 | 112 kB | 1,248 kB | 1,392 kB |
| S1 after mmap | 622,768 kB | 64 kB | 144 kB | 1,312 kB | 1,520 kB |
| S2 metadata + touch | 622,768 kB | 180,288 kB | 144 kB | 1,312 kB | 181,744 kB |
| S3 after C1 step | 622,768 kB | 180,288 kB | 1,264 kB | 1,312 kB | 182,864 kB |
| S4 after C2 step | 622,768 kB | 180,288 kB | 2,288 kB | 1,312 kB | 183,888 kB |
| S6 after teardown | 0 | 0 | 192 kB | 1,312 kB | 1,504 kB |

GGUF facts read from the file itself: version 3, **201
tensors, 23 metadata KVs**, `general.architecture=llama`,
`general.name=tinyllama_tinyllama-1.1b-chat-v1.0`,
alignment 32, metadata ends at byte 1,709,436, tensor data
starts at 1,709,440. Both walks completed exactly
(gates `metadata_kv_walk_complete`,
`tensor_info_walk_complete`).

The separation findings, each tied to its gate:

1. **mmap is not allocation.** At S1 the W mapping carries
   the whole file (Size 622,768 kB = the file, page-rounded)
   while every C1 counter is still exactly 0
   (`mmap_moves_no_allocator_counter`). At S2, after the
   metadata walk and the touch, W Rss has grown to
   180,288 kB and the counters are STILL exactly 0
   (`metadata_view_and_touch_move_no_counter`): viewing
   metadata through the mapping creates no C bytes (trap
   DC-7 held), and faulting weight pages moves no counter.
2. **W residency is the kernel's, and exceeds the touch.**
   The touch policy faults at most ~34 MiB directly, yet W
   Rss at S2 is 180,288 kB (~176 MiB): sequential access on
   a shared file mapping triggers kernel readahead. This is
   reported as measured, and it is exactly why residency is
   evidence, never a gate, and never an allocator figure.
3. **C1 moves only in the C1 step, by the exact sum.**
   S3 counters: `bytes_requested=1,050,618` - precisely
   the harness's own requested sum (23 records x 64 B +
   23 key copies totalling 570 B + the 1 MiB scratch =
   1,050,618; gate `C1_counter_delta_equals_harness_
   requested_sum`), `bytes_allocated=1,065,696`,
   `bytes_mapped=1,146,880`, `live_mappings=2`,
   `live_small=23`, `alloc_failures=0`. ANON Rss rises by
   1,120 kB in the same step - the /proc shadow of the
   counter account, reconciled, not added to it.
4. **C2 is invisible to the counters, visible in /proc.**
   Across the C2 step every C1 counter is byte-identical
   to S3 (gate `C2_step_moves_no_C1_counter`) while ANON
   Rss rises by exactly 1,024 kB - the touched 1 MiB
   stdlib scratch (the small key copies land inside the
   existing heap arena). Route known (this harness's own
   `malloc` calls), counters invariant: the growth is C2
   by route evidence, per S2.2 step 3.
5. **Teardown separates cleanly.** At S6: W = 0/0 (gate
   `W_returns_to_zero`); C1 `live_small` and
   `live_mappings` are back to the S0 baseline (gate
   `C1_live_returns_to_baseline`); `bytes_released` delta
   = 1,097,728 (the two mappings). What remains is named,
   not hidden (S5.6): `bytes_mapped=49,152` of retained
   empty span pages (`live_span_bytes=49,152`,
   `bytes_cached=0`) - the G-005 retained-span posture,
   reported, not gated - and ANON Rss 192 kB vs the 112 kB
   baseline, the harness's own staging/heap residue (R).
6. **Reconciliation (S3.2).** At S1, S2, S3, S4 and S6
   the three classified Rss terms sum EXACTLY to the
   rollup Rss, in all three reps (e.g. S2 181,744 kB,
   S4 183,888 kB, S6 1,504 kB). Only the S0 baseline sum
   sits 16-32 kB below its rollup - the snapshot reader's
   own transient pages at the first read; the reader is
   this process, its residue is R, and the direction
   (classified <= rollup) is the safe one. No byte is
   counted twice in any snapshot: the class terms are
   pairwise disjoint by construction of the S2.2 rule.

## 4. Measured results - Aesir component leg (C2-side workload)

The L-003-proven component was rebuilt from source on this
Pi and rerun under the harness driver:

- Command: `cd /home/pi/c2/aesir_engine && timeout 260
  mojo build tests/test_memory_refinements.mojo
  -o /tmp/l004_tmr` - **exit 0**; the binary is a native
  aarch64 ELF (88,800 B, sha256 `4de99645`).
- Run: **exit 0**, both tests PASS verbatim:
  "MimirWell allocation telemetry & arena recycling: PASS"
  and "Profiled linear BPE tokenizer merge performance:
  PASS". Child peak RSS 10,960 KiB (rusage).
- Class reading (per the established L-003 boundary):
  everything in that process is **C2** - MimirWell is ONE
  stdlib allocation bump-carved (its 1 MiB pool), and the
  tokenizer's tables are implicit stdlib heap. NO byte of
  it is MimirAlloc (C1), and the run's own telemetry
  (allocated/free/utilization inside the arena) is the
  component's internal account, not an allocator counter.
  The 10,960 KiB peak RSS is a process cross-check only:
  /proc evidence from outside cannot split that process's
  C2 from its R (runtime, libraries) mid-run, so no C2
  byte figure is claimed for this leg - the claim is the
  route classification + the PASS, both measured.

## 5. What this slice proves, and what it does not

Proven on this Pi, from the recorded run: the L-002
taxonomy is mechanically trackable around a real model
file's load + metadata phases; W, C1 and C2 move
independently and reconcile against the process total;
an mmap load and a metadata view move no allocator
counter; explicit `mimir_*` adoption moves the counters
by the exact requested sum; the stdlib route moves none.
This is the tracking instrument L-005+ comparisons run
under. It is a **first look** in the L-002 S5.2 sense:
3 reps of ONE workload arm, not the alternating
baseline/comparison series - there is no allocator
comparison result here and none is implied.

## 6. Owed / incomplete (INC-L004 and carried blockers)

- **INC-L004-01 (owed, blocker INC-L003-01/02):** Aesir's
  real GGUF loader path (`loader/gguf.mojo`, `external_call`
  mmap of weights + its own metadata parse) did NOT run.
  The full engine cannot build on this Pi: MAX is absent
  on aarch64 (INC-L003-01) and the stdlib InlineArray skew
  blocks even the loader itself (INC-L003-02). The probe's
  mmap + walk is this harness's own C code over the same
  file and the same route shape - it is NOT Aesir loading,
  and no Aesir load figure exists in this slice.
- **INC-L004-02 (owed, blocker INC-L003-04):** no Qwen3
  load of any kind: Qwen3-1.7B Q4_K_M is absent from this
  Pi (only TinyLlama-1.1B quants are present), and the
  checkout `model.gguf` files are 24-byte stubs. The
  Phase 12 reference-model profile remains owed wherever
  L-002 S7 already records it.
- **INC-L004-03 (bounded by design):** the component leg's
  C2 is classified by route with a peak-RSS cross-check
  only; a mid-run quiescent snapshot series INSIDE the
  Mojo process (its S2/S3 equivalents) was not taken -
  the component exposes no snapshot hook, and adding one
  means editing Aesir's tests, which is outside this
  slice's files. Owed to any slice that owns that hook.
- **INC-L004-04 (scope, not a defect):** KV-cache, prefill
  and decode phases (L-002 S2.3 rows 3-5) are untracked
  here because no runnable workload on this Pi reaches
  them; they belong to the L-005 baseline leg and inherit
  its blockers (INC-L003-01/02/04).
- **Carried honesty:** G stayed 0 in both legs because no
  device backend ran. Nothing in this slice measures,
  bounds or speaks for GPU/device memory in any form.
