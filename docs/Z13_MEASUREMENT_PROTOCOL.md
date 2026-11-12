# Z13 Measurement Protocol — pinning, affinity, repetition

Slice: Z-004 — Measurement law: pinning, affinity, repetition.
Status: this document IS the measurement law for the MimirAlloc Z13
line. Roadmap Law 3 ("Pinned methodology") points here: every bench
run on this line records its Z-002 environment header, follows the
interleave / repetition / median rules below, pattern-touches every
block, and proves workload equivalence across legs by identical
checksums (the K-002 discipline).

Implements: `scripts/z13/run_measured.sh` (the harness switches) and
`scripts/z13/summarize_measured.py` (median, agreement and drift
verdicts). Demonstration evidence: `tests/perf/recorded_run_z13_z004.txt`
(two identical legs agreeing inside the noise bound of section 7).

## 1. First principle

An unstamped timing is not evidence. On this shared host (the Aesir
forge, the Lifthrasir forge and the Tensile lab share the machine) a
number without its environment stamp, its pinning, its repetition
count and its load state proves nothing and may not be quoted as a
baseline. A proven loss recorded under this protocol is a result
(roadmap Law 2); a flattering number recorded outside it is not.

## 2. Environment stamp (governor / frequency recording)

Every measured run embeds, verbatim and unedited:

1. The full output of `scripts/z13/host_truth.sh` (Z-002) captured
   at run start — CPU model, governor, current/min/max frequency,
   cache topology, THP state, page size, glibc and kernel versions.
