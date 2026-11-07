# Phase 12 - Aesir Boundary Wiring: Idiom Proof Earned, Engine Blocked (L-003)

Slice: **L-003 (team-03)**. Wave 12, Phase 12 (Aesir
model-workload integration). Base:
`b18244741ee8e457ac9d87a3a2c4b1b4458b69b2` (Wave 11
COMPLETE) on branch `yrsa/mimiralloc-03`, worktree
`/home/pi/mimiralloc-team-03` on Raspberry Pi
`purpleviking1`. Deliverables:
`docs/PHASE_12_AESIR_BOUNDARY_WIRING.md` (this file),
`tests/mojo/test_l003.mojo` (the wiring probe, 300
lines), and `tests/mojo/recorded_run_l003.txt` (the
recorded run, sections S0-S12). No Makefile, README,
header, or C-source change: the probe is built by the
exact `mojo build` command recorded in S6, standalone,
and `make test` composition is unchanged (S11: exit 0,
4590 run / 0 failed). Work style: solo lead, no
sub-agents. Pi load stayed light (13 GiB available at
every check; Samtal HTTP 200 at S0, after the builds,
and at close, S12).

Governing text: roadmap Phase 12 - "Track where Aesir
actually allocates: model loading, GGUF metadata,
tokenizer, KV cache, prompt prefill, decode and
teardown. Separate mmap weight residency, CPU buffers
and GPU allocations. Verify allocator integration
through Aesir's actual boundary and compare
Qwen3-1.7B Q4_K_M on supported devices." All Aesir
source citations below are to the checkout
`/home/pi/c2/aesir_engine` only (checkouts diverge -
INC-L003-07). L-001/L-002 findings are treated as
given and were re-verified cheaply in recorded run S1.

> **HEADLINE 1 - PROOF EARNED (explicit adoption only):**
> a Mojo program written for this slice in Aesir's own
> allocation idiom (a MimirWell-style stdlib bump arena,
> Part A) plus explicit `mimir_*` calls (Part B) builds
> and runs natively on this aarch64 Pi, and Part B is
> proven at **binding grade** in the F-005 shape:
> exactly 4 `LD_DEBUG=bindings` lines tie the four
> `mimir_*` symbols to this tree's `libmimiralloc.so`;
> serving descriptors validate in both span and mapping
> forms; the large-allocation mapped-byte delta is
> exact (32768 = page_round_up(16384 + 8192)) and
> returns to baseline on free; the no-MimirAlloc
> control build fails with undefined `mimir_*`
> references only. 22/22 checks PASS, exit 0 (S6-S10).
> **This is EXPLICIT ADOPTION ONLY**: a generated
> program adopting MimirAlloc for its own calls, the
> Phase 6 B1+B2 boundary exercised in Aesir's idiom.
> It is NOT Aesir using MimirAlloc. Aesir source
> contains zero `mimir_*` references (S2, B-d).
>
> **HEADLINE 2 - BLOCKER (Aesir itself):** Aesir cannot
> build or run on this Pi today. Every prebuilt
> `aesir_main` in all eight checkouts is the same
> x86-64 binary (BuildID 833a8ec2...), and running it
> fails even through the installed box64 binfmt
> (exit 255, x86-64 libraries unloadable, S1). A
> bounded full-engine source build with the installed
> aarch64 Mojo 1.1.0 fails exit 1 with 30 errors in two
> classes: the MAX package is absent on aarch64 (Aesir
> `pixi.toml` declares `platforms = ["linux-64"]` and
> `max >=26.5`), and the source expects a stdlib
> layout this toolchain does not have (16 errors:
> `std.collections` has no `InlineArray`). One small
> Aesir component DOES build and run natively (S3) -
> the road is open at component scale, closed at
> engine scale. Section 5 names exactly what unblocks.
>
> **WARNING (INC-L001-04, carried):** Aesir's internal
> arena `MimirWell` (`core/mimir_well.mojo`) is Aesir
> code serviced by the Mojo stdlib allocator. It is
> NOT MimirAlloc, and nothing in this slice claims it
> as integration.

## 1. Verdict per element

