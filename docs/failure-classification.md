# Failure Classification Framework

| Field | Value |
|---|---|
| Slice | A-006 |
| Source | Roadmap Section 3.2, "Classify the failure" |
| Companion files | `docs/failure-taxonomy.json`, `docs/classification-record.schema.json`, `docs/classification-examples.json` |
| Evidence level of this framework | Proposed |
| Classification records location | `evidence/classifications/<run_id>.classification.json` |

This framework is Proposed. It is not Implemented, it is not Host-tested, and it is not Device-tested. It has never classified a real failure. Those statements describe the framework itself, not any packet it may later be applied to.

Future classification records live at `evidence/classifications/<run_id>.classification.json`, one JSON file per packet. Records are append-only: a correction does not edit an earlier record in place. A later record supersedes an earlier one and names the record it supersedes.

## 1. Purpose and scope

This document defines how a captured failure packet is given one primary classification, optional contributing classifications, a confidence value, cited evidence, and a single next investigation step.

The input is the output of the A-005 failure-capture work: a packet directory at `evidence/runs/<run_id>/` containing exactly these six files:

- `manifest.json`
- `case.json`
- `command.txt`
- `stdout.log`
- `stderr.log`
- `SHA256SUMS`

At capture time the manifest field `classification` is always `"unclassified"`. Capture records what happened; it does not classify it. Classification happens afterwards, separately, and produces a separate record file. No step in this framework edits a packet manifest to change its `classification` value.

The framework classifies packets. It does not diagnose causes beyond packet evidence. If a cause is not supported by fields and log text inside the packet, the framework must not assert it, however plausible it may seem from outside knowledge. Where the packet cannot support a category, the correct result is a non-category outcome that says so, not a best guess presented as a finding.

No classifier code is delivered in this slice. This slice delivers the framework, its machine-readable taxonomy, its record schema, and worked examples. Any program that later automates part of this procedure must follow the gates, caps, and honesty rules defined here.

## 2. Classification model

### 2.1 The seven roadmap categories

The seven categories, their roadmap clues, and their roadmap investigation steps are reproduced in the table below. The label, clues, and investigation wording in this table are the roadmap wording.

| id | Label | Roadmap clues | Roadmap investigation |
|---|---|---|---|
| `source-compile` | Source compile | C/C++ errors, unsupported intrinsics or headers | Reproduce the single target; identify platform macros and compiler/NDK version. |
| `link` | Link | Undefined symbols, duplicate definitions, incompatible ELF or missing library | Inspect the link line, ABI, linkage and symbol table. |
| `loader-startup` | Loader/startup | Missing interpreter or shared library, startup crash | Inspect ELF interpreter, dynamic dependencies and libc environment. |
| `address-space-configuration` | Address-space configuration | Failed mapping or tag/address assumptions | Verify actual virtual-address layout and source configuration; do not infer only from CPU name. |
| `system-call-or-sandbox` | System call or sandbox | Unsupported syscall, seccomp, permissions | Capture errno and environment; compare native Linux, Android and proot. |
| `resource-pressure` | Resource pressure | ENOMEM, OOM kill, huge virtual reservation | Record RSS/PSS, address-space limits, memory pressure and kernel logs where accessible. |
| `allocator-correctness` | Allocator correctness | Reproducible invalid free, race, corruption | Minimize; use sanitizers or trace tools on an appropriate host and inspect ownership. |

A category may be assigned as primary only under the confidence and evidence rules in Section 4 and the decision procedure in Section 3.

### 2.2 Non-category outcomes

The outcomes in this section are additions made by slice A-006. They are not roadmap text. They exist because many packets, handled honestly, do not yield a category: some are not failures at all, some are synthetic by design, some do not carry enough evidence, some support two categories equally, some were not captured validly, and some belong to a different environment from the one being asked about.

Each non-category outcome is a primary outcome in its own right, with the identifier shown.

**`not-a-failure`** — The packet verifies, and the recorded run passed: `outcome` is `"pass"`, `exit_code` is 0, `timed_out` is false, and `signal` is null. There is nothing to classify as a failure.

**`expected-synthetic`** — The packet verifies, the case is synthetic as evidenced inside the packet (for example, the packet `notes` declares the case synthetic), and `matches_expected` is `true`. A synthetic case is never assigned the category it was built to resemble. The category it was designed to resemble may be named in `notes` only, in the form `designed_to_mimic`. The primary outcome remains `expected-synthetic`.

**`insufficient-evidence`** — The packet verifies and a failure occurred, but no category reaches the primary threshold defined in Section 4, or the packet is missing fields that the one plausible category requires in order to be assigned. Categories that reach only the level of possibility are listed in `possible_candidates`, not promoted to primary.

**`multiple-candidate`** — Two or more categories each meet the primary threshold, and no discriminating field in the packet separates them. The tied categories are listed in `tied_candidates`. None of them is chosen as primary.

**`harness-or-capture-error`** — Any of the following holds: `sha256sum -c` fails; the harness verify command fails on a hash, consistency, or timestamp sub-check; the packet is incomplete (it does not contain exactly the six files listed in Section 1); or capture produced no valid child result (harness exit 3). No category may be assigned to such a packet. A host-mismatch verifier sub-check on its own is not a capture error: host identity differing from the packet does not invalidate the packet, and classification proceeds with that mismatch noted.

