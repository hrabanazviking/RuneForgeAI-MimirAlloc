# F-005 - allocator-use proof on the Mojo path, DYNAMIC leg
# (Phase 6; E-004 binding proof + E-005 dynamic leg, carried
# to the Mojo path; closes INC-F003-05 for this leg).
#
# The generated executable built from this source is linked
# against build/libmimiralloc.so (the F-003 B2 boundary) and
# against the test-only build/libf005probe.so (tests/mojo/
# f005_probe.c), which observes - from inside this process,
# holding no allocator state of its own - the effects that
# only MimirAlloc's implementation produces:
#
#   (1) descriptor validation (E-005 dynamic leg, unchanged
#       in substance): every block MimirAlloc publishes
#       carries its 96-byte descriptor (magic "MIMIRALL",
#       mapping_base == its own address, usable_offset ==
#       one page, requested_size == the requested size,
#       state == LIVE) in the reserve page before the user
#       pointer. Applied ONLY to pointers returned by this
#       program's own mimir_* calls; never to a foreign
#       pointer.
#   (2) mapped-bytes accounting (same source): each
#       successful mimir_* allocation adds exactly
#       page_round_up(page + size) bytes to the process's
#       total mapped extent; each mimir_free removes it;
#       small libc allocations add nothing. The free leg of
#       this accounting is the stronger free evidence
#       INC-F003-04 asks for on this route: mimir_free
#       returns void, but its effect - the mapped total
#       returning EXACTLY to the pre-allocation baseline -
#       is measured, not assumed.
#   (3) test-side call counting (INC-E001-07 shape (a)):
#       this program counts its own mimir_* call sites and
#       requires the descriptor-validated count to match
#       them exactly.
#
# The loader-binding half of this leg (LD_DEBUG=bindings
# tying each mimir_* reference of the generated executable
# to libmimiralloc.so at runtime, E-004's shape) is run
# against this same executable in the recorded run
# (tests/mojo/recorded_run_f005.txt).
#
# Quiescence discipline (INC-F005-01): the Mojo runtime
# lazily maps exactly 64 MiB (67108864 bytes) once, early
# in the process's life, at a nondeterministic moment -
# observed landing before the baseline measurement in
# some runs and during an allocation phase in others
# (2 failures in the first 13 runs of the unstabilised
# program, both with the exact +67108864 signature and
# with every MimirAlloc delta itself exact). E-005's C
# consumer had no such runtime. This program therefore
# warms up (prints, a pattern loop, mimir and libc
# alloc/free pairs, descriptor reads) and then samples
# the mapped total until it is stable across three
# consecutive 20 ms-spaced readings BEFORE the baseline
# is taken. The exactness claims below are made against
# that quiescent baseline; the runtime's own mapping is
# part of the baseline, never attributed to MimirAlloc.
#
# Scope discipline (F-001 Section 6): this proves only that
# THIS generated program's OWN EXPLICIT mimir_* calls are
# serviced by MimirAlloc. It says nothing about the Mojo
# compiler's own allocations, the stdlib's internal
# allocations, the Mojo runtime's allocations, or the
# tablet failure.
#
# Exit behaviour: one [ok]/[FAIL] line per check; on any
# failure it raises after the summary, so the process exits
# non-zero. On full success it prints the PASS line and
# returns normally (exit 0).

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


def libc_free(ptr: Pointer[UInt8, MutUntrackedOrigin]):
    external_call["free", NoneType, Pointer[UInt8, MutUntrackedOrigin]](ptr)


def f005_mapped_bytes() -> UInt64:
    return external_call["f005_mapped_bytes", UInt64]()


def f005_descriptor_valid(
    ptr: Pointer[UInt8, MutUntrackedOrigin], size: UInt64
) -> Int32:
    return external_call[
        "f005_descriptor_valid",
        Int32,
        Pointer[UInt8, MutUntrackedOrigin],
        UInt64,
    ](ptr, size)


def f005_page_size() -> Int64:
    return external_call["f005_page_size", Int64]()


