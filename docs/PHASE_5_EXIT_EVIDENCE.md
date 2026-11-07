# Phase 5 - Exit Evidence Packet

Slice: **E-009 (team-09)**. Wave 5, Phase 5 exit evidence
packet - the last Wave 5 slice. Synthesis only: this slice
adds this packet and its recorded fresh run
(`tests/abi/recorded_run_e009.txt`); it changes no
implementation, no test, and no Makefile line (the Phase 3
and Phase 4 exit packets, C-009 and D-009, likewise added
no Makefile target, so this packet adds none; `make test`
composition is unchanged).
Packet base: **ca77365c0133a5a817af2e70bf841e921a6cbfe4**
(banked main: E-001..E-008; Wave 5 = 8 of 9 at this base)
on branch `yrsa/mimiralloc-09`, worktree
`/home/pi/mimiralloc-team-09` on Raspberry Pi
`purpleviking1`. Host for every run cited: ARM64 Linux +
glibc - `Linux purpleviking1 6.12.112-v8-16k-hailo+
#1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64
GNU/Linux`, runtime-discovered system page size **16384**
(`getconf PAGESIZE`), compiler
`cc (Debian 12.2.0-14+deb12u1) 12.2.0`, glibc 2.36.
Governing text: Roadmap Section 8, Phase 5, decomposed
in `docs/PHASE_5_FOUNDATION_PLAN.md` (E-001, banked
`e9b6a5a`) into exit items X1-X4 and task items T1-T8,
read with Roadmap Sections 2, 5, 6, 13 and 15. Modelled
on the banked Phase 4 packet
`docs/PHASE_4_EXIT_EVIDENCE.md` (D-009) and the Phase 3
packet `docs/PHASE_3_EXIT_EVIDENCE.md` (C-009).
`README.md` was not touched by this slice (verified:
sha256 `bd7f6614` before and after the fresh run, and
`git diff ca77365..HEAD -- README.md` is empty).
`include/mimir_alloc.h` is likewise byte-identical to
base (sha256 `55107f05`).

> **EXIT VERDICT - Phase 5 exit is MET, in exactly the
> scope stated here and no wider.**
> The Roadmap Phase 5 exit sentence (Section 8, Phase 5)
> is, verbatim: *"independent test executable and target
> tool show the intended symbols binding to MimirAlloc
> and pass lifecycle tests. Exact link route recorded."*
> All four exit items decomposed from that sentence by
> E-001 (X1-X4, Section 4 of this packet) are **MET** on
> recorded evidence, and the packet's fresh run
> (Section 2) reproduces every load-bearing count at
> this base: an independent executable binds the four
> intended symbols to `libmimiralloc.so` with
> loader-level proof (E-004), second separately built
> consumers - a C++ consumer (E-007) and a multi-module
> program (E-008) - show the same binding under the
> "target tool" reading E-001 pinned for Wave 5,
> lifecycle tests pass through the bound route in every
> Wave 5 slice, and E-004 Section 4 names THE exact link
> route (R2, dynamic) with the proven static alternative
> (R1) recorded beside it.
>
> **The exact scope of that claim:** a **Pi-native**
> Phase 5 exit - one target only (native AArch64 Linux
> + glibc, 16384-byte pages, this Pi), one surface only
> (the four namespaced symbols `mimir_malloc`,
> `mimir_free`, `mimir_calloc`, `mimir_realloc`), one
> named link route (R2 dynamic, with R1 static recorded).
> It is **not** an Android, musl, x86-64, or Mojo claim;
> it is **not** an interposition claim (route R4,
> `LD_PRELOAD` of standard names, was rejected by E-001
> with reason recorded, and no standard-name symbol
> exists); it is **not** an ABI freeze (the header still
> declares the surface Proposed, U-01/U-03 open, no
> SONAME, no install story - Section 7). And it does
> **not** discharge the carried debts E-001 Section 6.3
> required this packet to restate: **Phase 4 exit
> remains UNMET** (D-009 verdict; the physical Android
> run is still OWED) and **Phase 3 exit remains UNMET**
> (the x86-64 suite run is still OWED). A Phase 5 exit
> claim quoted without this scope paragraph is
> incomplete by E-001's standard (INC-E001-08).

## 1. Verdict vocabulary and reading rules

