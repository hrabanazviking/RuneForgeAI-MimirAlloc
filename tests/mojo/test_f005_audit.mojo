# F-005 - allocator-use proof on the Mojo path, STATIC leg
# (Phase 6; E-005 audit-counter proof, shape (b), carried
# to the Mojo path; closes INC-F003-05 for this leg).
#
# The generated executable built from this source links the
# library OBJECTS (build/mimir_alloc.o, build/mimir_core.o,
# build/mimir_platform.o) and the test-only getter object
# (build/f005_audit.o, from tests/mojo/f005_audit.c)
# directly into itself - the E-005 static leg's
# construction. The six internal E-005 audit counters are
# therefore THIS executable's own counters: the L1 call
# counters in src/api/mimir_alloc.c and the core mapping
# counters in src/core/mimir_core.c, read through the
# f005_audit_* getter functions (Mojo's external_call calls
# functions; it cannot read a C global directly). In the
# shared library the counters are local and unreadable -
# which is why this leg is a separate build from the
# dynamic leg (tests/mojo/test_f005.mojo), exactly as in
# E-005.
#
# The phase script and its expected counts are E-005's
# static script, unchanged in substance; the expectations
# are derived from the calls themselves, not read back.
# G-004 re-teach (INC-G004-05, mirroring the G-003
# re-teach of the C static leg): every size in the
# script is <= 1024, so from Phase 7 the script's
# allocations are span-served and publish/retire in the
# G-003 span/object counters, NOT in the mapping
# counters. expect() therefore asserts both registries:
# mapping published/retired stay 0 throughout, and the
# small counters (small_alloc/small_free, the last two
# expect() arguments) carry exactly the totals the
# mapping counters carried at E-005:
#   Phase 1: 5 mimir_malloc, all succeed
#            -> call_malloc 5, small_alloc 5
#   Phase 2: libc bypass (3 malloc-family allocations,
#            1 realloc, 3 frees) -> no counter moves
#   Phase 3: 3 mimir_calloc, all succeed, extents zeroed
#            -> call_calloc 3, small_alloc 8
#   Phase 4: realloc grow x2 (each hands out a new span
#            object and retires the old), shrink x1,
#            same-size x1
#            -> call_realloc 4, small_alloc 10, small_free 2
#   Phase 5: oversize mimir_malloc fails NULL, overflow
#            mimir_calloc fails NULL
#            -> call_malloc 6, call_calloc 4, small_alloc 10
#   Phase 6: mimir_free(NULL) (call only), free 4 live
#            -> call_free 5, small_free 6
#   Phase 7: free the remaining 4 live blocks
#            -> call_free 9, small_free 10 = small_alloc
#
# The Phase 6/7 counter movements are the strongest free
# evidence this campaign has: mimir_free returns void
# (INC-F003-04), but each free of a live block provably
# retires exactly one object in the implementation, and
# the books balance exactly (published == retired in
# the mapping registry at 0 == 0, small_alloc ==
# small_free, live 0) only if every free this program
# issued was serviced.
#
# One Mojo-specific construction: a literal null Pointer
# cannot be written in Mojo source - the toolchain rejects
# Pointer(unsafe_from_address=0) at compile time ("Pointer
# is non-nullable"; the failed probe build is recorded in
# the slice document). Phase 6 therefore derives the zero
# address at runtime (a counter minus itself, printed and
# checked == 0 before the call). The value passed to
# mimir_free is genuinely NULL; only its spelling differs
# from C.
#
# Scope discipline (F-001 Section 6): this proves only that
# THIS generated program's OWN EXPLICIT mimir_* calls move
# the implementation's counters exactly. The counters are
# process-local: the Mojo runtime's and stdlib's own
# allocations never reach the mimir_* entry points (they
# are not libc-named either - the counters count mimir_*
# invocations only), so they cannot move them; Phase 2
# proves the same for explicit libc calls in-process.
#
# Exit behaviour: one AUDIT line per phase plus [ok]/[FAIL]
# lines; on any failure it raises after the summary, so
# the process exits non-zero. On full success it prints
# the PASS line and returns normally (exit 0).

from std.ffi import external_call
from std.memory import Pointer
from std.origin import MutUntrackedOrigin


def mimir_malloc(size: UInt64) -> Pointer[UInt8, MutUntrackedOrigin]:
    return external_call[
        "mimir_malloc", Pointer[UInt8, MutUntrackedOrigin], UInt64
    ](size)


def mimir_calloc(
    nmemb: UInt64, size: UInt64
) -> Pointer[UInt8, MutUntrackedOrigin]:
    return external_call[
        "mimir_calloc",
        Pointer[UInt8, MutUntrackedOrigin],
        UInt64,
        UInt64,
    ](nmemb, size)


def mimir_realloc(
    ptr: Pointer[UInt8, MutUntrackedOrigin], size: UInt64
) -> Pointer[UInt8, MutUntrackedOrigin]:
    return external_call[
        "mimir_realloc",
        Pointer[UInt8, MutUntrackedOrigin],
        Pointer[UInt8, MutUntrackedOrigin],
        UInt64,
    ](ptr, size)