def verdict(ok: Bool, label: String) -> Int:
    if ok:
        print("  [ok]", label)
        return 0
    print("  [FAIL]", label)
    return 1


def maplen(page: UInt64, size: UInt64) -> UInt64:
    return (page + size + page - 1) // page * page


def libc_usleep(usec: UInt32) -> Int32:
    return external_call["usleep", Int32, UInt32](usec)


def settle() -> UInt64:
    # Sample the mapped total until three consecutive
    # 20 ms-spaced readings agree (INC-F005-01 quiescence).
    var stable = 0
    var prev = f005_mapped_bytes()
    for _ in range(200):
        _ = libc_usleep(20000)
        var cur = f005_mapped_bytes()
        if cur == prev:
            stable += 1
            if stable >= 3:
                return cur
        else:
            stable = 0
            prev = cur
    return prev


def main() raises:
    print("F-005 dynamic leg (Mojo program linked against libmimiralloc.so only,")
    print("plus the test-only libf005probe.so observer)")
    var run = 0
    var failed = 0

    var page = UInt64(f005_page_size())
    run += 1
    failed += verdict(page > 0, "f005_page_size() > 0")
    print("PAGE:", page)

    # Warm-up (INC-F005-01): exercise every subsystem the
    # phases use - printing, a pattern loop, mimir and
    # libc alloc/free pairs, descriptor validation, mapped
    # sampling - so the runtime's one-shot lazy mapping
    # lands here, before the baseline, not during a phase.
    for w in range(3):
        var pw = mimir_malloc(64)
        if Int(pw) != 0:
            for i in range(64):
                pw[unsafe_offset=i] = UInt8((i * 3 + 17) % 256)
            _ = f005_descriptor_valid(pw, 64)
            mimir_free(pw)
        var qw = libc_malloc(64)
        if Int(qw) != 0:
            libc_free(qw)
        _ = f005_mapped_bytes()
        print("warmup pass", w, "done")
    var base = settle()
    print("MAPPED base=", base, "(quiescent baseline, INC-F005-01)")
    run += 1
    failed += verdict(base > 0, "mapped base > 0")

    # Test-side counters (shape (a)): call sites made by
    # this program, counted here in the consumer.
    var site_malloc: UInt64 = 0
    var site_calloc: UInt64 = 0
    var validated: UInt64 = 0

    # Four mimir_malloc(64) through the .so.
    var p0 = mimir_malloc(64)
    site_malloc += 1
    var p1 = mimir_malloc(64)
    site_malloc += 1
    var p2 = mimir_malloc(64)
    site_malloc += 1
    var p3 = mimir_malloc(64)
    site_malloc += 1
    run += 1
    failed += verdict(
        Int(p0) != 0 and Int(p1) != 0 and Int(p2) != 0 and Int(p3) != 0,
        "4x mimir_malloc(64) non-null",
    )
    if f005_descriptor_valid(p0, 64) == 1:
        validated += 1
    if f005_descriptor_valid(p1, 64) == 1:
        validated += 1
    if f005_descriptor_valid(p2, 64) == 1:
        validated += 1
    if f005_descriptor_valid(p3, 64) == 1:
        validated += 1
    run += 1
    failed += verdict(validated == site_malloc, "validated == site_malloc (4)")

    # p1 carries a known pattern, read back byte for byte.
    for i in range(64):
        p1[unsafe_offset=i] = UInt8((i * 3 + 17) % 256)
    var okp = True
    for i in range(64):
        if p1[unsafe_offset=i] != UInt8((i * 3 + 17) % 256):
            okp = False
    run += 1
    failed += verdict(okp, "mimir_malloc block pattern write/readback")

    var after_mimir = f005_mapped_bytes()
    print("MAPPED after_mimir_malloc_x4=", after_mimir, " delta=", after_mimir - base)
    run += 1
    failed += verdict(
        after_mimir - base == 4 * maplen(page, 64),
        "mapped delta == 4 * page_round_up(page + 64)",
    )

    # NEGATIVE: small libc allocations in the same process
    # add no mapped bytes; the mimir total must not move.
    # (Descriptor validation is never applied to these
    # foreign pointers - E-005 dynamic leg discipline.)
    var q0 = libc_malloc(64)
    var q1 = libc_malloc(64)
    var q2 = libc_malloc(64)
    var q3 = libc_malloc(64)
    run += 1
    failed += verdict(
        Int(q0) != 0 and Int(q1) != 0 and Int(q2) != 0 and Int(q3) != 0,
        "4x libc malloc(64) non-null",
    )
    var after_libc = f005_mapped_bytes()
    print("MAPPED after_libc_malloc_x4=", after_libc, " delta_vs_mimir=", after_libc - after_mimir)
    run += 1
    failed += verdict(after_libc == after_mimir, "libc allocations move no mapped bytes")
    libc_free(q0)
    libc_free(q1)
    libc_free(q2)
    libc_free(q3)
    run += 1
    failed += verdict(f005_mapped_bytes() == after_mimir, "mapped unchanged after libc frees")

    # Two mimir_calloc(16, 8) = 128 bytes through the .so:
    # descriptor records the 128-byte product; extent zero.
    var c0 = mimir_calloc(16, 8)
    site_calloc += 1
    var c1 = mimir_calloc(16, 8)
    site_calloc += 1
    run += 1
    failed += verdict(Int(c0) != 0 and Int(c1) != 0, "2x mimir_calloc(16, 8) non-null")
    if f005_descriptor_valid(c0, 128) == 1:
        validated += 1
    if f005_descriptor_valid(c1, 128) == 1:
        validated += 1
    var okz = True
    for i in range(128):
        if c0[unsafe_offset=i] != UInt8(0) or c1[unsafe_offset=i] != UInt8(0):
            okz = False
    run += 1
    failed += verdict(okz, "calloc extents arrive zeroed (128 bytes x2)")
    run += 1
    failed += verdict(
        validated == site_malloc + site_calloc,
        "validated == site_malloc + site_calloc (6)",
    )
    var after_calloc = f005_mapped_bytes()
    print("MAPPED after_mimir_calloc_x2=", after_calloc, " delta=", after_calloc - after_mimir)
    run += 1
    failed += verdict(
        after_calloc - after_mimir == 2 * maplen(page, 128),
        "mapped delta == 2 * page_round_up(page + 128)",
    )

    # realloc grow through the .so: the new pointer's
    # descriptor records the new size, contents preserved,
    # mapped total unchanged (one mapping published, the
    # old one retired, both the same length at these sizes).
    for i in range(64):
        p0[unsafe_offset=i] = UInt8(i + 3)
    var g = mimir_realloc(p0, 96)
    run += 1
    failed += verdict(Int(g) != 0, "mimir_realloc(p0, 96) non-null")
    if Int(g) != 0:
        run += 1
        failed += verdict(
            f005_descriptor_valid(g, 96) == 1,
            "grown block descriptor records 96 bytes",
        )
        var okg = True
        for i in range(64):
            if g[unsafe_offset=i] != UInt8(i + 3):
                okg = False
        run += 1
        failed += verdict(okg, "realloc grow preserves first 64 bytes")
        p0 = g
    run += 1
    failed += verdict(
        f005_mapped_bytes() == after_calloc,
        "mapped unchanged by realloc grow (publish + retire)",
    )

    # Free every MimirAlloc block: the mapped total returns
    # exactly to the baseline. This is the measured effect
    # of the void mimir_free calls (INC-F003-04).
    mimir_free(p0)
    mimir_free(p1)
    mimir_free(p2)
    mimir_free(p3)
    mimir_free(c0)
    mimir_free(c1)
    var final_bytes = f005_mapped_bytes()
    print("MAPPED final=", final_bytes, " base=", base)
    run += 1
    failed += verdict(final_bytes == base, "mapped returns exactly to baseline after all frees")

    print("CHECKS:", run, "run,", failed, "failed")
    if failed == 0:
        print("F-005 DYNAMIC LEG RESULT: PASS")
    else:
        print("F-005 DYNAMIC LEG RESULT: FAIL")
        raise Error("F-005: dynamic leg checks failed")
