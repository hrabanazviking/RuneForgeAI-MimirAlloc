# Z-088 — Failure Drills Under Preload

Wave 10 (Wire Up + RC). Roadmap gate (docs/MIMIRALLOC_Z13_ROADMAP.md, Z-088):

> Corruption and kill drills inside preloaded processes: the
> allocator must fail loudly through the diag channel, never corrupt
> silently (Wave-8 wiring under field conditions). Gate: drill log
> shows loud failure or the silent path is filed as a defect with an
> owner slice.

This slice meets the gate both ways the gate allows: the corruption
drills that the allocator can see fail loudly through the J-007 diag
channel (recorded below), and the two silent paths the drills exposed
are filed here as defects F-Z088-1 and F-Z088-2 with owner slice
Z-090 (Wave 10 verdict).

## Posture and safety law

SOLO-SEQUENTIAL: the team-04 lead ran all six Mythic Engineering
roles itself (Designer, Builder, Tester, Repairer, Gatekeeper,
Banker-preparer); no sub-agents were spawned for this slice.

SAFETY LAW (binding for this slice, from the campaign muster):
drills run only inside processes the drill runner itself spawns in
this worktree; no faults are injected into any running user or
system process; kill drills signal only PIDs the runner itself
spawned. The runner enforces this by construction: every victim is
`build/drill_z088`, started by the runner itself; the kill drills
keep the victim's PID from the runner's own spawn line and signal
only that PID.

## What was built

