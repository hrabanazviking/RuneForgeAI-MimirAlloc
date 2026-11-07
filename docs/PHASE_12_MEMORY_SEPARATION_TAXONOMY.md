# Phase 12 - Memory Separation Taxonomy and Measurement Definitions (Slice L-002)

Status: **DEFINITIONS.** This slice defines, precisely and
measurably, the three-way memory separation Phase 12 requires.
It claims NO model-workload measurement: no Aesir run was made
or is implied. Where the artefacts a run needs are absent on
this Pi, the run is recorded OWED (Section 7), never simulated.
Base: `b1824474` (Wave 11 COMPLETE). Roadmap source:
`docs/MimirAlloc_Research_and_Development_Roadmap.md` Phase 12:
"Separate mmap weight residency, CPU buffers and GPU
allocations"; exit requires "same model hash, prompt, context,
thread count, backend and thermal state used across baseline
and comparison; output correctness and tablet responsiveness
maintained." Counter definitions are the banked Phase 10
telemetry (`docs/PHASE_10_STATISTICS.md`): bytes_requested,
bytes_allocated, bytes_cached, bytes_mapped, bytes_released,
peak_mapped. Process-account method follows the banked Phase 9
separation proof (`docs/PHASE_9_TELEMETRY_SEPARATION.md`):
allocator accounts and process accounts are different accounts.

## 1. The taxonomy: three classes, one residual, one rule

Every byte attributed in a Phase 12 profile belongs to exactly
one of three classes, or is reported as residual R (Section 5.6).
A byte is classified by WHO OWNS ITS ALLOCATION and BY WHAT
ROUTE it was obtained - never by what data it currently holds.

| Class | Name | Owner / route | Allocator-managed? |
|---|---|---|---|
| W | mmap weight residency | The kernel file mapping of the model file itself (mmap of a GGUF file, file-backed) | NO |
| C | CPU buffers | Host RAM obtained for computation: C1 via the CPU allocator under test, C2 via an explicit non-allocator route | C1 YES, C2 NO |
| G | GPU / device allocations | A device backend (GPU, NPU, accelerator) through its own driver / runtime API | NO - unreachable |
| R | Residual / unclassified | Anything the evidence does not place in W, C or G (executable and library mappings, stacks, page tables, measurement overhead) | reported, never forced |

**Single-class rule (the whole taxonomy in one line):** for any
chosen size metric (Section 2.1), at any snapshot instant,
`attributed_total = W + C + G_attributed_in_process_evidence`,
the three terms are pairwise disjoint, and any byte that would
be counted twice is counted ONCE, in the class of its owning
route. Cross-check totals (process PSS/RSS) are checks, not a
fourth term: they are never added to the class sum.

### 1.1 Class W - mmap weight residency

- Definition: the resident (and separately, the virtual) extent
  of file-backed mappings whose backing file IS the model file
  under test (normally a `.gguf` file), created by the loader
  mapping the file rather than reading it into heap buffers.
- These bytes are the file's own pages, brought in by page
  faults and reclaimable by the kernel under pressure. They are
  NOT allocated by malloc or by MimirAlloc, were never published
  by any allocator, and no allocator counter moves when they
  fault in, are evicted, or are unmapped.
- Only the weight extents of the mapping are W. A mapping of the
  same file also carries the GGUF header and metadata region;
  that region is still W while it is only viewed through the
  mapping (no copy exists). The moment metadata is parsed into
  heap structures, the STRUCTURES are C1 and the mapped region
  remains W - two distinct byte sets, each counted once, in
  its own class (trap DC-7, Section 4).
- If a loader instead READS weights into a heap buffer, those
  buffer bytes are C1, not W: route decides, content does not.

### 1.2 Class C - CPU buffers

- C1, allocator-managed heap: blocks published by the CPU
  allocator under test through its public surface (malloc /
  calloc / realloc / aligned allocation and frees), including
  its internal spans and mapping-path blocks. This is the ONLY
  class the Phase 10 counters speak for (Section 3).
- C2, explicit CPU buffers: host RAM obtained by an explicit
  route that bypasses that allocator - a direct anonymous mmap
  by the runtime, a static/BSS buffer, a stack, a shared-memory
  segment. C2 is real CPU memory and belongs in the profile,
  but NO allocator counter can speak for it; it is measured
  from /proc evidence only (Section 2) and reported separately
  from C1, never merged into counter figures.
