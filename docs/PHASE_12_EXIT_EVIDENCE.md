# Phase 12 Exit Evidence Packet (Slice L-009)

Slice: **L-009 (team-09)**. Wave 12, Phase 12 (Aesir
model-workload integration) — the final Wave 12 slice.
Deliverables: this packet and the fresh recorded run
`tests/perf/recorded_run_l009.txt`. Doc + recorded run
ONLY: no `src/`, `Makefile`, `README.md` or
`include/mimir_alloc.h` change (byte-identity verified,
Section 5). Base: `bf95a8ae84ce158dd9f7a7d8df3c786599a62888`
(L-007 bank) on branch `yrsa/mimiralloc-09`, worktree
`/home/pi/mimiralloc-team-09` on Raspberry Pi
`purpleviking1`. Precedent: K-009 / H-009 / I-009 /
J-009 (honest MET / PARTIAL / UNMET / OWED grading;
banking-parallel evidence named as such, never as a
run on this base). No grade below exceeds its banked
evidence.

> **HEADLINE (plain first). PHASE 12 EXIT = NOT MET —
> the controlled-comparison core is OWED.** The exit
> requires a baseline-vs-comparison run of
> Qwen3-1.7B Q4_K_M under six identical controls. That
> run has never happened on any device in this record:
> the model is absent from the Pi (INC-L001-01 /
> INC-L003-04) and no aarch64 Aesir engine exists
> (INC-L003-01/02), so there was nothing to compare.
> What Wave 12 DID prove, at harness grade, is real:
> class-separated (W / C1 / C2 / G / R) profiling of
> model load, GGUF metadata, tokenizer, KV shape,
> prefill, decode and teardown against the real
> TinyLlama-1.1B Q4_0 GGUF on this Pi (L-004..L-006),
> Aesir's actual allocation boundary identified from
> source, and a binding-grade proof that a Mojo
> program in Aesir's idiom can adopt MimirAlloc
> explicitly through its four public symbols (L-003).
> Aesir itself has never allocated through MimirAlloc
> and this packet does not claim it has. The exit
> stays OPEN, owed to a device that holds both the
> model and a runnable aarch64 Aesir, executing the
> complete, gate-validated L-007 protocol unchanged.

Governing text: roadmap Phase 12 body and exit,
verbatim (via L-001 Section 1): body — *Track where
Aesir actually allocates: model loading, GGUF metadata,
tokenizer, KV cache, prompt prefill, decode and
teardown. Separate mmap weight residency, CPU buffers
and GPU allocations. Verify allocator integration
through Aesir's actual boundary and compare
Qwen3-1.7B Q4_K_M on supported devices. Tune
allocations only where profiles show useful benefit.*
Exit — *same model hash, prompt, context, thread
count, backend and thermal state used across baseline
and comparison; output correctness and tablet
responsiveness maintained.*

Evidence map (all banked at this base unless marked):
L-001 foundation plan `f4c590c`
(`docs/PHASE_12_FOUNDATION_PLAN.md`); L-002 taxonomy
`62cbb6f` (`docs/PHASE_12_MEMORY_SEPARATION_TAXONOMY.md`);
L-003 boundary wiring `2f2d8fe`
(`docs/PHASE_12_AESIR_BOUNDARY_WIRING.md`); L-004
load/metadata `27fa612`
(`docs/PHASE_12_LOAD_METADATA_TRACKING.md`,
`tests/perf/recorded_run_l004.txt`); L-005 tokenizer+KV
`05f3714` (`docs/PHASE_12_TOKENIZER_KV_PROFILE.md`,
`tests/perf/recorded_run_l005.txt`); L-006
prefill/decode/teardown `03436f3`
(`docs/PHASE_12_PREFILL_DECODE_TEARDOWN_PROFILE.md`,
`tests/perf/recorded_run_l006.txt`); L-007 Qwen3
protocol + owed record `bf95a8a` (this base,
`docs/PHASE_12_QWEN3_COMPARISON_PROTOCOL.md`,
`tests/perf/recorded_run_l007.txt`); L-008 tuning
evaluation — **banking-parallel**: commit
`72d6a3111028aea25c3052f3c984dda951dc3b82` on branch
`yrsa/mimiralloc-08` (parent `03436f3`), cited by its
keeper-verified figures in Section 3, NOT banked at
this base and NOT re-run here; the keeper reconciles
the citation at this packet's banking.

## 1. Phase 12 body tasks — graded