- `tests/abi/drill_z088.c` — the drill victim: an ordinary glibc
  consumer linked against no MimirAlloc artifact (-ldl only), so
  every effect observed under LD_PRELOAD is the interposing
  library's, not a linked test's. It resolves the public statistics
  snapshot inside the preloaded process via dlsym and prints facts
  only; the runner classifies. Modes: control, doublefree,
  doublefree-calloc, poison, overflow, badfree-int, badfree-foreign,
  badfree-large, huge, churn (the kill target). Compiled
  -fno-builtin so no builtin malloc-family assumption can rewrite a
  drill call (the Z-082 runner's note, applied here too).
- `tests/abi/run_z088.sh` — the runner: builds from clean, runs 15
  drill sections (S1-S16 with S16 the classification summary),
  writes one drill log per drill to
  `hidden_files/z088_<name>.log`, and exits non-zero on any failed
  check. Its stdout is the recorded run.
- `tests/abi/recorded_run_z13_z088.txt` — the recorded run
  (2026-11-13T02:46:21Z, purpleraider, kernel 7.3.0-10-generic,
  gcc 15.3.0, glibc 2.44, page size 4096, base head c575828):
  89 checks [ok], 0 [FAIL], "drills: total=15 ok=15 failed=0",
  "Z-088 RESULT: PASS".
- Makefile: standalone `make test-z088` target (victim rules +
  runner); NOT part of `make test`, so the standing gate counts are
  untouched.

Every drill leg first proves interposition with the D-013 malloc(0)
verdict; the plain (no-preload) leg in S2 proves the same binary
under glibc reports GLIBC and exposes no snapshot — so a missing or
mismatched preload can never produce a silent green.

## Drill outcomes (exactly as recorded)

Interposition in the victim is inherited across exec and spawns: no
drill re-preloads anything; each victim is a fresh process started
by the runner with LD_PRELOAD set, which is how real consumers meet
the library.

| Drill (section) | Fault | Outcome as recorded |
| --- | --- | --- |
| control (S3) | none | Clean exit 0; all detection/failure counters 0; stats struct self-validates (struct_size 1056 = sizeof 1056). |
| doublefree (S4) | free the same 64-byte block twice, diag on | LOUD: exactly one diag line `op=free cause=double-free`; `double_free_detections` 0->1; second free refused and returned; victim survives and allocates cleanly afterwards. |
| doublefree, diag off (S5) | same fault, MIMIRALLOC_DIAG unset | Detection without loudness, by the D-011 ruling: df 0->1, zero diag lines. Detection is always-on; emission is the opt-in. Not a drill failure. |
| doublefree-calloc (S6) | calloc block freed twice | LOUD: same diag line; df 0->1. The calloc course is guarded identically. |
| poison (S7) | use-after-free write into the freed block's poison range, then realloc the class | LOUD at handout: diag line `op=malloc cause=poison req=64`; `poison_detections` 0->1; the class was re-served from the very block that was poisoned (same pointer), so the detection is proven to guard that block's reuse. |
| overflow (S8) | write from a live block past its usable end into a freed adjacent neighbour's poison range | LOUD at handout: diag line `op=malloc cause=poison req=64`; poison 0->1. Adjacency located at run time (delta 64 = usable 64); the neighbour was re-served. |
| badfree-int (S9) | free(p+16), then free(p) properly | SILENT-SAFE: no diag line, no counter; live_small 1 -> 1 -> 0 across alloc / interior-free / proper-free proves the interior free freed nothing and broke nothing; the proper free afterwards is a clean first free (df stays 0); 256-block pattern churn clean. Filed F-Z088-1. |
| badfree-foreign (S10) | free of a pointer the allocator never handed out | SILENT-SAFE: no diag line, no counter, no state change; churn clean. Filed F-Z088-1. |
| badfree-large (S11) | free a 20000-byte mapping-path block twice | SILENT, state-safe: no diag line; first free retires the mapping (live_map falls), second free changes no state; no double-free tally on the large course. Z-070's banked scope exclusion behaving as designed. Filed F-Z088-2. |
| huge (S12) | malloc(2^62); calloc(2^32, 2^32); reallocarray(NULL, 2^33, 2^33) | LOUD three times, one diag line per refusal: `op=malloc cause=backing req=4611686018427387904`, `op=calloc cause=arithmetic req=18446744073709551615`, `op=realloc cause=arithmetic req=18446744073709551615`; all three returned NULL (no wrapped allocation); `alloc_failures` 0->3; allocator healthy afterwards. |
| kill SIGKILL (S13) | SIGKILL to a runner-spawned churn victim mid-churn | wait status 137 (128+SIGKILL); victim reaped, PID gone. |
| kill SIGTERM (S14) | SIGTERM to a runner-spawned churn victim mid-churn | wait status 143 (128+SIGTERM); victim reaped, PID gone. |
| post-kill control (S15) | none, fresh process after the kills | Clean: counters all 0, zero diag lines. The allocator holds no cross-process state, so a killed preloaded process contaminates nothing — proven, not assumed. |

## Classification

- LOUD through the diag channel (the roadmap's demand): doublefree,
  doublefree-calloc, poison, overflow, and all three unsatisfiable
  requests. In every loud case the victim process survives: the
  allocator refuses the bad operation, reports it, and keeps
  serving. No drill corrupted allocator state.
- Detection without loudness: doublefree with the diag gate closed
  (D-011 posture, working as banked).
- SILENT paths: badfree-int, badfree-foreign, badfree-large —
  filed below. None of them corrupted anything in these drills;
  the gap is loudness only.

## Defect filings

### F-Z088-1 — interior-pointer and foreign-pointer frees return silently

A free of a pointer the allocator cannot find (an interior pointer
into a live block, or a pointer it never handed out) returns with
no diag line and no counter. Non-corruption is PROVEN in this run:
live counts unchanged by the bad free, the original block frees
properly afterwards, and a 256-block pattern churn is clean. The
public header already declares these calls undefined behaviour with
no defined error, so this is a loudness gap against the roadmap's
"fail loudly" sentence, not a correctness defect. OWNER: Z-090
(Wave 10 verdict) — restate for preload users; any code change (a
membership check on the free path) is a later hardening slice's to
propose and price, not this slice's to smuggle in.

### F-Z088-2 — large-block double-free is silently ignored

A second free of a large mapping-path block is ignored with no
diag line and no tally. This is Z-070's banked scope exclusion
(membership and poison guard the small classes only) behaving as
designed, and it is state-safe: the mapping is already retired by
the first free (live_map proof in S11). OWNER: Z-090 — restate
beside F-Z088-1 in the verdict so preload users see both limits in
one place.

## Bugs found and fixed in this slice's own code

- Drill-victim compile under the house flags (-Werror): the first
  huge-request expressions constant-folded (calloc/reallocarray
  overflow warnings as errors). Fixed by routing every huge size
  through volatile variables — the requests genuinely reach the
  allocator at run time, which the three LOUD refusals in S12
  prove.
- No allocator (library) bugs were found by these drills: every
  silent path is either the banked D-011 posture, header-declared
  UB, or Z-070's banked scope exclusion, and all three are filed
  or cross-referenced above rather than patched out of scope.

## Gate

Standing gate on the final tree
(`hidden_files/z088_gate_final.log`): GREEN — checks=4988,
failed=0, skips=77, identical to the slice baseline
(`hidden_files/z088_gate_baseline.log`). The Z-088 battery is
standalone (`make test-z088`) and does not enter the gate counts,
per the Wave-10 house pattern.

Banking is the keeper's, in roadmap order after Z-087.