| # | Element | Verdict | Where |
|---|---|---|---|
| 1 | Aesir actual allocation boundary identified from source, with file/line citations | **PROVEN** (source inspection) | Section 2; S2 |
| 2 | Small Aesir component builds + runs natively on aarch64 | **PROVEN** - Aesir own `tests/test_memory_refinements.mojo` (MimirWell telemetry + tokenizer) built exit 0 to an ARM aarch64 ELF and ran exit 0, both subtests PASS | S3 |
| 3 | Full Aesir engine builds on aarch64 | **BLOCKED** - exit 1, 30 errors: MAX absent (6+6), stdlib `InlineArray` skew (16+1), parse failure (1) | S4; INC-L003-01/02 |
| 4 | Prebuilt Aesir runs on the Pi | **BLOCKED** - x86-64 only; box64 attempt exit 255 | S1; INC-L003-03 |
| 5 | Wiring proof in Aesir idiom + explicit `mimir_*`, binding grade | **PROVEN, explicit adoption only** - 22/22 checks, exactly 4 bindings, exact mapped delta, control fails | S6-S10 |
| 6 | Aesir itself allocates through MimirAlloc | **NOT CLAIMED, NOT TRUE on present evidence** - zero `mimir_*` references in Aesir source; no boundary hook (B5-class) exists in the source inspected | S2 B-d |
| 7 | Phase 12 exit comparison (Qwen3-1.7B Q4_K_M, same-hash baseline vs comparison) | **OWED** - model absent from the Pi; checkout `model.gguf` files are 24-byte stubs | S1; INC-L003-04 |
| 8 | `make test` regression | **PASS** - exit 0, 4590 run / 0 failed | S11 |

## 2. Aesir's actual allocation boundary (source-proven)

Aesir allocates through **three routes and no fourth**.
There is no allocator hook, no interposition point, and
no MimirAlloc reference anywhere in the source. All
citations: `/home/pi/c2/aesir_engine`, recorded in S2.

### B-a. The stdlib arena route (the engine's CPU workspace)

- Import: `core/mimir_well.mojo:8` -
  `from std.memory.alloc import alloc, dealloc, Layout`.
  This is Mojo's standard-library allocator surface.
- The arena: `struct MimirWell` (`core/mimir_well.mojo:
  1353`). Its constructor (line 1364) makes ONE stdlib
  allocation (line 1369):
  `alloc(Layout[Scalar[f16]](count=self.capacity))`,
  leaked into a raw pointer (`unsafe_leak`), zeroed,
  and then carved by bump offset in `allocate`
  (line 1375): bounds-checked `base_ptr.unsafe_offset
  (self.offset)`, offset advanced, exhaustion raises
  `MimirWell: memory pool exhausted`. Release is
  whole-pool only: `__deinit__` calls
  `base_ptr.unsafe_free()` (line 1442); `reset()`
  recycles by zeroing the offset. There is no per-block
  free inside the Well.
- Sizing: `aesir.mojo:29-50`
  (`calculate_runtime_pool_bytes`) derives the pool from
  model dimensions (converted norms + KV cache + layer
  workspace + knowledge store, in f16 bytes);
  `aesir.mojo:266` constructs `MimirWell(mimir_depth_
  bytes)`. The pool therefore covers, by construction:
  converted weights workspace, KV cache, per-layer
  prefill/decode scratch, and the knowledge store.
- Carve consumers (S2 B-c): inference activations
  (`core/inference.mojo:257,271-273,332,378,381,401`),
  KV cache (`core/mimir_well.mojo:856-857`), GPU/NPU
  buffer descriptors (lines 606, 680 - CPU-resident by
  their own docstrings; see B-e), the vector store
  (lines 1464, 1476), tensor shards (line 1800), and
  GGUF f32-to-f16 conversion copies
  (`loader/gguf.mojo:538`, `pool.allocate`).
- Direct stdlib allocations also occur OUTSIDE the
  Well, in the same idiom: the paged KV pool allocates
  its K and V blocks directly
  (`core/mimir_well.mojo:1054-1055, 1129-1130`), a
  shard fallback allocates directly (line 1832), and
  tree-wide there are 37 `alloc(Layout` call sites
  (core, cli, server, loader, tests; S2). Any route
  that replaced only the Well's one allocation would
  still miss these.

### B-b. The mmap weight route (model loading)