| # | Roadmap task | Grade | Banked evidence |
|---|---|---|---|
| T1 | Track model loading | **PARTIAL** | L-004: harness-grade, class-separated load profile on the real TinyLlama GGUF (W Size 622,768 kB; metadata + touch moves W Rss to 180,288 kB; 11/11 gates, 3 reps spread 0). Aesir-engine loading NOT measured — engine cannot build/run (INC-L003-01/02, INC-L004-01). |
| T2 | Track GGUF metadata | **PARTIAL** | L-004: metadata walk complete from the file itself — GGUF v3, 201 tensors, 23 metadata KVs, metadata ends byte 1,709,436. Same engine caveat as T1. |
| T3 | Track tokenizer | **PARTIAL** | L-005: the REAL Aesir tokenizer component (RuneWeaver BPE) built and ran natively on aarch64, both subtests PASS, child peak RSS 10,960 KiB; real GGUF tokenizer metadata parsed (vocab 32,000); C1 parsed-structure delta exactly 626,919 B. Engine-scale tokenizer figure OWED (INC-L005-01). |
| T4 | Track KV cache | **PARTIAL** | L-005: KV shape SOURCE-PROVEN from Aesir source (one MimirWell allocate for K, one for V, f16); shape at TinyLlama dims MODELLED and harness-measured (total 184,552,632 B, C1 delta exact). Aesir itself allocating KV on this Pi NOT measured, NOT claimed (INC-L005-01, route gap INC-L002-04). |
| T5 | Track prompt prefill | **PARTIAL** | L-006: harness-measured, class-separated (18/18 gates, 3 reps); C1 prefill delta equals harness requested sum exactly. Aesir-engine prefill OWED (INC-L006-01). |
| T6 | Track decode | **PARTIAL** | L-006: 32-step decode harness-measured; per-step KV slices and scratch separated; C1 decode delta exact. Engine decode OWED (INC-L006-01). |
| T7 | Track teardown | **PARTIAL** | L-006: teardown returns recorded per class; one retained span page (bytes_mapped 16,384, G-005 posture) reported not gated (INC-L006-06). Engine teardown OWED. |
| T8 | Separate mmap residency / CPU buffers / GPU allocations | **PARTIAL** | W vs C separation MET: L-002 taxonomy (single-class rule, W/C1/C2/G/R) applied and measured in L-004..L-006. Class G UNMET: G = 0 by absence of any device backend on this Pi in every profile (INC-L002-03, INC-L003-08, INC-L006-04); the GPU leg is owed to a supported device (INC-L001-07). |
| T9 | Verify integration through Aesir's actual boundary | **PARTIAL** | Boundary PROVEN from source (L-003 element 1: three routes, no fourth, no allocator hook). Explicit-adoption wiring PROVEN at binding grade in Aesir's idiom (L-003 element 5: 22/22 checks, exactly 4 bindings, exact mapped delta, control fails). But Aesir itself allocating through MimirAlloc is NOT TRUE on present evidence — zero mimir_* references in Aesir source (L-003 element 6; INC-L001-04: MimirWell is not MimirAlloc). |
| T10 | Compare Qwen3-1.7B Q4_K_M on supported devices | **UNMET — OWED** | Model absent (INC-L001-01 / INC-L003-04 / INC-L007-01); no runnable aarch64 Aesir (INC-L003-01/02/03, INC-L007-02). L-007 delivered the complete mechanically gated protocol + gate validator (4 cases pass as labelled, INC-L007-04); the run itself was never performed. TinyLlama substitution is a different, non-exit comparison and was never folded in (L-002 Section 7). |
| T11 | Tune only where profiles show useful benefit | **MET** (banking-parallel) | L-008 (cited, Section 3): profile-guided evaluation of retention_bound; verdict NO-CHANGE — bound retained at the default 2 because W1 medians overlap and W2 bound 0 measured +47% slower. No allocation was tuned without a profile citation; the rule itself is the deliverable and it held. |

Body tally: **1 MET / 9 PARTIAL / 1 UNMET.** Every
PARTIAL has the same shape and it is stated, not
hidden: the harness-grade and source-grade evidence
is complete and measured; the engine-grade evidence
is owed behind INC-L003-01/02/04.

## 2. Phase 12 exit elements — graded (L-001 Section 6 map)