**`out-of-scope-environment`** — The packet verifies, but `manifest.env_class` does not match the environment named in the record's `scope_question`. The category that would otherwise have been considered may be mentioned in `notes` only. It is not assigned.

### 2.3 Model rules

**R1 — Single primary.** Every record has exactly one primary: either one category or one non-category outcome. There are no co-primaries.

**R2 — Contributing categories.** The `contributing` list may contain categories only, never non-category outcomes. Each contributing entry is distinct from the primary and from every other contributing entry, carries its own directly diagnostic positive evidence, and has confidence `probable` or `possible` only. A contributing entry never carries `confirmed`.

**R3 — Never force.** A category must never be assigned merely to have an answer. Stage, exit code, or the thought that a failure "must be one of the seven" are not support for a category. If no category earns assignment, a non-category outcome is the correct primary.

**R4 — `classification_hint` is not evidence.** The `classification_hint` value is carried from the case and may be null. It is a hint supplied before the run, not an observation from the run, and it must never be cited as evidence. Likewise, the manifest `classification` value is always `"unclassified"` at capture and is not evidence for anything. Records live in separate files under `evidence/classifications/`; classification never edits a manifest.

**R5 — Gate precedence.** Gates are applied in this fixed order: `harness-or-capture-error`, then `out-of-scope-environment`, then `not-a-failure`, then `expected-synthetic`, then category / `multiple-candidate` / `insufficient-evidence`. An earlier gate that fires decides the primary, and later gates are not used to override it.

**R6 — Synthetic is always labelled.** Where a case is synthetic, the record sets `synthetic.is_synthetic` to `true` and cites the fields that prove it. Hypothetical examples are labelled as hypothetical in both their `run_id` and their `notes`, so no example can be mistaken for a real capture.

## 3. Decision procedure

Steps 0 to 6 are applied in order. No step is skippable. A step that stops the procedure states its outcome, and no later step is then used to assign a different primary.

### Step 0 — Completeness and scope read

Confirm the packet contains exactly the six files listed in Section 1. Read first, before any log analysis: `claim_scope`, `run_id`, `case_id`, `env_class`, `notes`, `classification`, and `classification_hint`. Write the `scope_question`: the single question this classification answers, naming the environment and the case the record is about. Every later gate is judged against that question.

### Step 1 — Packet verification

Run both checks and record the commands and their exit codes in `packet_verification`.

1. Inside the run directory, run `sha256sum -c SHA256SUMS`. Success is every file reported OK and exit code 0.
2. Run `python3 scripts/failure_capture.py verify evidence/runs/<run_id>`. Success is a `VERIFY PASS` result and exit code 0.

If verification fails on a hash check, a log-entry check, a `case_sha256` check, a `matches_expected` consistency check, or a timestamp check, the primary is `harness-or-capture-error` and the procedure stops.

If the only failure is a host-identity mismatch — the verifying host differs from the host recorded in the packet — that alone is not a capture error. Note the mismatch in `packet_verification` and proceed.

### Step 2 — Environment scope gate

Compare `manifest.env_class` with the environment named in `scope_question`. If they do not match, the primary is `out-of-scope-environment`, confidence is `confirmed`, and the procedure stops. The would-be category, if any, is named in `notes` only.

### Step 3 — Outcome gate

Apply the first row whose condition matches.

| Condition | Result |
|---|---|
| `outcome` is `"pass"` | Primary `not-a-failure`. Stop. |
| Case is synthetic (as evidenced inside the packet) and `matches_expected` is `true` | Primary `expected-synthetic`. Stop. |
| Capture is invalid (no valid child result; harness exit 3) | Primary `harness-or-capture-error`. Stop. |
| `timed_out` is `true` | Proceed. `resource-pressure` gains one corroborating clue only; a timeout alone is not diagnostic. |
| `signal` is 9 | Proceed. This is a corroborating clue for `resource-pressure` only. |
| `signal` is 11 or 6 | Proceed. This is a clue only; any category depends on stage and log text, not on the signal alone. |
| `matches_expected` is `false` on a real failure | Proceed on the actual outcome. Flag the mismatch in `notes`. |
| Any other non-zero exit | Proceed. The exit code is context only, not diagnostic evidence. |

### Step 4 — Clue matching

For each of the seven categories, match the packet against the per-category clue table in Section 4. Every claimed clue must cite the field and its exact value: a named manifest or case field, or an exact line from `stdout.log` or `stderr.log`. A clue that cannot be tied to a field and an exact value is not counted.

### Step 5 — Confidence and primary selection

Score each category under the confidence definitions and caps in Section 4, then select the primary under the primary-selection rules in Section 4, respecting the gate precedence in rule R5 and the contributing rules in rule R2.

### Step 6 — Emit the record

Write one record per packet to `evidence/classifications/<run_id>.classification.json`, following the record definition in Section 5. The record cites the evidence used, the negative evidence weighed, the evidence that is missing, and exactly one next investigation step.

## 4. Per-category clue table