- **MET** - the evidence the item specifies exists, in
  the form it specifies, in the banked repo (or, for the
  fresh-run items, in this packet's recorded run).
- **UNMET** - some evidence exists, but the item as
  written is not satisfied.
- **PARTIAL** - one named half of the item is met and
  the other named half is discharged only in a weaker,
  explicitly recorded sense, or is owed. (D-009 folded
  this case into UNMET; this packet names it separately
  because E-001 pre-defined the weaker sense for the one
  affected item, T7 - see Section 5.)
- **OWED** - the evidence is wholly absent at this base;
  the row names exactly what is missing and who or what
  it waits on.
- Counts are copied from the recorded-run files named
  in each row, never restated from memory or from
  another slice's summary. Where the fresh run in
  Section 2 reproduces a banked count, both are cited.
- Contract mappings for the namespaced `mimir_*`
  surface are by analogy only, under U-03, exactly as
  in every Wave 5 slice. Nothing here is an ABI freeze:
  the public header declares the surface Proposed, not
  stable.
- Every Wave 5 claim is single-target (Pi-native
  AArch64 Linux/glibc) unless a row says otherwise.
  No target is promoted to "supported" (Roadmap
  Section 5 sense) by anything in this packet.

## 2. Fresh verification run for this packet (E-009)

Run once on the Pi in `/home/pi/mimiralloc-team-09` at
base `ca77365` on 2026-11-06 (20:51 EST), from a clean
build, sequentially, load kept light. Raw combined log:
`tests/abi/recorded_run_e009.txt` (1,167 lines, 73,633
bytes, sha256
`57a14bc500456351ab7ab02eddb07037a340ed6db93395e3c4f1591b244f0017`),
sections S0-S12. The recorded run lives under
`tests/abi/` because every Wave 5 slice (E-002..E-008)
keeps its recorded run there; Wave 4's packet used
`tests/recorded_run_d009.txt` instead - the placement
seam is recorded as INC-E009-04. `free -g` before and
after: 15 GiB total, 13 GiB available; Samtal answered
HTTP 200 on port 8787 before and after - not starved.
(The run file's own S0 status listing names the run
file itself as untracked: the shell redirection creates
it before the run starts. No evidence depends on that
line.)

Commands and exit codes, exactly as run:

| Command | Exit | Headline counts as logged |
|---|---|---|
| `make clean` | **0** | - |
| `make test` | **0** | smoke `SMOKE 18/18 checks passed`; core `CHECKS: 29 run, 0 failed`; B-007 `CHECKS: 70 run, 0 failed`; C-005 `CHECKS: 95 run, 0 failed`; C-006 `CHECKS: 23 run, 0 failed`; C-007 `SUITE cases: 40 passed, 0 failed, 70 skipped, 9 inspected (of 119 registered)`, `CHECKS: 122 run, 0 failed`; C-008 `CHECKS: 42 run, 0 failed, 7 skipped-not-yet`; D-004 `CHECKS: 4590 run, 0 failed`, `ALLOCATIONS: 2088 successful, 2088 frees` - every suite `RESULT: PASS` |
| `make lib` | **0** | `build/libmimiralloc.so` built from the dedicated PIC objects with the anonymous-node version script |
| `nm -D --defined-only build/libmimiralloc.so` | (recorded in S3/S12) | **Count 4**, exactly: `mimir_calloc`, `mimir_free`, `mimir_malloc`, `mimir_realloc` - before and after all standalone targets |
| `make test-e002` | **0** | `CHECKS: 16 run, 0 failed` - `RESULT: PASS` (consumer linked against the `.so` only) |
| `make test-e003` | **0** | `E003 16/16 checks passed` - `RESULT: PASS` (static archive route) |
| `make test-e004` | **0** | `E004 19/19 checks passed` - `RESULT: PASS` (independent executable, dynamic route) |
| `make test-e005` | **0** | `CHECKS: 31 run, 0 failed` - `RESULT: PASS`; final AUDIT books `call_malloc=6 call_calloc=4 call_realloc=4 call_free=9 published=10 retired=10 live=0` |
| `make test-e005-dyn` | **0** | `CHECKS: 25 run, 0 failed` - `RESULT: PASS` (dynamic leg: descriptor validation + mapped-bytes accounting) |
| `make test-e006` | **0** | `CHECKS: 159 run, 0 failed` - `RESULT: PASS`; `OBSERVED page-modulus: 49 of 49` non-null returns 0 mod 16384 (construction observation, not a claim) |
| `make test-e007` | **0** | `E007 24/24 checks passed` - `RESULT: PASS` (C++17 consumer via the `.so`) |
| `make test-e008` | **0** | `E008-FIRST 9/9`; `E008-THREADS 13/13`; mixed probe `CHECKS: 9 run, 0 failed`; `E008-MAIN 33/33` - all `RESULT: PASS`, bracketed by the peer CTOR line (first) and DTOR verification line (last) |

Every fresh count is identical to the corresponding
banked recorded run for E-002..E-008 and to the banked
`make test` composition counts. Two honest notes:
(a) the fresh E-006 run's 32768-demand observation read
`ptr mod 32768 = 0` where E-006's own recorded run read
16384 - the non-determinism INC-E006-03 records,
reproduced live (INC-E009-06); (b) the scoped
`git diff ca77365 -- README.md include/mimir_alloc.h
Makefile src/` inside the run is empty, and both
sha256 values are unchanged across the run. The fresh
run adds no new claim; it confirms the banked evidence
still reproduces at this packet's base, on one tree,
in one run - which no earlier single run had done for
all eight standalone targets (INC-E009-07).

## 3. Banked evidence inventory (E-001..E-008)

All commits are banked main commits; counts are as
logged in the named file.

| Slice | Banked commit | Evidence files | Recorded result (as logged) |
|---|---|---|---|
| E-001 foundation plan | `e9b6a5a` | `docs/PHASE_5_FOUNDATION_PLAN.md` (763 lines) | Four-symbol de facto contract stated; INC-D002-02 remedy scoped; routes R1/R2 to test, R3 owed-by-decision, R4 rejected with reason; exit matrix X1-X4 all OWED, tasks OWED 7 of 8 with T1 in progress; carried debts C1-C7 stated. INC-E001-01..09 |
| E-002 shared library | `71ce730` | `docs/SHARED_LIBRARY_BUILD.md` (166 lines), `src/api/mimir_alloc.map`, `tests/abi/test_e002.c`, `tests/abi/recorded_run_e002.txt` | `make lib` builds `build/libmimiralloc.so` (71,048 bytes) from dedicated PIC objects (API `-fvisibility=default`, core/platform `-fvisibility=hidden`) + anonymous-node version script; `nm -D --defined-only` = exactly the 4 public symbols - **INC-D002-02 REMEDIED**; consumer 16 checks, 0 failed, PASS. INC-E002-01..04 |
| E-003 link routes | `966214d` | `docs/PHASE_5_LINK_ROUTES_E003.md` (387 lines), `tests/abi/test_e003.c`, `tests/abi/recorded_run_e003.txt` | R1 static PROVEN: archive `build/libmimiralloc.a` (12,070 bytes, 3 members), consumer 16/16 via the explicit archive path, `DT_NEEDED` libc only, link map recorded. R2 dynamic honestly OWED at its base (E-002 unbanked into its branch), exact proving commands recorded for E-004. INC-E003-01..06 |
| E-004 binding proof | `f282069` | `docs/PHASE_5_BINDING_PROOF_E004.md` (208 lines), `tests/abi/test_e004.c`, `tests/abi/recorded_run_e004.txt` | R2 dynamic PROVEN, INC-E003-01 CLOSED: independent executable `test_e004` 19/19 via `libmimiralloc.so`; `DT_NEEDED libmimiralloc.so`; `ldd` resolves to the built `.so`; `LD_DEBUG=bindings` exactly 1 line per public symbol (4 total); E-003's consumer 16/16 under its own S7 dynamic commands. **Section 4 names THE exact link route: R2 dynamic** (R1 the recorded alternative). INC-E004-01..04 |
| E-005 audit hook | `ea14048` | `docs/PHASE_5_AUDIT_HOOK_E005.md` (306 lines), `tests/abi/mimir_audit_e005.h`, `tests/abi/test_e005.c`, `tests/abi/test_e005_dyn.c`, `tests/abi/recorded_run_e005.txt` | INC-E001-07 shape (b): six internal counters (4 L1 call + 2 core mapping), Proposed-internal only, bound local in the `.so` (`nm -D` still exactly 4; audit-reference link probe vs the `.so` fails exit 1 as designed). Static leg 31 checks PASS, final books 6/4/4/9 calls, published 10, retired 10, live 0; libc-bypass phase moved no counter. Dynamic leg 25 checks PASS (descriptor validation + mapped bytes +131072 / +0 libc / +65536, final == baseline). INC-E005-01..06 |
| E-006 aligned surface | `8520a4e` | `docs/PHASE_5_ALIGNED_SURFACE_E006.md` (554 lines), `tests/abi/test_e006.c`, `tests/abi/recorded_run_e006.txt` | DECIDED: no aligned entry points; surface stays exactly 4 (`nm -D` count 4 re-proven). PROPOSED only: one namespaced `mimir_aligned_alloc` for a future contract+ABI slice, gated on reopen triggers R-1..R-5, U-11/V-03 left open deliberately. Alignment lifecycle 159 checks, 0 failed, PASS twice from clean (floor 16 on all 49 returns; page modulus 49/49 as construction observation). INC-E006-01..07 |
| E-007 C++ consumption | `d578523` | `docs/PHASE_5_CPP_CONSUMPTION_E007.md` (481 lines), `tests/abi/test_e007.cpp`, `tests/abi/header_only_e007.cpp`, `tests/abi/header_only_e007.c`, `tests/abi/recorded_run_e007.txt` | C++ consumption PROVEN with no header or library change: header C++-clean standalone and C11-clean; C++17 consumer 24/24 PASS via `libmimiralloc.so` in the R2 route; `mimir_*` references UNMANGLED (mangled class symbols as control); `LD_DEBUG` exactly 4 binding lines. DECIDED: no C++ native surface - no operator new/delete anywhere; `nm -D` stays exactly 4; A-009 Layer-1 shim trigger NOT met by consumption. INC-E007-01..07 |
| E-008 startup + ownership | `ca77365` | `docs/PHASE_5_STARTUP_OWNERSHIP_E008.md` (428 lines), `tests/abi/test_e008_main.c`, `test_e008_peer.c/.h`, `test_e008_first.c`, `test_e008_threads.c`, `test_e008_mixed.c`, `tests/abi/recorded_run_e008.txt` | Cross-module ownership PROVEN: two ELF modules, one `libmimiralloc.so` instance (both `DT_NEEDED`, `ldd` to the one file, exactly 8 `LD_DEBUG` binding lines), lifecycles both directions byte-for-byte, main 33/33. Startup ordering PROVEN with no init requirement: first-call 9/9, peer constructor before `main` + destructor after `main`, concurrent first calls from 8 threads 13/13. Mixed-allocator: no runtime rejection exists - foreign free is contract UB (INC-E008-01); safe audit-counter probes in a separate child 9/9. INC-E008-01..05 |
| E-009 this packet | (this slice) | `docs/PHASE_5_EXIT_EVIDENCE.md`, `tests/abi/recorded_run_e009.txt` | Fresh run in Section 2 |

## 4. Exit gate, item by item (E-001 X1-X4)

The exit items are E-001 Section 6.1's decomposition of
the Roadmap Phase 5 exit sentence, quoted at the head
of this packet.

| # | Exit item (E-001 wording, in substance) | Evidence | Verdict |
|---|---|---|---|
| X1 | An **independent test executable** shows the intended symbols binding to MimirAlloc | E-004: `tests/abi/test_e004.c`, an executable that is not one of the banked in-tree test binaries and links no loose objects - it links `build/libmimiralloc.so`; its object and dynsym carry exactly the four `mimir_*` symbols undefined (U); `readelf -d` shows `DT_NEEDED libmimiralloc.so`; `ldd` resolves to the built `.so`; `LD_DEBUG=bindings` shows exactly one binding line per public symbol, each to the built library (`recorded_run_e004.txt` S3-S5). E-002's `.so`-only consumer (16/16) is the second instance of the same form. Reproduced at this base: fresh run S6, `E004 19/19`, exit 0 | **MET** |
| X2 | A **target tool** shows the same binding | Under the reading E-001 pinned for Wave 5 (INC-E001-04): a second, separately built consumer binding the same library. Held twice over: E-007's C++17 consumer (24/24; unmangled references; `DT_NEEDED libmimiralloc.so`; exactly 4 `LD_DEBUG` binding lines; `recorded_run_e007.txt` S5) and E-008's multi-module program (two ELF modules, exactly 8 binding lines, all to the one `.so`; `recorded_run_e008.txt` S5). Reproduced at this base: fresh run S10/S11, exit 0. The Mojo/Aesir-tool reading is **not** held and is not claimed: it is Phase 6 scope, OWED beyond Wave 5 (carried item O-14) | **MET** (in the pinned Wave 5 reading; Mojo reading OWED to Phase 6) |
| X3 | **Lifecycle tests pass** through the bound route | Through the linked library, never loose objects: E-004 core lifecycle 19/19 (allocate/fill/verify/free across the four symbols, realloc growth preservation, calloc zeroing, `free(NULL)`, failure-null cases); E-005 dynamic leg 25/25; E-006 alignment lifecycle 159/159; E-007 C++ lifecycle 24/24; E-008 cross-module + startup lifecycles 33/33 + 9/9 + 13/13 + 9/9. Every one reproduced at this base in the fresh run (S4-S11), all exit 0 | **MET** |
| X4 | **Exact link route recorded** | Named by E-004 Section 4 on dual-route evidence, per E-001 Section 5.4's procedure: **THE route is R2 - shared library, dynamic link**: build with `make lib`, compile the consumer against `include/mimir_alloc.h`, link `-Lbuild -lmimiralloc` with an rpath naming the library location (`$ORIGIN` for the in-tree standalone targets; the absolute build dir in E-003's S7 form - both spellings recorded, INC-E004-02), run; the loader binds each undefined `mimir_*` reference to `libmimiralloc.so`. The other route's result is recorded beside it: R1 static via the explicit archive path `build/libmimiralloc.a`, proven by E-003 (16/16, `DT_NEEDED` libc only). R3 (`dlopen`) untested by decision; R4 rejected with reason | **MET** |

