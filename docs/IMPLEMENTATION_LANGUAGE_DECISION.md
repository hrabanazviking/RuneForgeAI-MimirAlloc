# MimirAlloc Implementation Language Decision

Slice: A-009
Team: MimirAlloc TEAM-09

Status: decision record for ledger entry D-005
(OPEN, owned by A-009).
Status line: decision recorded by A-009,
ledger closure pending keeper banking.

Evidence status: Proposed for the allocator,
with the narrow Device-tested probe claim
of Section 4 of this record.

Source of record:
roadmap `docs/MimirAlloc_Research_and_Development_Roadmap.md`
Section 7.2, plus ledger D-005 at
`/home/pi/mimiralloc-forge/docs/DECISION_LEDGER.md`
(read directly by this slice to confirm wording).

Companion document: `docs/ARCHITECTURE_LAYERS.md`
(slice A-009, architecture layers record).

Filename justification: this file sits flat in
`docs/` with an UPPER_SNAKE name, following the
convention of the banked slices in this repository.
Section 13's proposed `docs/decisions/` layout is a
proposed layout only; it is recorded as unsettled
in ledger notes N-5 and N-6, and as inconsistency
A9-1 in the companion document.

This record does not edit the roadmap.
This record does not edit the decision ledger.
This record does not edit `docs/ARCHITECTURE_LAYERS.md`.

---

## 1. The decision

The decision, in full, is:

> The MimirAlloc core starts as portable C11.
> The C ABI is the core's own ABI.
> No C++ is used in the core.
> Any C++ surface (e.g. operator new/delete
> variants, S6.1) is added only when a mapped
> integration actually requires it, and then
> only inside Layer 1, Public boundary, as a
> shim over the C ABI — it is not an eighth
> layer and it does not change the core language.
> A Mojo interface, where supported, is developed
> above the core (S7.2) and is never a bootstrap
> dependency.

Rationale. On the criteria the roadmap itself
settles textually, C11 wins. Section 7.2 states
that C11 'minimizes runtime and bootstrap
coupling', and that constrained C++ 'makes
startup, exceptions and C++ allocation recursion
more complex'. The one criterion favouring
C++17 — invariant expression, Section 7.2 — is
the accepted cost of this decision, and is
recorded as such in the matrix in Section 3.
The criterion Section 7.2 makes the choice
depend on — 'the actual Mojo integration and
dependency needs' — is presently UNKNOWN:
Section 15 questions Q1, Q2, Q4 and Q5 are open,
and the Section 3 exit gate is unmet. That
criterion therefore favours neither language
today, and it becomes a reopen trigger in
Section 6 of this record instead of a reason
to leave the decision open indefinitely.

---

## 2. Status of this record and how it discharges D-005

This record IS the A-009 decision record.
It contains:

- the decision itself (Section 1);
- the Section 7.2 basis for the decision
  (the decision matrix, Section 3);
- the working proof required by Section 7.2
  (Section 4, with sources in Appendix A and
  the run transcript in Appendix B).

It also proposes ready-to-paste ledger text
in Appendix C (proposed entry D-006).
It does NOT edit the ledger, for three reasons:

(i) `DECISION_LEDGER.md` is absent from the
team-09 worktree at base commit b0b8374.
Verified by this slice with `ls`: there is no
`docs/DECISION_LEDGER.md` in
`/home/pi/mimiralloc-team-09/docs/`. The ledger
read for this record lives in the forge tree at
`/home/pi/mimiralloc-forge/docs/DECISION_LEDGER.md`.

(ii) The ledger is append-only, and banking is
the keeper's step. A builder slice does not
bank its own record.

(iii) The slice instructions for A-009 forbid
the ledger edit.

The tension is recorded honestly. D-005's own
text says: 'Until A-009 appends a superseding
entry...' — while the append is in fact
executed by the keeper at banking, from
Appendix C of this record. The substance of
the ledger rule is satisfied: the owner slice
(A-009) authored the superseding entry, with
working proof. The mechanism differs from the
literal wording of D-005. This is recorded as
inconsistency A9-8 (see Section 7).

Until banking, the status of this record is:
decision recorded by A-009, ledger closure
pending keeper banking.