Clues are judged using only the A-005 field vocabulary: `first_relevant_error`, `error_lines`, `stdout.log`, `stderr.log`, `exit_code`, `signal`, `timed_out`, `outcome`, `matches_expected`, `stage`, `failing_process`, `argv`, `toolchain.*`, `target_ldd`, `page_size_bytes`, `uname.machine`, `libc.*`, `env_class`, `meminfo_before.*`, `meminfo_after.*`, `child_peak_rss_kb`, `loadavg_before`, `df_free_bytes_cwd`, and `duration_ms`. A field that is null is missing evidence. A null is never negative evidence.

For each category, Positive means evidence that counts toward that category, Negative means evidence that counts against it, and Missing means evidence the packet does not contain that limits confidence. The cap is the highest confidence a single packet may reach for that category, except where a stricter condition is stated.

### 4.1 `source-compile` — cap: confirmed

- **Positive:** `stage` is in {`compile`, `mojo-compile`}; a compiler diagnostic shape appears in `error_lines` or `stderr.log` (a `file:line` followed by `error:`, a `fatal error` line naming a header not found, or an unsupported-intrinsic diagnostic); exit is non-zero, `signal` is null, and `timed_out` is false.
- **Negative:** A linker or loader message explains the failure; or `stage` is `link` or a startup stage and the failure occurs before any compile output.
- **Missing:** No compiler diagnostic line at all caps the category at `possible`. A null `toolchain` removes version corroboration; null is missing, never negative.

### 4.2 `link` — cap: confirmed

- **Positive:** `stage` is `link`; log text contains an undefined reference or undefined symbol, a duplicate symbol, a relocation error, or an incompatible-ELF message.
- **Negative:** Compiler diagnostics precede the failure and no link step was reached; or `target_ldd` resolves and the failure occurs at runtime.
- **Missing:** The packet contains no link line and no symbol table; `target_ldd` is `ldd` output only and is not a symbol table. Stage plus a non-specific error alone caps the category at `possible`. Explicit undefined-symbol or duplicate-symbol log text is required for `confirmed`.

### 4.3 `loader-startup` — cap: confirmed only with not-found or missing-library evidence; otherwise probable

- **Positive:** `stage` is `tool-startup` or `generated-startup`; `target_ldd` contains `not found`; log text names a missing interpreter or a missing shared library; or a crash occurs before any application output. A short `duration_ms` is weak corroboration only.
- **Negative:** `target_ldd` resolves every library and substantial application output precedes the failure. A null `target_ldd` is missing evidence, not negative evidence.
- **Missing:** The manifest carries no ELF interpreter field.
- **Cap rule:** `confirmed` requires either a not-found entry in `target_ldd` or log text naming a missing library or interpreter. A startup crash alone caps the category at `probable`.

### 4.4 `address-space-configuration` — cap: possible; probable only on explicit log text; never confirmed from one packet

- **Positive:** Log text reports a failed mapping (`mmap failed`, `cannot map`, or an address or tag error); or the failure occurs with a recorded `page_size_bytes` that the source does not handle; and `meminfo_before` / `meminfo_after` show ample `MemAvailable`, which separates this category from `resource-pressure`.
- **Negative:** Genuine memory exhaustion in `meminfo_before` / `meminfo_after`; or `ENOMEM` occurring under recorded memory pressure.
- **Missing:** The packet has no virtual-address layout or VA-width field, and no `mmap` errno field. The category must never be inferred from `uname.machine` or a CPU name alone.
- **Cap rule:** The single-packet cap is `possible`. `probable` is allowed only if log text explicitly names a mapping, tag, or address cause. `confirmed` is never allowed from one packet.

### 4.5 `system-call-or-sandbox` — cap: possible from one packet; probable requires comparison packets

- **Positive:** Log text contains `EPERM`, `EACCES`, or `ENOSYS`, the phrase `Operation not permitted`, or seccomp wording; and `env_class` is a sandboxed class such as proot or Android.
- **Negative:** The identical case fails identically in a second packet whose `env_class` is native `arm64-linux`. A negative claim of this kind must cite both `run_id` values.
- **Missing:** The packet has no errno field, no seccomp status, and no kernel or audit log.
- **Cap rule:** A single packet caps at `possible`. `probable` requires at least two packets with the same `case_id` and different `env_class` values, where the failure occurs in the sandboxed class only.

### 4.6 `resource-pressure` — cap: confirmed, under the corroboration rule below

- **Positive:** Log text contains `ENOMEM`, `Out of memory`, or `OOM`; `timed_out` is `true`; `signal` is 9; `meminfo_after` `MemAvailable` is much less than `meminfo_before` or near zero; `child_peak_rss_kb` is large relative to `MemTotal`; `df_free_bytes_cwd` is near zero when `stage` is a fetch or compile stage; or log text reports a huge virtual reservation.
- **Negative:** `MemAvailable` is ample before and after, `child_peak_rss_kb` is small relative to `MemTotal`, and there is no timeout, no signal, and no `ENOMEM` text. The A-005 smoke packet is the standing example of this negative pattern: `child_peak_rss_kb` 18224, with `MemAvailable` about 14.4M KB of `MemTotal` 16598576 KB. `loadavg_before` is colour only and is never diagnostic on its own.
- **Missing:** The packet has no kernel OOM log and no PSS figure. `signal` 9 alone, or a timeout alone, caps the category at `possible`.
- **Cap rule:** `confirmed` requires `ENOMEM` or OOM log text plus at least one corroborating `meminfo` or RSS field. Without that combination, the stated caps for weaker evidence apply.

