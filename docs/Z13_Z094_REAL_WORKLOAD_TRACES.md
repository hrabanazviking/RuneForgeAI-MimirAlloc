# Z-094 - Real workload traces

Roadmap slice Z-094 (Wave 11): allocation traces
captured from real programs on this host (audit-hook
route): python import storm, git object churn, an
Ollama session; a replay harness that re-runs a trace
against any allocator leg. Gate: three traces in-tree
with replay checksums matching.

VERDICT: gate MET. Three canonical traces are in-tree
under tests/perf/traces/, and `make test-z094` replays
each of them on BOTH legs (glibc control and the
MimirAlloc namespaced library) with FNV-1a64 and
SHA-256 checksums, peak live bytes, and final live
residual all matching the trace headers exactly:
2,987,619 replayed ops, 0 failures, 0 dirty callocs.
The capture pipeline also proves itself fresh on every
run (a scripted smoke consumer is traced, summarised
and replayed on both legs before the banked traces are
checked). No timing or benchmark number is claimed
anywhere in this slice: traces are correctness
artefacts, and Z-095 owns trace-driven tuning.

Team note: the lead ran the six Mythic Engineering
roles sequentially itself (capture design, tracer
engineering, trace auditing, replay verification,
defect triage, ledger/risk); no sub-agents were
spawned for this slice and that is recorded plainly
here and in the team CAMPAIGN_STATE.md.

## S1 The audit-hook route

tests/perf/mimir_trace_z094.c is an LD_PRELOAD
interposition shim. It forwards every malloc-family
call it interposes to the real glibc entry (dlsym
RTLD_NEXT) unchanged and appends one compact text
record per call to a per-process raw file. The
allocator boundary itself is the hook: the traced
program runs on plain glibc, MimirAlloc is not in the
capture path at all, and the trace records what real
programs ASK FOR, leg-independently.

Interposed set (exactly eight): malloc, free, calloc,
realloc, reallocarray, aligned_alloc, posix_memalign,
memalign. Everything else (strdup and kin, __libc_*
internal allocations, mmap-served requests inside
other libraries) is OUTSIDE the trace boundary by
construction: those calls still work, served by
glibc; they are simply not PLT-boundary calls from
the traced program and its non-libc libraries. The
boundary is named, never implied to be total.

Safety engineering, because the shim is preloaded
into real programs: one spinlock serialises the real
call, the pointer-to-id map update and the log append,
so a raw stream's order is a true serial order of
completed calls; logging uses only write(2) on a
statically buffered fd, with hand-rolled formatting
(no stdio, no allocation, no re-entrant PLT call);
resolver-time calloc is served from a static bootstrap
arena; a 100M-record per-process cap stops logging
rather than fill a disk, and the summariser refuses a
capped stream. The price of the lock is throughput
under malloc contention - accepted, because no number
from a traced run is a benchmark.

## S2 Canonical form and the replay contract

