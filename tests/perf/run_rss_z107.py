#!/usr/bin/env python3
"""run_rss_z107.py - Z-107 RSS/PSS runner (Z-006 protocol, 3 legs).

Adapted from run_rss_z006.py for the closing head-to-head:
spawns the Z-006 probe (tests/perf/probe_z006.c, which wraps
the UNMODIFIED banked bench_k002.c workloads, one workload
per process) for THREE legs - glibc (defaults), glibc_tuned
(the per-workload Z-098 winning GLIBC_TUNABLES, invocation 1
of record), mimiralloc - pinned with taskset to CPU 2
(Z-004 protocol), and at each probe PHASE marker reads
/proc/<pid>/smaps_rollup three times (20 ms apart, median
per field) before releasing the probe with a line on stdin.

Legs alternate process by process (glibc, glibc_tuned,
mimiralloc) per workload, house pattern, so machine drift
lands on all legs. Default 5 measured processes per leg
per workload.

Probe binaries are built by run_z107.sh and passed in via
Z107_RSS_GBIN / Z107_RSS_MBIN (counting-house separation:
this driver never builds).

Outputs (prefix from argv[1]):
  <prefix>.raw.csv  one row per sample set:
    leg,workload,proc,phase,Rss,Pss,Pss_Anon,Pss_File,
    Private_Clean,Private_Dirty,Shared_Clean,Shared_Dirty,
    Swap  (all kB, medians of 3 reads)
    plus LIVE rows: leg,workload,proc,LIVEBYTES,<bytes>,...
  <prefix>.header   run header: date, legs, tunables, cpus,
    procs, load at start/end, host_truth.sh verbatim.

Usage: run_rss_z107.py <out-prefix> [procs]
"""
import os
import statistics
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GBIN = os.environ.get("Z107_RSS_GBIN",
                      os.path.join(ROOT, "build", "probe_z107_z006_glibc"))
MBIN = os.environ.get("Z107_RSS_MBIN",
                      os.path.join(ROOT, "build", "probe_z107_z006_mimir"))
CPUS = "2"
FIELDS = ["Rss", "Pss", "Pss_Anon", "Pss_File", "Private_Clean",
          "Private_Dirty", "Shared_Clean", "Shared_Dirty", "Swap"]
REPS_FOR = {"small_churn": 20, "mixed_batch": 5,
            "large_batch": 20, "mixed_lifecycle": 10}
WORKLOADS = ["small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"]
# Per-workload tuned-glibc column: Z-098 invocation 1 winners.
TUNED_FOR = {
    "small_churn": "glibc.malloc.mmap_threshold=32768",
    "mixed_batch": "glibc.malloc.tcache_max=65536",
    "large_batch": "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536",
    "mixed_lifecycle":
        "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536",
}


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
    for b in (GBIN, MBIN):
        if not os.access(b, os.X_OK):
            print(f"FAIL probe binary missing: {b}", file=sys.stderr)
            sys.exit(1)
    with open(prefix + ".header", "w") as h:
        h.write("run_rss_z107_version: 1\n")
        h.write(f"date_local: {time.strftime('%Y-%m-%dT%H:%M:%S%z')}\n")
        h.write(f"leg_glibc: {GBIN}\nleg_mimir: {MBIN}\n")
        h.write("leg_glibc_tuned: glibc binary under per-workload "
                "GLIBC_TUNABLES (Z-098 winners)\n")
        for w in WORKLOADS:
            h.write(f"tuned {w}: {TUNED_FOR[w]}\n")
        h.write(f"cpus: {CPUS}\nprocs_per_leg: {procs}\n")
        h.write(f"loadavg_start: {loadavg()}\n")
        ht = os.path.join(ROOT, "scripts", "z13", "host_truth.sh")
        if os.access(ht, os.X_OK):
            h.write("--- host_truth (scripts/z13/host_truth.sh, verbatim) ---\n")
            h.write(subprocess.run([ht], capture_output=True,
                                   text=True).stdout)
            h.write("--- end host_truth ---\n")
        else:
            h.write("host_truth: NOT FOUND at scripts/z13/host_truth.sh\n")
    base_env = dict(os.environ)
    base_env.pop("GLIBC_TUNABLES", None)
    rows = []
    for w in WORKLOADS:
        reps = REPS_FOR[w]
        for p in range(1, procs + 1):
            legs = [("glibc", GBIN, None),
                    ("glibc_tuned", GBIN, TUNED_FOR[w]),
                    ("mimiralloc", MBIN, None)]
            for leg, binpath, tun in legs:
                env = dict(base_env)
                if tun:
                    env["GLIBC_TUNABLES"] = tun
                proc = subprocess.Popen(
                    ["taskset", "-c", CPUS, binpath, w, str(reps)],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    text=True, cwd=ROOT, env=env)
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