def mimir_free(ptr: Pointer[UInt8, MutUntrackedOrigin]):
    external_call[
        "mimir_free", NoneType, Pointer[UInt8, MutUntrackedOrigin]
    ](ptr)


def libc_malloc(size: UInt64) -> Pointer[UInt8, MutUntrackedOrigin]:
    return external_call[
        "malloc", Pointer[UInt8, MutUntrackedOrigin], UInt64
    ](size)


def libc_calloc(
    nmemb: UInt64, size: UInt64
) -> Pointer[UInt8, MutUntrackedOrigin]:
    return external_call[
        "calloc", Pointer[UInt8, MutUntrackedOrigin], UInt64, UInt64
    ](nmemb, size)


def libc_realloc(
    ptr: Pointer[UInt8, MutUntrackedOrigin], size: UInt64
) -> Pointer[UInt8, MutUntrackedOrigin]:
    return external_call[
        "realloc",
        Pointer[UInt8, MutUntrackedOrigin],
        Pointer[UInt8, MutUntrackedOrigin],
        UInt64,
    ](ptr, size)


def libc_free(ptr: Pointer[UInt8, MutUntrackedOrigin]):
    external_call["free", NoneType, Pointer[UInt8, MutUntrackedOrigin]](ptr)


def a_malloc() -> UInt64:
    return external_call["f005_audit_call_malloc", UInt64]()


def a_calloc() -> UInt64:
    return external_call["f005_audit_call_calloc", UInt64]()


def a_realloc() -> UInt64:
    return external_call["f005_audit_call_realloc", UInt64]()


def a_free() -> UInt64:
    return external_call["f005_audit_call_free", UInt64]()


def a_published() -> UInt64:
    return external_call["f005_audit_published", UInt64]()


def a_retired() -> UInt64:
    return external_call["f005_audit_retired", UInt64]()


def a_small_alloc() -> UInt64:
    return external_call["f005_audit_small_alloc", UInt64]()


def a_small_free() -> UInt64:
    return external_call["f005_audit_small_free", UInt64]()


def verdict(ok: Bool, label: String) -> Int:
    if ok:
        print("  [ok]", label)
        return 0
    print("  [FAIL]", label)
    return 1


def expect(
    phase: String,
    cm: UInt64,
    cc: UInt64,
    cr: UInt64,
    cf: UInt64,
    pub: UInt64,
    ret: UInt64,
    sa: UInt64,
    sf: UInt64,
) -> Int:
    var ok = (
        a_malloc() == cm
        and a_calloc() == cc
        and a_realloc() == cr
        and a_free() == cf
        and a_published() == pub
        and a_retired() == ret
        and a_small_alloc() == sa
        and a_small_free() == sf
    )
    print(
        "AUDIT",
        phase,
        " call_malloc=",
        a_malloc(),
        " call_calloc=",
        a_calloc(),
        " call_realloc=",
        a_realloc(),
        " call_free=",
        a_free(),
        " published=",
        a_published(),
        " retired=",
        a_retired(),
        " live=",
        a_published() - a_retired(),
        " small_alloc=",
        a_small_alloc(),
        " small_free=",
        a_small_free(),
    )
    if ok:
        return 0
    return 1


