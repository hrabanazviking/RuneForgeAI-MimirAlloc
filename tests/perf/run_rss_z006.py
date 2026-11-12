#!/usr/bin/env python3
"""run_rss_z006.py - Z-006 RSS/PSS runner.

Spawns the Z-006 probe (tests/perf/probe_z006.c, which wraps
the UNMODIFIED banked bench_k002.c workloads, one workload
per process) for both legs, pinned with taskset to CPU 2
(Z-004 protocol), and at each probe PHASE marker reads
/proc/<pid>/smaps_rollup three times (20 ms apart, median
per field) before releasing the probe with a line on stdin.

Legs alternate process by process (glibc, mimiralloc) per
workload, house pattern, so machine drift lands on both
legs. Default 5 measured processes per leg per workload.

Outputs (prefix from argv[1]):
  <prefix>.raw.csv  one row per sample set:
    leg,workload,proc,phase,Rss,Pss,Pss_Anon,Pss_File,
    Private_Clean,Private_Dirty,Shared_Clean,Shared_Dirty,
    Swap  (all kB, medians of 3 reads)
    plus LIVE rows: leg,workload,proc,LIVEBYTES,<bytes>,...
  <prefix>.header   run_measured-style header: date, legs,
    cpus, reps, load at start/end, host_truth.sh verbatim.

Usage: run_rss_z006.py <out-prefix> [procs]
"""
import os
import statistics
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GBIN = os.path.join(ROOT, "build", "probe_z006_glibc")
MBIN = os.path.join(ROOT, "build", "probe_z006_mimir")
CPUS = "2"
FIELDS = ["Rss", "Pss", "Pss_Anon", "Pss_File", "Private_Clean",
          "Private_Dirty", "Shared_Clean", "Shared_Dirty", "Swap"]
REPS_FOR = {"small_churn": 20, "mixed_batch": 5,
            "large_batch": 20, "mixed_lifecycle": 10}
WORKLOADS = ["small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"]


def build():
    if os.access(GBIN, os.X_OK) and os.access(MBIN, os.X_OK):
        return
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    if not os.path.exists(os.path.join(ROOT, "build", "mimir_alloc.o")):
        subprocess.run(["make", "bench-k002"], cwd=ROOT,
                       check=True, capture_output=True)
    cc = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
          "-pthread", "-Iinclude", "-Isrc", "-O2"]
    subprocess.run(cc + ["-DLEG_GLIBC", "tests/perf/probe_z006.c",
                         "-o", GBIN], cwd=ROOT, check=True)
    subprocess.run(cc + ["tests/perf/probe_z006.c",
                         "build/mimir_alloc.o", "build/mimir_core.o",
                         "build/mimir_platform.o", "-o", MBIN],
                   cwd=ROOT, check=True)


def read_rollup(pid):
    vals = {}
    with open(f"/proc/{pid}/smaps_rollup") as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2 and parts[0].rstrip(":") in FIELDS:
                vals[parts[0].rstrip(":")] = int(parts[1])
    return vals


def sample(pid):
    reads = []
    for _ in range(3):
        reads.append(read_rollup(pid))
        time.sleep(0.02)
    return {k: int(statistics.median([r.get(k, 0) for r in reads]))
            for k in FIELDS}


def loadavg():
    with open("/proc/loadavg") as f:
        return " ".join(f.read().split()[:3])


def main():
    prefix = sys.argv[1]
    procs = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    build()
    with open(prefix + ".header", "w") as h:
        h.write("run_rss_z006_version: 1\n")
        h.write(f"date_local: {time.strftime('%Y-%m-%dT%H:%M:%S%z')}\n")
        h.write(f"leg_a: {GBIN}\nleg_b: {MBIN}\n")
        h.write(f"cpus: {CPUS}\nprocs_per_leg: {procs}\n")
        h.write(f"loadavg_start: {loadavg()}\n")
        ht = os.path.join(ROOT, "..", "mimiralloc-z13-forge", "scripts", "z13", "host_truth.sh")
        if os.access(ht, os.X_OK):
            h.write("--- host_truth (scripts/z13/host_truth.sh, verbatim) ---\n")
            h.write(subprocess.run([ht], capture_output=True,
                                   text=True).stdout)
            h.write("--- end host_truth ---\n")
        else:
            h.write("host_truth: NOT FOUND at seat scripts/z13/host_truth.sh\n")
    rows = []
    for w in WORKLOADS:
        reps = REPS_FOR[w]
        for p in range(1, procs + 1):
            for leg, binpath in (("glibc", GBIN), ("mimiralloc", MBIN)):
                proc = subprocess.Popen(
                    ["taskset", "-c", CPUS, binpath, w, str(reps)],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    text=True, cwd=ROOT)
                live = None
                csv_line = None
                while True:
                    line = proc.stdout.readline()
                    if not line:
                        break
                    line = line.strip()
                    if line.startswith("PHASE "):
                        ph = line.split()[1]
                        s = sample(proc.pid)
                        rows.append([leg, w, p, ph] +
                                    [s[k] for k in FIELDS])
                        proc.stdin.write("go\n")
                        proc.stdin.flush()
                    elif line.startswith("LIVE "):
                        live = int(line.split()[1])
                        rows.append([leg, w, p, "LIVEBYTES", live] +
                                    [""] * (len(FIELDS) - 1))
                    elif "," in line:
                        csv_line = line
                rc = proc.wait()
                if rc != 0 or csv_line is None:
                    print(f"FAIL {leg} {w} proc={p} rc={rc}",
                          file=sys.stderr)
                    sys.exit(1)
                rows.append([leg, w, p, "CSV", csv_line] +
                            [""] * (len(FIELDS) - 1))
                print(f"ok {leg} {w} proc={p} live={live}",
                      file=sys.stderr)
    with open(prefix + ".raw.csv", "w") as f:
        f.write("leg,workload,proc,phase," + ",".join(FIELDS) + "\n")
        for r in rows:
            f.write(",".join(str(x) for x in r) + "\n")
    with open(prefix + ".header", "a") as h:
        h.write(f"loadavg_end: {loadavg()}\n")
    print(f"rows: {len(rows)}")


if __name__ == "__main__":
    main()