Ledger rule, as stated in the ledger itself:
an OPEN entry is closed only by its owner slice
appending a superseding entry with a new
sequential ID (the next free ID is D-006),
naming the ID superseded, and stating the
decision and its evidence. Appendix C follows
that rule exactly.

---

## 3. Decision matrix

Preamble. Findings in this matrix are
qualitative only: Advantage C11, Advantage
C++17, Neutral, or Unknown. There are NO
numeric scores and NO weights. The roadmap
assigns none, and inventing them would
fabricate measurement that does not exist.
Each row cites what the roadmap actually
states; no row claims more than that text.

| # | Criterion | Roadmap citation | What the roadmap actually states | C11 reading | C++17 reading | Finding |
|---|---|---|---|---|---|---|
| 1 | Bootstrap coupling | S7.2 + S6.3 + ledger D-004 | Core must work even if Mojo is unavailable; allocation-free startup (D-004) | Minimal bootstrap coupling | Heavier bootstrap coupling | Advantage C11 |
| 2 | Runtime dependency | S7.2 | 'C11 minimizes runtime and bootstrap coupling' | Minimized runtime dependency | Larger runtime dependency | Advantage C11 |
| 3 | Startup / exception / recursion complexity | S7.2 + S6.3 | Constrained C++ makes startup, exceptions and C++ allocation recursion more complex; S6.3: avoid allocator-dependent C++ containers during init | Avoids that complexity class | Incurs that complexity class | Advantage C11 |
| 4 | Invariant expression | S7.2 | 'carefully constrained C++ can help express invariants' | Weaker invariant expression | Stronger invariant expression | Advantage C++17 |
| 5 | Mojo integration needs | S7.2 + S3, S15 Q1/Q2/Q4/Q5 | Choice depends on 'the actual Mojo integration and dependency needs'; those questions are unresolved | Satisfies 'Mojo above core, no bootstrap dependency' | Also satisfies that condition | Unknown |
| 6 | Portability | S7.2 + S5 | 'small portable C11 or C++17 core' — both called portable | Portable per roadmap | Portable per roadmap | Neutral |
| 7 | C ABI exposure | S7.2 + S6.1 | 'expose a C ABI' either way; S6.1: C++ operators only when integration requires | See note below | See note below | Neutral |
| 8 | Toolchain availability | S3.1 + S5 | Evidence practice: per-target compile evidence | Compiles on the Pi (App. B) | Compiles on the Pi (App. B) | Neutral on the Pi; Unknown for Android |

Row 4 is recorded as the decision's accepted
cost: choosing C11 gives up the invariant-
expression help that constrained C++ offers.

Row 5 favours neither language. Both options
satisfy the roadmap condition that a Mojo
interface sits above the core and that no
Mojo compiler is a bootstrap dependency.
Because Q1, Q2, Q4 and Q5 in Section 15 are
unresolved and the Section 3 exit gate is
unmet, no Mojo-based finding is possible yet.

Row 7 is Neutral as a requirement: the roadmap
requires a C ABI under either language.
Note, flagged explicitly as DERIVATION, NOT
QUOTATION: with a C11 core, the C ABI is the
core's native ABI rather than a wrapped ABI.
That observation is this record's derivation
from the decision, not roadmap wording.

Row 8 is Neutral on the Pi: the probe in
Appendix B shows both standards compile, link
and run there. For Android NDK / tablet it is
Unknown: no evidence was collected in this
slice for that target, and none is claimed.

---

## 4. Working proof

Definition used in this record:

> In a documentation-only phase with no
> allocator code in the repository, S7.2's
> working proof can only mean: the decision's
> premises were compile-, link- and run-checked
> on a named physical device, with sources and
> output recorded so anyone can repeat them.

Evidence labelling, per Section 2 of the
roadmap:

- The probe claim is Device-tested: compile,
  link and run of stub probes on a Raspberry Pi
  ARM64 Linux device, purpleviking1.
- The allocator itself remains Proposed.
- Nothing in this record is labelled
  Implemented.

Limits of the proof, stated plainly:

- A successful compile is NOT proof of runtime
  interposition.
- It is NOT proof of allocator correctness.
- It is NOT proof of Android support.
- It is NOT proof of performance.
- No such claim is made anywhere in this record.
- NO benchmarks were run, and none are reported.

Probe placement and hygiene:

