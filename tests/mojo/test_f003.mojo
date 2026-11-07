# F-003 - boundary wiring proof (Phase 6, boundaries B2+B1).
#
# A small, original, clean-room Mojo program that calls the
# MimirAlloc C ABI EXPLICITLY from Mojo source, for allocations
# it makes itself:
#
#   mimir_malloc   - allocate block 1, write a known pattern,
#                    read it back byte for byte
#   mimir_calloc   - allocate block 2, verify every byte is zero
#   mimir_realloc  - grow block 1, verify the original bytes are
#                    preserved across the grow
#   mimir_free     - release both blocks
#
# Foreign-function mechanism: Mojo's documented FFI surface as
# confirmed from on-Pi Mojo 1.1.0 evidence (working programs
# under /home/pi/c2/spikes/phase_a/ on this Pi, e.g.
# gguf_parse_spike.mojo and neon_fp16_matmul.mojo, both built
# and run with this exact toolchain):
#
#   from std.ffi import external_call
#   external_call["name", ReturnType, ArgType...](args...)
#
# with Pointer[UInt8, MutUntrackedOrigin] (std.memory /
# std.origin) as the void* representation, UInt64 as size_t,
# and NoneType as the void return (the form /home/pi/c2 uses
# for external_call["_exit", NoneType]).
#
# Null checks: this toolchain's Pointer is non-null by design
# (Bool(ptr) is a compile error - see the slice document), so
# a possibly-null C return is tested by address: Int(ptr) == 0,
# the ptr_addr idiom proven in /home/pi/c2's posix_baseline.mojo
# (which also proves Pointer(unsafe_from_address=Int) for the
# reverse direction).
#
# Scope discipline (F-001 Section 6): this proves only that a
# generated Mojo program's OWN EXPLICIT allocations can be
# serviced by the four namespaced symbols when the library is
# placed on its link line (B2) and called explicitly (B1). It
# says nothing about the Mojo compiler's own allocations, the
# stdlib's internal allocations, or the tablet failure.
#
# Exit behaviour: prints one PASS/FAIL line per step; on any
# failure it raises after cleanup, so the process exits
# non-zero (Mojo's unhandled-error path). On full success it
# prints the summary PASS line and returns normally (exit 0).

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


def pattern_byte(i: Int) -> UInt8:
    return UInt8((i * 3 + 17) % 256)


def main() raises:
    var failures = 0

    # Step 1: mimir_malloc(64) + known pattern write/readback.
    var p1 = mimir_malloc(64)
    if Int(p1) == 0:
        print("STEP1 mimir_malloc(64): FAIL (null return)")
        raise Error("F-003: mimir_malloc returned null")
    for i in range(64):
        p1[unsafe_offset=i] = pattern_byte(i)
    var ok1 = True
    for i in range(64):
        if p1[unsafe_offset=i] != pattern_byte(i):
            ok1 = False
    if ok1:
        print("STEP1 mimir_malloc(64) + pattern write/readback: PASS")
    else:
        print("STEP1 mimir_malloc(64) + pattern write/readback: FAIL")
        failures += 1

    # Step 2: mimir_calloc(16, 4) = 64 bytes, must arrive zeroed.
    var p2 = mimir_calloc(16, 4)
    if Int(p2) == 0:
        print("STEP2 mimir_calloc(16, 4): FAIL (null return)")
        mimir_free(p1)
        raise Error("F-003: mimir_calloc returned null")
    var ok2 = True
    for i in range(64):
        if p2[unsafe_offset=i] != UInt8(0):
            ok2 = False
    if ok2:
        print("STEP2 mimir_calloc(16, 4) arrives zeroed (64 bytes): PASS")
    else:
        print("STEP2 mimir_calloc(16, 4) arrives zeroed (64 bytes): FAIL")
        failures += 1

    # Step 3: mimir_realloc(p1, 256); first 64 bytes preserved.
    # On failure the original block stays owned (header contract),
    # so track the live pointer either way and free it once.
    var live = mimir_realloc(p1, 256)
    if Int(live) == 0:
        print("STEP3 mimir_realloc(p1, 256): FAIL (null return)")
        failures += 1
        live = p1
    else:
        var ok3 = True
        for i in range(64):
            if live[unsafe_offset=i] != pattern_byte(i):
                ok3 = False
        # The grown tail must be writable across the new extent.
        for i in range(64, 256):
            live[unsafe_offset=i] = pattern_byte(i)
        for i in range(64, 256):
            if live[unsafe_offset=i] != pattern_byte(i):
                ok3 = False
        if ok3:
            print(
                "STEP3 mimir_realloc grow 64 -> 256, contents"
                " preserved + tail writable: PASS"
            )
        else:
            print(
                "STEP3 mimir_realloc grow 64 -> 256, contents"
                " preserved + tail writable: FAIL"
            )
            failures += 1

    # Step 4: mimir_free both blocks; defined return is normal
    # completion (mimir_free has no failure channel in the ABI).
    mimir_free(live)
    mimir_free(p2)
    print("STEP4 mimir_free(realloc block) + mimir_free(calloc block): PASS")

    if failures == 0:
        print("F-003 RESULT: PASS (4/4 steps)")
    else:
        print("F-003 RESULT: FAIL (", failures, "step(s) failed )")
        raise Error("F-003: one or more steps failed")