### 4.7 `allocator-correctness` — cap: probable from one packet; confirmed requires reproduction evidence

- **Positive:** Log text contains `invalid free`, `double free`, `heap-buffer-overflow`, or `corrupted`, or an AddressSanitizer or ThreadSanitizer header; or `signal` is 6 or 11 at `stage` `runtime` together with corruption wording in the log.
- **Negative:** A deterministic compile, link, or loader error explains the run; or the run ends in a clean non-zero exit with no signal and no corruption wording.
- **Missing:** The packet has no stack trace, no ownership trace, and no sanitizer-run field, unless the child itself printed them into a log. The roadmap clue says "Reproducible": one packet can never establish reproducibility.
- **Cap rule:** A single packet caps at `probable`. `confirmed` requires at least two packets with the same `case_id` agreeing on the stable fields `exit_code`, `outcome`, and `first_relevant_error`, plus sanitizer evidence or minimised-reproduction evidence.

### 4.8 Confidence definitions

- **`confirmed`** — The packet is verified; the category has at least two independent positives, at least one of which is directly diagnostic log text and at least one of which is a structured field; there is no unexplained negative; no field the category requires is missing; and the category cap permits `confirmed`.
- **`probable`** — Exactly one directly diagnostic positive plus at least one corroborating field, with no contradictory negative; or a `confirmed` pattern in which one required field is missing, or whose cap is `probable`.
- **`possible`** — A directly diagnostic positive is present, but corroboration is missing or a negative remains unresolved; or the category cap is `possible`.
- **`unclassifiable`** — Used only when the primary is `insufficient-evidence` or `multiple-candidate`. It is not a confidence a category primary may carry.

A primary category requires at least one directly diagnostic positive. Non-diagnostic clues — `stage`, exit code, `signal`, or timeout alone — can corroborate, but they can never make a category primary by themselves.

### 4.9 Primary selection

The primary category is the unique category at the highest confidence that is at least `possible` and that has a directly diagnostic positive. Other categories that satisfy rule R2 are listed in `contributing`. A category that had a diagnostic positive but lost is listed in `considered_and_rejected`, with the discriminating field and its value cited as the reason.

If two or more categories tie at the top and no discriminating field separates them, the primary is `multiple-candidate` and the tied categories go in `tied_candidates`. If no category reaches the primary threshold, the primary is `insufficient-evidence` and any categories at `possible` go in `possible_candidates`.

### 4.10 Next steps for non-category primaries

For a category primary, the next step is that category's roadmap investigation step from Section 2.1, reproduced verbatim in the record. For a non-category primary, the next step is fixed by the table below. For `insufficient-evidence` and `multiple-candidate`, the sentence shown is instantiated with the specific field or test the record names.

| Primary | Next investigation step |
|---|---|
| `not-a-failure` | No investigation. Record stands as a passing capture; if a failure was expected, re-examine the case expected block. |
| `expected-synthetic` | None for any real blocker. Use the packet only as harness/capture-path proof; capture the real case when its command and revisions are known. |
| `insufficient-evidence` | Name the single most decisive missing field and how to obtain it, and obtain that evidence before classifying again. |
| `multiple-candidate` | Name the discriminating test between the tied categories, drawn from their Section 3.2 roadmap investigation steps, and run that test. |
| `harness-or-capture-error` | Repair/re-capture the packet and re-verify; do not classify the discarded packet. |
| `out-of-scope-environment` | Capture the same case in the environment named by scope_question, with env_class set explicitly, and classify that packet instead. |

## 5. Classification record

One record is written per packet, as JSON, to `evidence/classifications/<run_id>.classification.json`. The fields below define the record. The companion schema file, `docs/classification-record.schema.json`, expresses the same definition in machine-readable form.