- The probe ran only in
  `/tmp/mimiralloc-a009-probe/` on the device.
- It never ran in the worktree.
- It was never committed.
- Its sources survive only as Appendix A of
  this record.
- The probe files contain no allocator
  implementation.
- The probe files contain no malloc-family
  symbols. The only symbol exercised is
  `mimir_probe_abi_version`.

Link note. The C++17 binary was linked with
the `c++` driver. `ldd` showed no `libstdc++`
dependency for either binary: the probe uses
an `extern "C"` function and no C++ library
facilities. This is recorded as an observation
about THIS probe only, not as a general claim
about C++ linking.

Full sources: Appendix A.
Full transcript: Appendix B.

---

## 5. Consequences and placement rules

1. The core is C11. Phase 3 (Section 8), the
   minimal allocator, is to be written in C11
   under this decision.
2. The C ABI is the core's own ABI.
3. No C++ is used in the core.
4. Any C++ surface is placed only inside
   Layer 1, Public boundary, as a shim over
   the C ABI, and only when a mapped
   integration actually requires it.
5. A C++ shim is not an eighth layer, and it
   does not change the core language.
6. A Mojo interface, where supported, is
   developed only above the core. Mojo is
   never a bootstrap dependency.
7. No slice may treat C++17 as settled.
   Until a reopening under Section 6, C++17
   remains the rejected alternative, not a
   parallel option.

---

## 6. Reopen criteria

This decision is reopened only by ledger
supersession: a new ledger entry, with a new
sequential ID, naming the entry it supersedes,
under the ledger's own supersession rules.
No slice reopens it by assumption or by use.

Reopening is triggered only by one of:

(i) A-007 / Phase 1 evidence identifying an
actual Mojo integration that requires a C++
core — not a hypothetical integration, and
not a preference for C++.

(ii) A Section 5 target whose toolchain lacks
a conforming C11 implementation, shown by
per-target compile evidence for that target.

(iii) Project-owner direction.

The presently-Unknown Mojo criterion (matrix
row 5) is the expected path to trigger (i):
if Phase 1 resolves Q1, Q2, Q4 or Q5 with an
integration that genuinely requires C++, that
evidence, and only that evidence, reopens
this decision through the ledger.

---

## 7. Inconsistencies and gaps

This section records the language-relevant
subset only. The full A9 list is in
`docs/ARCHITECTURE_LAYERS.md` Section 16;
this record cross-refers to it and does not
duplicate it.

- A9-2: Section 7.2's heading promises a
  decision the text never makes (ledger note
  N-2). Discharged by this record plus the
  proposed D-006 in Appendix C, pending
  keeper banking.
- A9-3: Section 7.2 depends on Mojo evidence
  that does not exist yet (Section 3 gate
  unmet; Section 15 Q1/Q2/Q4/Q5 open). The
  decision is therefore made on the criteria
  the roadmap does settle, with Mojo as a
  reopen trigger.
- A9-4: Section 15's 13 questions omit the
  language question entirely; D-005 is its
  only register. This is the same pattern as
  ledger note N-3.
- A9-5: C++ placement has no home in
  Section 7.1. This record places any C++
  surface inside Layer 1, Public boundary.
  The roadmap does not state that placement.
- A9-8: Closure mechanism versus worktree
  reality: D-005 says A-009 appends, but the
  ledger is absent from the team-09 worktree
  and banking is the keeper's step. See
  Section 2 of this record.
- A9-10: 'Working proof' is demanded by
  Section 7.2 and defined nowhere. Section 2
  of the roadmap warns, in effect, that a
  compile is not interposition. Section 4 of
  this record supplies a narrow definition
  and states its ceiling explicitly.

---

## Appendix A. Probe sources

Verbatim sources of the executed probe.
SHA-256 hashes are recorded in Appendix B.

### `mimir_probe.h`

```c
#ifndef MIMIR_PROBE_H
#define MIMIR_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned int mimir_probe_abi_version(void);

#ifdef __cplusplus
}
#endif

#endif
```

### `probe_c11.c`

```c
#include "mimir_probe.h"

unsigned int mimir_probe_abi_version(void)
{
    return 1u;
}
```

### `probe_cpp17.cpp`

