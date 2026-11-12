#!/usr/bin/env python3
"""summarize_z056.py - Z-056 soak harness summariser.

Reads probe_z056 raw files (one per leg). Per leg it
checks COMPLETENESS of the sample series - the Z-056
gate quantity: the scheduled samples are t = 0,
interval, 2*interval, ..., seconds (seconds/interval + 1
SAMPLE lines), every one present exactly once, strictly
increasing, followed by exactly one DRAINED and one DONE
line. It then reports the drift series in compact form:
first / median / peak / final for RSS, net_factor
((Rss - Rss_t0) / live_bytes), hole bytes, and the
latency batches (median + worst sample), plus DRAINED
retention vs the t=0 baseline.

Usage: summarize_z056.py <raw> [<raw> ...]
Exit 0 iff every leg is COMPLETE.
"""
import statistics
import sys

FIELDS = ["leg", "t_s", "ops", "live_count", "live_bytes",
          "rss_kib", "pss_kib", "vma_count", "vma_delta",
          "avma_count", "avma_kib", "hole_count", "hole_bytes",
          "largest_hole", "unmapped_bytes", "gap_bytes",
          "lat_small_ns", "lat_large_ns", "cksum"]


def summarise(path):
    soak = None
    samples = []
    drained = None
    done = None
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("SOAK,"):
                _, leg, sec, iv = line.split(",")
                soak = (leg, int(sec), int(iv))
            elif line.startswith("SAMPLE,"):
                parts = line.split(",")
                row = dict(zip(FIELDS, parts[1:]))
                for k in FIELDS[1:]:
                    row[k] = int(row[k])
                samples.append(row)
            elif line.startswith("DRAINED,"):
                parts = line.split(",")
                drained = {"leg": parts[1],
                           "rss_kib": int(parts[2]),
                           "pss_kib": int(parts[3]),
                           "vma_count": int(parts[4]),
                           "vma_delta": int(parts[5])}
            elif line.startswith("DONE,"):
                parts = line.split(",")
                done = {"leg": parts[1], "ops": int(parts[2]),
                        "samples": int(parts[3]),
                        "peak_rss_kib": int(parts[4])}
    if soak is None:
        print(f"{path}: NO SOAK LINE - incomplete")
        return False
    leg, seconds, interval = soak
    expected = list(range(0, seconds + 1, interval))
    got = [r["t_s"] for r in samples]
    complete = (got == expected and drained is not None
                and done is not None
                and done["samples"] == len(expected))
    print(f"=== leg {leg} ({path}) ===")
    print(f"schedule: seconds={seconds} interval_s={interval} "
          f"expected_samples={len(expected)} got_samples={len(got)}")
    print("completeness: " + ("COMPLETE - every scheduled sample "
          "present exactly once, DRAINED + DONE present"
          if complete else
          f"INCOMPLETE - got t={got} drained={drained is not None} "
          f"done={done is not None}"))
    if not samples:
        return False
    base_rss = samples[0]["rss_kib"]
    for r in samples:
        r["net_rss_kib"] = r["rss_kib"] - base_rss
        r["net_factor"] = (r["net_rss_kib"] * 1024 / r["live_bytes"]
                           if r["live_bytes"] > 0 else 0.0)
    med = lambda k: statistics.median(r[k] for r in samples)
    print(f"ops_total={done['ops'] if done else '?' } "
          f"peak_rss_kib={done['peak_rss_kib'] if done else '?'}")
    print(f"rss_kib: first={samples[0]['rss_kib']} "
          f"median={int(med('rss_kib'))} "
          f"peak={max(r['rss_kib'] for r in samples)} "
          f"final={samples[-1]['rss_kib']} (t0 baseline "
          f"{base_rss})")
    live = [r for r in samples if r["live_bytes"] > 0]
    if live:
        print(f"live_bytes: median={int(statistics.median(r['live_bytes'] for r in live))} "
              f"peak={max(r['live_bytes'] for r in live)}")
        print(f"net_factor: median={statistics.median(r['net_factor'] for r in live):.3f} "
              f"worst={max(r['net_factor'] for r in live):.3f} "
              f"final={live[-1]['net_factor']:.3f}")
        print(f"holes: median_count={int(statistics.median(r['hole_count'] for r in live))} "
              f"median_bytes={int(statistics.median(r['hole_bytes'] for r in live))} "
              f"worst_largest={max(r['largest_hole'] for r in live)}")
        print(f"latency ns/op: small median={int(statistics.median(r['lat_small_ns'] for r in live))} "
              f"worst={max(r['lat_small_ns'] for r in live)}; "
              f"large median={int(statistics.median(r['lat_large_ns'] for r in live))} "
              f"worst={max(r['lat_large_ns'] for r in live)}")
    if drained is not None:
        print(f"drained: rss_kib={drained['rss_kib']} "
              f"retained_net_kib={drained['rss_kib'] - base_rss} "
              f"vma_delta={drained['vma_delta']}")
    print("series (every sample):")
    print("  t_s      ops  live      live_B    rss_KiB   net_f "
          " holes  hole_B   lat_s  lat_l")
    for r in samples:
        print(f"  {r['t_s']:>4d} {r['ops']:>8d} {r['live_count']:>5d} "
              f"{r['live_bytes']:>10d} {r['rss_kib']:>9d} "
              f"{r['net_factor']:6.3f} {r['hole_count']:>5d} "
              f"{r['hole_bytes']:>9d} {r['lat_small_ns']:>6d} "
              f"{r['lat_large_ns']:>6d}")
    return complete


def main():
    ok = True
    for path in sys.argv[1:]:
        if not summarise(path):
            ok = False
    print("\nSOAK COMPLETENESS: " + ("ALL LEGS COMPLETE"
          if ok else "INCOMPLETE - see above"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