| # | Exit element | Grade | Evidence |
|---|---|---|---|
| E1 | Same model hash across baseline and comparison | **UNMET — OWED** | No Qwen3-1.7B Q4_K_M exists on the Pi (L-001 S3.2 search; L-007 S2 re-search at bf95a8a: only a Hailo manifest with no blob, INC-L007-03, and 24-byte checkout stubs, INC-L001-03). No legs exist to share a hash. |
| E2 | Same prompt, context, thread count, backend across legs | **UNMET — OWED** | No runnable Aesir (INC-L001-02, INC-L003-03). The L-007 protocol freezes all four controls and gates them mechanically; nothing has run under them. |
| E3 | Same thermal state across legs | **UNMET — OWED** | Control CAPTURE is validated (L-007 gate validator incl. live thermal read 56.8 C, INC-L007-04; the 2 C thermal-start rule is in the protocol). No comparison legs exist whose thermal states could be the same. |
| E4 | Output correctness maintained | **UNMET — OWED** | No comparison output exists. The protocol's output-equivalence gate is validated on synthetic cases only (L-007). |
| E5 | Tablet responsiveness maintained | **UNMET — OWED** | No tablet Aesir run exists (INC-L001-08). Samtal HTTP 200 on the Pi during L-slice runs is a Pi cohabitation gate, NOT a tablet responsiveness claim, and is never graded as one. |
| E6 | Integration verified through the actual boundary | **PARTIAL** | As T9: boundary source-proven + explicit-adoption wiring binding-grade PROVEN (L-003); Aesir itself serviced by MimirAlloc NOT proven and NOT true on present evidence. |
| E7 | Tuning only where profiles show benefit | **MET** (banking-parallel) | As T11: L-008 NO-CHANGE verdict on keeper-verified figures (Section 3). |

Exit tally: **1 MET / 1 PARTIAL / 5 UNMET-OWED.**
**Phase 12 exit verdict: NOT MET.** The phase closes
as a wave with its evidence complete and its exit
OPEN: the single missing thing is the comparison run
itself, blocked by two named preconditions (model,
engine), each with a named unblock path (L-007 S9).
Nothing else in the exit is silently missing — E6 and
E7 are discharged to the grades above, and E1–E5 fail
for one reason only: the run they grade never ran.

## 3. L-008 cited evidence (banking-parallel)

L-008 (tuning evaluation) is cited from its commit
`72d6a3111028aea25c3052f3c984dda951dc3b82` on
`yrsa/mimiralloc-08`, parent `03436f3`, by its
keeper-verified figures — the H-009 / I-009 precedent.
It was NOT re-run on this base. Figures: retention_bound
RETAINED at the default 2 (the W1 medians overlap, so
no benefit is shown; the W2 bound-0 leg measured +47%
slower, a recorded negative result); Samtal answered
154/154 HTTP 200 during its sweep; `make test` 4590
run / 0 failed; no `src/` change. Verdict class:
measured NO-CHANGE — the Phase 12 tuning rule (T11 /
E7) discharged by an evaluation that changed nothing
because the profiles showed no useful benefit.

## 4. Fresh verification run (this base, this slice)

Full record: `tests/perf/recorded_run_l009.txt`,
run on this Pi at base `bf95a8a`. Summary:

- `make clean` exit 0; `make test` exit 0 —
  CHECKS: 4590 run, 0 failed; RESULT: PASS;
  ALLOC/FREE BALANCE 2088/2088 ok.
- `make lib`, then `nm -D --defined-only` on
  `build/libmimiralloc.so`: exactly 4 dynamic
  symbols — mimir_malloc, mimir_calloc, mimir_realloc,
  mimir_free. (The run file records one earlier nm
  attempt before the .so target existed — `make test`
  does not leave it; see INC-L009-01. The corrected
  sequence is the record.)
- Standalone L-series legs at this base, each run
  fresh and recorded with its exit code: run_l004
  exit 0; run_l005 exit 0; run_l006 exit 0 (driver;
  its Aesir KV component leg remains BLOCKED by
  INC-L003-02 and is recorded as owed inside the
  run, exactly as L-006 designed); run_l007 exit 0
  (protocol gates PASS as labelled).
- Cohabitation: Samtal HTTP 200 before AND after;
  free -g: 13 GiB available before and after;
  thermals temp=57.1'C / throttled=0x0 before,
  temp=59.3'C / throttled=0x0 after. No run started
  or ended throttled.

## 5. Byte-identity (packet slices change NO code)

`git diff bf95a8a -- README.md include Makefile src`
= 0 lines, recorded at the end of the recorded run.
README.md, the public header, the Makefile and all
of `src/` are byte-identical to the base. This
packet's only files are this document and the
recorded run.

## 6. Wave 12 tally

| Slice | Bank | Deliverable in one line |
|---|---|---|
| L-001 | f4c590c | Foundation plan; exit checklist mapped; comparison leg found impossible here, owed honestly |
| L-002 | 62cbb6f | W/C1/C2/G/R separation taxonomy + measurement definitions; no run claimed |
| L-003 | 2f2d8fe | Aesir boundary source-proven; small component native aarch64 PROVEN; engine build BLOCKED (30 errors); explicit-adoption wiring binding-grade PROVEN |
| L-004 | 27fa612 | Load + GGUF metadata tracking, harness-grade measured on real TinyLlama |
| L-005 | 05f3714 | Tokenizer (real Aesir component, native) + KV profile (source-proven shape, harness-measured) |
| L-006 | 03436f3 | Prefill / decode / teardown profile, harness-grade measured, class-separated |
| L-007 | bf95a8a | Qwen3 comparison protocol, complete + gate-validated; the run recorded OWED with blocker evidence |
| L-008 | 72d6a31 (banking-parallel) | Tuning evaluation: measured NO-CHANGE, retention_bound retained at 2 |
| L-009 | this packet | Phase 12 exit evidence: exit NOT MET, core OWED, grades + owed register below |