| Field | Definition |
|---|---|
| `schema_version` | Always `1` for records under this framework. |
| `record_id` | `CLS-<run_id>` for a real packet; `CLS-EXAMPLE-<id>` for a hypothetical example. |
| `run_id` | The `run_id` of the packet classified. Hypothetical examples carry a `run_id` that labels them hypothetical. |
| `packet_path` | Path of the packet directory classified. |
| `packet_verification` | Object with `sha256sum_c` and `harness_verify`, each one of `pass`, `fail`, or `not-run-hypothetical`; plus `verified_on_host_env_class` and `notes`. Real records record the actual commands and exit codes from Step 1. |
| `scope_question` | The single question written at Step 0, naming the environment and case this record answers for. |
| `classifier` | Object with `name`, `method`, and `date_utc`, identifying who or what produced the classification, how, and when. |
| `synthetic` | Object with `is_synthetic` (boolean) and `basis` (list of the packet fields that prove or disprove synthetic status). |
| `primary` | Object with `kind` (`category` or `non-category`), `id`, and `label`. Exactly one primary per record (rule R1). |
| `contributing` | List of objects with `id`, `confidence` (`probable` or `possible` only), and `evidence_refs`. Categories only; see rule R2. Empty when none qualify. |
| `tied_candidates` | List of category ids tied at the top when the primary is `multiple-candidate`. Empty in all other records. |
| `possible_candidates` | List of category ids at `possible` when the primary is `insufficient-evidence`. Empty when the primary is a category or a tied outcome applies instead. |
| `considered_and_rejected` | List of objects with `id` and `reason`, where the reason cites the discriminating field and its value that ruled the category out. |
| `confidence` | One of `confirmed`, `probable`, `possible`, or `unclassifiable`, under the definitions and validation rules below. |
| `evidence_cited` | List of objects with `field`, `value`, `source_file`, and `supports`. `value` is exact and verbatim; log text is truncated at 500 characters. `source_file` is one of `manifest.json`, `case.json`, `command.txt`, `stdout.log`, `stderr.log`, or `SHA256SUMS`. `supports` is `primary`, `contributing:<id>`, or `gate:<step>`. |
| `negative_evidence` | List of objects with `field`, `value`, `source_file`, and `against`, recording evidence weighed against a category or outcome. |
| `missing_evidence` | List of strings, each naming one item of evidence the packet does not contain that limited this classification. Null probe results belong here; see rule H6. |
| `next_investigation_step` | Exactly one step. For a category primary, the verbatim roadmap investigation step for that category. For a non-category primary, the step fixed in Section 4.10, instantiated where that section requires it. |
| `evidence_level` | One of the seven evidence-level labels defined in roadmap Section 2, or `"Not assigned"`. See rule H2. |
| `evidence_level_basis` | The source — an existing assignment in the packet or in a source document — from which `evidence_level` was copied, or a statement that no such assignment exists. |
| `claim_scope` | Object with `packet_claim_scope`, copied verbatim from the packet manifest, and `classification_claim`, a single sentence stating what this classification claims, no broader than the packet claim scope. |
| `supersedes` | The `record_id` this record supersedes, or `null` if it supersedes none. |
| `status` | `current` or `superseded`. |
| `notes` | Free notes, including any host-mismatch note, any `matches_expected` flag from Step 3, any `designed_to_mimic` naming for a synthetic case, and any would-be category for an out-of-scope packet. |

### 5.1 Honesty rules

**H1 — Verbatim values only.** Every cited value is copied exactly from its source file. Paraphrase belongs in analysis prose, never in an evidence `value` field.

**H2 — No inferred evidence levels.** An `evidence_level` is copied only from an existing assignment found in the packet or in a source document; it is never inferred from the strength of a classification. Roadmap Section 2 assigns no evidence level to its own claims, so the default is `"Not assigned"`. Classifying a packet never raises an evidence level, and a classification record never claims `Integration-proven` on the basis of classification alone.

**H3 — No promotion.** No record may promote a result across boundaries the evidence does not cross: an emulator result is not a device result, a synthetic result is not application compatibility, and a compile result is not interposition. Claims stay inside the environment and the stage actually captured.

**H4 — Synthetic labelled synthetic.** A synthetic case is always recorded with `synthetic.is_synthetic` set to `true` and its basis cited, and it is never presented as a real failure or a real pass of a real case.

**H5 — Append-only records.** Records are append-only. A correction is made by writing a new record that supersedes the earlier one and names it in `supersedes`, following the A-002 pattern. The earlier record is marked `superseded` in status handling outside the packet; its content is not rewritten to hide the original classification.

**H6 — Nulls are data.** A null field is a recorded result, not an omission. A null probe is missing evidence: it is listed in `missing_evidence`, it is never treated as a failure, and it is never dropped from the record as if the probe had not been part of the packet.

### 5.2 Validation rules

A record is valid only if all of the following hold. These rules are stated here in summary; the companion schema and taxonomy files carry the machine-readable form.

- **V1** — A category primary is present if and only if `confidence` is `confirmed`, `probable`, or `possible`. `confidence` is `unclassifiable` if and only if the primary is `insufficient-evidence` or `multiple-candidate`.
- **V2** — Every other non-category primary (all except `insufficient-evidence` and `multiple-candidate`) carries `confidence` `confirmed`.
- **V3** — `contributing` is disjoint from the primary, and each contributing entry cites at least one directly diagnostic evidence reference.
- **V4** — The per-category confidence caps in Section 4 are respected.
- **V5** — A real record cites at least one `gate:step-1` verification item in `evidence_cited`.
- **V6** — `claim_scope.packet_claim_scope` is character-identical to the value in the packet manifest.
- **V7** — A synthetic case is never given a category primary in A-006.
- **V8** — For a category primary, `next_investigation_step` is character-identical to that category's `roadmap_investigation` value in the taxonomy.
- **V9** — A record about the A-005 smoke packet uses values that match the real manifest of that packet.
- **V10** — `tied_candidates` is non-empty only when the primary is `multiple-candidate`, and `possible_candidates` is non-empty only when the primary is `insufficient-evidence`; each is empty in all other records.
- **V11** — No `evidence_level` is stated above what its `evidence_level_basis` supports.
- **V12** — In a hypothetical record, both `packet_verification.sha256sum_c` and `packet_verification.harness_verify` are `not-run-hypothetical`.

