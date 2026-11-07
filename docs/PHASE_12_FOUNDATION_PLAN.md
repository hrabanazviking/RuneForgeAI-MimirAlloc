# Phase 12 Foundation Plan - Aesir Model-Workload Integration (L-001)

Slice: L-001 (team-01). Wave 12, Phase 12 (Aesir model-workload
integration). Base: b18244741ee8e457ac9d87a3a2c4b1b4458b69b2
(Wave 11 COMPLETE) on branch yrsa/mimiralloc-01, worktree
/home/pi/mimiralloc-team-01 on Raspberry Pi purpleviking1.
Doc-only slice: no code, harness, Makefile, README or header
change. Every statement below is either traced to a banked
document at this base or to a fresh on-Pi check run and
recorded in this slice (Section 3). Nothing is claimed as
measured that was not run and logged on the Pi.

Evidence grades used in this document:
- BANKED: present in a banked docs/ file at this base.
- L001-CHECKED: a command run on the Pi in this slice, with
  the exact command and its result recorded in Section 3.
- OWED: no evidence exists yet; a named future slice owes it.

Headline findings (details in Sections 2 and 3):
1. No Aesir run exists in the banked record and none was
   performed in this slice. No banked record contains an
   Aesir allocation size, lifetime or count (K-003).
2. Aesir Mojo source IS present on the Pi in several
   out-of-repo checkouts, but every Aesir binary found is
   x86-64 and the Pi is aarch64, and every model.gguf in
   those checkouts is a 24-byte stub. There is no runnable
   Aesir and no real model in the checkouts.
3. No Qwen3-1.7B Q4_K_M GGUF exists anywhere searched on
   the Pi. The only real GGUF models on the Pi are
   TinyLlama-1.1B files under /mnt/yggdrasil/models.
4. The only MimirAlloc integration boundary ever proven is
   the Phase 6 B1+B2 route for a generated Mojo program
   making its own explicit mimir_* calls. Aesir itself has
   never been shown to allocate through MimirAlloc, and
   Aesir carries its own internal arena named MimirWell,
   which is not MimirAlloc (see INC-L001-04).

## 1. Roadmap Phase 12, restated exactly

Source: docs/MimirAlloc_Research_and_Development_Roadmap.md,
Phase 12 heading and body, copied verbatim:

### Phase 12 - Aesir model-workload integration

Track where Aesir actually allocates: model loading, GGUF
metadata, tokenizer, KV cache, prompt prefill, decode and
teardown. Separate mmap weight residency, CPU buffers and
GPU allocations. Verify allocator integration through
Aesir's actual boundary and compare Qwen3-1.7B Q4_K_M on
supported devices. Tune allocations only where profiles
show useful benefit.

Exit, verbatim:

Exit: same model hash, prompt, context, thread count,
backend and thermal state used across baseline and
comparison; output correctness and tablet responsiveness
maintained.

The body and the exit line above are character-exact,
including the exit list of six controlled variables
(model hash, prompt, context, thread count, backend,
thermal state) and its two maintained properties (output
correctness, tablet responsiveness).

Roadmap context that binds this phase:
- Section 11 benchmark class 7: Aesir load, prefill,
  decode and unload on the same model/configuration.
- Section 11 method: fresh process runs, pinned versions,
  thermal state recorded, median plus p95/p99 plus
  outliers, peak RSS/PSS, retained memory after freeing,
  mapping count, startup time; raw data, harness revision
  and negative results stored.
- Exit-proof rule (roadmap Section 12 area): Aesir
  integration, if claimed, is demonstrated on an actual
  workload with identical model/configuration and
  validated output.

## 2. Inventory from BANKED evidence only

### 2.1 The integration-layer map (A-007)

docs/INTEGRATION_LAYER_MAP.md, Layer L5 (Aesir-specific
allocation): status Blocked/Unverified. Findings held:
- Aesir material in ~/aesir-build is Python/source-level
  only (arms*.py files); no ELF linkage to inspect.
- No Aesir ELF built for ARM was found; the Aesir-named
  binaries found (~/c2/aesir_main, ~/c9base/aesir_main)
  are x86-64 ELFs.
- No Android process captured; no Android NDK on the Pi
  (ANDROID_NDK_HOME empty). The Android row is separate
  and never merged with the Linux-host row.
