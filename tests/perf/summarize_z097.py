#!/usr/bin/env python3
"""summarize_z097.py - Z-097 per-process tax summariser.

Reads raw storm CSV produced by probe_z097.c via
run_z097.sh:
  world,mode,count,elapsed_ns,ns_per_child,user_ns,
  sys_ns,minflt_delta,ok_children
world is plain (glibc) or preload (the Z-082 interposing
build under LD_PRELOAD); mode is a child_z097.c payload
shape (none / first / multi / churn).

Prints the Z-097 gate table - the per-process tax table
vs glibc - from medians over the storm repetitions:
  per mode: glibc median ns/process, MimirAlloc median
  ns/process, the tax (delta ns and %), the factor, and
  the grade. Grades follow the campaign rule with the
  Z-004 5% noise bound as the tie band: TIE inside
  +/-5%, WIN when the preload leg is more than 5%
  faster, LOSS when it is more than 5% slower.

Then the decomposition the roadmap line asks for:
  .so load tax     = none-leg delta (preload - plain):
                     what every short-lived process pays
                     even if it never allocates.
  first-alloc tax  = the first-mode increment over the
                     none leg, preload minus plain: the
                     extra price of the first allocation
                     itself, load cost removed.
  multi / churn increments likewise (allocator work
                     beyond load + first touch).
And the storm projection: tax per 1,000 and per 10,000
short-lived processes, per mode.

Hard FAIL (exit 1) on: any storm row whose children did
not all exit 0, any missing (world, mode) cell, or a
per-mode child count that differs between worlds (the
two worlds must have run identical storms).

Usage: summarize_z097.py <raw>
"""
import statistics
import sys
from collections import defaultdict

MODES = ["none", "first", "multi", "churn"]
WORLDS = ["plain", "preload"]


def med(v):
    return statistics.median(v)


def grade(delta_pct):
    if delta_pct > 5.0:
        return "LOSS"
    if delta_pct < -5.0:
        return "WIN"
    return "TIE"


def main():
    if len(sys.argv) != 2:
        print("usage: summarize_z097.py <raw>")
        return 1
    per = defaultdict(list)        # (world, mode) -> [ns/child]
    counts = {}                    # (world, mode) -> count
    user = defaultdict(list)
    sysv = defaultdict(list)
    minflt = defaultdict(list)
    bad = 0
    for ln in open(sys.argv[1]):
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        p = ln.split(",")
        if len(p) != 9:
            continue
        world, mode, count = p[0], p[1], int(p[2])
        el, npc = int(p[3]), int(p[4])
        usr, sy, mf, ok = int(p[5]), int(p[6]), int(p[7]), int(p[8])
        if ok != count:
            print(f"FAIL: storm {world},{mode} had "
                  f"{count - ok} failed children")
            bad = 1
        key = (world, mode)
        if key in counts and counts[key] != count:
            print(f"FAIL: storm count changed for {key}: "
                  f"{counts[key]} vs {count}")
            bad = 1
        counts[key] = count
        per[key].append(float(npc))
        user[key].append(usr / count)
        sysv[key].append(sy / count)
        minflt[key].append(mf / count)
    if bad:
        return 1
    for mode in MODES:
        for world in WORLDS:
            if (world, mode) not in per:
                print(f"FAIL: missing cell {world},{mode}")
                return 1
        if counts[("plain", mode)] != counts[("preload", mode)]:
            print(f"FAIL: child count differs across worlds "
                  f"for mode {mode}")
            return 1

    print("=== PER-PROCESS TAX TABLE vs glibc "
          "(median ns per process over storm reps) ===")
    print(f"{'mode':7} {'n/proc':>6} {'reps':>4} "
          f"{'glibc':>10} {'mimir':>10} {'tax_ns':>10} "
          f"{'tax_%':>8} {'factor':>7}  grade")
    medians = {}
    for mode in MODES:
        g = med(per[("plain", mode)])
        m = med(per[("preload", mode)])
        medians[mode] = (g, m)
        d = m - g
        dp = 100.0 * d / g
        print(f"{mode:7} {counts[('plain', mode)]:>6} "
              f"{len(per[('plain', mode)]):>4} "
              f"{g:>10.0f} {m:>10.0f} {d:>+10.0f} "
              f"{dp:>+7.2f}% {m / g:>6.2f}x  {grade(dp)}")
    print("--- per-process CPU split and faults "
          "(medians per process) ---")
    for mode in MODES:
        for world in WORLDS:
            print(f"{world:8} {mode:7} "
                  f"user_ns={med(user[(world, mode)]):9.0f} "
                  f"sys_ns={med(sysv[(world, mode)]):9.0f} "
                  f"minflt={med(minflt[(world, mode)]):7.1f}")
    print()
    print("=== DECOMPOSITION (median ns per process) ===")
    g0, m0 = medians["none"]
    print(f"so load tax (none delta): {m0 - g0:+.0f} ns "
          f"({100.0 * (m0 - g0) / g0:+.2f}% of the glibc "
          f"no-alloc process)")
    for mode in ["first", "multi", "churn"]:
        g, m = medians[mode]
        gi, mi = g - g0, m - m0
        print(f"{mode:7} increment over none: glibc "
              f"{gi:+.0f} ns, mimir {mi:+.0f} ns, "
              f"allocator-work tax {mi - gi:+.0f} ns")
    print()
    print("=== STORM PROJECTION (median tax) ===")
    for mode in MODES:
        g, m = medians[mode]
        d = m - g
        print(f"{mode:7} tax per 1000 procs "
              f"{d * 1000 / 1e6:+8.2f} ms, per 10000 procs "
              f"{d * 10000 / 1e6:+9.2f} ms")
    return 0


if __name__ == "__main__":
    sys.exit(main())
