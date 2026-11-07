# E-008 - Startup ordering + cross-module ownership

Phase 5 (ABI completeness and interposition), Wave 5, slice 8 of 9.
Base: d578523b4328376642d36002b6b58874f4be9b60 (E-007 banked;
Wave 5 = 7 of 9 at slice start).
Branch: yrsa/mimiralloc-08.
Worktree: /home/pi/mimiralloc-team-08 on purpleviking1
(Raspberry Pi, Cortex-A76), user pi.
Recorded run date: 2026-11-06T20:41-05:00 (the run file's own
S0/S9 stamps read 20:41:18 / 20:41:22 EST).
Toolchain: Debian GNU/Linux 12 (bookworm); cc 12.2.0
(Debian 12.2.0-14+deb12u1); GNU ld 2.40; kernel
6.12.112-v8-16k-hailo+; getconf PAGESIZE = 16384; glibc 2.36.
Pi load stayed light (sequential small makes only; free -g
showed 13 GiB available; Samtal on port 8787 answered
HTTP 200 before and after the runs, recorded run S0 / S9).

Governing texts: roadmap Phase 5; docs/PHASE_5_FOUNDATION_PLAN.md
(E-001 - Section 2 startup facts, task items T7 and T8,
Section 5.4 note that E-008 only proves shared ownership if
both modules bind the one instance); include/mimir_alloc.h
(the public surface and its by-analogy ownership rows);
docs/PHASE_5_BINDING_PROOF_E004.md (E-004 - the R2 dynamic
route consumed here); docs/PHASE_5_AUDIT_HOOK_E005.md
(E-005 - the audit counters and their shape-(b) limits);
the Wave 5 queue line for E-008 in WAVE_PLAN.md.

## 0. Bottom line

- PROVEN (cross-module ownership, same instance): a block's
  allocation, fill, realloc and free can execute in
  DIFFERENT ELF modules against the one libmimiralloc.so
  instance, in both directions, with contents preserved
  byte-for-byte at every hand-off. The main consumer
  (tests/abi/test_e008_main.c) passes 33/33 checks and the
  peer module (tests/abi/test_e008_peer.c, built as its own
  shared library build/libe008peer.so) performs the
  complementary half of each lifecycle from inside itself
  (recorded run S4). Sameness of instance is observable
  three ways (Section 2): DT_NEEDED naming libmimiralloc.so
  on BOTH modules, ldd resolving both to the one built .so
  path, and exactly 8 LD_DEBUG binding lines - each of the
  four mimir_* symbols bound once from the executable and
  once from the peer library, every line to the same
  libmimiralloc.so file (recorded run S5).
- PROVEN (startup ordering): the allocator imposes NO
  explicit or implicit init ordering requirement, because
  there is nothing to initialise: the banked sources
  contain no constructor, no pthread_once and no init
  function (grep evidence, recorded run S3); the one global
  lock is statically initialised (PTHREAD_MUTEX_INITIALIZER,
  src/platform/mimir_platform.c:140) and the registry
  anchors are static storage in their starting state
  (src/core/mimir_core.c:180-182) - the B-007 C2 / B-006
  F-08 "clean by construction" design fact. Tested as
  actually built, three ways, all through the linked .so:
  (a) allocation as the very first MimirAlloc call of a
  program - test_e008_first, 9/9 (recorded run S4);
  (b) allocation from a module constructor before main,
  the block verified and written in main and freed by the
  peer destructor after main - the CTOR line is the first
  line of the run and the DTOR verification line follows
  main's RESULT line (recorded run S4 / S6);
  (c) foundation T8's second startup case - the first-ever
  calls raced from 8 threads released together behind a
  barrier: all succeed, all blocks distinct, all patterns
  intact - test_e008_threads, 13/13 (recorded run S4).
  No ordering guarantee beyond "no setup is needed" is
  claimed, because the code provides none to claim.
