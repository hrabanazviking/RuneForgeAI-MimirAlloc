# L-003 - Aesir boundary wiring probe (Wave 12, Phase 12).
#
# Two parts, kept strictly separate in every claim:
#
# PART A (Aesir allocation idiom, stdlib-serviced): a
# miniature bump arena built exactly the way Aesir builds
# its MimirWell pool in /home/pi/c2/aesir_engine/core/
# mimir_well.mojo (MimirWell.__init__ and allocate,
# lines 1364-1386 of that file): one stdlib allocation
# via std.memory.alloc alloc(Layout[Scalar[f16]]),
# unsafe_leak, then bump carving by offset, with reset
# recycling. This part proves the Aesir arena idiom
# compiles and runs natively on this aarch64 Pi with the
# installed Mojo 1.1.0 toolchain. Its bytes are serviced
# by the Mojo stdlib allocator. MimirAlloc does NOT
# service Part A, and no claim below says otherwise.
# MimirWell is Aesir internal code; it is NOT MimirAlloc
# (INC-L001-04).
#
# PART B (explicit adoption, B1+B2, binding grade): this
# program calls mimir_malloc / mimir_calloc /
# mimir_realloc / mimir_free explicitly from Mojo source
# (B1) with build/libmimiralloc.so placed on its link by
# mojo build -Xlinker flags (B2), proven at binding grade
# in the F-005 shape: serving-descriptor validation and
# exact mapped-byte deltas observed by the test-only
# f005 probe observer, plus LD_DEBUG=bindings evidence
# in the recorded run. Scope: EXPLICIT ADOPTION ONLY -
# this generated program adopting MimirAlloc for its own
# calls. It is NOT evidence that Aesir itself allocates
# through MimirAlloc; Aesir source contains no mimir_*
# reference (grep count 0, recorded in the slice doc).
#
# Exit behaviour: one [ok]/[FAIL] line per check; raises
# after the summary on any failure (non-zero exit). On
# full success prints the PASS line and exits 0.

from std.ffi import external_call
from std.memory import Pointer, unsafe_memset_zero
from std.origin import MutUntrackedOrigin
from std.memory.alloc import alloc, Layout

comptime f16 = DType.float16


struct L003Well:
    var base_ptr: Pointer[Scalar[f16], MutUntrackedOrigin]
    var capacity: Int
    var offset: Int

    def __init__(out self, size_in_bytes: Int) raises:
        if size_in_bytes <= 0:
            raise Error("L003Well: pool size must be positive")
        self.capacity = size_in_bytes // 2
        var allocation = alloc(Layout[Scalar[f16]](count=self.capacity))
        self.base_ptr = allocation^.unsafe_leak()
        self.offset = 0
        unsafe_memset_zero(self.base_ptr, self.capacity)

    def allocate(
        mut self, elements: Int
    ) raises -> Pointer[Scalar[f16], MutUntrackedOrigin]:
        if elements <= 0:
            raise Error("L003Well: allocation count must be positive")
        if self.offset + elements > self.capacity:
            raise Error("L003Well: memory pool exhausted")
        var ptr = self.base_ptr.unsafe_offset(self.offset)
        self.offset += elements
        return ptr

    def allocated_bytes(self) -> Int:
        return self.offset * 2

    def capacity_bytes(self) -> Int:
        return self.capacity * 2

    def reset(mut self):
        self.offset = 0


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


def libc_usleep(usec: UInt32) -> Int32:
    return external_call["usleep", Int32, UInt32](usec)


def verdict(ok: Bool, label: String) -> Int:
    if ok:
        print("  [ok]", label)
        return 0
    print("  [FAIL]", label)
    return 1


def maplen(page: UInt64, size: UInt64) -> UInt64:
    return (page + size + page - 1) // page * page