## 6. Worked examples

This section applies the procedure of Sections 1-5 to eight records in
`docs/classification-examples.json` (taxonomy `schema_version` 1).
Example 0 is the only real record: it classifies the one real packet in
the repository. Examples 1-7 are labelled hypotheticals with invented
packet fields; they demonstrate one category each and are not evidence.

### 6.0 Example 0 — the real smoke packet, step by step

Record: `CLS-20261106T114752Z-smoke-fail-0f4a3c`
Run: `20261106T114752Z-smoke-fail-0f4a3c`
Packet: `evidence/runs/20261106T114752Z-smoke-fail-0f4a3c`
Scope question: Why did case smoke-fail produce its recorded result in
env_class arm64-linux on Mimir?

- **Step 0 — Identify the packet.** The record names the run, the packet
  path above, and env_class `arm64-linux`. Nothing outside that packet
  is classified.
- **Step 1 — Verify the packet (gate).** `sha256sum -c`: all 5 files OK
  (manifest.json, case.json, command.txt, stdout.log, stderr.log);
  harness verify: VERIFY PASS. The record cites this as its
  `gate:step-1` evidence item. A packet that fails this gate is not
  classified; it is a harness-or-capture-error (see Section 2.2).
- **Step 2 — Read the recorded facts, exactly.** From manifest.json:
  `notes` = "Synthetic case. NOT the Mojo/tcmalloc failure.";
  `expected` = {"exit_code": 3, "outcome": "fail"}; `exit_code` = 3;
  `outcome` = "fail"; `matches_expected` = true; `timed_out` = false;
  `signal` = null; `first_relevant_error` = "smoke: first relevant
  error: deliberate exit 3"; `classification_hint` = null;
  `classification` = "unclassified"; `stage` = "other";
  `failing_process` = "other"; `claim_scope` = "harness-evidence-only:
  proves this command ran here with this result; not evidence of the
  Mojo/tcmalloc blocker".
- **Step 3 — Synthetic gate.** The manifest notes and the case notes
  both say the case is synthetic, and the run matched its expected
  result exactly (exit 3, outcome fail, `matches_expected` = true).
  A matched synthetic failure is `expected-synthetic`. The procedure
  stops here for the primary outcome: no category clue matching is
  performed and no category is assigned.
- **Step 4 — Why not the alternatives.** `not-a-failure` was rejected
  because the child outcome is fail: the run did fail, by design.
  Resource pressure is contradicted by negative evidence in the same
  packet: `meminfo_after.MemAvailable` = 14442880 of
  `meminfo_after.MemTotal` = 16598576 and `child_peak_rss_kb` = 18224,
  with `timed_out` = false, `signal` = null and no ENOMEM/OOM wording.
  Allocator correctness has no diagnostic evidence: no signal, no
  corruption wording, and exit 3 is the designed exit. The remaining
  five categories each have no diagnostic evidence either; the record
  lists all seven categories under `considered_and_rejected`, each
  with the discriminating field and value that excludes it.
- **Step 5 — Confidence and caps.** Primary `expected-synthetic` is a
  non-category outcome decided by a gate, so confidence is
  `confirmed`. No category cap is engaged because no category is
  assigned.
- **Step 6 — Level, scope, next step.** Evidence level is
  `Device-tested`, taken from docs/failure-capture.md (A-005):
  "Device-tested (harness, on Mimir)"; the classification adds no
  level. `packet_claim_scope` reproduces the manifest `claim_scope`
  character-for-character. Next investigation step: "None for any real
  blocker. Use the packet only as harness/capture-path proof; capture
  the real case when its command and revisions are known."

**Conclusion (Example 0): primary `expected-synthetic`, confidence
`confirmed`, no category.** This is the honest result for the only real
packet: it proves the capture path works and says nothing about the
Mojo/tcmalloc blocker.

### 6.1 Example 1 — source-compile

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| stage | compile |
| exit_code | 1 |
| signal | null |
| timed_out | false |
| error_lines[0].text | widget.c:42:10: fatal error: 'arm_neon_extra.h' file not found |
| toolchain.clang | Debian clang version 14.0.6 |

Walk-through: the gates pass by construction (hypothetical). The stage
is compile and the first error is a compiler diagnostic naming a
missing header and a source position, which is diagnostic of
source-compile and of no other category. Primary:
`source-compile`, confidence `confirmed`. Teaching point: a
compile-stage diagnostic with file, line and a fatal-error message is
enough for confirmed; the missing fields (platform macros, NDK
version) go in `missing_evidence` and set the investigation step, they
do not lower confidence in what the diagnostic already shows.

### 6.2 Example 2 — link

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| stage | link |
| exit_code | 1 |
| first_relevant_error | /usr/bin/ld: main.o: undefined reference to 'mimir_alloc' |
| toolchain.ld | GNU ld (GNU Binutils for Debian) 2.40 |

Walk-through: the first relevant error is emitted by the linker (`ld`)
and reports an undefined reference, at stage link. That is diagnostic
of link. Primary: `link`, confidence `confirmed`. Teaching point:
link and loader-startup both involve libraries and symbols; the
discriminating field is the stage — an undefined reference while
linking is link, a library not found while starting is
loader-startup.

