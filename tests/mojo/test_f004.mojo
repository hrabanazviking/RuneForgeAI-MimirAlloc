# F-004 - minimal Mojo program, WITHOUT-half baseline (Phase 6, T3).
#
# A small, original, clean-room Mojo program. It is NOT a bare
# hello-world: it builds a growable List[Int] buffer of known
# contents (192 entries, forcing growth past the initial
# capacity), mutates every entry in place, and verifies the
# result against independently computed expected checksums.
# The allocation it touches is the Mojo stdlib List's own
# growable storage, allocated by the Mojo runtime's default
# allocator - this WITHOUT leg uses no MimirAlloc symbol,
# no -Xlinker wiring, and no external_call at all.
#
# Expected values (computed independently of Mojo, in
# Python, from the same formulas):
#   initial v_i = (i*7+3) % 256          for i in 0..<192
#   mutated v_i = v_i*3 + 5
#   count = 192, sum = 72096, weighted_sum = 7130112
#     where weighted_sum = sum((i+1) * mutated_v_i)
#
# On full success it prints the F004 PASS line and returns
# normally (exit 0). On any mismatch it prints FAIL lines
# and raises, so the process exits non-zero.

def main() raises:
    var buf = List[Int]()
    for i in range(192):
        buf.append((i * 7 + 3) % 256)
    if len(buf) != 192:
        print("F004 count: FAIL len=", len(buf))
        raise Error("F-004: wrong count after build")
    print("F004 count: 192")

    for i in range(192):
        buf[i] = buf[i] * 3 + 5

    var total = 0
    var weighted = 0
    for i in range(192):
        total += buf[i]
        weighted += (i + 1) * buf[i]

    print("F004 checksum:", total)
    print("F004 weighted:", weighted)
    if total == 72096 and weighted == 7130112:
        print("F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)")
    else:
        print("F004 RESULT: FAIL")
        raise Error("F-004: checksum mismatch")