- HONEST VERDICT (mixed-allocator ownership): there is NO
  runtime rejection to demonstrate, and this slice
  demonstrates none. The header's contract (by analogy to
  MA-6.1-FREE-03/-04, MA-6.1-REALLOC-05) makes freeing or
  reallocating a foreign pointer undefined behaviour with
  no defined error, return or recovery. The "rejection" is
  exactly that contract posture - absence of any ownership
  transfer for foreign pointers - plus a test-design
  prohibition: NO test in this slice ever passes a foreign
  pointer to a mimir_* entry point. The safe observable
  probes that do exist were run in a separate child process
  (test_e008_mixed, 9 checks, recorded run S4 / S7): a full
  libc malloc/calloc/realloc/free cycle moves NO MimirAlloc
  audit counter (the families' books are disjoint), while a
  MimirAlloc alloc/free moves the counters exactly +1/+1/+1/
  +1. The gap between the Wave 5 queue wording ("rejected
  per contract") and the contract's actual UB posture is
  INC-E008-01.
- The public surface is unchanged: nm -D on the rebuilt
  libmimiralloc.so shows exactly the four defined dynamic
  symbols (recorded run S2). make test composition is
  unchanged and exits 0; make test-e008 exits 0; both were
  run twice in the recorded run, the second time from clean,
  with identical counts (S1 / S4 / S8). README.md and
  include/mimir_alloc.h are byte-identical to base (sha256
  in S9). Phase 5 exit stays UNMET until E-009's packet.
  One commit on yrsa/mimiralloc-08; no push or merge (the
  keeper banks in roadmap order).

Evidence file: tests/abi/recorded_run_e008.txt (871 lines,
63,102 bytes, sha256 67e58aa9) - every command with its exit
code, in sections S0-S9, including a full clean re-run (S8)
reproducing every count. This document is the slice record;
the recorded run is the evidence.

## 1. What was built

Six new test files under tests/abi/, one additive Makefile
block (+64 lines, 0 deletions), this document, and the
recorded run. No file under src/ or include/ was modified.

- tests/abi/test_e008_peer.h (80 lines, 3,344 bytes) - the
  peer module's test-only interface: ctor-block accessors
  and pattern, plus fill / check / alloc / calloc / realloc /
  free wrappers that call the public API from inside the
  peer. Declared in no installed header; libmimiralloc.so
  neither knows nor exports any of it.
- tests/abi/test_e008_peer.c (145 lines, 4,019 bytes) - the
  peer module itself, built -fPIC as build/libe008peer.so
  and linked against build/libmimiralloc.so with its own
  DT_NEEDED entry and $ORIGIN RUNPATH. Its constructor
  allocates 128 bytes through mimir_malloc and fills them
  with a position pattern; its destructor verifies the
  block (main's 32-byte sentinel + the pattern) and frees
  it through mimir_free. Both print one line each; those
  lines' positions in the output are the ordering evidence.
- tests/abi/test_e008_main.c (245 lines, 9,600 bytes) - the
  cross-module consumer: 33 checks (Section 2). Links both
  libraries in the E-004 R2 dynamic route.
- tests/abi/test_e008_first.c (100 lines, 3,652 bytes) - the
  first-call consumer: no constructor, no other MimirAlloc
  call site; the first statement of main IS the mimir_malloc
  call (before any printf; stdio may allocate through libc,
  never through MimirAlloc - the claim is exactly "first
  MimirAlloc call" and is worded that way in the source).
  9 checks.
- tests/abi/test_e008_threads.c (147 lines, 4,727 bytes) -
  the concurrent first-call consumer: 8 threads created
  before any MimirAlloc call in the process, released
  together behind a pthread barrier, each allocating as its
  first action. 13 checks. (One build fix was needed and is
  in the source: pthread barriers need _POSIX_C_SOURCE
  200809L under strict -std=c11 - the same feature-test
  pattern src/platform/mimir_platform.c uses.)
- tests/abi/test_e008_mixed.c (166 lines, 6,571 bytes) - the
  mixed-allocator safe-probe child process (Section 4).
  Links the library OBJECTS, like test_e005.c, solely to
  read the E-005 audit counters. 9 checks.
- Makefile block - PIC object + shared-library rules for
  the peer, object/executable rules for the four programs
  (the mixed probe links LIBOBJS; the other three link the
  .so in the R2 route), and the standalone test-e008 target
  running the four programs in order first / threads / mixed
  / main. make test is untouched (its prerequisite line is
  reprinted in recorded run S9).

## 2. Cross-module ownership (recorded run S4, S5)

"Same instance" is pinned before the lifecycles run. The
main executable and the peer library EACH carry DT_NEEDED
libmimiralloc.so and EACH hold undefined references to all
four mimir_* symbols (S5.1-S5.4). ldd resolves the
executable's and the peer's libmimiralloc.so to the one
built file in this worktree (S5.5-S5.6). Under
LD_DEBUG=bindings, exactly 8 binding lines name a mimir_*
symbol: mimir_malloc / mimir_realloc / mimir_free /
mimir_calloc bound from build/libe008peer.so, and the same
four bound from ./build/test_e008 - every one of them TO
the same libmimiralloc.so path (S5.7). Two modules, one
mapped library, one registry and lock behind it: a block
either module allocates is a live block of the allocator
the other module operates on. The lifecycles then prove
the operational consequence, contents checked byte-for-byte
at every hand-off (33/33 in S4):

- Direction A (main -> peer): main allocates 96 bytes; the
  peer fills them (0x41); main verifies; the peer reallocs
  to 240 (grow); main verifies the preserved 96-byte prefix;
  main fills the tail (0x42); the peer verifies both regions
  from its side; the peer frees. Main then allocates afresh
  successfully (allocator healthy after a cross-module free).
- Direction B (peer -> main): the peer allocates 80 bytes;
  main fills a position pattern; main reallocs to 200; main
  verifies the preserved prefix byte-for-byte; the peer
  verifies the uniform tail main wrote; main frees.
- calloc both ways: peer calloc(10, 16) verified zeroed in
  main, filled in main, verified in the peer, freed in the
  peer; main calloc(8, 24) verified zeroed BY the peer,
  realloc-shrunk to 64 IN the peer with the 64-byte prefix
  verified preserved in main, freed in main.
- The ctor block (Section 3) is simultaneously a live
  cross-module block throughout: allocated in the peer,
  verified and written in main, re-verified in the peer,
  still valid at the end of main, freed in the peer.
- Distinctness is checked while blocks are live (main and
  peer blocks each != the live ctor block), and
  mimir_free(NULL) is exercised from both modules as the
  defined no-op.

Foundation plan Section 5.4's warning is discharged: this
is not the ambiguous case of two separately linked copies -
there is exactly one .so in the process, and the binding
evidence above is what makes the sharing a proof rather
than an assumption.

## 3. Startup ordering (recorded run S3, S4, S6)

The sources were checked BEFORE any test was written, per
the slice order. What is actually there (S3): no
constructor attribute, no pthread_once, no call_once and
no init function anywhere in src/ or include/ - the three
grep hits for "constructor" are comments stating there is
none. The L6 lock is a statically initialised
pthread_mutex_t (PTHREAD_MUTEX_INITIALIZER). The core's
registry head and live counters are static storage starting
NULL / 0. The public API layer adds only its four atomic
call counters, also static zero. There is therefore NO
explicit init step a caller could perform and NO lazy-init
path a first call could trip over: the design has no
ordering requirement at all, and the honest test is to
attack the moments an ordering requirement would bite:

- (a) Very first call of a program (test_e008_first, 9/9):
  the first statement of main is mimir_malloc(128), in a
  process whose only MimirAlloc call sites are in that one
  TU and whose linked library runs no init. The call
  succeeds with no prior setup; the block is aligned,
  writable over its full extent, realloc-grows with bytes
  preserved, and a calloc / free(NULL) / fresh-malloc tail
  all behave. PROVEN on the Pi.
- (b) Constructor before main, destructor after main
  (test_e008 + peer): the peer constructor's mimir_malloc
  is, for that module, a first call issued by the dynamic
  loader before main exists as running code. In the run
  output the CTOR line is literally the first line (S6:
  head -n 1), ahead of the consumer's own banner. Main
  finds the block live, verifies all 128 pattern bytes,
  writes its sentinel into the head, and the peer verifies
  the combined contents from its side; at the end of main
  the block is checked still valid. After main's RESULT
  line, the DTOR line reports the contents "verified
  byte-for-byte" and the free executed in the peer (S6:
  tail -n 2). PROVEN on the Pi - with the recorded limit
  that the destructor's verdict can only exist as that
  printed line (INC-E008-03).
- (c) First calls raced from multiple threads
  (test_e008_threads, 13/13): 8 threads, no prior
  MimirAlloc call in the process, barrier release, each
  thread's first action a mimir_malloc. All 8 succeed;
  blocks are pairwise distinct; each holds its own thread's
  pattern byte-for-byte (no cross-talk under the statically
  initialised lock). As a bonus observation only, each
  block was then freed from a DIFFERENT thread than its
  allocator, all returning normally - cross-thread
  ownership is not a Phase 5 contract row and is claimed
  as nothing more than this recorded behaviour. PROVEN on
  the Pi (the race; the bonus as observed behaviour).

What is NOT claimed: any ordering guarantee BETWEEN
multiple consumer constructors (the loader's library-init
order is a loader property, tested here for exactly one
peer library); any fork or async-signal property (none is
promised by the header; U-17..U-21 stay open); any claim
about dlopen-time construction (route R3 remains OWED by
design, foundation Section 6.3 item C7 - the peer here is
a DT_NEEDED library loaded at startup, not a dlopen plug-in).

## 4. Mixed-allocator ownership: the honest verdict

The contract posture, restated exactly: a pointer must be
released by the allocator family that allocated it, through
the same library instance. Freeing or reallocating a
foreign, interior or already-freed pointer is undefined
behaviour with no defined error, return or recovery
(include/mimir_alloc.h, by analogy to MA-6.1-FREE-03/-04
and MA-6.1-REALLOC-05; B-006 S8.3 says the same for the
core). There is NO promised runtime rejection - no error
code exists for mimir_free to return (it returns void on
every defined path), no validation result is specified,
and the implementation performs no foreign-pointer check
that a test could cite: owner determination walks the
registry comparing pointer VALUES and never dereferences
the candidate pointer's memory (src/core/mimir_core.c,
find_candidate_locked), so a foreign pointer simply never
matches a live descriptor - and what the code then does
with a non-matching pointer on the free path is an
implementation behaviour under a UB contract, NOT a
rejection promise, and this slice deliberately does not
probe it: passing a libc pointer to mimir_free to see
what happens would be executing undefined behaviour and
reporting the weather. A crash would prove nothing; a
clean return would prove nothing; neither is evidence.

So "rejection" in this slice means, and only means:
(1) the contract rule above, which assigns foreign
pointers NO ownership transfer - there is no sense in
which MimirAlloc accepts a libc block; and (2) the
test-design prohibition this slice actually enforced -
no program here passes a foreign pointer to a mimir_*
entry point, in the main test process or anywhere else.

The safe probes that remain were run in the separate
child process test_e008_mixed (9/9, S4 / S7):
- A complete libc cycle (malloc 64, calloc 16x8, malloc 32
  realloc'd to 96, all freed with libc free) leaves every
  E-005 audit counter at exactly 0 - libc ownership never
  enters MimirAlloc's books, observably.
- One mimir_malloc / mimir_free pair then moves
  call_malloc, published, call_free and retired exactly
  +1 each and returns live mappings to 0 - MimirAlloc
  ownership exists only for blocks the family created.
- Construction observation, labelled NOT a contract claim:
  the MimirAlloc block is page-aligned (its own OS mapping,
  the B-006 baseline); no property of any libc block is
  asserted or compared.

If a future phase wants a detectable rejection (a debug
build that aborts on a non-registry pointer, say), that is
a contract change requiring its own decision - exactly as
foundation T7(b) warned - and is recorded here as OWED to
whoever owns that decision, not slipped into this slice.

## 5. Verification (recorded run S1, S2, S4, S8, S9)

- make test: exit 0, twice (S1 and the clean re-run S8);
  7 RESULT: PASS lines each time; composition unchanged
  (the test: prerequisite line is reprinted in S9 and
  contains no E-008 program).
- make test-e008: exit 0, twice (S4 and S8), the second
  time from clean. Counts, identical both times:
  test_e008_first 9/9 PASS; test_e008_threads 13/13 PASS;
  test_e008_mixed 9 run / 0 failed PASS; test_e008_main
  33/33 PASS, bracketed by the peer CTOR line (first) and
  DTOR verification line (last).
- nm -D --defined-only on the rebuilt libmimiralloc.so:
  exactly 4 (mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc), S2. The peer library's own exports are
  test plumbing in a separate file and are not part of
  the allocator's surface.
- LD_DEBUG binding lines naming a mimir_* symbol: exactly
  8 (4 symbols x 2 modules), all to the one .so (S5.7).
- README.md sha256 bd7f6614 and include/mimir_alloc.h
  sha256 55107f05, both byte-identical to base d578523
  (S9). Makefile diff vs base: +64 / -0. git status at
  record time: the six new test files, the recorded run
  and this document (added after the run, so it appears
  in the commit, not in S9's status listing) plus the
  pre-existing untracked hidden_files/ and build/.
- Samtal HTTP 200 before and after (S0 / S9); free -g
  13 GiB available throughout; no heavy step was run.

## 6. Claims register

- Cross-module same-instance lifecycles, both directions,
  contents byte-for-byte: PROVEN on the Pi (S4, S5).
- No init / lazy-init exists in the allocator sources:
  PROVEN on the Pi by inspection (S3).
- First-call, constructor-before-main, destructor-after-
  main, and concurrent-first-call behaviour: PROVEN on
  the Pi (S4, S6).
- Mixed-allocator contract posture (foreign free = UB, no
  promised rejection): by-analogy contract reading of the
  banked header (U-03 applies to the whole namespaced
  surface); the counter-separation observation is PROVEN
  on the Pi (S7).
- Cross-thread free returning normally: observed behaviour
  on the Pi, NOT a contract claim (Section 3(c)).
- dlopen (R3) ordering and ownership: OWED by design.
- Runtime rejection of foreign pointers: OWED to a future
  contract decision; does not exist and is not promised.
- Phase 5 exit: UNMET - E-009's packet must still gather
  X1-X4; this slice supplies the T7 / T8 evidence it cites.

## 7. Inconsistencies recorded by this slice (INC-E008-*)

- INC-E008-01 - The Wave 5 queue wording overpromises a
  rejection the contract does not contain. The queue line
  reads "mixed-allocator ownership rejected per contract";
  the contract (header, by analogy to MA-6.1-FREE-03/-04)
  defines foreign-pointer free as undefined behaviour with
  NO defined error or recovery - there is no rejection
  behaviour to test and none was implemented or probed.
  The foundation plan's T7(b) already carries the honest
  reading (documented rule + test-design prohibition);
  this slice executed exactly that reading and records
  the queue phrase as discharged ONLY in that sense. Any
  reader who takes the queue line literally will look for
  a runtime check that does not exist.
- INC-E008-02 - Two different "startup ordering" scopes
  circulate. The queue line and this slice's order name
  the first-call and constructor cases; foundation T8
  additionally requires a first-call-from-multiple-threads
  case. Both scopes were executed (Sections 3(a)-(c)) so
  no evidence gap remains, but the queue text on its own
  understates T8 and a future reader reconciling the two
  should treat T8 as the complete list.
- INC-E008-03 - The destructor half of the startup proof
  is observable only as printed output. A library
  destructor runs after main has returned and cannot hand
  a status to any checker; its verification verdict exists
  solely as the DTOR line's position and wording in the
  run output (S6). If that line's wording regressed to a
  mismatch report, the test binary itself would still have
  exited 0 - make test-e008 does NOT gate on the
  destructor's verdict. Recorded honestly: the after-main
  free is proven to have RUN and to have verified, by the
  recorded line; it is not wired into the exit code, and
  no mechanism within a single process run can wire it.
- INC-E008-04 - No single binary proves both the dynamic
  binding story and the counter story. The E-005 audit
  counters are bound local in libmimiralloc.so by design,
  so the .so consumers (main / first / threads) prove
  sameness by DT_NEEDED + LD_DEBUG + pointer/contents
  evidence, while the counter-exactness evidence lives in
  the statically linked mixed probe - a different binary
  running a different link route. The two legs corroborate
  each other but are never the same run; a reader must
  not cite the mixed probe's counters as observations OF
  the cross-module run.
- INC-E008-05 - Constructor ORDER is a loader property,
  not an allocator property, and only one point of it is
  tested. What the allocator contributes is solely the
  absence of any init need (S3). The proof covers one
  DT_NEEDED peer library initialised before one
  executable's main; it says nothing about the relative
  order of several consumer libraries' constructors, nor
  about constructors in the executable itself (the
  consumers here deliberately have none), nor about
  dlopen-loaded modules (R3, OWED by design).