- Typical C members in a model workload: KV cache (C1 if the
  runtime allocates it through the allocator, C2 if the runtime
  mmaps it itself - the profile must state which), activation
  and scratch buffers, prompt/token buffers, parsed GGUF
  metadata structures, tokenizer tables built on the heap,
  output/logit buffers.

### 1.3 Class G - GPU / device allocations

- Definition: memory allocated by a compute backend through a
  device API (for example a Vulkan / CUDA / vendor runtime
  allocation, a dma-buf, or an accelerator's own buffer pool),
  owned and freed by that backend, on the device side of the
  driver boundary.
- **Boundary, stated plainly: a CPU allocator cannot allocate,
  free, resize, or observe these bytes. MimirAlloc has no
  authority over class G, and no Phase 10 counter can move
  because a device allocation happened.** Any profile that
  attributes a change in G to the CPU allocator is wrong by
  construction.
- G is measured ONLY from the backend's own reporting (its API
  counters or driver accounting), recorded as a separate line
  in its own units, and never added to /proc-derived CPU totals.
- Shared-memory caveat (this Pi included): on a unified-memory
  SoC the device and CPU share physical RAM, and a dma-buf may
  also be mapped into the process. Shared physical RAM does NOT
  move a byte into class C: classification follows the owning
  route, so a backend-owned dma-buf is G even when it is also
  visible in /proc mappings (trap DC-8, Section 4). Its
  in-process mapping extent is reported as the G cross-check
  line, not as C.

## 2. Identification in /proc evidence

### 2.1 Metrics - never mix them

For each mapping, `/proc/<pid>/smaps` gives, among others:
`Size` (virtual extent), `Rss` (resident, shared pages counted
in full in EVERY process that maps them), `Pss` (resident with
shared pages divided among their mappers), `Pss_File` /
`Pss_Anon`, `Shared_Clean` / `Shared_Dirty`, `Private_Clean` /
`Private_Dirty`, `Anonymous`, `Swap`, `FilePmdMapped`, and the
header line carries the address range, permissions, offset,
device:inode and the PATHNAME (empty = anonymous; a bracket
name such as `[heap]` or `[stack]` = a kernel label, not a
file). A profile reports Size, Rss and Pss as three separate
columns and sums only within one column. Summing Rss across
processes, or adding Size to Rss, is a defect (traps DC-2/DC-3).

`/proc/<pid>/smaps_rollup` aggregates ALL mappings: it has no
pathname and therefore CANNOT classify anything by itself. Its
lawful uses are exactly two: the process total cross-check
(rollup Rss/Pss against the sum of the classified smaps terms),
and the anonymous/file split (`Pss_Anon` vs `Pss_File`) as a
sanity bound on C-plus-anonymous-R versus W-plus-file-R.

### 2.2 Decision procedure (apply in this order, per mapping)

1. Pathname is the model file under test (match by the recorded
   model path AND sha256 from Section 5.1, never by extension
   alone) -> **W**.
2. Pathname is a device node or a driver label - for example
   `/dev/dri/*`, `/dev/dma_heap/*`, or an anon dma-buf label ->
   the mapping is the in-process shadow of **G**; record its
   extent on the G cross-check line, add nothing to C.
3. Pathname is empty, `[heap]`, or an anonymous label, AND the
   allocation route is known: through the allocator under test
   -> **C1**; a named explicit runtime buffer -> **C2**.
4. Pathname is empty or anonymous and the route is NOT known
   -> **R**, with its size recorded. Unknown route never
   defaults to C1: allocator credit requires allocator evidence.
5. Any other file pathname (executable, shared libraries, fonts,
   other data files) -> **R**. A file mapping is W only when the
   file is the model under test.

Anonymous is a necessary, not sufficient, sign of C: allocator
spans and mapping-path blocks are anonymous, but so are stacks,
thread arenas the allocator does not own, and runtime mmaps.
That is why step 3 demands the route, and why the C1 figure from
/proc is an UPPER bound that must reconcile with the counters
(Section 3.2), not a counter substitute.

### 2.3 Workload-phase expectations (what a correct profile shows)

| Phase | W | C | G |
|---|---|---|---|
| Model loading | rises as weight pages fault in; Size jumps at mmap, Rss/Pss grow with faults | C1 metadata structures, loader scratch | backend may allocate device weights copy - G only if the backend reports it |
| GGUF metadata / tokenizer | unchanged (viewed through mapping) | C1 parsed structures and tokenizer tables | normally none |
| KV cache allocation | unchanged | rises by the KV allocation, in C1 or C2 per the actual route, stated in the profile | device-side KV, if any, is G |
| Prefill | may rise (more weight pages touched) | C scratch/activations rise and partly release | backend scratch is G |
| Decode (steady state) | stable at touched-weight residency | stable apart from per-token buffers; peak C is read here and at prefill | stable apart from backend per-token buffers |
| Teardown | falls to zero as the file mapping is unmapped | falls: bytes_released rises, live C returns to the pre-load value | backend reports its release; CPU counters must NOT move for it |

