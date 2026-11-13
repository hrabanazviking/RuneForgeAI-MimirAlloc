#!/usr/bin/env python3
"""summarize_z102.py - Z-102 memory-pressure chaos summariser.

Reads the recorded run produced by run_z102.sh and prints,
per ceiling, every leg's verdict or storm classification,
the exit code behind it, and the journal OOM evidence.
Exit 0 iff the run's own verdict is PASS (runner_failures 0)
and every expected leg line is present -- the summariser
never regrades a leg the runner failed.
"""
import re
import sys

EXPECTED = {
    "4G": ["dense_mimir", "dense_glibc", "poll_mimir", "poll_glibc",
           "small_mimir", "small_glibc", "budget_mimir",
           "heritage_mimir", "heritage_glibc"],
    "16G": ["dense_mimir", "dense_glibc", "poll_mimir", "poll_glibc",
            "budget_mimir", "heritage_mimir", "heritage_glibc"],
}


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_z102.py <recorded-run>", file=sys.stderr)
        return 2
    text = open(sys.argv[1], encoding="utf-8").read()
    ok = True
    ceilings = re.split(r"^===== CEILING ", text, flags=re.M)[1:]
    seen = {}
    for sec in ceilings:
        ceil = sec.split(" ", 1)[0].strip()
        seen[ceil] = sec
        print(f"CEILING {ceil}")
        for tag in EXPECTED.get(ceil, []):
            ex = re.search(rf"^leg_{tag}_exit: (\d+)", sec, flags=re.M)
            cl = re.search(rf"^(?:classification|verdict)_{tag}: (.+)$",
                           sec, flags=re.M)
            label = cl.group(1) if cl else "MISSING"
            print(f"  {tag}: exit={ex.group(1) if ex else 'MISSING'} "
                  f"{label}")
            if not ex or not cl or "FAIL" in label:
                ok = False
    for ceil in EXPECTED:
        if ceil not in seen:
            print(f"CEILING {ceil} MISSING")
            ok = False
    oom = re.findall(r"oom-kill", text)
    print(f"journal oom-kill lines in recorded run: {len(oom)}")
    wit = re.search(r"witness_pid_end: plasmashell=(\d+) \(start was (\d+)\)",
                    text)
    if wit:
        print(f"witness plasmashell pid start={wit.group(2)} "
              f"end={wit.group(1)} "
              f"{'SAME' if wit.group(1) == wit.group(2) else 'CHANGED'}")
        if wit.group(1) != wit.group(2):
            ok = False
    else:
        print("witness line MISSING")
        ok = False
    m = re.search(r"runner_failures: (\d+)", text)
    res = re.search(r"Z-102 RESULT: (\w+)", text)
    print(f"runner_failures={m.group(1) if m else 'MISSING'} "
          f"result={res.group(1) if res else 'MISSING'}")
    if not m or m.group(1) != "0" or not res or res.group(1) != "PASS":
        ok = False
    print("SUMMARY RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