- `loader/gguf.mojo:398` `_open_and_map`: `open64` +
  `fstat` via `external_call`, then line 424:
  `external_call["mmap", Int](0, file_size, PROT_READ
  (1), MAP_SHARED (1), fd, 0)` - the GGUF file is
  mapped read-only and shared; weights are consumed as
  views into the mapping (`loader/gguf.mojo:533`) or
  converted into the Well (line 538). Teardown:
  `_cleanup` (line 686) `munmap`s (line 689) and
  closes the fd. This is L-002 class **W** (mmap
  weight residency): kernel-owned, never allocator-
  managed, and no allocator integration can or should
  claim it.

### B-c. The implicit stdlib heap route (everything else)

- Tokenizer tables, GGUF metadata dictionaries,
  strings, chat templates, server and CLI state are
  ordinary Mojo `List` / `Dict` / `String` values
  (e.g. `loader/gguf.mojo:180-182` tensor dictionaries;
  tokenizers under `loader/`). These allocate through
  the Mojo runtime/stdlib allocator implicitly, per
  value, with no single boundary in Aesir source to
  wire. `core/inference_memory.mojo` only ACCOUNTS
  for buffers (checked byte arithmetic, fit
  explanations); it allocates nothing (S2 B-e: no
  allocation call in the file).

### B-d. What is NOT there

- `grep -rn mimir_malloc|mimir_calloc|mimir_realloc|
  mimir_free|libmimiralloc` over the whole Aesir tree:
  **0 lines** (S2). No FFI call, no link reference, no
  configuration switch names MimirAlloc.
- No documented allocator hook exists in the source
  (the Phase 6 B5 finding stands for this tree): the
  only documented substitution surfaces are the ones
  Phase 6 already catalogued - explicit calls (B1)
  and link placement (B2) - which is why Section 4's
  proof has exactly that shape.

### B-e. GPU / NPU (L-002 class G)

- `MimirWell.allocate_gpu_buffer` /
  `allocate_npu_buffer` (`core/mimir_well.mojo:
  1387-1403` region) return, by their own docstrings,
  CPU-resident host descriptors; no device allocation,
  mapping, or dispatch occurs. Real device execution
  in the tree goes through the MAX/CUDA modules
  (`core/gemma4_cuda.mojo`, `core/llama3_cuda.mojo`,
  `from max.gpu.host import ...`) - which are exactly
  the modules that cannot build here (INC-L003-01).
  On this Pi, class G is therefore **unreachable**, as
  L-002's taxonomy already records.

### Workload map (roadmap Phase 12 tracking list)

| Phase 12 stage | Route | L-002 class | Wirable today? |
|---|---|---|---|
| Model loading (weights) | mmap of GGUF file (B-b) | W | No - kernel-owned by design |
| GGUF metadata, tokenizer | implicit stdlib heap (B-c) | C2 | No single boundary |
| KV cache | Well carve (B-a) + direct stdlib blocks (B-a) | C2 | Only by source patch to Aesir |
| Prompt prefill / decode scratch | Well carve (B-a) | C2 | Only by source patch to Aesir |
| Teardown | whole-pool `unsafe_free`, `munmap` | C2 / W | n/a |
| GPU / NPU buffers | descriptors only on CPU path (B-e) | G unreachable | No |

"Wirable only by source patch" is precise: the one
place a future integration could honestly attach is
the Well's single backing allocation (line 1369) plus
the direct `alloc(Layout` sites - INSIDE Aesir source,
which is out-of-repo, divergent across checkouts
(INC-L003-07), and not buildable at engine scale here
(INC-L003-01/02). Proposing that patch is future work
for a canonical checkout on a supported device; it is
not claimed by this slice.

## 3. The bounded aarch64 road (what was attempted)

Toolchain: Mojo 1.1.0 via the pixi wrapper
(`/usr/local/bin/mojo` -> `pixi run --manifest-path
/home/pi/mojo-dev/pixi.toml mojo`), environment
`linux-aarch64`. Every attempt bounded, one at a time;
full outputs in the recorded run.

