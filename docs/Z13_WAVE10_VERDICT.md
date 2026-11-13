# Z13 Wave 10 Verdict

Slice: Z-090 - Wave 10 verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-12 by team-05. The six Mythic Engineering roles were run
SEQUENTIALLY by the lead as reviewers (evidence audit, safety review,
performance review, contract/D-013 review, ops/rollback review,
gate/commit-scope review) - no parallel sub-agents were spawned this
run, and that is recorded plainly here and in CAMPAIGN_STATE.md.
Base: merge 3a367bc of seat main cb5bf36 (banked Z-001..Z-086,
ls-remote verified cb5bf3666a03d7d79a0429dc561b398d94377b6a) into
yrsa/mimiralloc-team-05 - clean merge, no conflicts; the merge added
exactly the banked Z-085 and Z-086 artifacts (7 files) on top of this
branch's banked Z-084 commit fcb31fc. Baseline standing gate on the
merged tree before any Z-090 edit: GREEN 4988 checks / 0 failed /
77 by-design skips (hidden_files/z090_baseline_gate.log).

STATUS: PARTIAL-FINAL, with one OWED section, stated up front.
FINAL below: Z-082, Z-083, Z-084, Z-085, Z-086 (all banked at the
seat), Z-087 (folded in - committed 816a6e4 on team-03, gate GREEN,
queueing for banking) and Z-089 (committed 615c935 on
yrsa/mimiralloc-team-06, gate GREEN, queueing behind Z-087/Z-088
for banking in roadmap order).
OWED below: Z-088 only (section 7.2 - no slice artifacts on disk
at verdict time beyond the baseline gate). Z-087 was OWED-DRAFT in
this verdict's first commit (86cde4c) and is FOLDED IN as FINAL by
the fold-in amendment below: it committed as 816a6e4 on
team-03 (doc + recorded run + final gate GREEN) minutes after the
first commit, in parallel, and was verified first-hand before
folding - the section 7.1 fold-in note records the sequence.
This follows the established OWED pattern (precedents Z-063,
Z-072's predecessor builds): an OWED section names the slice,
states what is missing, and invents no verdict; a fold-in
amendment completes it when the slice lands, exactly as Z-063 was.
The headline the FINAL evidence supports: the opt-in interposing
variant is CORRECT everywhere it was tried on real programs - and
it is SLOW on allocation-heavy work, and it has exactly one known
compatibility break, the D-013 zero-size contract meeting glibc's
regex compiler. Preload is therefore a per-program, opt-in,
 reversible choice today - safe for the tested correctness classes,
not advisable where speed matters, and forbidden for the
never-preload list in section 5.

## 1. The plain answer - where preload is safe today

For a user deciding whether to run a program under
LD_PRELOAD=build/libmimiralloc_interpose.so on this host:

SAFE TODAY (correctness proven, this machine):
- Ordinary one-shot command-line programs of the coreutils / git /
  bash / python3 class, and the repository's own test binaries:
  13 of 13 subjects passed the Z-084 smoke battery with byte-parity
  against the glibc control (section 3.3).
- A real Python process doing an import storm plus heavy small-
  object churn: exact checksum equality with the glibc run (Z-085).
- A short Ollama generation: byte-identical response text and
  context arrays, plain vs preloaded (Z-085).
- Contained KDE desktop applications (kcalc, kate, dolphin,
  gwenview, okular, and a plasmashell component): all passed in
  the contained offscreen session (Z-086).
- Rollback from any of the above: unset LD_PRELOAD and respawn.
  Proven from a deliberately broken state by Z-089's drill -
  exactly one environment line differs, and the respawned process
  is byte-identical to the glibc control.

SAFE FOR CORRECTNESS, BAD FOR SPEED (measured, section 6):
- Python allocation churn: 5.01x slower overall, peak RSS +11.5%.
- Ollama / llama.cpp model loading: ~22-24x slower load, every
  cold load; generation itself is a TIE. Do not preload
  performance-critical Python or model-loading jobs (Z-089's
  never-preload entry 9 rests on exactly these numbers).

NOT SAFE / KNOWN TO BREAK:
- Any program whose glibc regcomp use includes leading-anchor
  patterns (grep with a leading ^ is the filed example): it fails
  under preload with a spurious out-of-memory error. This is the
  D-013 contract divergence T-084-1, restated for users in
  section 4. It is not heap corruption and not a wrong answer on
  any non-zero allocation - it is a decided contract meeting a
  caller that treats the contract NULL as OOM.

NEVER PRELOAD (ops law, section 5): systemd, the display manager,
the banking tooling itself, the live desktop session, package and
repair tooling, sshd, long-lived shared services, backup/restore
tooling - each with its reason, from Z-089.

PROVEN SINCE FIRST WRITING (folded in, section 3.6): the Aesir
CPU engine under preload is correctness-TIE (identical token
streams) with a small contended-window LOSS observation (Z-087).
NOT YET KNOWN (OWED, section 7.2): failure drills under preload
(Z-088, no evidence yet). No claim is made either way.

## 2. Scope and honesty - read first

- Wave 10 (Z-082..Z-090) is the interposition wave: it built the
  opt-in variant that lets unmodified programs use this allocator
  (Z-082), closed the symbol gaps real programs hit (Z-083), then
  tried the variant on real programs in widening rings - smoke
  battery (Z-084), Python/Ollama workloads (Z-085), desktop
  applications (Z-086), the Aesir engine (Z-087, folded in), failure
  drills (Z-088, OWED) - and wrote the rollback / never-preload
  packet (Z-089) before this verdict.
- The roadmap's Z-090 text asks for: where preload is safe today,
  where it is not, and the evidence for each, stated plainly
  enough for a user to decide; gate: verdict doc traceable to
  battery reports. Section 1 is the plain statement; sections 3,
  5 and 6 are the evidence; section 9 is the traceability index.
  The gate is met for every FINAL section. The one OWED section
  are the honest remainder, not a hidden gap.
- Every FINAL number below traces to a banked recorded run (or,
  for Z-089, its committed recorded run), re-read first-hand from
  disk on the merged seat tree by this lead. Grades use the
  roadmap's own WIN / TIE / LOSS vocabulary, this machine only.
- The default library is untouched by this wave's verdict: it
  still exports exactly 13 namespaced symbols and no standard
  name (Z-083). Interposition exists only in the separate opt-in
  build/libmimiralloc_interpose.so (20 exports). Nothing in this
  verdict recommends changing the default build.

## 3. What was built and what the batteries found (FINAL)

### 3.1 Z-082 - the interposing variant (banked 23a05d0)
A separate opt-in library interposing exactly ten standard names
(malloc, calloc, realloc, free, cfree, aligned_alloc,
posix_memalign, reallocarray, malloc_usable_size, malloc_trim),
each a pure forwarder to its namespaced counterpart; 20 exports
total (10 namespaced + 10 standard), one allocator instance.
Recorded run: PASS, zero FAIL - the default library proven to
carry NONE of the ten standard names, the full glibc family audit
filed with every symbol INTERPOSED, MISSING-NAMED or INTERNAL
(no unnamed symbol), and the preload probe pair proven live:
plain = GLIBC verdict, preloaded = MIMIR verdict. Decision D-015.
The interposer build is byte-stable across the wave: sha256
9a6836d5318fb3a269fcbaeb7b6b03834bd88cb375f38a1d88677fd5188e2491
reappears identically in the Z-085, Z-087 (draft) and Z-089 runs.

### 3.2 Z-083 - gap closure for real programs (banked ad63bb9)
The namespaced memalign family real programs call was added over
the Z-074 construction: mimir_memalign / mimir_valloc /
mimir_pvalloc (default library 10 -> 13 exports). Recorded run:
PASS (test_z083 301 checks / 0 failed; dyn + cpp PASS). The full
host glibc 2.44 gap list was shrunk to its filed remainder: every
row is now SERVED, SHIMMED, owned by Z-082, ASSESSED with its
reason (memalignment, free_sized / free_aligned_sized, cfree,
strdup class, mallopt, mallinfo family - none shimmed, each
reason written), or proven UNREACHABLE (the __libc_* internals
and libc-internal callers, which no interposition route can
reach). Decision D-016. The strdup-class finding matters for
preload users: a program's OWN strdup calls can be interposed
in principle under the variant, but libc-internal strdup users
cannot - that boundary is filed, not papered over.

### 3.3 Z-084 - LD_PRELOAD smoke battery (banked 39a7fc8)
13 subjects (coreutils, git, python3, bash, 8 repo test binaries,
the preload probe) x plain / preloaded legs under the three-part
pass rule (proven loaded via LD_DEBUG + exit 0 + stdout parity;
probe leg inverted by design). Final recorded run: PASS,
74 [ok] / 0 [FAIL]. One failure was found during the build and
triaged to its named cause (finding T-084-1): preloaded
grep -c with a leading-anchor pattern exited 2, Memory
exhausted - CONTRACT-DIVERGENCE-D013-ZERO-SIZE (section 4).
Disposition of record: NOT an allocator defect; no code fix
owed; the battery leg was corrected to the unanchored count and
the divergence pinned as an asserted expectation in the runner,
so any future change fails the battery loudly.

### 3.4 Z-085 - Python and Ollama-class workload (banked 9a568b2)
Correctness PASS on both workloads, exact: the Python import
storm + churn checksum identical across all 6 reps and both
modes; the Ollama generation byte-identical (response text and
context arrays) in plain, preload and a confirmatory preload leg.
Performance LOSS, honestly graded (section 6): Python TOTAL
median 0.2772 s plain vs 1.3876 s preload (5.01x); Ollama model
LOAD 0.542 s plain vs 12.920 s and 12.093 s preload in two
independent legs (~22-24x), while generation eval is near-TIE.
One runner defect was found and fixed in scope (private Ollama
servers missed the systemd service model store; the runner now
discovers it and fails any leg whose model list lacks the model).
No allocator defect was found; no WIN is claimed anywhere.

### 3.5 Z-086 - desktop battery (banked cb5bf36)
KDE battery under full containment (private HOME/XDG +
dbus-run-session + QT_QPA_PLATFORM=offscreen; the live
plasmashell / kwin PIDs proven identical before and after, at
team and seat re-runs). Recorded run: PASS, 15 ok / 0 FAIL.
kcalc, kate (carrying the kwrite class), dolphin, gwenview,
okular and the plasmawindowed digitalclock component: PASS in
both modes. Two rows are named non-evidence, not failures:
full plasmashell is ENV-ONLY (it segfaults identically on the
glibc control - it needs a live compositor, so it is NOT TESTED
as allocator evidence), and kwrite is MISSING-APP (no binary on
this host; kate carries the class). Rollback re-proven in the
battery: kcalc with LD_PRELOAD unset is ALIVE on glibc.

### 3.6 Z-087 - Aesir boundary trial (committed 816a6e4, FOLDED IN)
The Aesir CPU engine (source build of record from the Aesir seat
head b99b702, fixture Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf,
sha256-pinned) ran 4 alternating legs (plain / preload x2) on a
32-token deterministic prompt, read-only toward the Aesir forge
(git status proven UNCHANGED across the legs). Recorded run:
FAIL=0, every leg [ok]; final standing gate GREEN 4988/0/77.
Correctness: TIE - token streams identical across all 4 legs
(1 distinct stream, sha256 filed), genuine generation text.
Performance: LOSS as an observation on a CONTENDED host
(loadavg 1.98-4.43, cross-campaign load throughout) - wall
+5.1%, decode +6.3%, minor page faults +14.1%, RSS +0.1%,
prefill and load inside noise. The slice itself rules these
NOT baselines; a quiet-machine re-run is the road to a baseline
grade, and none is claimed here. Attempt-1 finding: running the
engine via mojo run under preload interposes the Mojo COMPILER,
whose compile did not finish inside a 600 s bound (plain leg
20.14 s) - filed as an observation about compiler-in-process
preload, not a defect of either project. Three Aesir-side
blockers named, not fixed (the gate's own terms): B-087-1 stale
prebuilt binary segfaults in prefill on a PLAIN run; B-087-2
engine-tree binary is a canned-text stub; B-087-3 engine-tree
model.gguf is a 24-byte empty header.

## 4. DECISION D-013 restated for users (this slice's assignment)

Filed at Z-082, owed since Z-084's finding T-084-1, discharged
here. D-013 (decided at Z-078, banked in docs/DECISION_LEDGER.md)
settles what a zero-size request means in this allocator. In
plain words, for anyone using the namespaced API or the
interposing variant:

- Asking for zero bytes is a DEFINED REFUSAL, not a failure and
  not a tiny allocation. malloc(0), calloc with any zero factor,
  realloc(NULL, 0), reallocarray with a zero factor, and the
  aligned entries with size 0 return exactly NULL. No storage is
  allocated and no ownership passes. posix_memalign with size 0
  returns ENOMEM and leaves the output pointer untouched.
- This refusal is NOT counted as an allocation failure. It is
  counted separately (alloc_posture_nulls, now counting contract
  nulls). A program - or a monitoring dashboard - must not read
  a D-013 NULL as out-of-memory.
- realloc(p, 0) NEVER frees your block. It returns NULL and the
  block stays live, contents intact, still yours to use, grow or
  free. This is a recorded deviation from glibc, whose manual
  reading frees the block; code ported from glibc assumptions
  must not free-or-forget on a realloc-to-zero NULL.
- errno is never promised. On zero-size paths and every checked
  refusal that never reaches the operating system, errno is left
  exactly as it was on entry. Do not branch on errno after these
  calls; branch on the return value.
- Why it is decided this way (D-013's recorded grounds): a live
  block here always has a non-zero usable extent, so a zero-size
  live block cannot be represented honestly; a uniform realloc
  NULL with the original always preserved leaves no ownership
  trap for a caller to audit (the freeing reading is the classic
  trap - C23 made realloc(p, 0) undefined for this divergence);
  and several refusal classes have no honest errno value.
- The one place this contract BITES under preload (T-084-1):
  glibc's own regex compiler, on the leading-anchor path only,
  asks for zero bytes and treats the NULL as OOM. Under the
  interposing variant, regcomp of a leading-^ pattern returns
  REG_ESPACE (12) where glibc returns 0, and grep with such a
  pattern exits 2 with Memory exhausted - even on a 3-line file.
  Unanchored patterns, -F, and every non-zero allocation path
  are unaffected (realloc growth to 128 MiB passed; 20,000-line
  sorts byte-identical). If a program you preload compiles
  anchored regular expressions through glibc regcomp, expect
  this failure; it is the contract behaving as decided, filed
  here so no user has to rediscover it.

## 5. Rollback and the never-preload list (Z-089, FINAL)

Z-089 (committed 615c935 on team-06, gate GREEN 4988/0/77,
queueing for banking behind Z-087/Z-088) proved rollback from a
deliberately broken state: a spawned harness under a poison
preload (malloc family always NULL) failed deterministically
(rc 3, twice); the environment diff between broken and
rolled-back was exactly ONE line (LD_PRELOAD removed, nothing
added); the rolled-back harness exited 0 byte-identical to the
glibc control. Live PID 1, sddm, plasmashell and kwin PIDs were
identical before and after. Drill result: PASS, 24 [ok] / 0 [FAIL].
One runner bug was found and fixed in scope (the host env binary
allocates at startup and died under the poison; replaced by a
write(2)-only zero-allocation dumper, filed in the ops doc).

NEVER preload, restated from docs/Z13_Z089_ROLLBACK_NEVER_PRELOAD.md
S2 (the ops doc owns the full reasoning; this is its list):
1. systemd - PID 1 and the user manager (breakage there is a
   broken machine; PID 1 cannot be respawned without the variable).
2. The display manager (sddm) - rollback must never depend on a
   component the breakage disables.
3. The banking tooling itself - git, ssh, gh, the keeper scripts
   (the road that banks, verifies and rolls back must run on
   stock glibc; Z-084's one-shot spawned git subject is evidence
   about a spawned subject, not a licence to preload the seat).
4. The live desktop session - plasmashell, kwin_wayland, the
   live session bus (only contained duplicates are proven).
5. Package management and repair tooling - apt/dpkg, ldconfig,
   the compiler/linker (the repair path must not inherit breakage).
6. sshd and any remote-recovery path - never preload the door
   you would climb back through.
7. Long-lived shared services generally - the host Ollama system
   service is the filed example (other users inherit the
   experiment; a restart is not a one-variable rollback for them).
8. Backup and restore tooling - untested in every banked slice,
   and its failure mode is the silent kind T-084-1 demonstrated.
9. Performance-critical Python jobs and Ollama / model loaders -
   excluded on the measured Z-085 numbers (section 6), until a
   later slice re-proves them.
The proven pattern for everything else (Z-089 S3): per-command
environment only, on a process you spawn, with a glibc control
leg; never export LD_PRELOAD in a profile, never write
/etc/ld.so.preload, never bake it into a unit or a session.

## 6. Performance standings of record (this machine only)

Wave 10's only quiet-window graded performance slice is Z-085;
its numbers are the wave's performance verdict. Z-087's
contended observation is recorded separately (section 3.6).
No WIN is claimed anywhere in Wave 10.

| Workload (Z-085) | Grade | Numbers of record |
|---|---|---|
| Python TOTAL (import storm + churn) | LOSS | median 0.2772 s plain vs 1.3876 s preload = 5.01x |
| Python imports | LOSS | median 0.0411 s vs 0.0646 s = 1.57x |
| Python churn | LOSS | median 0.2367 s vs 1.3230 s = 5.59x |
| Python peak RSS | LOSS | median 68,340 KiB vs 76,192 KiB = +11.5% |
| Ollama model load | LOSS | load_duration 0.542 s plain vs 12.920 s / 12.093 s preload, two legs = ~22-24x |
| Ollama prompt_eval | TIE | 0.0296 s vs 0.0244 s, inside noise |
| Ollama generation eval | TIE (near) | 0.0367 s vs 0.0409 / 0.0411 s on 8 tokens, inside run-to-run spread |
| Ollama children RSS | LOSS (small) | +18,492 KiB = +3.6% under preload |

Load honesty: Z-085's run of record ran in a quiet window
(loadavg ~0.5-0.67, no cross-campaign benchmark observed);
deltas of 5x and 22x are far outside contention noise at that
load, and the Python picture reproduced across two full runs.
The losses are measurements of record for the interposing
build: the small-object path is markedly slower than glibc and
the model-load path pays ~12 s per cold load. They belong to
the roadmap's perf lines; no code was changed on the strength
of this single-workload slice, and none is claimed here.
Correctness grades are separate and are PASS everywhere FINAL:
Z-084 battery PASS, Z-085 correctness PASS (exact equality),
Z-086 battery PASS, Z-089 drill PASS. Z-087 (folded in) adds a
contended-window observation, NOT a baseline grade: wall +5.1%,
decode +6.3%, minor faults +14.1% under preload at loadavg
1.98-4.43, correctness TIE - recorded in section 3.6, kept out
of the table above because the table holds quiet-window grades.

## 7. OWED sections and fold-ins - named, not hidden

### 7.1 Z-087 - Aesir boundary trial - FOLDED IN (was OWED)
Fold-in note: in this verdict's first commit (86cde4c) this
section stood OWED-DRAFT - team-03's recorded run was on disk
but the slice was uncommitted, with no doc and no final gate,
and its draft facts were quoted as DRAFT only. Minutes later
the slice committed as 816a6e4 (4 files: Makefile,
docs/Z13_Z087_AESIR_BOUNDARY_TRIAL.md,
tests/abi/recorded_run_z13_z087.txt, tests/abi/run_z087.sh),
with its final standing gate GREEN 4988/0/77 read back from
hidden_files/z087_gate_final.log. This lead re-read the
committed doc and recorded run first-hand; the section is now
FINAL as section 3.6 above (correctness TIE; contended-window
LOSS observation, not a baseline; three Aesir-side blockers
named not fixed). The slice is committed, not yet banked; it
queues first in the remaining banking order.

### 7.2 Z-088 - Failure drills under preload - OWED (not built)
State on disk at verdict time (team-04 worktree, first-hand):
merge c575828 of seat cb5bf36; baseline standing gate GREEN
4988/0/77 (hidden_files/z088_gate_baseline.log); NO slice
artifacts - no runner, no recorded run, no doc, working tree
clean. There is no evidence to quote, draft or otherwise.
OWED in full: the drill log the roadmap gate requires (loud
failure through the diag channel under corruption and kill
drills inside preloaded processes, or any silent path filed as
a defect with an owner slice). Until Z-088 banks, this verdict
makes NO claim that Wave-8 hardening fires loudly under real
preload field conditions - the wiring is proven in-process by
Wave 8, unproven under preload. That is the wave's largest
honest gap and it is stated at full size here.

## 8. What this verdict does NOT prove

- The live desktop session under preload: never tested, by
design (Z-086 containment; Z-089 entry 4). Contained evidence
must not be quoted as live-session evidence.
- Full plasmashell under preload: ENV-ONLY, untested as
allocator evidence (identical segfault on the glibc control).
- Long-running or multi-hour preload stability: every FINAL
run is a battery or a short workload; no soak exists in Wave 10.
- Any program class outside the filed batteries: the safe list
in section 1 names tested classes, not all programs. The
T-084-1 class (anchored regcomp) is the standing warning that
untested callers can meet contract divergences.
- Performance beyond Z-085's two workloads, and any BASELINE
timing from the contended Z-087 window (observation only).
- Failure behaviour under preload (Z-088, OWED, section 7.2).

## 9. Evidence index (traceability - the Z-090 gate)

FINAL sources, re-read first-hand on the merged tree:
- Z-082 seat 23a05d0: docs/Z13_Z082_INTERPOSING_VARIANT.md;
  tests/abi/recorded_run_z13_z082.txt (PASS, 20 exports, probe
  pair GLIBC/MIMIR); DECISION_LEDGER D-015.
- Z-083 seat ad63bb9: docs/Z13_Z083_GAP_CLOSURE.md;
  tests/abi/recorded_run_z13_z083.txt (PASS, 13 exports,
  test_z083 301/0); DECISION_LEDGER D-016.
- Z-084 seat 39a7fc8: docs/Z13_Z084_PRELOAD_BATTERY.md;
  tests/abi/recorded_run_z13_z084.txt (PASS, 74 ok / 0 FAIL,
  finding T-084-1); DECISION_LEDGER D-013 (via Z-078).
- Z-085 seat 9a568b2: tests/perf/recorded_run_z13_z085.txt
  (correctness PASS exact; Python LOSS 5.01x; Ollama load LOSS
  ~22-24x, generation TIE; quiet window loadavg ~0.5-0.67).
- Z-086 seat cb5bf36: docs/Z13_Z086_DESKTOP_BATTERY.md;
  tests/abi/recorded_run_z13_z086.txt (PASS, 15 ok / 0 FAIL;
  plasmashell ENV-ONLY; kwrite MISSING-APP).
- Z-089 team-06 commit 615c935 (committed, gate GREEN,
  unbanked at verdict time): docs/Z13_Z089_ROLLBACK_NEVER_PRELOAD.md;
  tests/abi/recorded_run_z13_z089.txt (PASS, 24 ok / 0 FAIL;
  one-variable rollback proven; live PIDs unchanged).
D-013 sources: docs/DECISION_LEDGER.md D-013;
  docs/Z13_Z078_ZERO_SIZE_ERRNO.md (measured glibc contrast).
- Z-087 team-03 commit 816a6e4 (committed, gate GREEN,
  unbanked at fold-in time): docs/Z13_Z087_AESIR_BOUNDARY_TRIAL.md;
  tests/abi/recorded_run_z13_z087.txt (FAIL=0, token streams
  identical, Aesir tree unchanged; timings contended).
OWED source (NOT evidence of record): Z-088: no artifacts exist
  (team-04 baseline gate only).
This slice's own fresh executions: the baseline gate on the
merged tree and the standing gate on the final tree, both
GREEN 4988/0/77 (recorded run, tests/perf/recorded_run_z13_z090.txt).
No new measurement is derived by this slice: every number is a
synthesis of the sources above, in the Z-072 verdict pattern.

## 10. Verdict - the Wave 10 standings of record

- Interposition exists and is opt-in only: a separate library,
  20 exports, default library unchanged at 13 namespaced
  symbols (Z-082, Z-083). FINAL.
- Correctness under preload is PROVEN for the filed classes:
  CLI/coreutils/git/bash/python3 subjects (Z-084), a real
  Python workload and a short Ollama generation with exact
  output equality (Z-085), and contained KDE applications
  (Z-086). FINAL.
- Performance under preload is a measured LOSS where it was
  graded: Python 5.01x overall, Ollama model load ~22-24x,
  generation TIE (Z-085). No WIN claimed. FINAL.
- Exactly one compatibility break is known: the D-013
  zero-size contract vs glibc anchored regcomp (T-084-1),
  restated for users in section 4. It is a decided contract,
  not a defect; no fix is owed and none is filed. FINAL.
- Rollback is PROVEN one-variable from a broken state, and the
  nine-entry never-preload list is ops law for this host
  (Z-089). FINAL (committed; banking queues in order).
- The Aesir boundary trial is FOLDED IN as FINAL (Z-087,
  section 3.6): correctness TIE on the token path, contended
  LOSS observation, blockers named not fixed.
- OWED: the failure drills only (Z-088 - not built). A second
  fold-in amendment completes this verdict when Z-088 banks,
  in the Z-063 pattern. Wave 10 is therefore 8 of 9 slices
  FINAL at this writing (Z-082..Z-087, Z-089, and this
  verdict), with Z-088 the named remainder.

Bugs found/fixed by this slice: none in the allocator, the
suite, or any source slice. The two defects Wave 10 itself
found were fixed inside their own slices (Z-085's runner
model-store discovery; Z-089's env-dump runner bug); T-084-1
was triaged in its own slice as a contract divergence, not a
defect. This synthesis discovered no new defect; the gaps it
carries (sections 7-8) are named with their owner slices.