### 6.3 Example 3 — loader-startup

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| stage | generated-startup |
| exit_code | 127 |
| target_ldd | libmimir.so => not found (among resolved entries) |
| stdout.log | (empty) |
| duration_ms | 3 |

Walk-through: linking had completed (stage generated-startup), the
binary started and died in 3 ms with empty stdout, and `target_ldd`
shows a needed library not found. That points to loader-startup.
Primary: `loader-startup`, confidence `probable`. Teaching point:
there is no ELF interpreter field in the packet, so the record cannot
prove whether the interpreter or the missing library is the exact
loader failure point; without that field the outcome is probable,
not confirmed.

### 6.4 Example 4 — address-space-configuration

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| stage | runtime |
| stderr.log | mmap failed: Invalid argument |
| page_size_bytes | 16384 |
| meminfo_before.MemAvailable | 14000000 (invented) |
| meminfo_after.MemAvailable | 14000000 (invented) |
| signal | null |

Walk-through: a mapping request failed with EINVAL at runtime while
memory was ample before and after (the meminfo values above are
invented for this example), which excludes resource pressure and
points at address-space configuration — for example a VA-layout or
page-size assumption that the actual layout rejects. Primary:
`address-space-configuration`, confidence `probable`. Teaching
point: this category is capped at probable — the packet holds no
actual virtual-address layout and no source configuration, so the
roadmap warning applies: do not infer the layout only from the CPU
name.

### 6.5 Example 5 — system-call-or-sandbox

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| env_class | proot-android |
| stderr.log | seccomp: operation not permitted (EPERM) |

Walk-through: the invented run happened under proot-android and the
error is a seccomp permission denial (EPERM), which is the typical
clue for system-call-or-sandbox. Primary: `system-call-or-sandbox`,
confidence `possible`. Teaching point: this category is capped at
possible on a single packet, however suggestive the wording is,
because the same denial can come from the sandbox policy, the proot
layer, or the host; the roadmap investigation — compare native Linux,
Android and proot — needs comparison packets that do not exist here.

### 6.6 Example 6 — resource-pressure

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| timed_out | false |
| signal | 9 |
| stderr.log | Out of memory: Killed process |
| meminfo_before.MemAvailable | 12000000 (invented) |
| meminfo_after.MemAvailable | 120000 (invented) |
| child_peak_rss_kb | 15000000 (invented, of MemTotal 16598576) |

Walk-through: three independent fields agree — the kernel OOM message
in stderr, MemAvailable collapsing from 12000000 to 120000, and a
child peak RSS near MemTotal — and `timed_out` = false excludes a
harness timeout kill. Primary: `resource-pressure`, confidence
`confirmed`. Teaching point: signal 9 alone would only be possible,
because a timeout or an external kill produces the same signal; it is
the corroborating memory evidence, not the signal, that confirms.

### 6.7 Example 7 — allocator-correctness

SYNTHETIC HYPOTHETICAL EXAMPLE — INVENTED PACKET FIELDS, NOT A REAL RUN, NOT EVIDENCE

| Invented field | Invented value |
|---|---|
| stage | runtime |
| signal | 6 |
| stderr.log | AddressSanitizer: heap-use-after-free |

Walk-through: a sanitizer reports a heap-use-after-free at runtime
and the process aborts (signal 6). That is the typical clue for
allocator correctness. Primary: `allocator-correctness`, confidence
`probable`. Teaching point: the roadmap clue for this category
requires reproducibility, and one packet cannot show reproducibility,
so a single packet is capped at probable; the next investigation step
is the verbatim roadmap text, and reproducibility needs a second
packet capturing the same failure.

## 7. Roadmap and capture inconsistencies recorded by A-006

The items below are recorded, not resolved. Resolving any of them is a
roadmap-owner decision; A-006 changes no roadmap text and no existing
file.

- **A6-1 — Section 3.2 has no outcome framework around its table.** The
  roadmap's classification section (3.2) lists seven categories with
  clues and an initial investigation, but it defines no non-category
  outcomes (not-a-failure, expected-synthetic, insufficient-evidence,
  multiple-candidate, harness-or-capture-error,
  out-of-scope-environment), no notion of confidence, and no record
  format. Section 2.2 of docs/failure-classification.md is therefore
  new A-006 framework, not roadmap text. The same gap covers timeouts
  and harness errors: a harness timeout and a capture failure have no
  Section 3.2 category at all, and a timeout kill and an OOM kill can
  present with the same signal 9, so the roadmap gives no home and no
  disambiguation rule for them.
- **A6-2 — Categories are not mutually exclusive and no precedence or
  tie rule exists.** One packet can match several categories: ENOMEM
  sits under resource pressure, but failed mappings print ENOMEM too;
  a missing library can be argued under link or under loader-startup.
  A-006's gate order, tie handling (multiple-candidate) and the
  many-to-many relation between the A-005 stage vocabulary (fetch,
  compile, link, tool-startup, mojo-compile, generated-startup,
  runtime, other) and the seven categories are A-006 proposals; the
  roadmap states none of them. Stage is evidence for a category, never
  the category itself.