| Attempt | Command (bounded) | Exit | Result |
|---|---|---|---|
| Prebuilt binary | `timeout 15 ./aesir_main --help` in `/home/pi/c2/aesir_engine` | 255 | box64 binfmt intercepts the x86-64 ELF and fails: `Error: Loading needed libs in elf` (S1) |
| Small component | `timeout 120 mojo build tests/test_memory_refinements.mojo -o /tmp/l003_aesir_memref` (in `aesir_engine`) | 0 | ARM aarch64 ELF, BuildID e501ac89...; run `timeout 30` exit 0: `MimirWell allocation telemetry & arena recycling: PASS`, `Profiled linear BPE tokenizer merge performance: PASS` (S3) |
| Full engine | `timeout 180 mojo build main.mojo -o /tmp/l003_aesir_main_arm` (in `aesir_engine`) | 1 | 30 errors: 16x `package collections does not contain InlineArray`, 1x unknown `InlineArray`, 6x `unable to locate module max`, 6x `gpu does not refer to a nested package`, 1x parse failure (S4) |

Reading: the Pi's Mojo CAN compile and run real Aesir
code at component scale today - S3 is, to this
campaign's knowledge, the first native aarch64 Aesir
run recorded on the Pi. The engine as a whole is
blocked by its dependency envelope (MAX, x86-64-only
pixi platform) and by stdlib version skew, not by
anything in MimirAlloc and not by the Pi's capacity
(loads stayed light; 13 GiB available throughout).

## 4. The wiring proof that WAS earned (S5-S10)

`tests/mojo/test_l003.mojo` (300 lines, original to
this repository) has two strictly separated parts:

- **Part A - the Aesir idiom, honestly labelled.** A
  miniature arena (`L003Well`) written line-for-line
  in the MimirWell idiom of Section B-a: one
  `alloc(Layout[Scalar[f16]])` + `unsafe_leak`,
  bump carve, exhaustion raise, reset recycle.
  Checks A1-A7 (capacity, carve offsets, disjoint
  patterns, byte accounting, exhaustion, reset) all
  PASS. Its bytes are serviced by the **Mojo stdlib
  allocator**; the program says so in its own output.
  Part A proves the idiom runs natively here; it is
  a re-implementation for probing, NOT Aesir code
  compiled from the checkout - the genuine-component
  evidence is S3 (INC-L003-06).
- **Part B - explicit adoption at binding grade.**
  The same program calls the four `mimir_*` symbols
  explicitly (B1) with `libmimiralloc.so` on its link
  via `mojo build -Xlinker` flags (B2; exact command
  in S6, build exit 0). Evidence, F-005 shape:
  serving descriptors validate for a span-served
  64-byte block, a calloc block, and a mapping-served
  8192-byte block (B4, B8, B12); mapped bytes move
  exactly `page_round_up(page + 8192)` = 32768 on the
  large allocation and return to the quiescent
  baseline on its free (B5, B6); pattern, zeroing,
  and realloc-preservation checks pass (B9, B11,
  B14); the final mapped total stands at baseline +
  exactly one 16384 page - the retained empty span of
  the realloc-grown class, the G-003 retention shape
  F-005 also recorded. `readelf -d` shows
  `DT_NEEDED [libmimiralloc.so]` (S9).
  `LD_DEBUG=bindings` shows **exactly 4** binding
  lines to `libmimiralloc.so`, one per `mimir_*`
  symbol (S10). The control build (identical source
  and flags minus `-lmimiralloc`) fails exit 1 with
  undefined references to the four `mimir_*` symbols
  at nine call sites and nothing else (S7) - the
  link is load-bearing and no other library in the
  toolchain provides these symbols.

Scope, once more: Part B is the Phase 6 B1+B2
boundary exercised by a program written in Aesir's
allocation idiom. It demonstrates that IF a Mojo
program on this stack adopts MimirAlloc explicitly,
the adoption is real and provable at binding grade.
It does not patch, wrap, or run Aesir, and no Aesir
allocation was serviced by MimirAlloc in this slice.

## 5. What blocks full integration, and what unblocks it