Wave 12 = 9 of 9 slices delivered (8 banked at this
base + L-008 banking-parallel + this packet, which
banks last and completes the wave).

## 7. Reproducibility packet

- Model actually profiled (non-exit): TinyLlama-1.1B
  Chat v1.0 Q4_0 GGUF, 637,699,456 bytes, sha256
  da3087fb14aede55fde6eb81a0e55e886810e43509ec82ecdc7aa5d62a03b556
  at /mnt/yggdrasil/models/tinyllama/ (hash recorded
  in tests/perf/recorded_run_l007.txt).
- Harnesses: tests/perf/probe_l004.c .. probe_l007.c
  with drivers run_l004.sh .. run_l007.sh; raw data:
  tests/perf/recorded_run_l004.txt .. l007.txt and
  recorded_run_l009.txt (this slice).
- Roadmap model (exit): Qwen3-1.7B Q4_K_M — ABSENT;
  no hash exists to record. That absence, with its
  search evidence, IS the reproducibility fact for
  the exit comparison (L-001 S3.2, L-007 S2).
- Canonical Aesir checkout used by L-003..L-006:
  /home/pi/c2/aesir_engine (see INC-L003-07: the
  eight checkouts diverge; c2 is the cited one).

## 8. Carried-owed register (nothing below is closed by this packet)

Phase 12 owed, carried forward OPEN:
- INC-L001-01 / INC-L003-04 / INC-L007-01: the
  Qwen3-1.7B Q4_K_M comparison run. Unblock: model
  placed on a device by a legitimate route at
  Volmarr's word + a runnable engine there; the
  L-007 protocol then executes unchanged.
- INC-L001-02 / INC-L003-01/02/03 / INC-L007-02: no
  runnable aarch64 Aesir engine (MAX absent; stdlib
  InlineArray skew; prebuilts x86-64 only, box64
  exit 255).
- INC-L001-07 / INC-L002-03 / INC-L003-08 /
  INC-L006-04: class G (GPU/device) never measured —
  no backend on this Pi; owed to a supported device.
- INC-L001-08: tablet responsiveness leg — no tablet
  Aesir run exists; not replaceable by a Pi claim.
- INC-L002-04 / INC-L006-03: Aesir's real KV /
  prefill / decode allocation ROUTE at engine scale
  remains a route-evidence gap behind the engine
  blockers.
- INC-L003-07: eight Aesir checkouts diverge; one
  canonical checkout is owed before any source-level
  integration patch is proposed.
- INC-L005-03: re-running Aesir's own test_kv_cache
  natively is owed (blocked by the same skew).

Earlier phases, carried forward UNMET (unchanged by
Wave 12, restated so they are never lost):
- **Phase 3 exit — UNMET.** The x86-64 suite run in
  full remains owed (carried since the Phase 3
  packet; restated in the Phase 11 packet, K-009).
- **Phase 4 exit — UNMET.** The physical Android
  device run owed since Phase 4 remains owed.
- INC-I001-14: budget enforcement does not exist in
  the allocator (D-FAIL-BUDGET is 0-by-absence);
  carried, not discharged.
- INC-I009-01: `docs/SUPPORTED_TARGETS.md` is stale
  relative to the banked target evidence; carried,
  not discharged.

## 9. New items surfaced by this grading (INC-L009)

- INC-L009-01 (harness observation, minor): `make
  test` does not leave `build/libmimiralloc.so` in
  the tree, so an nm -D check run immediately after
  the suite finds no file. The ABI check requires an
  explicit `make lib` first. Recorded in the run
  file; no Makefile change is made by this packet —
  owed to any slice that owns the build surface.
- INC-L009-02 (map divergence, record honesty): the
  L-001 Section 5 slice map described L-004 as the
  model-acquisition leg and L-005..L-007 as baseline
  / integration / comparison RUNS. Because both
  preconditions failed, the delivered L-004..L-007
  are the harness-grade profiles and the protocol +
  owed record instead — each slice recorded the
  substitution at its own head. The L-001 map must
  not be followed literally for what Wave 12
  delivered; this packet's Sections 1-2 and the
  tally in Section 6 are the accurate map (same
  class as INC-L001-06).

*End of L-009. Deliverables: this document and
tests/perf/recorded_run_l009.txt. No model run is
claimed that was not run; no grade exceeds its
banked evidence; Phase 12 exit stands NOT MET with
its comparison core OWED to named preconditions.*
