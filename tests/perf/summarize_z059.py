#!/usr/bin/env python3
"""summarize_z059.py - Z-059 placement policy trial.

Reads raw_current_run{1,2,3}.txt, raw_first_run{1,2,3}.txt
and raw_best_run{1,2,3}.txt from the directory named by
argv[1] (produced by tests/perf/run_z059.sh), verifies:
  * every run RESULT: PASS with failures=0 and exit 0;
  * determinism: all METRIC lines identical across the
    three runs of each policy;
  * leg A control: f3/tails/qmapped and per-class
    bear/live identical across policies (Z-055 values
    are asserted inside the probe itself);
then prints the policy comparison table and applies the
pre-registered decision rule from probe_z059.c, printing
DECISION: CHOSEN-BESTFIT-FLAGGATED or DECISION:
NO-ADOPTION with the clause-by-clause record.
Exit 0 iff all verification passed (the DECISION itself
may be either outcome; both are a PASS of the trial).
"""
import re
import sys
from pathlib import Path

POLICIES = ["current", "first", "best"]
NAMES = {"current": "current", "first": "first-fit",
         "best": "best-fit"}


def parse(path):
    text = path.read_text()
    metrics = {}
    order = []
    for line in text.splitlines():
        if line.startswith("METRIC "):
            order.append(line)
            parts = line.split()
            kind = parts[1]
            kv = {}
            for tok in parts[2:]:
                if "=" in tok:
                    k, v = tok.split("=", 1)
                    kv[k] = v
            metrics.setdefault(kind, []).append(kv)
    m = re.search(r"checks=(\d+) failures=(\d+)", text)
    checks = int(m.group(1)) if m else -1
    fails = int(m.group(2)) if m else -1
    result = "PASS" if "\nRESULT: PASS" in text else "FAIL"
    exit0 = "# probe_exit=0" in text
    return metrics, order, checks, fails, result, exit0


def main():
    out = Path(sys.argv[1])
    ok = True
    data = {}
    for pol in POLICIES:
        runs = []
        for r in (1, 2, 3):
            p = out / f"raw_{pol}_run{r}.txt"
            if not p.exists():
                print(f"MISSING {p}")
                return 1
            runs.append(parse(p))
        base_metrics, base_order = runs[0][0], runs[0][1]
        for i, run in enumerate(runs, 1):
            metrics, order, checks, fails, result, exit0 = run
            if result != "PASS" or fails != 0 or not exit0:
                print(f"{pol} run{i}: NOT PASS "
                      f"(result={result} failures={fails} "
                      f"exit0={exit0})")
                ok = False
            if order != base_order:
                print(f"{pol} run{i}: METRIC lines differ "
                      f"from run1 (nondeterminism)")
                ok = False
        data[pol] = base_metrics
        print(f"{NAMES[pol]}: 3 runs, checks={runs[0][2]} "
              f"failures=0 each, METRIC lines identical "
              f"across runs")
    # leg A control identical across policies
    for kind in ("legA", "legA_class"):
        ref = data["current"][kind]
        for pol in ("first", "best"):
            if data[pol][kind] != ref:
                print(f"LEG A CONTROL FAIL: {kind} differs "
                      f"for {pol}")
                ok = False
    la = data["current"]["legA"][0]
    print(f"leg A (all policies, == Z-055): f3={la['f3']} "
          f"tails={la['tails']} rext={la['rext']} "
          f"rint={la['rint']} qmapped={la['qmapped']} "
          f"qretained={la['qretained']}")
    for kv in data["current"]["legA_class"]:
        print(f"  leg A class {kv['req']}: acq={kv['acq']} "
              f"rel={kv['rel']} ret={kv['ret']} "
              f"bear={kv['bear']} live={kv['live']}")
    # comparison table (leg B)
    print("\npolicy comparison (leg B, follow-on=24/class):")
    print(f"{'class':>6} {'policy':>10} {'groups':>6} "
          f"{'f3class':>8} {'touched':>7} {'newspans':>8} "
          f"{'empty_tot':>9}")
    legb = {}
    for pol in POLICIES:
        legb[pol] = {int(kv["req"]): kv
                     for kv in data[pol]["legB"]}
        for req in (64, 256):
            kv = legb[pol][req]
            print(f"{req:>6} {NAMES[pol]:>10} "
                  f"{kv['groups']:>6} {kv['f3class']:>8} "
                  f"{kv['touched']:>7} {kv['newspans']:>8} "
                  f"{kv['empty_total']:>9}")
    def summed(pol, key):
        return sum(int(legb[pol][r][key]) for r in (64, 256))
    def per_class_ok(key, better_low):
        for r in (64, 256):
            b = int(legb["best"][r][key])
            c = int(legb["current"][r][key])
            if better_low and b > c:
                return False
            if not better_low and b < c:
                return False
        return True
    clauses = {}
    clauses["a_legA_control"] = ok
    clauses["b_touched"] = (
        summed("best", "touched") < summed("current", "touched")
        and per_class_ok("touched", True))
    clauses["c_empty"] = (
        summed("best", "empty_total") >= summed("current", "empty_total")
        and per_class_ok("empty_total", False))
    clauses["d_newspans_zero"] = all(
        summed(p, "newspans") == 0 for p in POLICIES)
    clauses["e_not_firstfit"] = not (
        summed("first", "touched") == summed("best", "touched")
        and summed("first", "empty_total") == summed("best", "empty_total"))
    clauses["f_all_checks_pass"] = ok
    print("\ndecision rule (pre-registered in probe_z059.c):")
    for k, v in clauses.items():
        print(f"  {k}: {'MET' if v else 'NOT MET'}")
    print(f"  touched sums: current={summed('current','touched')} "
          f"first={summed('first','touched')} "
          f"best={summed('best','touched')}")
    print(f"  empty sums:   current={summed('current','empty_total')} "
          f"first={summed('first','empty_total')} "
          f"best={summed('best','empty_total')}")
    if all(clauses.values()):
        print("DECISION: CHOSEN-BESTFIT-FLAGGATED "
              "(production default unchanged; default "
              "adoption reserved to a later slice)")
    else:
        print("DECISION: NO-ADOPTION (current policy "
              "stays; trial flags remain available)")
    print("SUMMARY RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