| Blocker | Evidence | Unblock (any one path) |
|---|---|---|
| No aarch64 MAX: Aesir CUDA/MAX modules cannot compile; Aesir `pixi.toml` is `linux-64` only, `max >=26.5,<27` | S4 (12 of 30 errors); S1 platform lines | An aarch64 MAX build in the Mojo environment, OR an upstream Aesir build configuration that excludes the MAX/CUDA modules for CPU-only targets |
| Stdlib skew: source imports `InlineArray` from `std.collections`; installed Mojo 1.1.0 stdlib does not provide it there | S4 (17 of 30 errors, incl. `loader/gguf.mojo:4` - the model loader itself) | Align the Aesir source to the installed stdlib, OR install the Mojo version the source was written against, then re-run the S4 command |
| No runnable binary: all checkouts carry the same x86-64 `aesir_main`; box64 cannot load its libraries | S1 | A native build (above), OR run Aesir on a supported x86-64 device - the roadmap's comparison is explicitly "on supported devices" |
| No comparison model: Qwen3-1.7B Q4_K_M absent; checkout GGUFs are 24-byte stubs; only real GGUF on the Pi is TinyLlama-1.1B Q4_0 (637,699,456 bytes) | S1 | Place a real Qwen3-1.7B Q4_K_M GGUF on the device and record its hash; TinyLlama could serve a NON-exit smoke only, never the exit comparison |
| No canonical Aesir checkout: the eight checkouts diverge (`core/mimir_well.mojo` md5 differs across c2 / c10work / aesir-finish-line) | Section 2 note; INC-L003-07 | Designate one checkout canonical before any source-level integration patch is proposed |

## 6. Phase 12 exit assessment

Phase 12 exit requires the same model hash, prompt,
context, thread count, backend and thermal state
across baseline and comparison, with output
correctness and responsiveness maintained. **Not
met, and not claimable from this Pi today**: the
comparison model and a runnable engine are both
absent (INC-L003-03/04). What this slice discharges
is the boundary evidence the exit presupposes: where Aesir allocates
(Section 2), through which boundary integration
could ever be verified (B1+B2 explicit adoption,
Section 4), and the first native aarch64 component
run (S3). The exit itself remains OWED to a supported
device with a real model, per L-001's register.

## 7. Incidents and carried items (INC-L003)

- **INC-L003-01**: Full-engine aarch64 build blocked
  by absent MAX package (Aesir pixi is linux-64
  only). Owner: upstream toolchain/Aesir build
  configuration. See Section 5.
- **INC-L003-02**: Stdlib version skew blocks even
  MAX-free files on the engine path (17 errors;
  `InlineArray` in `std.collections`). Owner:
  toolchain alignment. See Section 5.
- **INC-L003-03**: No runnable Aesir binary exists on
  the Pi: eight checkouts, one identical x86-64
  BuildID (833a8ec2136a83afdc56f721a9dab3198f4c6481);
  box64 binfmt present but its library load fails
  (exit 255).
- **INC-L003-04**: Phase 12 exit comparison model
  (Qwen3-1.7B Q4_K_M) absent from the Pi; checkout
  `model.gguf` files are 24-byte stubs. Exit OWED.
- **INC-L003-05** (carries INC-L001-04): the
  MimirWell name collision is a live classification
  hazard - Well bytes are L-002 class C2 (explicit
  non-allocator route via the stdlib), never C1, and
  never MimirAlloc-serviced. Any future Aesir profile
  must classify them so.
- **INC-L003-06**: Part A of the probe is an original
  miniature of the MimirWell idiom, not compiled
  Aesir code; the compiled-Aesir evidence is S3
  alone. Do not cite Part A as an Aesir run.
- **INC-L003-07**: Aesir checkouts on the Pi diverge
  (verified by md5 on `core/mimir_well.mojo` across
  three checkouts). All L-003 citations are to
  `/home/pi/c2` only; a canonical checkout is OWED
  before integration work can target the source.
- **INC-L003-08**: Class G (GPU/NPU) allocations are
  unreachable on the Pi's Aesir path: the Well's GPU/
  NPU buffers are CPU-resident descriptors by source
  admission, and the real device modules are the
  MAX-blocked ones (INC-L003-01).

## 8. Files and reproduction

| File | Lines | Role |
|---|---|---|
| `docs/PHASE_12_AESIR_BOUNDARY_WIRING.md` | this file | Slice record |
| `tests/mojo/test_l003.mojo` | 300 | Wiring probe (Parts A+B) |
| `tests/mojo/recorded_run_l003.txt` | 278 | Recorded run S0-S12 |

Reproduce: `make lib`, build `build/libf005probe.so`
from `tests/mojo/f005_probe.c` (banked F-005 observer,
reused unchanged), then the S6 `mojo build` command,
run `./build/test_l003` (expect `CHECKS: 22 run,
0 failed` and `L-003 RESULT: PASS`), and re-run S10
for the 4 binding lines. `make test` must still exit
0 at 4590/0 (S11).