def main() raises:
    print("F-005 audit leg (Mojo program, library objects linked directly;")
    print("E-005 counters read through f005_audit_* getters)")
    var run = 0
    var failed = 0

    # Start state: every counter zero before any call.
    run += 1
    failed += expect("start", 0, 0, 0, 0, 0, 0, 0, 0)

    # Phase 1: five mimir_malloc through the public path.
    var m0 = mimir_malloc(32)
    var m1 = mimir_malloc(64)
    var m2 = mimir_malloc(128)
    var m3 = mimir_malloc(256)
    var m4 = mimir_malloc(512)
    run += 1
    failed += verdict(
        Int(m0) != 0 and Int(m1) != 0 and Int(m2) != 0
        and Int(m3) != 0 and Int(m4) != 0,
        "phase1: 5x mimir_malloc non-null",
    )
    m0[unsafe_offset=0] = UInt8(0x40)
    m1[unsafe_offset=0] = UInt8(0x41)
    m2[unsafe_offset=0] = UInt8(0x42)
    m3[unsafe_offset=0] = UInt8(0x43)
    m4[unsafe_offset=0] = UInt8(0x44)
    run += 1
    failed += expect("phase1-mimir-malloc-x5", 5, 0, 0, 0, 0, 0, 5, 0)

    # Phase 2 (NEGATIVE): libc calls in the same process
    # bypass MimirAlloc entirely; no counter may move.
    var x = libc_malloc(64)
    var y = libc_calloc(16, 8)
    var z = libc_malloc(32)
    run += 1
    failed += verdict(
        Int(x) != 0 and Int(y) != 0 and Int(z) != 0,
        "phase2: libc allocations non-null",
    )
    if Int(z) != 0:
        var z2 = libc_realloc(z, 96)
        run += 1
        failed += verdict(Int(z2) != 0, "phase2: libc realloc non-null")
        if Int(z2) != 0:
            z = z2
    libc_free(x)
    libc_free(y)
    libc_free(z)
    run += 1
    failed += expect("phase2-libc-bypass", 5, 0, 0, 0, 0, 0, 5, 0)

    # Phase 3: three mimir_calloc; extents read zero.
    var c0 = mimir_calloc(4, 16)
    var c1 = mimir_calloc(8, 8)
    var c2 = mimir_calloc(1, 100)
    run += 1
    failed += verdict(
        Int(c0) != 0 and Int(c1) != 0 and Int(c2) != 0,
        "phase3: 3x mimir_calloc non-null",
    )
    var okz = True
    for i in range(64):
        if c0[unsafe_offset=i] != UInt8(0) or c1[unsafe_offset=i] != UInt8(0):
            okz = False
    for i in range(100):
        if c2[unsafe_offset=i] != UInt8(0):
            okz = False
    run += 1
    failed += verdict(okz, "phase3: calloc extents zeroed")
    run += 1
    failed += expect("phase3-mimir-calloc-x3", 5, 3, 0, 0, 0, 0, 8, 0)

    # Phase 4: realloc - grow x2 moves (hand out + retire
    # a span object each), shrink x1 and same-size x1 move
    # no counter.
    for i in range(32):
        m0[unsafe_offset=i] = UInt8(i + 1)
    var g0 = mimir_realloc(m0, 96)
    run += 1
    failed += verdict(Int(g0) != 0, "phase4: realloc grow m0 32 -> 96 non-null")
    if Int(g0) != 0:
        var okg = True
        for i in range(32):
            if g0[unsafe_offset=i] != UInt8(i + 1):
                okg = False
        run += 1
        failed += verdict(okg, "phase4: grow preserves first 32 bytes")
        m0 = g0
    var g1 = mimir_realloc(m1, 128)
    run += 1
    failed += verdict(Int(g1) != 0, "phase4: realloc grow m1 64 -> 128 non-null")
    if Int(g1) != 0:
        m1 = g1
    run += 1
    failed += expect("phase4a-realloc-grow-x2", 5, 3, 2, 0, 0, 0, 10, 2)
    var s4 = mimir_realloc(m4, 128)
    run += 1
    failed += verdict(Int(s4) == Int(m4), "phase4: shrink m4 512 -> 128 retains pointer")
    var s3 = mimir_realloc(m3, 256)
    run += 1
    failed += verdict(Int(s3) == Int(m3), "phase4: same-size m3 256 -> 256 retains pointer")
    run += 1
    failed += expect("phase4b-realloc-shrink-same", 5, 3, 4, 0, 0, 0, 10, 2)

    # Phase 5: defined failures. The call counters move
    # (the call reached MimirAlloc); the publication
    # counters do not (nothing was published).
    var big = mimir_malloc(UInt64(18446744073709551615))
    run += 1
    failed += verdict(Int(big) == 0, "phase5: oversize mimir_malloc returns null")
    var ovf = mimir_calloc(UInt64(18446744073709551615), 2)
    run += 1
    failed += verdict(Int(ovf) == 0, "phase5: overflow mimir_calloc returns null")
    run += 1
    failed += expect("phase5-defined-failures", 6, 4, 4, 0, 0, 0, 10, 2)

    # Phase 6: free(NULL) counts as a call and retires
    # nothing; four live frees retire four span objects. The
    # null address is derived at runtime (see file header):
    # a literal 0 is rejected at compile time by this
    # toolchain. The derived value is printed and checked.
    var zero_addr = Int(a_free() - a_free())
    var np = Pointer[UInt8, MutUntrackedOrigin](unsafe_from_address=zero_addr)
    run += 1
    failed += verdict(Int(np) == 0, "phase6: runtime-derived null address is 0")
    mimir_free(np)
    run += 1
    failed += expect("phase6a-free-null", 6, 4, 4, 1, 0, 0, 10, 2)
    mimir_free(m2)
    mimir_free(m3)
    mimir_free(c0)
    mimir_free(c1)
    run += 1
    failed += expect("phase6b-free-x4", 6, 4, 4, 5, 0, 0, 10, 6)

    # Phase 7: free the remaining four live blocks; the
    # books balance exactly in both registries.
    mimir_free(m0)
    mimir_free(m1)
    mimir_free(m4)
    mimir_free(c2)
    run += 1
    failed += expect("phase7-free-rest", 6, 4, 4, 9, 0, 0, 10, 10)
    run += 1
    failed += verdict(
        a_published() == a_retired() and a_small_alloc() == a_small_free(), "phase7: published == retired and small_alloc == small_free (live 0)"
    )

    print("CHECKS:", run, "run,", failed, "failed")
    if failed == 0:
        print("F-005 AUDIT LEG RESULT: PASS")
    else:
        print("F-005 AUDIT LEG RESULT: FAIL")
        raise Error("F-005: audit leg checks failed")