2. The load state at run start and run end (`/proc/loadavg` plus a
   plain-words note of known co-tenant work, e.g. "Tensile bench
   running" or "no known co-tenant load").
3. The exact harness switches of section 11 (pin list, reps,
   warm-ups, noise and drift bounds) and the two leg commands.

The protocol RECORDS the governor and frequency; it never changes
them. Changing the governor needs root, is system-wide (it would
corrupt every co-tenant's timings, not just ours), and is out of
scope for campaign runs. If the recorded governor is ever not
`performance`, the run header says so and cross-session comparisons
against `performance` runs are labelled NOT COMPARABLE. If a human
ever changes the governor by hand for an experiment, the original
value is recorded, restored in the same session, and the restore is
verified by re-reading it — that is an experiment note, never a
campaign baseline run.

## 3. Pinning and affinity (CCD law)

Host topology, measured on the seat host 2026-11-12 (`lscpu`,
`/sys/devices/system/cpu/cpuN/topology/die_id`): AMD Ryzen AI MAX+
395, 16 physical cores / 32 threads, 2 CCDs, one NUMA node (UMA).

- CCD0: physical CPUs 0-7, their SMT siblings 16-23.
- CCD1: physical CPUs 8-15, their SMT siblings 24-31.

Rules:

1. Every measured process is pinned with `taskset -c <list>`. An
   unpinned measurement is invalid.
2. Single-threaded legs pin to ONE physical core. The campaign
   default is CPU 2 (CCD0; CPU 0 is left to housekeeping). The pin
   actually used is recorded in the run header.
3. Multithreaded legs pin to whole CCDs (e.g. `0-7` for one CCD,
   `0-15` for both), one thread per physical core. SMT siblings
   (16-31) are used only when SMT behaviour is itself the measured
   variable, and the header says so.
4. Both legs of a comparison use the IDENTICAL pin list. Pinning
   one leg and not the other, or pinning legs to different CCDs,
   invalidates the comparison (cross-CCD traffic would be charged
   to the allocator).
5. The pin is verified, not assumed: the runner records the
   requested mask in the header, and a pin that `taskset` refuses
   (non-zero exit) aborts the run before any number is taken.
6. Topology is re-derived live (`lscpu -e`, topology sysfs) when a
   run's header is written for a new host or kernel; the table
   above is this host's measured truth, not a universal constant.

## 4. Warm-up

1. Harness-level: the K-002 harness already runs an untimed warm-up
   pass at 1/10 scale inside every process; that stays.
2. Process-level: before the first measured process, each leg runs
   `--warmups` full processes (default 3) whose output is discarded,
   interleaved A,B like the measured processes. Warm-up exists to
   bring frequency, caches and page tables to steady state before
   the first kept number; a run with warm-ups skipped is labelled
   NO-WARMUP and is not a baseline.

## 5. Interleave

Measured processes alternate leg-by-leg for the whole run:
A,B,A,B,... (the K-002 runner's order, kept). Running all of leg A
and then all of leg B is forbidden: slow machine drift (thermal,
frequency, co-tenant load) would land on one leg only. Interleave
makes drift land on both legs, and section 8 catches what is left.

## 6. Repetition and the median-of-processes

1. Repetitions are separate PROCESSES, never loops inside one
   process (a fresh process re-pays mapping, cache and frequency
   reality every time).
2. Default `--reps 7` processes per leg; minimum 5; headline /
   baseline-of-record runs use 9.
3. The headline figure for a (leg, workload) is the MEDIAN of its
   per-process `ns_per_op` values. Min and max are always printed
   beside it as the spread. A single process's number is never a
   verdict, and a mean is never the headline (one contended process
   would drag it).

## 7. Noise bound (identical-legs agreement)

Two IDENTICAL legs (same binary, same pin, same switches) must
agree: for every workload,

    agreement % = 100 * |medianA - medianB| / mean(medianA, medianB)

must be at most the noise bound, default `--noise-pct 5` (5%).
A workload outside the bound reads DISAGREE and invalidates the
run pair: the machine was too noisy for that workload at that
moment — re-run, do not average the disagreement away and do not
widen the bound after seeing the numbers. A bound change is a
protocol amendment, recorded here with its measured justification,
never a per-run edit. This bound is what the Z-004 demonstration
run is gated against. The same bound is the smallest claimable
effect between DIFFERENT legs: a median difference inside 5% is
WITHIN NOISE and no win/loss claim may be made from it.

## 8. Drift rejection

For each (leg, workload), split the per-process values in run
order into first and second halves and compare their medians:

    drift % = 100 * |median(second half) - median(first half)|
              / median(all values)

Drift above `--drift-pct 8` (8%) marks that (leg, workload)
DRIFT-REJECT: the machine moved during the run (a co-tenant
started or stopped, frequency or thermals shifted), so the medians
cannot be trusted even if the legs happen to agree. A run carrying
a DRIFT-REJECT or a checksum failure (section 9) is void as a
baseline: it is kept in the team's logs with its reason, and the
measurement is re-taken. Voided runs are never silently deleted
from the record of attempts.

## 9. Checksum equivalence

Per workload, the set of checksums printed across BOTH legs and
all repetitions must contain exactly ONE value (the K-002
discipline: pattern-touch every block, print the checksum, a
silent no-op would show). More than one distinct checksum means
the legs did different work: stop, the comparison is void, and
the cause is found before any number is quoted.

## 10. Contention and baselines

1. The run header records load at start and end (section 2). A run
   taken while other campaigns' heavy work is known to be running
   is labelled CONTENDED in plain words in its header.
2. CONTENDED numbers are indicative only: they may prove a loss
   table's existence (as Z-003's first table did) but are NOT
   baselines and may not be compared against quiet-machine runs.
3. A baseline-of-record run requires: all of sections 2-9 green,
   no DRIFT-REJECT, checksums clean, and no known heavy co-tenant
   load during the run.
4. Thermal honesty: on this thin chassis, long bench series heat-
   soak the SoC. Series longer than a few minutes insert pauses
   between process groups, and a run started on a heat-soaked
   machine is labelled HEAT-SOAKED with the same not-a-baseline
   force as CONTENDED.

## 11. Harness switches (`scripts/z13/run_measured.sh`)

The switches are the protocol in executable form. Environment
variables set defaults; flags override them.

| Switch | Env | Default | Meaning |
|---|---|---|---|
| `--leg-a <path>` | `Z13_LEG_A` | (required) | Leg A executable |
| `--leg-b <path>` | `Z13_LEG_B` | (required) | Leg B executable |
| `--out <prefix>` | `Z13_OUT` | (required) | Output prefix: `<prefix>.header`, `<prefix>.legA.raw`, `<prefix>.legB.raw` |
| `--cpus <list>` | `Z13_CPUS` | `2` | `taskset -c` pin list, identical for both legs (section 3) |
| `--reps <n>` | `Z13_REPS` | `7` | Measured processes per leg, minimum 5 (section 6) |
| `--warmups <n>` | `Z13_WARMUPS` | `3` | Discarded warm-up processes per leg (section 4) |
| `--noise-pct <p>` | `Z13_NOISE_PCT` | `5` | Identical-legs noise bound, % (section 7) |
| `--drift-pct <p>` | `Z13_DRIFT_PCT` | `8` | Drift rejection bound, % (section 8) |

Verdicts are computed by `scripts/z13/summarize_measured.py
<legA.raw> <legB.raw> [--noise-pct P] [--drift-pct P]`, which
prints per-workload medians for both legs, agreement %, drift %
per leg, checksum status, and a final PROTOCOL GATE: PASS/FAIL
(exit code 0 only on PASS).

## 12. What a recorded run under this protocol contains

1. Header: slice, date, base commit, the section-2 environment
   stamp, the section-11 switches, load state in plain words.
2. The raw CSV lines for both legs, complete (no cherry-picking).
3. The summarizer's table and verdict lines.
4. The standing correctness gate counts for the same tree
   (`make test`, read from its log file).
5. A verdict paragraph in plain words: what agreed, what was
   rejected, and whether the run is a baseline or indicative only.

Amendment (Z-004 demonstration day, measured): the warm-up default
rose from 1 to 3 processes per leg after demonstration attempts 1-7
showed a cross-process ramp in large_batch (the first measured
processes ran about 10 percent slower than later ones with only 1
warm-up process). The noise bound (5 percent) and drift bound
(8 percent) were not changed. Attempt logs: team hidden_files.