A profile whose W moves at KV allocation, whose counters move
at a device allocation, or whose C does not return to its
pre-load value after teardown (leak or retained cache, which
must then be named) fails separation and is reported as such.

## 3. What the Phase 10 counters can and cannot say

### 3.1 Per-counter ruling (banked definitions, Phase 10)

| Counter | Banked meaning | Speaks for | Cannot speak for |
|---|---|---|---|
| bytes_requested | Cumulative caller-requested sizes of successful new block publications; monotone | C1 demand over time | Any instant live total (it never decreases); W; C2; G |
| bytes_allocated | Cumulative usable bytes granted at those publications (class size, or mapping usable size); >= requested, difference = rounding slack | C1 granted volume over time | Live totals; W; C2; G |
| bytes_cached | Instant sum of mapping_length over registered spans with live_count == 0 (retained-empty spans) | The retained-cache part INSIDE C1 at a snapshot instant | A separate memory pool (it is already inside bytes_mapped); W; G |
| bytes_mapped | Instant allocator total: live mapping-path bytes + all registered span bytes (live and retained-empty) | C1's full mapped footprint at a snapshot instant | File mappings of any kind - despite its name it counts ONLY this allocator's anonymous mappings; never W; never G |
| bytes_released | Cumulative mapping_length over successful releases of published mappings/spans; monotone | C1 release volume over time | Instant totals; device releases (a G release moves nothing here) |
| peak_mapped | Maximum bytes_mapped sampled at the publication points (exhaustive, because C1 mapped bytes grow only at publications) | The C1 peak a comparison must beat or match | A process peak (VmPeak/RSS peak includes W, C2 and R); a device peak |

No counter, and no combination of counters, yields W or G.
A profile that derives weight residency or device memory from
allocator telemetry is invalid, however plausible its numbers.
Equally, no counter yields C2: explicit buffers bypass the
allocator entirely, by definition of the route.

### 3.2 Reconciliation identities a profile must check

At every quiescent snapshot (Section 5.3):

1. `bytes_cached <= bytes_mapped`, and the cached part is
   counted once, inside C1 - never added on top of bytes_mapped.
2. The allocator's anonymous mappings in smaps (the spans and
   mapping-path blocks identifiable by the run's own mapping
   record) sum, in Size, to bytes_mapped at the same instant.
   A mismatch means the mapping record or the snapshot is
   wrong; the profile stops and says so.
3. C1 in /proc terms is reported as that reconciled Size sum
   (virtual) and its Rss/Pss parts - three columns, per
   Section 2.1 - while the counters are reported in their own
   column. Counter bytes and smaps bytes describe the same C1
   bytes from two accounts; they are CROSS-CHECKS of each
   other, never summands (trap DC-5).
4. Conservation across the run: at teardown-complete,
   bytes_released plus any still-mapped C1 equals the total
   C1 ever published; final live C1 returns to its pre-load
   value or the difference is named (retained cache or leak).

## 4. Double-counting traps (each byte counted exactly once)

- **DC-1 File size is not residency.** The GGUF file's size on
  disk is not a memory figure. W counts mapped pages only,
  per metric column; an untouched weight page is in W Size
  but in neither W Rss nor W Pss.
- **DC-2 Rss counts shared pages in full, per process.** Two
  processes mapping the same weights each show the full Rss.
  Cross-process or system totals use Pss, or are not made.
- **DC-3 Size + Rss.** Virtual extent and resident extent of
  the same mapping are two views of one byte set. Never add.
- **DC-4 Rollup + detail.** smaps_rollup is the sum of the
  smaps entries. Adding rollup figures to per-mapping figures
  counts everything twice.
- **DC-5 Counters + smaps.** bytes_mapped and the smaps Size
  of the allocator's own anonymous mappings are the same C1
  bytes in two accounts (Section 3.2.3). Report both; add
  neither to the other.
- **DC-6 Cache inside mapped.** bytes_cached is a subset of
  bytes_mapped. `mapped + cached` double-counts the cache.