tests/perf/summarize_z094.py folds the per-process
raw streams into one canonical trace: shim ids become
global logical ids; a realloc move keeps ONE logical
identity; the retire form (realloc(p, 0) under glibc)
and the failure form pass through; foreign frees
(F 0) pass through as the no-ops they were. Process
streams are concatenated in file order, so lifetimes
that overlapped in wall time replay as a sequence:
for multi-process traces the header's peak and final
live figures are therefore the SUM over process
segments - an upper accounting that the replay
reproduces exactly, never a claim about one instant
of the original run. The header records workload,
source command, capture timestamp, load average at
capture, per-op counts, allocation failures, peak and
final live bytes in REQUESTED sizes, and two hashes
over the exact canonical op bytes: FNV-1a64 (the
roadmap gate's checksum) and SHA-256. The .gz is
written with mtime 0.

tests/perf/replay_z094.py re-runs a canonical trace
against a leg (glibc via libc.so.6, or MimirAlloc via
build/libmimiralloc.so's mimir_* entries, both through
ctypes): every allocation is really serviced and
touched, calloc heads are verified zero, alignment is
verified, and the outcome stream the replay emits is
hashed the same way. MATCH requires checksum equality
AND equal op count, peak, final count and final bytes
AND zero outcome failures. The two deliberate D-013
divergences (zero-size refusal; realloc(p, 0) leaving
the block live) are normalised and COUNTED in the
replay line (d013_zero_size, d013_realloc_zero), never
allowed to falsify or to hide inside the checksum.

## S3 The three traces

Captured 2026-11-13T04:27:34Z on purpleraider by
tests/perf/capture_z094.sh (raw streams and the full
log stay in the team hidden_files; the traces are the
artefact). Machine load at capture: loadavg 0.91 at
start, 1.29 at end, WITH the Aesir forge's mojo suite
as co-tenant - recorded here and in every trace
header, per the shared-machine law; it cannot distort
any claim this slice makes, because the slice claims
no timings.

python_import_storm - 1 process, 28,694 ops
(M 14,339 / C 734 / A 1 / R 2,007 / F 11,613),
peak live 8,645,075 B, final live 3,461 blocks /
595,116 B, checksum_fnv1a64 478338dba68de321,
ops_sha256 e74b229c9771698a50e704f16146c95946e320c49fa2352b9bf5377f18626ee3, .gz 88,594 B
(sha256 fb988ae82cd82707880cd29619d1ea9c33cbcae6d44531ec88c064dd9e05aa2f). Workload: CPython importing
26 stdlib modules plus json/gzip/sqlite work
(5,000-row in-memory table). A python trace is not
byte-reproducible across captures (hash randomisation
reorders allocations); re-capture is not the contract,
replay against the banked header is.

git_object_churn - 622 processes, 576,623 ops
(M 307,899 / C 34,230 / R 9,829 / F 224,665),
peak live 508,627,126 B and final live 119,401 blocks
/ 508,619,240 B (sequential-concatenation accounting,
S2), checksum_fnv1a64 2ff165b183a9a269, ops_sha256
3f225e3113585417b9db4f63582afbb27c8385a7d0cd5c05492916383e499fbe, .gz 1,750,895 B (sha256
13932427263d710c068ebcd3c1e72142fa25e7a35fdbbe7ea7291b2b137e20ec). Workload: git init, 150 commits over
three churning files, gc --aggressive, repack -a -d,
fsck --strict, the whole process tree traced.

ollama_session - 11 processes, 2,382,302 ops
(M 1,381,621 / C 6,948 / A 1,353 / R 649 / F 991,731),
peak live 594,768,279 B, final live 398,191 blocks /
594,748,917 B (same accounting), checksum_fnv1a64
8c99dd8de5e6658d, ops_sha256 83e9a670834a725364df2752b48f602510575f4aea3535dfc3491c6cd4e1b521, .gz
6,270,694 B (sha256 64982f46de3e698f3be5ae737f4773759fa393e5f3ba8684c491faeec4b88c7d). Workload: a
private ollama serve on 127.0.0.1:11499 (the system
service is never touched) plus one POST /api/generate
for qwen2.5:0.5b, num_predict 12. The session is
proven real in the capture log, not assumed: the
response begins 'The sea, a vast and boundless body
of water,', eval_count 12, done_reason length, and
/api/ps shows the model loaded under the llamacpp
runner; the capture script now REFUSES to summarise
unless that proof holds.

Replay verdicts (make test-z094, log in team
hidden_files/z094_replay_run.log): smoke MATCH on
both legs; python MATCH / MATCH; git MATCH / MATCH
(mimir leg d013_zero_size 303); ollama MATCH / MATCH
(mimir leg d013_zero_size 318). Every leg: failures
0, calloc_dirty 0. Z-094 RESULT: PASS.

## S4 Defects found and fixed in scope

All four were in this slice's own new tooling; none
in the banked allocator core. Standing rule: none
left unfixed.

1. Fork inheritance poisoned multi-process streams.
   The shim's per-process id map is inherited across
   fork; a git child freeing pointers its parent had
   allocated logged inherited shim ids its own stream
   never issued, and the summariser rightly refused
   the first git capture ('double or unknown free of
   shim id 268', trace.3271294.raw line 101: the file
   opens at id 353 and closes by freeing ids
   268/266/255/123/... allocated in ancestor
   streams). Fix: a pthread_atfork child handler
   resets all per-process shim state (id map, id
   counter, log state, op counter, spinlock), with a
   defensive reset in the pid-change path; inherited
   pointers are foreign to the child's stream and log
   as F 0, exactly what they are. Proven by a targeted
   fork regression (parent stream balanced; child
   stream F 0 / own ids from 1 / F 0; summariser
   green) and by the 622-process git capture itself.
2. Killed processes left TORN final records. The
   first flush wrote byte-wise at 64 KiB boundaries,
   so a record could straddle a flush; the Ollama
   server, killed by design at capture end, left
   streams ending mid-record ('M 24', 'M 34423' at
   exact 64 KiB file sizes). Fix: log_flush is
   record-aligned - it writes only up to the last
   newline and keeps any partial tail buffered, so a
   process killed at any instant leaves a true record
   PREFIX (it may lose whole buffered records, never
   half a record) - and the summariser now hard-refuses
   any stream not ending at a record boundary, so a
   torn stream can never pose as complete. Negative
   test: the first capture's torn stream is refused
   by name.
3. The private Ollama serve could not see the model
   store. The system service's override sets
   OLLAMA_MODELS=/mnt/vault/models/ollama; a private
   serve with the default ~/.ollama/models reported
   'model qwen2.5:0.5b not found' and the first
   capture attempt banked an empty session. Fix: the
   capture script points the private serve at the
   real store and hard-checks the generate response
   (done true, eval_count > 0) before summarising -
   a session that did not generate can never be
   captured as one again.
4. The smoke consumer overwrote a live pointer with a
   failing realloc's NULL. b = realloc(b, 2^62)
   destroyed the only copy of the original block;
   GCC's -Wnonnull fortify check caught the resulting
   memset(NULL, ..., 4096) at compile time (-Werror),
   which is why the landed make test-z094 had never
   run green. Fix: the result goes to a scratch
   variable, per the E-form contract (failure returns
   NULL; the old block stays live in the caller's own
   variable).

## S5 Verification

- make test-z094 (tests/perf/run_z094.sh): PASS -
  smoke pipeline fresh-traced and replayed MATCH on
  both legs, then all three banked traces replayed
  MATCH on both legs (S3).
- Standing correctness gate scripts/z13/run_gate.sh:
  baseline on the merged tree (keeper's
  gate_z094_baseline.log) and final on the complete
  tree (gate_z094_final.log) both GREEN, GATE COUNTS
  checks=4988 failed=0 skips=77, make exit 0 - counts
  read from the logs. The slice is standalone
  (test-z094 is NOT part of make test), so the gate
  total does not move.

## S6 Limits, stated

- A signal-killed traced process loses at most its
  final buffered whole records (bounded by one
  64 KiB buffer): its stream is a true prefix, and
  its unrecorded tail can only inflate the trace's
  final-live residual, never falsify an op.
- Streams of processes that exec immediately can be
  empty (0-byte raw files occurred in both git and
  Ollama captures); they contribute zero ops and are
  counted in the header's process count.
- The shim serialises allocation, so traced runs are
  slower and differently scheduled than native runs;
  allocation REQUESTS are unaffected, and requests
  are all a trace records.
- Z-095 consumes these traces for tuning; any tuning
  claim must cite replay on these exact files.