- What would change the verdict: an Aesir engine built
  for AArch64 Linux, captured running, symbol-inspected
  (Linux row), or an Aesir-bearing Android process
  inspected with NDK tools (Android row).

### 2.2 Phase 6 Mojo boundary lessons (F-series)

docs/PHASE_6_FOUNDATION_PLAN.md (F-001), F-003, F-005 and
docs/PHASE_6_EXIT_EVIDENCE.md (F-009) jointly hold:
- B1+B2 PROVEN: a generated Mojo executable can call the
  four namespaced symbols mimir_malloc, mimir_calloc,
  mimir_realloc, mimir_free explicitly via std.ffi
  external_call (B1), with libmimiralloc.so placed as a
  DT_NEEDED by documented mojo build -Xlinker flags (B2).
  F-005 raised this to binding grade (LD_DEBUG bindings,
  audit counters, mapped-bytes accounting).
- Scope limit, stated in every F doc: this proves only a
  generated programs own explicit allocations. It does
  not touch the Mojo compiler process, the Mojo stdlib
  internals, or any Aesir process.
- B5 (a documented Mojo allocator hook): absent in the
  banked sources; mojo build --help shows no allocator
  option. B6 (substituting the allocator inside the Mojo
  compiler process): BLOCKED (prebuilt stripped binary,
  no source checkout or Bazel road on the Pi).
- B3/B4/B7 (standard-name interposition, link wrapping,
  runtime substitution): documented-only; each needs a
  standard-name surface that does not exist in this tree.
- X1 (the original Phase 1 failing command): BLOCKED;
  Phase 6 exit is UNMET via X1. No F-slice result is an
  Aesir result.
