# Failure capture harness (slice A-005)

## What this is

A small, standard-library-only harness that runs one described command
(a "case"), records what actually happened on this device, and emits a
self-checking evidence packet that anyone can verify afterwards.

- Harness: `scripts/failure_capture.py`
- Recorded run in this slice: the synthetic smoke case
  `scripts/failure_cases/smoke_fail.json`, captured on Raspberry Pi Mimir
  (device `purpleviking1`, worktree `/home/pi/mimiralloc-team-05`,
  branch `yrsa/mimiralloc-05`).
- Evidence level: **Device-tested (harness, on Mimir)** — the packet
  below was produced and verified on the device itself.

## What this is not

- The smoke case is **synthetic**. Its workload,
  `scripts/failure_cases/smoke_fail.sh`, prints two lines and exits 3 on
  purpose. It is **not** the Mojo failure and **not** evidence about the
  Mojo/tcmalloc blocker.
- This is **not a Phase 1 exit-gate artefact**. The roadmap Phase 1 exit
  gate requires reproducing the actual tcmalloc/Mojo problem; no such
  failure, revision, or log exists in this repository yet (see
  "Roadmap inconsistencies" below).
- The manifest's own `claim_scope` says the same thing, in the packet
  itself:

  > harness-evidence-only: proves this command ran here with this
  > result; not evidence of the Mojo/tcmalloc blocker

- A packet proves only that the recorded command ran on the recorded
  host with the recorded result. It proves nothing about why any other
  command fails anywhere else.

## File map