- **A6-3 — The allocator-correctness clue requires reproducibility
  that one packet cannot show.** Section 3.2 describes the category
  with "Reproducible invalid free, race, corruption", but a single
  evidence packet is a single run. A-006 therefore caps
  allocator-correctness at probable for a single packet; the roadmap
  does not state any such cap.
- **A6-4 — Section 3.1 asks for fields the A-005 capture does not
  record.** There is no tcmalloc revision field, no errno field, no
  virtual-address-layout field, no kernel/OOM log field, no
  ELF-interpreter or symbol-table field, and no NDK fields; the Bazel
  probe exists in the toolchain block but the smoke packet records it
  as null. Several Section 3.2 investigations cannot be completed
  from an A-005 packet alone, which is why the records list these as
  `missing_evidence` per category.
- **A6-5 — Environment-class hazard is unaddressed in Section 3.2.**
  The motivating evidence behind the roadmap is an Android tablet,
  while the only packet in the repository is arm64-linux on a
  Raspberry Pi. A failure recorded in one environment class is not
  evidence about another, and Section 3.2 gives no rule for that.
  The out-of-scope-environment gate in Section 2.2 is an A-006
  proposal, not roadmap text.
- **A6-6 — Three unreconciled Phase 1 exit gates.** Section 3.3 sets
  one exit gate (someone else can reproduce the failure and explain
  the integration point), Section 8 Phase 1 sets another (a documented
  causal hypothesis and a repeatable validation command), and Section
  17 items 1 and 5 set a third (a reproducible documented cause with
  the boundary mapped; the original failure fixed with verified
  bindings or accurately marked blocked). A-006 records classify
  packets and do not claim to satisfy any Phase 1 exit gate.
- **A6-7 — Section 2 assigns no evidence level to its own claims.**
  The roadmap's evidence table (A-002 material) carries no evidence
  level per claim, and A-005 assigns a level only to its harness run
  ("Device-tested (harness, on Mimir)"). The record default is
  therefore "Not assigned": a classification record inherits a level
  only where a source document assigns one, as Example 0 does.
- **A6-8 — Capture-time classification is always "unclassified", and
  the Section 13 layout has no home for classification records.** The
  A-005 harness writes `classification` = "unclassified" into every
  manifest, and the suggested repository layout in Section 13 does
  not place classification records anywhere. The proposed home
  `evidence/classifications/` is an unratified A-006 proposal.
- **A6-9 — `classification_hint` provenance is undefined.** Packets
  carry a `classification_hint` field (null in the smoke packet), but
  the roadmap never says who may set it or what weight it carries.
  Rule R4 in docs/failure-classification.md — a hint is never
  evidence — is an A-006 safeguard, not a roadmap rule.
- **A6-10 — A-006's own limit.** This framework is Proposed. It has
  never been exercised against a real Mojo/tcmalloc failure, because
  no such failure packet exists in the repository. The first real
  classification may force revision of this framework; any such
  revision happens by supersession (a new record that names the one
  it replaces), never by silent edit of a current record.

## 8. Using this framework

One-page checklist. Work the steps in order; stop at the first gate
that decides.

- [ ] **Step 0 — Identify.** Name the run, packet path and env_class.
  Classify only what that packet records.
- [ ] **Step 1 — Verify (gate).** Run `sha256sum -c` in the packet
  directory and the harness `verify` subcommand. Either fails: stop;
  the outcome is harness-or-capture-error, not a category.
- [ ] **Step 2 — Scope gate.** env_class outside the scope the
  investigation covers: stop; the outcome is
  out-of-scope-environment.
- [ ] **Step 3 — Synthetic / expected gate.** The case is synthetic
  and the result matches its expected result: the outcome is
  expected-synthetic. Synthetic and matched with no failure: the
  outcome is not-a-failure.
- [ ] **Step 4 — Failure gate.** `outcome` is not fail and no failure
  signal, timeout or error is recorded: the outcome is not-a-failure.
- [ ] **Step 5 — Clue matching.** Match the packet's exact field
  values against the Section 3.2 clues. Cite every value with its
  field and source file; list negative evidence; list what each
  rejected category lacks. A `classification_hint` is never evidence.
  No category with diagnostic evidence: insufficient-evidence. Two or
  more categories tied on diagnostic evidence: multiple-candidate.
- [ ] **Step 6 — Confidence, caps, record.** Apply the caps
  (address-space-configuration at most probable;
  system-call-or-sandbox at most possible on a single packet;
  allocator-correctness at most probable on a single packet). Set the
  next investigation step to the verbatim roadmap text for the
  category. Copy `claim_scope` from the manifest
  character-for-character. Write the record; never edit a current
  record silently — supersede it.

Future record path (proposed, unratified — see A6-8):
`evidence/classifications/<run_id>.classification.json`, one JSON
record per classified run, in the schema of
`docs/classification-record.schema.json`.

What A-006 is and is not: A-006 touches no existing file and
classifies no real failure. It demonstrates the procedure on the
synthetic smoke packet — whose honest result is expected-synthetic —
and on seven labelled hypotheticals, and it records the roadmap and
capture inconsistencies above for the roadmap owner to resolve.