```cpp
#include "mimir_probe.h"

extern "C" unsigned int mimir_probe_abi_version(void)
{
    return 1u;
}
```

### `probe_consumer.c`

```c
#include "mimir_probe.h"

int main(void)
{
    return mimir_probe_abi_version() == 1u ? 0 : 1;
}
```

---

## Appendix B. Probe run transcript

Reproduced faithfully from the executed probe.

Environment:

- Architecture: aarch64
- `getconf PAGESIZE` = 16384
- `gcc (Debian 12.2.0-14+deb12u1) 12.2.0`
- `g++` — same version as gcc above
- Date: Fri Nov 6 07:41:47 AM EST 2026
- Mojo: `/usr/local/bin/mojo`,
  Mojo 1.1.0 (8189361e) — present, and NO
  build command invokes Mojo; all builds
  used `cc` / `c++` only.
- `free -g`: Mem 15 total, 2 used,
  13 available.

C11 build and run:

```sh
cc -std=c11 -pedantic -Wall -Wextra -Werror -c probe_c11.c -o probe_c11.o
# EXIT:0
# consumer compiled with same flags
# linked with cc -> probe_c11_bin, EXIT:0
./probe_c11_bin
# EXIT:0
```

C++17 build and run:

```sh
c++ -std=c++17 -pedantic -Wall -Wextra -Werror -c probe_cpp17.cpp -o probe_cpp17.o
# EXIT:0
# consumer compiled as C11
# linked with c++ -> probe_cpp17_bin, EXIT:0
./probe_cpp17_bin
# EXIT:0
```

Symbol check, `nm -g`:

```text
C11_OBJECT:   0000000000000000 T mimir_probe_abi_version
CPP17_OBJECT: 0000000000000000 T mimir_probe_abi_version
```

The symbol is identical and unmangled in both
objects.

Dynamic libraries, `ldd` on both binaries:

```text
linux-vdso.so.1
libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6
/lib/ld-linux-aarch64.so.1
```

No other libraries for either binary.

SHA-256 of probe sources:

```text
06dba64d93420db67cd2427deddee1eaa98e20493ca97b0a3c93518c1b238e4d  mimir_probe.h
d8d49a8226a8f255e4bf78f299f8a29e4de84ee469ad3ab3e60796660977e0d6  probe_c11.c
1121dd341b8d3c58a4fe424b132bea7a7ee5bcabfa263d3f12eb8910710a1bb6  probe_cpp17.cpp
a3aaf8c1602549c021e9a232268b821696ab0e86a76f8348358228c3fcac81bf  probe_consumer.c
```

These hashes were re-checked by this slice
against the files in `/tmp/mimiralloc-a009-probe/`
before writing this record, and they matched.

---

## Appendix C. Proposed ledger appendix text (D-006)

Ready for the keeper to paste at banking with zero rewriting, in the ledger's existing entry format (mirroring D-001 to D-005).

```markdown
## D-006 - Implementation language

- **Status:** Adopted
- **Roadmap basis:** Section 7.2, "Implementation language decision".
- **Decision:** The MimirAlloc core starts as portable C11. The C ABI is the core's own ABI. No C++ is used in the core. Any C++ surface (e.g. operator new/delete variants, S6.1) is added only when a mapped integration actually requires it, and then only inside Layer 1, Public boundary, as a shim over the C ABI — it is not an eighth layer and it does not change the core language. A Mojo interface, where supported, is developed above the core (S7.2) and is never a bootstrap dependency.
- **Working proof:** Doc `IMPLEMENTATION_LANGUAGE_DECISION.md` Appendices A-B; purpleviking1, aarch64, gcc/g++ 12.2.0, both -std=c11 and -std=c++17 probes compiled/linked/ran exit 0, identical unmangled C symbol, no Mojo invoked; ceiling: proves toolchain/ABI feasibility only, not interposition/correctness/performance.
- **Supersedes:** D-005. **Status of entry:** Current.
```

Exact index-table edits for the keeper at banking:

- D-005 row: status -> 'Superseded by D-006'.
- New D-006 row: `| D-006 | Implementation language — C11 core | Adopted | Section 7.2 | A-009 |`

End of record. Slice A-009, TEAM-09.
Status until banking: decision recorded by A-009, ledger closure pending keeper banking.