| Path | What it is |
|---|---|
| `scripts/failure_capture.py` | The harness CLI (`run` and `verify` subcommands). Python 3, standard library only. |
| `scripts/failure_cases/` | Case files. This slice adds `smoke_fail.json` (the case) and `smoke_fail.sh` (its deliberate-failure workload). |
| `tests/failure/` | Test home for the failure-capture line (reserved in the layout for this slice's tests). |
| `evidence/runs/` | Captured packets, one directory per run: `evidence/runs/<run_id>/`. |
| `docs/failure-capture.md` | This document. |

## Case-file fields

A case is a single JSON object. The smoke case, verbatim in structure:

| Field | Type | Meaning / rule in the harness |
|---|---|---|
| `schema_version` | number | Case/packet schema version. Smoke case: `1`. |
| `id` | string, required | Case identifier; sanitised into the `run_id`. Smoke case: `"smoke-fail"`. |
| `title` | string | Human-readable title, copied into the manifest. |
| `stage` | string | One of `fetch`, `compile`, `link`, `tool-startup`, `mojo-compile`, `generated-startup`, `runtime`, `other`. Smoke case: `"other"`. Unknown values are rejected. |
| `failing_process` | string | One of `build-tool`, `mojo-compiler`, `mojo-runtime`, `generated-binary`, `aesir`, `other`. Smoke case: `"other"`. Unknown values are rejected. |
| `argv` | array of strings, required, non-empty | The command to run. It must be an array, never a single shell string; every entry must be a string. |
| `cwd` | string | Working directory for the child, relative to the repository root (absolute paths are rejected). Smoke case: `"."`. |
| `env_add` | object | Extra environment variables for the child. Keys matching a secret-like pattern are refused (see "Privacy and redaction"). Smoke case: `{}`. |
| `timeout_seconds` | number | Child timeout; must be positive and at most the hard cap of 300. Smoke case: `10`. A timed-out child is killed and the run returns harness exit code 4. |
| `expected.outcome` | string | `"pass"` or `"fail"`. Smoke case: `"fail"`. |
| `expected.exit_code` | number | Expected child exit code. Smoke case: `3`. |
| `classification_hint` | string or null | Optional hint, carried into the manifest. The manifest's own `classification` is always `"unclassified"` at capture time. Smoke case: `null`. |
| `notes` | string | Free-text notes, copied into the manifest. Smoke case: `"Synthetic case. NOT the Mojo/tcmalloc failure."` |
| `env_class` | string, optional | Environment class recorded in the manifest. When absent the harness records `"arm64-linux"`. |

## Packet and manifest fields

Each run creates `evidence/runs/<run_id>/` containing exactly:

| File | Contents |
|---|---|
| `manifest.json` | The full run record (fields below). |
| `case.json` | Byte-for-byte copy of the case file that was run. |
| `command.txt` | The child's `argv`, shell-joined, one line. For the recorded run: `/bin/sh scripts/failure_cases/smoke_fail.sh` |
| `stdout.log` | Child standard output, raw. |
| `stderr.log` | Child standard error, raw. |
| `SHA256SUMS` | SHA-256 of each of the five files above, in `sha256sum` format. |

Manifest field summary (recorded run values are in the next section):

- Identity and scope: `schema_version`, `run_id`, `case_id`, `title`,
  `claim_scope`, `stage`, `failing_process`, `classification_hint`,
  `classification` (`"unclassified"`), `notes`.
- Command: `argv`, `cwd`.
- Timing and result: `started_at_utc`, `finished_at_utc`,
  `duration_ms`, `exit_code`, `signal`, `timed_out`, `outcome`
  (`"pass"` only when the child exits 0 in time, otherwise `"fail"`),
  `expected`, `matches_expected`.
- Errors: `first_relevant_error` (first non-empty stderr line, capped
  at 500 characters) and `error_lines` (up to five lines matching the
  harness error pattern, with stream and line number).
- Environment class: `env_class`.
- Host: `uname` (`sysname`, `release`, `machine` only),
  `os_release_pretty`, `page_size_bytes`, `nproc`, `libc`
  (`ldd_version`, `gnu_get_libc_version`, `summary`),
  `python_version`.
- Toolchain: `toolchain` — one `--version` first line, or `null`, for
  each allowlisted tool: `cc`, `gcc`, `clang`, `ld`, `readelf`,
  `bazel`, `mojo`.
- Target linkage: `target_ldd` — `ldd` output when `argv[0]` resolves
  to an ELF file, otherwise `null`.
- Resources: `meminfo_before` / `meminfo_after` (only `MemTotal`,
  `MemAvailable`, `SwapTotal`, `SwapFree`), `loadavg_before`,
  `df_free_bytes_cwd`, `child_peak_rss_kb`.
- Provenance: `git` (`head_sha`, `branch`, `dirty_bool`),
  `harness_sha256`, `case_sha256`, and `logs` / `stdout_log` /
  `stderr_log` entries (`sha256`, `size_bytes` per log).

Harness exit codes for `run`: `0` = captured and the result matches
`expected`; `2` = captured but does not match; `3` = harness error
(for example an invalid case or a spawn failure); `4` = child timed
out.

## The recorded run (A-005)

Only the two commands in this section and the verification commands in
the next section are presented as working commands in this document,
because they are the commands actually run for this slice, from the
worktree root on Mimir.

### Capture

```text
$ python3 scripts/failure_capture.py run --case scripts/failure_cases/smoke_fail.json
/home/pi/mimiralloc-team-05/evidence/runs/20261106T114752Z-smoke-fail-0f4a3c
{"exit_code": 3, "matches_expected": true, "outcome": "fail", "run_id": "20261106T114752Z-smoke-fail-0f4a3c", "signal": null, "timed_out": false}
```

Harness exit code: **0** (captured, and the result matches the case's
`expected`). Child exit code: **3** (deliberate). `matches_expected`:
**true**.

Key manifest values for run `20261106T114752Z-smoke-fail-0f4a3c`:

| Field | Value |
|---|---|
| `run_id` | `20261106T114752Z-smoke-fail-0f4a3c` |
| `started_at_utc` / `finished_at_utc` | `2026-11-06T11:47:52Z` / `2026-11-06T11:47:52Z` |
| `duration_ms` | `1` |
| `exit_code` | `3` |
| `signal` | `null` |
| `timed_out` | `false` |
| `outcome` | `"fail"` |
| `matches_expected` | `true` |
| `expected` | `{"exit_code": 3, "outcome": "fail"}` |
| `first_relevant_error` | `smoke: first relevant error: deliberate exit 3` |
| `env_class` | `arm64-linux` |
| `page_size_bytes` | `16384` |
| `libc` | `gnu_get_libc_version`: `"2.36"`; `ldd_version`: `ldd (Debian GLIBC 2.36-9+rpt2+deb12u14) 2.36` |
| `toolchain.bazel` | `null` (Bazel is not installed on this device; recorded as a null probe, not an error) |
| `toolchain.mojo` | `Mojo 1.1.0 (8189361e)` |
| `toolchain` (other tools) | `cc` / `gcc`: `12.2.0`; `clang`: `Debian clang version 14.0.6`; `ld`: `GNU ld (GNU Binutils for Debian) 2.40`; `readelf`: `GNU readelf (GNU Binutils for Debian) 2.40` |
| `os_release_pretty` | `Debian GNU/Linux 12 (bookworm)` |
| `uname` | `Linux`, release `6.12.112-v8-16k-hailo+`, machine `aarch64` |
| `nproc` | `4` |
| `python_version` | `3.11.2` |
| `git` | branch `yrsa/mimiralloc-05`, head `b0b837453da351700987bb563126c90be2b8978f`, `dirty_bool`: `true` |
| `child_peak_rss_kb` | `18224` |
| `claim_scope` | `harness-evidence-only: proves this command ran here with this result; not evidence of the Mojo/tcmalloc blocker` |

Child output captured in the packet:

- `stdout.log`: `smoke: deliberate failure workload ran`
- `stderr.log`: `smoke: first relevant error: deliberate exit 3`

## How to verify a packet

Three independent checks, in order. The first two were both run on the
recorded packet for this slice and both passed; their real output is
shown. The third is a comparison method, described in prose: no second
run was made for this slice, because this slice's deliverable is the
one recorded run above.

### 1. Check the hashes inside the packet

Run, from inside the run directory:

```text
$ sha256sum -c SHA256SUMS
manifest.json: OK
case.json: OK
command.txt: OK
stdout.log: OK
stderr.log: OK
```

(`sha256sum -c` itself exited 0.)

### 2. Run the harness verifier

Run, from the worktree root:

```text
$ python3 scripts/failure_capture.py verify evidence/runs/20261106T114752Z-smoke-fail-0f4a3c
OK   sha256 manifest.json
OK   sha256 case.json
OK   sha256 command.txt
OK   sha256 stdout.log
OK   sha256 stderr.log
OK   manifest log entry stdout.log
OK   manifest log entry stderr.log
OK   case_sha256
OK   matches_expected consistency
OK   timestamp order
OK   host machine arch
OK   host page size
OK   host OS pretty
OK   host libc version
VERIFY PASS
```

Verifier exit code: **0**. Beyond the file hashes, the verifier
re-checks the manifest's log entries against the log files,
`case_sha256` against `case.json`, that `matches_expected` is
consistent with the case's `expected` and the manifest's outcome and
exit code, that the timestamps are in order, and that the manifest's
machine architecture, page size, OS, and libc version still match the
host running the verifier. That last group means a packet is expected
to verify cleanly on the host class that produced it; a mismatch there
is information about the host, not by itself proof of tampering — the
hash checks are the tamper evidence.

### 3. Re-run comparison

Capturing the same case again produces a new packet with a new
`run_id` (new timestamp and random suffix). A genuine re-run should
agree with the original on the stable fields — `case_id`, `argv`,
`exit_code`, `outcome`, `matches_expected`, `first_relevant_error`,
`page_size_bytes`, `libc`, and the toolchain entries — while timings,
resource figures, and `run_id` itself will differ. Disagreement on a
stable field means the environment or the case changed, and the two
manifests show exactly which field moved.

## Privacy and redaction rules

The harness records an allowlist, not the environment:

- **Allowlist probes only.** Host data comes from a fixed set of
  probes: `uname` (system, release, machine — no hostname/nodename),
  `/etc/os-release` pretty name, page size, CPU count, libc version,
  Python version, four named `/proc/meminfo` counters, load average,
  free disk bytes, and one `--version` line for each of the seven
  allowlisted tools (`cc`, `gcc`, `clang`, `ld`, `readelf`, `bazel`,
  `mojo`). A tool that is not installed is recorded as `null`.
- **Secret-key rejection.** A case whose `env_add` contains a key
  matching the harness's secret-like pattern (`TOKEN`, `SECRET`,
  `PASSWORD`, `KEY`, or `CREDENTIAL`, case-insensitive) is refused
  with a harness error before anything runs.