- **DC-7 The copy is a second byte set, not a second count.**
  Weights or metadata copied from the mapping into heap exist
  twice physically: the mapped original is W, the copy is C1.
  Counting both is CORRECT; counting either one twice, or
  moving the original to C because a copy exists, is the trap.
  Pointers INTO the mapping (no copy) add zero bytes anywhere.
- **DC-8 Unified-memory device buffers.** A dma-buf visible in
  smaps and also reported by the backend is ONE byte set: it
  is G, recorded once on the G line; its smaps extent is a
  cross-check only and is excluded from C and from the
  process-evidence class sum (Section 1, single-class rule).
- **DC-9 Page cache is not process memory.** Weight pages in
  the kernel page cache but not resident in this process's
  mapping belong to no class of this profile; system-wide
  `free` cache figures are context, never a class term.
- **DC-10 Peak mixing.** peak_mapped (C1, allocator account),
  VmPeak (whole process, virtual) and peak RSS are three
  different peaks. A comparison states which peak it compares
  and never substitutes one for another.

## 5. Measurement protocol for L-004 to L-007 (binding)

Any later Phase 12 slice that profiles a model workload
follows this protocol. A run that skips a step is a sketch,
not a profile, and must label itself as such.

### 5.1 Freeze the exit conditions BEFORE the first run

Record, in the run's evidence header, identically for the
baseline (system allocator) and the comparison (MimirAlloc):

- model file: absolute path and sha256 of the model file
  (the Phase 12 reference model is Qwen3-1.7B Q4_K_M where
  available; whatever model is used, its hash is the identity);
- prompt: the exact prompt file, its sha256, and token count;
- context: context length and KV parameters;
- threads: compute thread count (and the command line in full);
- backend: backend name and build identity (binary path,
  sha256, version string) for BOTH runs - the allocator is the
  only permitted difference between baseline and comparison;
- thermal state: SoC temperature and throttling status at the
  start of each run (on this Pi: `vcgencmd measure_temp` and
  `vcgencmd get_throttled`, or the thermal_zone sysfs value).
  Runs start only when the start temperatures differ by at
  most 2 degrees C and neither run starts throttled; a run in
  which throttling begins is marked THERMAL-INVALID and is
  evidence about thermals, not about the allocator.

### 5.2 Run order and repetition

Baseline and comparison alternate (B, C, B, C) with at least
three valid runs of each, in one session on one device, so
drift and warm-up are shared. The reported figure per class
per phase is the median of the valid runs, with the spread
(min, max) beside it. A single run of each proves nothing and
is reported as a first look, never as the comparison result.

### 5.3 Snapshots at quiescent points

Snapshots are taken only at quiescent points - no allocation
in flight - at these workload boundaries: S0 before load,
S1 after load, S2 after metadata/tokenizer ready, S3 after KV
allocation, S4 after prefill, S5 steady decode (after a fixed
recorded token count), S6 after teardown. Each snapshot
captures, atomically enough for the comparison: the full
`/proc/<pid>/smaps`, `smaps_rollup`, and `status`; the Phase 10
counter snapshot if the allocator under test is linked; the
backend's own device-memory report if a device backend is in
use; temperature and throttling. Snapshot collection itself
must not allocate on the heap of the measured process (the
banked Phase 9 method: raw open/read into static buffers, or
an external reader process - whose own memory is a different
process and therefore never in this profile).

### 5.4 Classification and reporting

Each snapshot's mappings are classified by the Section 2.2
procedure, mechanically, by a recorded rule (script or stated
hand procedure) that a reader can re-apply to the archived
smaps and get the same classes. The report per snapshot is a
table: rows W, C1, C2, G (backend line, plus its in-process
cross-check extent), R; columns Size, Rss, Pss; counter values
in their own separate block; and the Section 3.2 reconciliation
results. Class figures are deltas against S0 as well as
absolutes, so pre-existing process memory is never credited
to the workload.

### 5.5 Correctness and responsiveness gates

A comparison counts only if, in both arms: the generated
output matches the recorded expected output for the frozen
prompt (exact token sequence, or a stated equivalence with
its evidence), AND the host stayed responsive - on this Pi,
the Samtal service answering HTTP 200 on port 8787 before and
after each run, recorded in the evidence header. A faster or
smaller run with wrong output is a failure, not a result.

### 5.6 Residual honesty

R is printed, never hidden inside another class and never
silently dropped. If R's Rss changes between S0 and S5 by more
than the run-to-run spread of R itself, the profile names the
suspected contents (for example a runtime arena) and either
reclassifies them WITH route evidence or leaves them in R and
states that the class comparison is bounded by that movement.
Conclusions are drawn only from W, C and G movements that
exceed their own spreads.