**Exit tally: MET 4 / UNMET 0 / PARTIAL 0 / OWED 0,
of 4. Phase 5 exit: MET** - in the scope stated in
the verdict block at the head of this packet (Pi-native
target, four-symbol namespaced surface, R2 route), and
with the carried debts of Section 7 untouched.

## 5. Task map, item by item (E-001 T1-T8)

The task items are E-001 Section 6.2's decomposition of
the Roadmap Phase 5 task sentence: *"Implement only the
documented symbol contract for the integration route.
Test C/C++ variants if needed, exported symbol
visibility, startup ordering, shared libraries,
static/dynamic linking, aligned allocations and
cross-module ownership. Use a visible allocation
counter or explicit audit hook to prove the intended
call path invokes MimirAlloc. Reject ambiguous
mixed-allocator ownership."*

| # | Task item | Evidence | Verdict |
|---|---|---|---|
| T1 | Implement **only** the documented symbol contract for the integration route | E-001 Section 3 stated the four-symbol de facto contract; E-002's export proof and every later slice's re-proof hold the dynamic surface at exactly those four (fresh run S3/S12: count 4, the four names listed). Negative compliance also held: no slice added a symbol - no aligned entry point (E-006 decision), no C++ operator (E-007 decision), no audit export (E-005 shape (b), counters bound local) | **MET** |
| T2 | Test **C/C++ variants if needed** | E-007, in the consumption reading E-001 pinned (INC-E001-06): C++17 TU consuming the banked header under its `extern "C"` guards, compiled + linked + run against the library, 24/24, header byte-identical to base. The operator-family reading is excluded by decision, not by oversight (INC-E007-01) | **MET** (consumption reading only) |
| T3 | Test **exported symbol visibility** | E-002: exactly-4 `nm -D` result with both control mechanisms named (compile-time `-fvisibility=hidden` on core/platform + anonymous-node version script), internal names absent from the dynamic table, INC-D002-02 remedied. Re-proven by E-004, E-005, E-006, E-007, E-008 and by this packet's fresh run | **MET** |
| T4 | Test **shared libraries** and **static/dynamic linking** | E-002 (shared library built + export proof); E-003 (R1 static proven; R2 recorded as owed with proving commands); E-004 (R2 proven, INC-E003-01 closed). Both routes' full evidence columns (commands, `nm`, `readelf -d`, `ldd`, loader record, lifecycle run) exist in the banked record | **MET** |
| T5 | Prove the intended call path with a **visible allocation counter or explicit audit hook** | E-005: six implementation-resident counters track a scripted lifecycle exactly (final books balance, live 0) and a libc-bypass phase in the same process moves no counter; the dynamic leg proves servicing from outside the `.so` by descriptor validation + exact mapped-bytes accounting. Single-threaded evidence (see O-03) | **MET** (Pi-native, single-threaded counter scope) |
| T6 | Test **aligned allocations** | E-006, in the scoped form E-001 prescribed (INC-E001-05): a decision record (surface stays empty; the four symbols' floor is the only promise) + an alignment lifecycle of the existing four symbols through the linked `.so` (159 checks; floor 16 on all 49 returns). No aligned entry point exists to test - by recorded decision, with the future route fenced in E-006 Section 5.2 | **MET** (in the E-001 scoped reading) |
| T7 | **Reject ambiguous mixed-allocator ownership**, and test **cross-module ownership** | Cross-module half MET in full: E-008 proves same-instance sharing (alloc/fill/realloc/free split across two ELF modules bound to the one `.so` instance, contents byte-for-byte, 33/33). Rejection half: discharged **only** in the sense E-001 T7(b) prescribed in advance - the documented contract rule (a pointer is freed by the family that allocated it, through the same instance; foreign free/realloc is UB with no defined error) + the enforced test-design prohibition (no Wave 5 test ever passes a foreign pointer to a `mimir_*` entry point) + the safe counter-separation probes (E-008 mixed child, 9/9). **No runtime rejection exists**, none was demonstrated, and none is promised by the contract; a detectable rejection is a future contract change (O-11) | **PARTIAL** - cross-module MET; rejection held as contract posture only, exactly as E-001 prescribed, with no runtime mechanism |
| T8 | Test **startup ordering** | E-008, all three cases through the linked `.so`: (a) allocation as the very first MimirAlloc call of a program, 9/9; (b) allocation from a module constructor before `main`, verified in `main`, freed by the peer destructor after `main`; (c) first-ever calls raced from 8 threads behind a barrier, 13/13. The underlying design fact (no constructor, no `pthread_once`, statically initialised lock) is proven by inspection in the same slice (recorded run S3) | **MET** |

**Task tally: MET 7 / PARTIAL 1 / UNMET 0 / OWED 0,
of 8** (T7 the partial, for the reason stated in its
row). No task tally changes the exit verdict, which
rests on X1-X4 (Section 4).

## 6. Route register (E-001 Section 5, final state)

| Route | Final state at this base |
|---|---|
| R1 - static archive, direct link | **PROVEN** (E-003; explicit archive path; reproduced fresh run S5) |
| R2 - shared library, dynamic link | **PROVEN** (E-002 library + E-004 binding proof) - **THE exact link route** named for Phase 5 exit purposes (E-004 Section 4) |
| R3 - shared library via `dlopen`/`dlsym` | **Untested by decision** (E-001 Section 5.2). No Wave 5 slice was assigned it; its absence is a decision, not an oversight (O-10) |
| R4 - `LD_PRELOAD` interposition of standard names | **REJECTED, with reason recorded** (E-001 Sections 5.2-5.3, INC-E001-01): the library defines no standard-name symbols, so preloading interposes nothing; adding them would be a silent ABI expansion into undecided U-01 territory, and Roadmap Section 13 gates even the long term ("No default global interposition until well validated" - a Phase 13 gate). The Phase 5 heading's word "interposition" is therefore **not** covered by this phase, and no document may cite Wave 5 as interposition coverage |

## 7. Carried-owed register (nothing here blocks X1-X4; everything here survives the exit)

Collected from E-001..E-008 as E-001 Sections 6.1/6.3
and each slice's INC register require. Each item names
its future home or its reopen trigger.

| # | Owed item | Source | Future home / trigger |
|---|---|---|---|
| O-01 | No SONAME, no versioned file name (`libmimiralloc.so.1`), no `install` target or `ldconfig` story; consumers' `DT_NEEDED` names the plain filename | INC-E002-01, INC-E003-06, INC-E004-01 | The future ABI-freeze slice (with the named version node); the ABI is expressly not frozen (U-03 open) |
| O-02 | Shared-vs-static behavioural parity across the **whole** contract surface: `make test` still runs against the loose objects; only the standalone consumers run against the `.so`/archive | INC-E002-04 | A future slice, before any claim that the shared build is behaviourally equivalent across the whole surface |
| O-03 | Threaded audit-counter run: T5 counter exactness under concurrency is argued (atomics + global lock), not run | INC-E005-05 | OWED if T5 evidence is ever cited for concurrent call paths |
| O-04 | `mimir_realloc(NULL, n)` counter accounting rests on code reading, not on a run (inspection-grade) | INC-E005-06 | Any future audit-counter slice that extends the script |
| O-05 | The core's banked "L7 observability - compiled out" design note and E-005's permanent internal counters disagree in wording | INC-E005-01 | A future B-006 design revision: bless an audit-counter exception or retire the counters now that this packet has consumed the evidence |
| O-06 | The PROPOSED `mimir_aligned_alloc` is unimplemented by design; its contract debts (U-11/V-03 per target, U-09 interaction, U-03 posture, versioning interaction) are unsettled | E-006 Sections 5.2/6, INC-E006-01 | A future contract+ABI slice, commissioned only when a reopen trigger fires: R-1 a real consumer records a need for guaranteed over-alignment; R-2 the Phase 7 redesign is designed; R-3 an ABI-freeze/SONAME slice is commissioned; R-4 a second target's facts differ materially; R-5 U-01 settles including `aligned_alloc`/`posix_memalign` |
| O-07 | Threaded run of `test_e006`, and alignment runs on any second target (4096-page host, musl, bionic) | INC-E006-05 | OWED if E-006 evidence is cited beyond the Pi-native single-threaded glibc target (host/libc legs carried as INC-D003-02, INC-D008-06/-07) |
| O-08 | Header C++-cleanliness is proven, not continuously guarded: `make test` builds no C++ file, so a future header edit could break the `extern "C"` guards with `make test` still green | INC-E007-02 | A future CI / ABI-freeze slice (manual-only workflow law bars hosted CI by default) |
| O-09 | C++ legs beyond the proven one: a clang-built consumer link/run, any second C++ standard, any second target; the C++ operator family and its pre-main design problem | INC-E007-04, INC-E007-05, INC-E007-06 | OWED if E-007 evidence is cited beyond the Pi-native g++ C++17 target; the operator family additionally requires U-06 settled + a named integration requiring it (A-009 Layer-1 shim discipline) |
| O-10 | Route R3 (`dlopen`/`dlsym`), including dlopen-time construction and ownership | E-001 Section 6.3 C7, INC-E008-05 | A future deliberate slice, if a phase ever needs plugin-style loading |
| O-11 | A **detectable** runtime rejection of foreign pointers (e.g. a debug build aborting on a non-registry pointer) does not exist and is not promised | E-008 Section 4, INC-E008-01 | A future contract decision; it is a contract change, not an implementation fix |
| O-12 | The peer destructor's verification verdict exists only as a printed line; `make test-e008`'s exit code does not and cannot gate on it within a single process run | INC-E008-03 | Structural - recorded permanently; any future gate would need an out-of-process checker design |
| O-13 | The static archive exposes the nine internal names as global `T` symbols (normal archive behaviour; the E-002 remedy governs the `.so` dynamic surface only). A static consumer that declares an internal name could link it; protection is the documented contract + discipline | INC-E003-02 | Recorded permanently, so no reader cites E-002's export fix as a static-archive fix |
| O-14 | The Mojo/Aesir "target tool" demonstration (X2's wider reading) and the whole of Phase 6 integration proof | E-001 Section 6.3 C6, INC-E001-04 | Phase 6 slices, using this packet's recorded R2 route |
| O-15 | U-01 (final symbol set), U-03 (namespaced-API contract scope), U-06 (C++ operators), U-07 (ABI versioning) remain **open** in `docs/contracts.md`. Wave 5 worked under them on the four-symbol de facto surface; a later decision that adds symbols reopens the export-control and binding evidence, which must then be re-proven, not assumed to carry over | INC-E001-02, E-001 Section 6.3 C5 | A project-owner decision at the Roadmap Section 15 Q4 gate |
| O-16 | **Phase 4 exit UNMET**: physical-device allocate/fill/free + multithread tests on Android, with page size and ABI recorded (D-009: exit MET 1 / UNMET 4; combined 5/8/15 of 28); with it the NDK build, target-API decision, native Android run, Termux and app verifications | E-001 Section 6.3 C1/C2 | An Android NDK (install reserved for Volmarr's word), an NDK build, and a physical SM-X400 run of the ready D-007 program |
| O-17 | **Phase 3 exit UNMET**: the x86-64 suite run (D-005: BUILD leg blocked on the missing x86-64 libc sysroot, install reserved for Volmarr's word); the 4096-page-host allocator run (INC-D003-02) | E-001 Section 6.3 C3 | An x86-64 sysroot + suite run; a 4096-page environment run (the O-16 Android run discharges the latter if it records page size 4096 in-run) |
| O-18 | musl leg of libc compatibility BLOCKED (no musl toolchain on the Pi; install reserved for Volmarr's word); bionic leg beyond by-analogy OWED | E-001 Section 6.3 C4, INC-D008-06/-07 | musl package at Volmarr's word; the O-16 NDK road for bionic |

## 8. Supported-target position (Roadmap Section 5)

Unchanged in kind from D-009, updated for Wave 5:
**no target is supported** in the Section 5 sense. The
Linux AArch64 + glibc row is the strongest - it now
carries, on top of its Phase 3/4 record, a complete
Phase 5 record: a controlled-export shared library, two
proven link routes with one named as THE route,
loader-level binding proof, an audit-counter use-proof,
an aligned-surface decision, a C++ consumption proof,
and startup/cross-module proofs - all Pi-native, all
reproduced in this packet's fresh run - and it still
lacks native CI. Every other row lacks more (Section
7, O-16/O-17/O-18). No performance claim exists
anywhere in the repository, and none is made here.

## 9. Inconsistencies recorded by this slice (INC-E009-*)

1. **INC-E009-01 - THE MAKEFILE HEADER IS STALER
   STILL (INC-D009-01, CARRIED AND WORSENED).** The
   Makefile's top comment still reads "Phase 3 build
   (slices C-001..C-007)" and its per-slice comment
   block still stops at D-004, while the file in fact
   wires D-003..D-008 and E-002..E-008 targets (each
   with its own comment further down). D-009 recorded
   this at the Wave 4 base; no Wave 5 slice touched the
   header either (every Wave 5 Makefile change was an
   additive appended block). Cosmetic only - the fresh
   run exercised every target successfully - but a
   reader of the header alone would under-scope the
   build by two phases. Fixing it is a Makefile comment
   change, outside this slice's synthesis-only scope;
   recorded for the next slice that legitimately
   touches the Makefile.
2. **INC-E009-02 - INC-D001-02 IS STILL OPEN:
   SUPPORTED_TARGETS.md WAS NEVER REFRESHED
   (INC-D009-02, CARRIED).** D-001 recorded that A-003's
   status reasoning is stale and that a future slice
   should refresh the file or mark it superseded in
   place. No Wave 4 or Wave 5 slice did so: at this
   base the file still contains no Phase 4 or Phase 5
   reference at all, and its conclusion (no target
   supported) is correct only for the newer reasons in
   D-001 Section 7, D-009 Section 8 and Section 8 of
   this packet. Recorded again so the debt is not lost
   at wave end.
3. **INC-E009-03 - E-005's DOCUMENT CITES A BASE TWO
   BANKS BEHIND ITS BANKED POSITION.** The banked
   `docs/PHASE_5_AUDIT_HOOK_E005.md` header reads "Base:
   966214d (E-003 banked)", because E-005 was built in
   parallel from the E-003 base while E-004 was still
   unbanked, and the keeper merged its Makefile block
   onto E-004's at banking time (banked parent of
   E-005 is `f282069`, E-004). The slice's own text is
   consistent with that history - it names the exact
   route as still E-004's to name and claims nothing
   from E-004 - so no evidence is mis-stated; the seam
   is recorded so a reader reconciling bases is not
   confused by the header.
4. **INC-E009-04 - THE EXIT PACKETS' RECORDED RUNS
   LIVE IN THREE DIFFERENT PLACES.** C-009's fresh run
   was a `/tmp` file on the Pi at run time (not a repo
   artefact; its counts are banked only as quoted in
   the packet); D-009's is `tests/recorded_run_d009.txt`;
   this packet's is `tests/abi/recorded_run_e009.txt`,
   following the uniform Wave 5 placement
   (E-002..E-008). All three packets cite their paths
   correctly, so no evidence is lost; recorded so
   future readers look in all three places for
   exit-packet runs.
5. **INC-E009-05 - THE ROADMAP'S PHASE 5 HEADING
   STILL PROMISES "INTERPOSITION" (INC-E001-01,
   CARRIED, STILL UNCORRECTED).** Wave 5 delivered the
   binding half of the heading completely and the
   interposition half not at all, by contract, and
   rejected R4 with its reasoning recorded (Section
   6). The roadmap wording itself is its owner's to
   correct; until it is corrected, this item is the
   standing counterweight: no Wave 5 result, and not
   this packet's MET verdict, may be cited as
   interposition coverage.
6. **INC-E009-06 - THE FRESH RUN REPRODUCED
   INC-E006-03's NON-DETERMINISM LIVE.** E-006's
   recorded run read the 32768-demand observation as
   `ptr mod 32768 = 16384`; this packet's fresh run of
   the same test, same host, read `= 0`. Both runs pass
   every check (the value is an OBSERVED line, never a
   check). Recorded as a confirmation of INC-E006-03,
   not a new defect: above the page size, the
   construction delivers nothing promised, and a
   single observation in either direction proves
   nothing.
7. **INC-E009-07 - NO SINGLE BANKED RUN BEFORE THIS
   PACKET CONTAINED ALL EIGHT PHASE 5 STANDALONE
   TARGETS.** Each Wave 5 slice ran its own target plus
   `make test`; the keeper's per-bank verification runs
   came closest (all targets green at `ca77365`). The
   fresh run in Section 2 is the first recorded run on
   one tree, from clean, containing `make test` and
   test-e002, test-e003, test-e004, test-e005,
   test-e005-dyn, test-e006, test-e007 and test-e008
   together - the same seam C-009 recorded for its
   wave (INC-C009-01), recorded here so no reader
   assumes an earlier all-target run exists.

## 10. Bottom line

Wave 5 built the whole Phase 5 record on the Pi-native
target and proved it repeatedly: a shared library whose
dynamic surface is exactly the four documented symbols,
with the 13-symbol leak of INC-D002-02 remedied by two
independent control layers (E-002); both link routes
built and recorded, with R2 dynamic proven at loader
level by an independent executable and named as THE
exact route (E-003, E-004); implementation-resident
audit counters proving the intended call path serviced
the calls, with a libc-bypass negative that moved
nothing (E-005); an aligned-surface decision that added
nothing and promised nothing beyond the measured floor
(E-006); C++ consumption proven with no surface added
(E-007); and startup ordering plus cross-module
ownership proven against the one shared library
instance, with mixed-allocator ownership stated
honestly as a contract posture rather than a runtime
rejection (E-008). This packet's fresh run reproduces
every one of those counts at the Wave 5 final base.

**Phase 5 exit is MET** - exit items **MET 4 of 4**;
task items **MET 7 / PARTIAL 1 of 8** (T7's rejection
half held as contract posture only) - **in the
Pi-native scope stated at the head of this packet**:
native AArch64 Linux + glibc on this Pi, the
four-symbol namespaced surface, the R2 dynamic route.
It becomes false the moment it is quoted wider: not
Android, not x86-64, not musl, not Mojo, not
interposition, not an ABI freeze. The carried register
(Section 7) survives the exit in full - headed by
Phase 4 exit UNMET with its physical Android run OWED
(O-16), Phase 3 exit UNMET with its x86-64 run OWED
(O-17), the open U-01/U-03 contract questions (O-15),
and the SONAME/install story owed to an ABI-freeze
slice (O-01). Wave 5 is complete at 9 of 9 slices with
this packet; the next phase's work (Phase 6, Mojo
integration) starts from the recorded R2 route, not
from any wider reading of this verdict.

`README.md` was not touched by this slice.
