# Phase 12 Qwen3 Comparison Protocol + Owed Record (L-007)

Slice: L-007 (team-07). Wave 12, Phase 12 (Aesir model-
workload integration). Base: 05f37148980f296c692292dbfd8e0a6e25662e59
(L-005 bank) on branch yrsa/mimiralloc-07, worktree
/home/pi/mimiralloc-team-07 on Raspberry Pi purpleviking1.
Companion files: tests/perf/probe_l007.c,
tests/perf/run_l007.sh, tests/perf/recorded_run_l007.txt.
Disjoint from L-006 files. No README, header, Makefile or
src change.

## 0. Verdict of this slice (read first)

**The comparison run is OWED, not performed.** An
exhaustive fresh search of this Pi on 2026-11-07 (Section 2,
full log in recorded_run_l007.txt) found NO Qwen3-1.7B
Q4_K_M GGUF and no Qwen3 GGUF of any quant anywhere on
the device. This confirms INC-L001-01 / INC-L002-02 /
INC-L003-04; it does not supersede them, it re-verifies
them at this base. No model was downloaded (no download
without Volmarr's word), no run was simulated, and no
comparison figure of any kind is claimed.

What this slice DOES deliver, complete and usable the day
the preconditions land:

1. The full controlled comparison protocol (Sections 3-8):
   exactly how the baseline-vs-MimirAlloc Qwen3-1.7B
   Q4_K_M run executes, under the Phase 12 exit controls
   and the binding L-002 Section 5 protocol.
2. A working gate validator (probe_l007.c + run_l007.sh)
   that mechanically checks a pair of run headers against
   the six exit controls, the 2 C thermal-start rule, the
   not-throttled rule, and byte-identical outputs. All
   four validation cases behaved as labelled in the
   recorded run (PASS / THREADS-DIFF / THERMAL-DIFF /
   OUTPUT-DIFF).
3. The honest owed record with blocker evidence
   (Sections 2 and 9, INC-L007).

A second, independent blocker also stands: no runnable
aarch64 Aesir engine exists on this Pi (INC-L003-01/02;
banked L-003: explicit-adoption proof at binding grade
for a generated program, full Aesir engine build BLOCKED
on aarch64). The protocol's precondition gates (Section 3)
name both blockers; neither is hidden inside the other.

## 1. Governing requirements (banked, traced)

- Roadmap Phase 12 exit, verbatim (via L-001 Section 1):
  "same model hash, prompt, context, thread count, backend
  and thermal state used across baseline and comparison;
  output correctness and tablet responsiveness maintained."
  On this Pi the responsiveness leg is the Samtal service
  (L-002 Section 5.5: HTTP 200 on port 8787 before and
  after each run).
- L-001 Section 5 slice map: L-007 owes "side-by-side
  table of the six exit variables proving identity; output
  equivalence check result; tail latency, RSS/PSS deltas;
  negative results recorded, not dropped".
- L-002 Section 5 (binding measurement protocol): freeze
  exit conditions before the first run (5.1); alternating
  B,C,B,C order, at least three valid runs per arm, median
  + spread (5.2); snapshots only at quiescent points
  S0-S6 (5.3); classification W/C1/C2/G/R mechanically
  re-appliable (5.4); correctness + responsiveness gates
  (5.5); residual R printed, never hidden (5.6).
- L-002 Section 6 tuning rule: tuning may target C1 only;
  a total-RSS change from fewer weight pages touched is a
  workload fact, not an allocator result.
- Roadmap Section 11 method (via L-001): fresh process
  runs, pinned versions, median plus p95/p99 plus outliers,
  peak RSS/PSS, retained after freeing, mapping count,
  startup time; raw data + harness revision + negative
  results stored.

## 2. Model search, 2026-11-07 (this slice, on this Pi)

Commands actually run (full output in
tests/perf/recorded_run_l007.txt; search transcript also
kept at run time in /tmp/l007_search.txt):

1. `find / -iname '*qwen*'` (whole filesystem, errors
   suppressed). Qwen3 hits were ONLY: source files in a
   llama.cpp/ollama harvest checkout (qwen3.cpp etc. -
   code, not a model); NVIDIA NIM skill names;
   `/usr/share/hailo-ollama/models/manifests/qwen3/1.7b`
   (see item 5); and the Hailo pull log
   `hailo10h-twister/pull-logs-5.3/qwen3_1.7b.jsonl`
   (a log, not a model - as already recorded in L-001).
2. `find /home/pi /mnt /opt /srv -iname '*.gguf'`. Every
   Aesir-checkout entry is named model.gguf; the rest are
   small test fixtures under Documents/Codex (none Qwen,
   none above fixture size) and Mojo testdata tiny_llama
   files (16M / 24M).
3. Sizes + sha256 of the checkout GGUFs: all four
   (/home/pi/c2, c7f1, c9base, c9work aesir_engine/
   model.gguf) are **24 bytes**, sha256
   a4e5e156ddec27e286f75328784d7106b60a4eb1d246e950a001a3f944fbda99
   (c2 copy hashed in this slice; banked L-002 hashed the
   same value). Hex dump: GGUF magic, version 3, zero
   tensors, zero metadata. Stubs, not models.
4. Model store `/mnt/yggdrasil/models` (target of
   ~/models): ONLY TinyLlama-1.1B GGUFs, hashed in this
   slice - Q4_0 637,699,456 B sha256 da3087fb14aede55f
   de6eb81a0e55e886810e43509ec82ecdc7aa5d62a03b556;
   Q5_K_M aa54a5fb99ace5b964859cf072346631b2da6109715
   a805d07161d157c66ce7f; Q6_K 9fb2be7c8bbc0ca358ae88c
   ef6d9e6844b81929eaffa5016a543e6f56b22cb6d; Q8_0
   a4c9bb1dbaa372f6381a035fa5c02ef087aaa1ff1f843a56a2
   22328114f03fc59. No file over 10M anywhere else is a
   candidate: `find -iname '*.gguf' -size +10M` returns
   exactly the two Mojo tiny_llama testdata files and
   the four TinyLlama files above.
5. **Hailo manifest is NOT the model.** `/usr/share/
   hailo-ollama/models/manifests/qwen3/1.7b/manifest.json`
   names format "hef", family qwen3, parameter_size 1.7B,
   quantization_level **Q4_0** - a Hailo HEF artefact
   descriptor, not a GGUF and not Q4_K_M. Its hef digest
   cc9b9d1c92e35249b5a9b7bc31fbd652f03bba1232e99b9a8271
   845ad6f17821 has **no corresponding blob file** on
   the device (checked in the hailo-ollama blob store
   and by a *.hef find: the only .hef present is an
   unrelated MiniLM file). A manifest without its blob
   is metadata, not a runnable model, and in the wrong
   format and quant regardless.
6. `find / -iname '*1.7b*' -o -iname '*Q4_K_M*'`:
   remaining hits are pytest fixture files in /tmp with
   names like fixture-7b-Q4_K_M.gguf / alpha--Q4_K_M.gguf
   (synthetic store fixtures, not Qwen3, not 1.7B).

**Search verdict: Qwen3-1.7B Q4_K_M is ABSENT from this
Pi.** Any future claim of presence must come with a path,
a byte size of the expected order (~1.2 GB for a 1.7B
Q4_K_M GGUF), and a sha256 - filename or manifest
presence alone is the INC-L001-03 error and is rejected
by this protocol at gate P0.

## 3. Precondition gates (all must pass before any run)

- **P0 Model identity.** A real Qwen3-1.7B Q4_K_M GGUF
  exists at a recorded absolute path; size and sha256
  recorded; GGUF header parsed (magic GGUF, tensor and
  metadata counts > 0, architecture qwen3, quant Q4_K_M
  per metadata). 24-byte stubs and HEF manifests fail P0.
- **P1 Engine.** A runnable Aesir engine binary for
  aarch64 exists: `file` says aarch64, sha256 + version
  recorded. (Today BLOCKED, INC-L003-01/02.)
- **P2 Adoption route.** The comparison arm's MimirAlloc
  servicing is proven at binding grade by the L-003
  route (LD_DEBUG / audit counters / mapped-bytes),
  with the NOT-serviced set listed (mmap weight
  residency class W, any device allocations class G).
  Baseline arm = same binary, same flags, system
  allocator; the allocator is the ONLY permitted
  difference between arms (L-002 5.1).
- **P3 Host safety.** free -g shows comfortable headroom
  (this Pi: 13 GiB available at this slice's checks);
  Samtal HTTP 200 on :8787; SoC not throttled
  (`vcgencmd get_throttled` = 0x0). Any failure: stop,
  record, do not run.

## 4. Frozen controls (the six exit variables)

Before the first run of either arm, freeze and record in
BOTH run headers (format in Section 8):

| Control | Freeze rule |
|---|---|
| Model hash | P0 path + sha256; identical string in both headers |
| Prompt | One prompt file, created once; its sha256 + token count in both headers; never edited mid-session |
| Context | One context length + KV parameters (the model's recorded context, or a stated smaller fixed value, same both arms) |
| Thread count | One compute thread count + the full command line, same both arms |
| Backend | Same backend name; backend binary path + sha256 + version in both headers; allocator linkage is the only difference |
| Thermal state | Start temp (vcgencmd measure_temp AND thermal_zone0 sysfs) + get_throttled per run; pair starts differ by <= 2.0 C; neither starts throttled |

Fixed generation settings are part of "prompt/context"
for this protocol: greedy decoding (temperature 0) or a
recorded fixed seed, a fixed max output token count, and
the same stop conditions - without them the correctness
gate in Section 6 is meaningless. Any control that
cannot be frozen is named in the header as UNFROZEN with
its reason, and the run is labelled a first look, never
the comparison result (L-002 5.2).

Mechanical enforcement: probe_l007 (this slice) parses
both headers and GATEs same_model_hash, same_prompt,
same_context, same_thread_count, same_backend,
thermal_start_within_2C, neither_arm_throttled_at_start,
all_six_controls_present_both_arms. A comparison whose
headers fail the probe does not count, whatever its
timings say.

## 5. Run procedure

1. P0-P3 pass. Record the session header (device,
   `uname -m`, date, base commit, harness revisions).
2. Samtal check + free -g + thermal capture (P3 values
   re-read immediately before the session).
3. Arms alternate B, C, B, C, ... with **at least three
   valid runs per arm**, fresh process per run, in one
   session on this one device. Between runs, wait until
   SoC temp returns within 2.0 C of the session's first
   recorded start temp; a run that starts hotter is
   postponed, not fudged.
4. Per run, snapshots at the L-002 quiescent points:
   S0 before load, S1 after load, S2 after metadata/
   tokenizer ready, S3 after KV allocation, S4 after
   prefill, S5 steady decode (after the fixed recorded
   token count from the frozen settings), S6 after
   teardown. Each snapshot: full smaps + smaps_rollup +
   status of the measured process, MimirAlloc counter
   snapshot in the comparison arm (and a counter-
   invariance check in the baseline arm), temperature +
   throttling, wall times for load / prefill / decode /
   teardown, per-token decode latencies.
5. **Thermal discipline during a run:** if throttling
   begins mid-run, the run is marked THERMAL-INVALID,
   kept in the raw data, and excluded from medians; it
   is evidence about thermals, not the allocator
   (L-002 5.1). Three THERMAL-INVALID runs in a session
   end the session; reschedule cooler.
6. Responsiveness sampling: Samtal HTTP status + a
   simple request latency sample recorded before the
   session, after each run, and after the session.
   Any non-200 is a gate failure for that run (Section 6).
7. Runs stay SMALL and bounded: the frozen output token
   count is the minimum that reaches steady decode past
   S5 (hundreds of tokens, not thousands); one session
   is six-to-eight runs total. No soak, no sweep inside
   the comparison session - sweeps are separate slices.

## 6. Correctness and responsiveness gates (both arms)

- **Output correctness:** generated output must be
  byte-identical between arms for the frozen prompt and
  settings (probe_l007 outputs_byte_identical gate on
  the saved output files). If the backend is shown
  non-deterministic under the frozen settings, the
  fallback is a stated equivalence (same token sequence
  read back from the backend's own log) WITH its
  evidence - never an eyeballed "looks the same".
  A faster or smaller run with wrong output is a
  failure, not a result (L-002 5.5).
- **Adoption correctness (comparison arm only):** P2
  binding proof re-captured per session, plus the
  allocator counters reconcile with the C1 class
  figures (Section 7) within their own spread.
- **Responsiveness:** Samtal HTTP 200 before and after
  every run (Section 5 step 6). A run during which the
  host stopped answering is invalid as comparison
  evidence and is recorded as a negative result.

## 7. Metrics, per L-002 classes (reported per snapshot)

Per snapshot S0-S6, per arm, the L-002 Section 5.4 table:
rows **W** (mmap weight residency), **C1** (MimirAlloc-
serviced, comparison arm; baseline arm reports the
counter-invariance check instead), **C2** (other explicit
CPU routes incl. stdlib heap), **G** (device/backend
allocations from the backend's OWN report - on this Pi
no GPU backend runs, so G is expected 0/absent and any
non-zero G needs its backend report attached), **R**
(residual, printed always); columns Size, Rss, Pss (kB);
counter values in their own separate block. Figures are
deltas against S0 as well as absolutes.

Per-phase derived metrics (median of valid runs, with
min/max spread; p95/p99 for per-token decode latency):

- load: wall time, W Size/Rss at S1, mapping count delta;
- metadata/tokenizer: C1+C2 delta S1->S2, by route;
- KV: C-class delta S2->S3 + bytes per context token;
- prefill: wall time, peak Rss/Pss at S4;
- decode: per-token latency median/p95/p99, C-class
  growth S4->S5, tokens/sec;
- teardown: retained Rss/Pss at S6 vs S0, live counter
  values returning to baseline (the L-004/L-005 teardown
  gates' shape), mappings after exit.

Comparison reporting: side-by-side per-arm medians +
spreads for every metric above; the six-control identity
table (L-001's L-007 owed evidence); correctness and
responsiveness gate results; ALL negative and invalid
runs listed with their marks (THERMAL-INVALID, gate
failures) - recorded, not dropped. Conclusions only
from W/C/G movements exceeding their own spreads
(L-002 5.6). No tuning is done in the comparison
session (L-002 Section 6; tuning is L-008's, profile-
cited).

## 8. Recording format

One recorded_run file per session, in the established
tests/perf/recorded_run_*.txt shape:

```
L-007 comparison recorded run - <date>
<base commit> / <branch> / aarch64 / nproc
--- precondition gates P0-P3 --- (each: command + result)
--- frozen controls --- (both headers, verbatim)
--- probe_l007 header validation --- (all GATE lines)
--- per run i (arm B/C, order as run) ---
header: model/prompt/backend sha256, context, threads,
  temp_start_c, throttled, full command line
snapshots S0..S6 (Section 7 table + counters)
times, per-token latencies, output file sha256
samtal_after: <code>  temp_end, throttled_end, marks
--- summary --- (medians/spreads, gate verdicts,
  negative/invalid list, INC updates)
```

Raw smaps/status captures are stored beside the recorded
run, named by session, run and snapshot, so the L-002
5.4 classification can be re-applied by a reader.

## 9. Owed record + unblock path (INC-L007)

- **INC-L007-01 (OWED RUN, blocker INC-L001-01):** the
  Qwen3-1.7B Q4_K_M comparison itself. Evidence:
  Section 2 search at this base. Unblock: the model is
  placed on this Pi by a legitimate route **at
  Volmarr's word** (this slice was forbidden to
  download), P0 records its hash, and this protocol
  executes unchanged. TinyLlama substitution would be
  a different, non-exit comparison and is NOT folded
  into Phase 12 exit evidence (L-002 Section 7).
- **INC-L007-02 (OWED ENGINE, blocker INC-L003-01/02):**
  no runnable aarch64 Aesir engine; even with the model
  present, P1 fails until an aarch64 build exists.
  The protocol is engine-agnostic about HOW P1 is met
  but not about WHETHER it is met.
- **INC-L007-03 (MANIFEST TRAP, new evidence):** a
  Hailo qwen3/1.7b manifest (HEF, Q4_0) exists on this
  Pi with no blob present. Recorded here so no future
  inventory reads it as model presence - same class of
  error as INC-L001-03 (stub GGUFs).
- **INC-L007-04 (PROTOCOL VALIDATED, RUN OWED):** the
  gate validator's four cases pass as labelled (see
  recorded run), including live thermal capture on this
  host (56.8 C read from thermal_zone0 during the
  validation runs). This validates control CAPTURE
  only; it is not a comparison result and claims no
  allocator performance anything.

Phase 12 exit grade after L-007: still OPEN, now with a
complete, mechanically gated protocol ready to execute
the moment P0 and P1 can both pass on one device.

*End of L-007. Deliverables: this document,
tests/perf/probe_l007.c, tests/perf/run_l007.sh,
tests/perf/recorded_run_l007.txt. No model run is
claimed; nothing was downloaded; no README, header,
Makefile or src file was changed.*