def settle() -> UInt64:
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
    print("L-003 Aesir boundary wiring probe")
    var run = 0
    var failed = 0

    print("PART A: Aesir MimirWell idiom arena (stdlib-serviced)")
    var well = L003Well(8192)
    run += 1
    failed += verdict(well.capacity_bytes() == 8192, "A1 capacity_bytes == 8192")
    var a = well.allocate(128)
    var b = well.allocate(64)
    run += 1
    failed += verdict(Int(a) == Int(well.base_ptr), "A2 first carve at pool base")
    run += 1
    failed += verdict(Int(b) - Int(a) == 256, "A3 second carve at byte offset 256")
    for i in range(128):
        a[unsafe_offset=i] = Float16(42.0)
    for i in range(64):
        b[unsafe_offset=i] = Float16(7.0)
    var oka = True
    for i in range(128):
        if a[unsafe_offset=i] != Float16(42.0):
            oka = False
    var okb = True
    for i in range(64):
        if b[unsafe_offset=i] != Float16(7.0):
            okb = False
    run += 1
    failed += verdict(oka and okb, "A4 carved slices hold disjoint patterns")
    run += 1
    failed += verdict(well.allocated_bytes() == 384, "A5 allocated_bytes == 384")
    var raised = False
    try:
        _ = well.allocate(100000)
    except:
        raised = True
    run += 1
    failed += verdict(raised, "A6 over-capacity carve raises (pool exhausted)")
    well.reset()
    var c = well.allocate(16)
    run += 1
    failed += verdict(
        Int(c) == Int(well.base_ptr) and well.allocated_bytes() == 32,
        "A7 reset recycles arena to base",
    )
    well.base_ptr.unsafe_free()
    print("PART A done (arena serviced by Mojo stdlib allocator, not MimirAlloc)")

    print("PART B: explicit mimir_* adoption (B1+B2, binding grade)")
    var page = UInt64(f005_page_size())
    run += 1
    failed += verdict(page > 0, "B1 f005_page_size() > 0")
    print("PAGE:", page)
    for _ in range(3):
        var pw = mimir_malloc(64)
        if Int(pw) != 0:
            mimir_free(pw)
        _ = f005_mapped_bytes()
    var base = settle()
    print("MAPPED base=", base, "(quiescent baseline, INC-F005-01 discipline)")
    run += 1
    failed += verdict(base > 0, "B2 mapped base > 0")
    var big = mimir_malloc(8192)
    run += 1
    failed += verdict(Int(big) != 0, "B3 mimir_malloc(8192) non-null")
    run += 1
    failed += verdict(
        f005_descriptor_valid(big, 8192) == 1,
        "B4 large block serving descriptor validates (mapping form)",
    )
    var after_big = f005_mapped_bytes()
    print("MAPPED after_big=", after_big, " delta=", after_big - base)
    run += 1
    failed += verdict(
        after_big - base == maplen(page, 8192),
        "B5 mapped delta == page_round_up(page + 8192) exactly",
    )
    mimir_free(big)
    run += 1
    failed += verdict(
        f005_mapped_bytes() == base,
        "B6 mapped total returns to baseline after large free",
    )
    var p = mimir_malloc(64)
    run += 1
    failed += verdict(Int(p) != 0, "B7 mimir_malloc(64) non-null")
    run += 1
    failed += verdict(
        f005_descriptor_valid(p, 64) == 1,
        "B8 small block serving descriptor validates (span form)",
    )
    for i in range(64):
        p[unsafe_offset=i] = UInt8((i * 3 + 17) % 256)
    var okp = True
    for i in range(64):
        if p[unsafe_offset=i] != UInt8((i * 3 + 17) % 256):
            okp = False
    run += 1
    failed += verdict(okp, "B9 malloc block pattern write/readback")
    var cz = mimir_calloc(16, 4)
    run += 1
    failed += verdict(Int(cz) != 0, "B10 mimir_calloc(16, 4) non-null")
    var okz = True
    for i in range(64):
        if cz[unsafe_offset=i] != UInt8(0):
            okz = False
    run += 1
    failed += verdict(okz, "B11 calloc extent arrives zeroed")
    run += 1
    failed += verdict(
        f005_descriptor_valid(cz, 64) == 1,
        "B12 calloc block serving descriptor validates",
    )
    var g = mimir_realloc(p, 256)
    run += 1
    failed += verdict(Int(g) != 0, "B13 mimir_realloc(p, 256) non-null")
    if Int(g) != 0:
        var okg = True
        for i in range(64):
            if g[unsafe_offset=i] != UInt8((i * 3 + 17) % 256):
                okg = False
        run += 1
        failed += verdict(okg, "B14 realloc grow preserves first 64 bytes")
        p = g
    mimir_free(p)
    mimir_free(cz)
    run += 1
    failed += verdict(True, "B15 all mimir_free calls returned normally")
    print("MAPPED final=", f005_mapped_bytes(), " base=", base)
    print("CHECKS:", run, "run,", failed, "failed")
    if failed == 0:
        print("L-003 RESULT: PASS")
    else:
        print("L-003 RESULT: FAIL")
        raise Error("L-003: checks failed")