## 6. Tuning rule that follows from the taxonomy

Phase 12 permits tuning "only where profiles show useful
benefit". Under this taxonomy that means: MimirAlloc tuning
may target C1 only; a claimed improvement must appear as a
C1 movement (counter and reconciled /proc figures agreeing)
under the Section 5 protocol, with W, G and output correctness
unchanged. A total-RSS improvement caused by fewer weight
pages being touched is a workload fact, not an allocator
result, and must be reported as one.

## 7. Honest limits - evidence on this Pi, 2026-11-07

Checked on Mimir (purpleviking1, `uname -m` = aarch64) during
this slice, read-only, at light load (free: 13 GiB available;
Samtal HTTP 200 on port 8787 before and after the checks):

- **No runnable Aesir binary on this Pi.** Aesir-named binaries
  exist at `/home/pi/c2/aesir_engine/aesir_main` (223 KiB,
  sha256 9b6bd709470c27d810dc2159bd74fcf5073904b174c9ad211c4
  b754cd496fca2) and `/home/pi/c2/aesir_main` (631 KiB), but
  `file` identifies BOTH as `ELF 64-bit LSB pie executable,
  x86-64` - they cannot execute on this aarch64 host. No ARM64
  Aesir binary was found in the locations searched.
- **No Qwen3-1.7B Q4_K_M model on this Pi.** The file at
  `/home/pi/c2/aesir_engine/model.gguf` is a 24-byte GGUF
  header stub (sha256 a4e5e156ddec27e286f75328784d7106b60a4e
  b1d246e950a001a3f944fbda99), not a model. The model store
  `/home/pi/models` -> `/mnt/yggdrasil/models` holds only
  TinyLlama files: `tinyllama-1.1b-chat-v1.0.Q4_0.gguf`
  (609 MB), `tinyllama-1.1b-chat-v1.0.Q5_K_M.gguf` (747 MB),
  `tinyllama-1.1b-chat-v1.0.Q6_K.gguf` (863 MB) and
  `tinyllama-1.1b-chat-v1.0.Q8_0.gguf` (1.1 GB). No file
  matching Qwen3-1.7B Q4_K_M was found.
- **Device side exists but is unmeasured.** This Pi exposes
  `/dev/dri/card0`, `/dev/dri/renderD128` and
  `/dev/dma_heap/system` + `linux,cma` (VideoCore, unified
  memory) - so class G is architecturally real here, but no
  backend run has reported a G figure, and none is claimed.
- **Thermal evidence is available:** `vcgencmd measure_temp`
  read 57.6 C and `vcgencmd get_throttled` read 0x0 (not
  throttled) at check time; page size on this host is 16384
  (banked Phase 9 evidence and live smaps KernelPageSize).

Therefore the definitions and protocol above STAND as written,
and the model-workload runs themselves are OWED - they become
possible when an ARM64 Aesir build and the Phase 12 reference
model are present on a supported device, at which point
Section 5 applies unchanged. Substituting TinyLlama for the
reference model, or an x86-64 host for this Pi, would be a
different comparison and must be commissioned and labelled
as such, never silently folded into Phase 12 exit evidence.

## 8. Incomplete / owed items (INC-L002)

- **INC-L002-01 (OWED RUN):** No baseline-vs-MimirAlloc model-
  workload profile exists yet. It is owed by L-004..L-007
  under Section 5, on a device holding a runnable Aesir build
  and the reference model. Blocker evidence: Section 7.
- **INC-L002-02 (OWED ARTEFACT):** An ARM64 Aesir binary and a
  Qwen3-1.7B Q4_K_M model file (with recorded sha256 for each)
  are absent on this Pi. Providing them is outside this
  slice's scope; their absence does not block the definitions.
- **INC-L002-03 (UNMEASURED CLASS):** Class G has no measured
  figure on any MimirAlloc evidence to date. The first G line
  in any profile must come from a backend's own reporting
  during a Section 5 run, not from /proc inference.
- **INC-L002-04 (ROUTE EVIDENCE GAP):** Whether Aesir's KV
  cache and scratch buffers are allocated through the CPU
  allocator (C1) or by explicit runtime routes (C2) is not
  established by any banked evidence. L-004..L-007 must
  establish the route per buffer before classifying it;
  until then those bytes would sit in R, by Section 2.2 step 4.

*End of L-002. Deliverable: this document only. No code, no
header, no README change. Definitions stand regardless of the
owed runs; no measurement is claimed by this slice.*