- F-001 INC-F001-11 and F-003 Section 2.1 already noted
  Mojo source under /home/pi/c2 (an Aesir checkout, for
  example aesir_engine/*.mojo) existing outside the repo
  with no MimirAlloc provenance record. F-003 used those
  programs only as evidence of FFI syntax; no Aesir code
  was copied into this repository.

### 2.3 Phase 11 traces and the D-series

docs/PHASE_11_WORKLOAD_TRACES.md (K-003), Section 4:
- The Aesir-like trace is labelled MODELLED and mostly
  OWED to Phase 12. Quote-grade summary: no banked record
  contains an Aesir allocation size, lifetime, or count;
  the comparator doc cautions its probes prove nothing
  about Aesir; no Aesir consumer is proven at that base.
- Shape hypotheses held for Phase 12 to measure (not
  observations): load = very large long-lived weight-
  adjacent allocations, much of it mmap residency that is
  NOT allocator traffic; prefill = burst of medium/large
  activation and KV allocations; decode = small-to-medium
  per-token churn beside a growing long-lived KV cache;
  teardown = near-total release.
docs/PHASE_2_EVALUATION_PLAN.md and the comparator docs:
- Class 7 (Aesir) may be run only where integration is
  proven for that mode. No Aesir result exists in the
  Phase 2 record.
D-series (Phase 8 concurrency, for example D-004) and all
other banked slices: synthetic or Mojo-compile workloads
only. None names an Aesir process, model, prompt, KV
cache, or GPU allocation. There is no D- or F-slice
Aesir measurement to inherit.

### 2.4 What the Aesir allocation boundary actually is, on banked evidence

Held answer, with grades:
- The MimirAlloc public boundary is the four-symbol
  namespaced C ABI in include/mimir_alloc.h (BANKED).
- The only proven crossing of that boundary by any Mojo
  program is explicit adoption: the program itself calls
  mimir_* (BANKED, Phase 6 B1+B2, binding grade F-005).
- No banked evidence shows Aesir, in any checkout or
  process, calling mimir_*, linking libmimiralloc, or
  being serviced by MimirAlloc in any mode (LD_PRELOAD,
  direct link, static, namespaced). Grade: NOT PROVEN,
  and the L5 layer verdict remains Blocked/Unverified.
- Therefore Phase 12 integration must be verified
  through the actual boundary Aesir presents when it is
  built and run (L-003), not assumed to be the Phase 6
  generated-program boundary.

## 3. Fresh on-Pi presence check (L-001, 2026-11-07)

All commands run on purpleviking1 in this slice. Pi state
at start: free -g showed 13 GiB available; Samtal on port
8787 returned HTTP 200. uname -m: aarch64.

### 3.1 Aesir code: PRESENT as source, ABSENT as a runnable binary

Command: ls -ld /home/pi/c2 /home/pi/c2/aesir_engine
/home/pi/aesir-build /home/pi/c9base
Result: all exist. /home/pi/c2/aesir_engine holds Mojo
source (aesir.mojo, cli/, core/, loader/, server/, tests/
and more). At least eight parallel checkouts carry an
aesir_engine tree: c2, c9base, c7f1, c9work, c10work, c3,
mimisbrunnr_c8, aesir-finish-line (find, maxdepth 3).
/home/pi/aesir-build holds Python arms*.py material, as
A-007 recorded.

Command: file /home/pi/c2/aesir_engine/aesir_main
/home/pi/c2/aesir_main /home/pi/c9base/aesir_main
Result: every one is an ELF 64-bit x86-64 pie executable.
No AArch64 Aesir binary was found. On this aarch64 Pi
these binaries cannot run. Grade: L001-CHECKED.

Source-level allocation sites visible in the c2 checkout
(static reading only; nothing was run):
- loader/gguf.mojo (749 lines): a bounds-checked GGUF v3
  loader with mmap-backed tensor views; it calls mmap and
  munmap via external_call, parses GGUF metadata and
  tokenizer data out of the mapping, and hands tensor
  views as offsets into the mmap.
- loader/tokenizer.mojo (437 lines) and loader/
  llama3_tokenizer.mojo (251 lines): tokenizer code.
- core/inference.mojo (757 lines): inference driver.
- core/inference_memory.mojo (160 lines): explicit byte
  accounting for native CUDA profiles (weights, KV and
  buffer byte counts as arithmetic, not a host run).
- core/mimir_well.mojo: Aesir-internal arena type named
  MimirWell, used across Aesir tests. This is Aesir code,
  unrelated to MimirAlloc; see INC-L001-04.
- CUDA gate/upload modules exist in core/ (cuda_gate,
  cuda_upload, cuda_resources): GPU allocation is a
  source-described path. The Pi has no CUDA backend, so
  no GPU allocation can be measured here.

### 3.2 Models: no Qwen3-1.7B Q4_K_M GGUF on the Pi

Command: find /home/pi -maxdepth 4 -iname *.gguf
Result: exactly eight files, all named model.gguf inside
the aesir_engine checkouts listed in Section 3.1.

Command: ls -lh on each of those files; cat on the c2 copy
Result: every file is 24 bytes. Content is a GGUF header
only (magic GGUF, version 3, zero tensor count, zero
metadata count). These are stubs or placeholders, not
models. A filename must never be read as model presence
(INC-L001-03).

Command: find /home/pi /mnt/yggdrasil for names matching
Qwen3 1.7B patterns, plus a find for *qwen* names
Result: no Qwen3-1.7B GGUF anywhere. The only Qwen3-1.7B
artifact found is a Hailo pull log (hailo10h-twister/
pull-logs-5.3/qwen3_1.7b.jsonl), which is a log, not a
model. Qwen-named files in the Aesir checkouts are Mojo
test/profile sources and a quant verification script.

Command: ls -lh /mnt/yggdrasil/models (the target of the
~/models symlink)
Result: only TinyLlama-1.1B GGUFs: tinyllama Q4_0 (609M),
Q5_K_M (747M), Q6_K (863M) and Q8_0 (1.1G). Real models,
but not the roadmap model, not the roadmap quant.

Conclusion, stated plainly: the Phase 12 comparison leg
(Qwen3-1.7B Q4_K_M, identical hash across baseline and
comparison) cannot be run on this Pi today. It is OWED
to a device that has that model and a runnable Aesir, or
to an acquisition plus build leg (L-003 and L-004) that
creates those preconditions here. No Aesir run is claimed
in this slice.

## 4. Allocation-site tracking plan

For each roadmap site: what must be separated, and the
evidence a measuring slice owes. Categories in every row:
MMAP (weight residency, not allocator traffic), CPU
(host buffers serviced by some allocator), GPU (device
allocations, separate backend accounting).

| Site | Separate | Owed evidence |
|---|---|---|
| Model loading | MMAP file residency vs CPU parse buffers vs GPU upload | per-phase byte counts by category; mapping count; load time |
| GGUF metadata | CPU parse allocations out of the mmap vs the mmap itself | metadata byte/count record traced to loader code paths |
| Tokenizer | CPU vocab/merge structures; lifetime = process or session | sizes, counts, lifetime, teardown release check |
| KV cache | CPU vs GPU placement; growth per token; context-length scaling | growth curve vs tokens; placement record per backend |
| Prompt prefill | CPU activation/workspace churn vs GPU workspace | burst profile; peak RSS/PSS; per-category peaks |
| Decode | per-token CPU churn vs KV growth vs GPU per-token work | per-token allocation counts/sizes; tail latency |
| Teardown | release completeness in all three categories | retained-after-free bytes; mappings after exit; GPU release record |

## 5. Slice map L-002 to L-009, with owed evidence

| Slice | Deliverable | Owed evidence (exit of the slice) |
|---|---|---|
| L-002 | Static allocation-site census of one pinned Aesir checkout: every mmap/munmap, external_call allocation, MimirWell arena use, and container growth site in loader/, core/, server/, with file and line anchors | Census doc; checkout path plus revision/hash pinned; MimirWell sites listed separately from any MimirAlloc surface; zero run claims |
| L-003 | Runnable-Aesir determination: build the pinned Aesir checkout for aarch64 with the on-Pi Mojo 1.1.0 toolchain in small steps, or record the exact blocker | recorded_run with every build command and exit code; on success: ELF file output (aarch64), sha256, BuildID; on failure: an F-008-grade blocker record naming the exact failing step |
| L-004 | Model identity leg: obtain Qwen3-1.7B Q4_K_M GGUF by a legitimate route, or record why it cannot be obtained here | sha256 and byte size of the model file; GGUF metadata dump (architecture, quant, context length); storage location; if not obtained, the named device that has it |
| L-005 | Baseline run (system allocator) on a supported device: Aesir load, prefill, decode, teardown on the L-004 model | recorded_run: model hash, prompt, context, thread count, backend, thermal state; output text; per-site byte counts by MMAP/CPU/GPU category; peak RSS/PSS; responsiveness note where a tablet is the device |
| L-006 | MimirAlloc integration run through the actual boundary L-003 proves: Aesir serviced by MimirAlloc for a named allocation set | binding-grade proof in the F-005 shape (loader bindings or audit counters or mapped-bytes effects) tying serviced allocations to libmimiralloc; explicit list of what was NOT serviced (mmap residency, GPU) |
| L-007 | Comparison run and correctness: identical controlled variables to L-005, MimirAlloc leg against baseline leg | side-by-side table of the six exit variables proving identity; output equivalence check result; tail latency, RSS/PSS deltas; negative results recorded, not dropped |
| L-008 | Profile-guided tuning: change allocations only at sites where L-005 to L-007 profiles show useful benefit | one change at a time, each with before/after measurements under identical variables; null and negative results recorded; no tuning without a profile citation |
| L-009 | Phase 12 exit evidence packet | the Section 6 checklist filled item by item with grades; INC-L001 closure or carry list; reproducibility packet (model hash, harness revisions, raw data locations) |

Order law: L-002 and the L-003/L-004 precondition legs
gate everything. L-005 has no meaning before a runnable
Aesir and the pinned model both exist on one device.
If that device is not this Pi, the packet says so and
the runs are recorded on that device, never implied to
have happened here.

## 6. Exit checklist (verbatim exit, mapped)

Exit text: same model hash, prompt, context, thread
count, backend and thermal state used across baseline
and comparison; output correctness and tablet
responsiveness maintained.

| Exit element | Held by | Grade today |
|---|---|---|
| Same model hash across legs | L-004 pins it; L-005/L-007 record it | OWED (no model, INC-L001-01) |
| Same prompt, context, thread count, backend | L-005/L-007 run records | OWED (no runnable Aesir, INC-L001-02) |
| Same thermal state | L-005/L-007 recorded thermal state per Section 11 method | OWED |
| Output correctness maintained | L-007 equivalence check | OWED |
| Tablet responsiveness maintained | L-005/L-007 on a tablet, or a named tablet leg | OWED (INC-L001-08) |
| Integration verified through the actual boundary | L-003 boundary proof plus L-006 servicing proof | OWED (boundary unproven for Aesir, Section 2.4) |
| Tuning only where profiles show benefit | L-008 profile citations | OWED (no profiles exist) |

Phase 12 grade at L-001: OPEN. Nothing in the exit is
met, and this plan claims nothing toward it beyond the
inventory and the precondition findings.

## 7. Inconsistency register INC-L001

- INC-L001-01 (no roadmap model): no Qwen3-1.7B Q4_K_M
  GGUF exists on the Pi (Section 3.2). The comparison
  leg is owed to a device that has the model, or to
  L-004 acquisition. Blocks L-005 to L-008 here.
- INC-L001-02 (no runnable Aesir): all Aesir binaries
  on the Pi are x86-64; the Pi is aarch64; no AArch64
  Aesir ELF exists (Section 3.1, A-007 L5). Blocks any
  local Aesir run until L-003 resolves build or blocker.
- INC-L001-03 (stub models): every model.gguf in the
  Aesir checkouts is a 24-byte GGUF header stub. Any
  inventory reading filename presence as model presence
  is wrong; this register entry is the correction.
- INC-L001-04 (name collision): Aesir contains its own
  arena named MimirWell (core/mimir_well.mojo). It is
  not MimirAlloc and services allocations internally to
  Aesir. Any future claim that Aesir uses MimirAlloc
  because it uses MimirWell would be false; L-002 must
  keep the two strictly separate.
- INC-L001-05 (A-007 detail superseded, verdict not):
  A-007 described the on-Pi Aesir material as Python/
  source-level, centred on ~/aesir-build. In fact Mojo
  Aesir source checkouts also exist (already noted by
  F-001 INC-F001-11 and used for FFI syntax by F-003).
  The L5 verdict (no ARM ELF, no process, no integration
  proof) is unchanged and re-verified in Section 3.
- INC-L001-06 (orphaned owed list): K-003 assigned the
  Aesir-trace readiness owed list to K-008, but the
  banked K-008 slice is the tail-latency, RSS/PSS and
  energy packet; no banked slice ever took the Aesir
  readiness list. It is adopted here as the L-002 to
  L-004 precondition legs. The K-003 cross-reference is
  stale and should not be followed literally.
- INC-L001-07 (GPU leg has no local backend): Aesir
  source describes CUDA profiles and GPU allocations,
  but this Pi has no CUDA backend and no GPU leg can
  be measured here. The GPU separation leg is owed to
  a supported device with the backend Aesir actually
  uses; on the Pi only MMAP and CPU categories exist.
- INC-L001-08 (tablet leg carried debt): the exit
  requires tablet responsiveness maintained, yet no
  banked Aesir tablet run exists and the Phase 4
  Android exit remains UNMET in the banked record. The
  tablet leg stays owed and must not be replaced by a
  Pi responsiveness claim.
- INC-L001-09 (no Aesir quantities exist): consistent
  with K-003, there is no banked Aesir allocation size,
  lifetime or count anywhere in the tree. The modelled
  shapes in K-003 Section 4 are hypotheses only; no
  Phase 12 document may cite them as measurements.

## 8. Verification record for this slice

Command: make test (in this worktree, at base b182447)
Result: exit 0. Final suite line: CHECKS: 4590 run,
0 failed; RESULT: PASS. (Full log captured during the
slice run; the tail also shows the D-004 balance line
ALLOC/FREE BALANCE: allocs=2088 frees=2088 ok.)
Pi care: free -g showed 13 GiB available before and
after the work; Samtal on port 8787 returned HTTP 200
before the work and remained the standing check for the
slice. No Aesir binary was executed; no model was
loaded; the only runs were the presence checks of
Section 3 and make test.
Scope check: this slice adds only this document.
README.md sha256 at base and in this worktree:
bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702
include/mimir_alloc.h sha256:
55107f05b3eb8c120230f76b2193fdc953af800af57a91caa405c84971af8772
Both verified unchanged (git diff against HEAD: empty)
before commit.