- **Not recorded:** the process environment (the child's full
  `environ` is never written; `env_add` values are passed to the child
  but are not copied into the manifest), hostname, device serial
  numbers, user or home-directory identifiers, and network identifiers.
  Git provenance is limited to head SHA, branch name, and a dirty
  boolean.
- Case authors must still keep secrets out of `argv`, `notes`, and the
  workload's own output: whatever a child prints lands verbatim in
  `stdout.log` / `stderr.log`.

## Adding a real Mojo case

Do this only when the real failing command and the exact revisions are
known; until then there is nothing honest to capture.

1. Record the prerequisites first, in the case's `notes` or a companion
   note: the exact Mojo revision, the implicated tcmalloc revision and
   build options, the build/run command exactly as issued, and which
   process is believed to fail (build tool, Mojo compiler, Mojo
   runtime, generated binary, or Aesir).
2. Create a new JSON file under `scripts/failure_cases/` following the
   field table above: `argv` as an array, `cwd` relative to the
   repository root, `stage` and `failing_process` set to the real
   values (not `"other"` unless that is the truth), `env_class` set
   explicitly to the real environment class, and `expected` set to the
   outcome and exit code the real failure is expected to produce. Keep
   secrets out of `env_add` — the harness will refuse secret-like keys
   anyway.
3. Capture it with the same `run` command shape used for the smoke
   case, substituting the new case file. That exact command has only
   been run for the smoke case so far; the Mojo capture has **not**
   been run and must not be described as working until it has been.
4. Verify the new packet with the same two verification steps above,
   and state its evidence level and claim scope as narrowly as the
   smoke packet's: a Mojo packet proves that command failed on that
   host with that result — the causal claim still needs the Section 3
   classification and integration mapping.

## Roadmap inconsistencies found while building this slice

These are recorded, not resolved, by A-005. Resolving them is a
roadmap-editing decision for the project owner.

1. **No concrete Mojo failure exists in the repo yet.** There is no
   recorded Mojo failure command, revision, or failure log anywhere in
   this repository, so the Phase 1 exit gate cannot be met by A-005
   alone. A-005 delivers the capture harness and a synthetic proof of
   the capture path, nothing more.
2. **The motivating evidence is from a different environment class.**
   The field notes motivating this project come from an Android tablet,
   while the recorded run here is on an ARM64-Linux/glibc Raspberry Pi.
   Android/bionic and ARM64 Linux/glibc are distinct environments. The
   `env_class` field and the manifest `claim_scope` exist as guards so
   a packet from one class is never read as evidence about the other.
3. **Layout drift between Section 13 and Section 3.3.** Section 13's
   suggested layout has a flat `evidence/` directory and no
   `scripts/failure_cases/` or `tests/failure/` placement matching
   Section 3.3's named deliverables (`evidence/mojo-arm-repro.md`,
   `evidence/symbol-contract.md`, build logs). This slice adds
   `evidence/runs/` and `scripts/failure_cases/` as a minimal extension
   of the Section 13 layout; the two sections are not yet reconciled.
4. **Section 3.1 asks for Bazel/NDK versions unconditionally.** On
   this device Bazel is absent and no NDK is in play, so the honest
   capture is a null probe: the recorded manifest has
   `toolchain.bazel: null`. The harness records nulls rather than
   failing, and no NDK field exists in the manifest at all.
5. **Three differing Phase 1 exit gates are unreconciled.** Section
   3.3's exit gate (someone else can reproduce the failure and explain
   the integration point), Section 8 Phase 1's exit (a documented
   causal hypothesis and a repeatable validation command), and Section
   17's definition of done (items 1 and 5: a reproducible documented
   cause with the integration boundary mapped, and the original failure
   either demonstrably fixed or accurately marked blocked) state three
   different bars. Which one governs Phase 1 is not settled in the
   roadmap.
6. **Capture-field mismatch between the Engineering Guide template and
   Section 3.1.** The Guide's report template and Section 3.1's
   capture list do not name the same fields (for example Section 3.1
   asks for page size, environment class, stage, and tcmalloc revision;
   the Guide template asks for allocator revision and link mode). The
   harness implements the **union** of both lists where a field is
   capturable by an allowlist probe, and records `null` where it is
   not (as with Bazel above).
