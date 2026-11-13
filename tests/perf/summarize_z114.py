#!/usr/bin/env python3
"""summarize_z114.py - Z-114 perf-per-watt summariser.

Reads the raw file produced by run_z114.sh: parallel-batch
blocks (=== leg workload proc=p reps=R procs=P wall_ns=N ===)
with P probe_z007 CSV lines and perf stat -a RAPL lines per
block, plus idle sleep samples. Method = the banked Z-007
idle-subtraction method (as summarize_z107.py energy mode):
  idle_W = median idle package W; net_J = gross_J - idle_W*wall
  net nJ/op per block from summed ops; headline = median.
Perf-per-watt = ops per net joule = 1e9 / net_nJ_per_op.
Grades (5% band, Z-004): PPW tuned70 vs untuned per workload
WIN > 1.05x, TIE 0.95-1.05x, LOSS < 0.95x. A reference median
net <= 0 is BELOW IDLE-SUBTRACTION RESOLUTION: no ratio is
claimed and gross medians are reported for direction (the
Z-007 honesty rule). Checksum proof per block: summed CSV
checksum must equal P * reps * the Z-003 per-pass checksum.
Envelope evidence: per-block gross package W = gross_J/wall_s
is printed per leg with median and max; the tuned70 leg is
ENVELOPE-HELD at median gross W <= 70.0.
"""
import re, statistics, sys
from collections import defaultdict
WORKLOADS=["small_churn","mixed_batch","large_batch","mixed_lifecycle"]
Z003_CHK={"small_churn":12756656,"mixed_batch":34727552,"large_batch":1106008,"mixed_lifecycle":40359936}
def grade_ppw(t,u):
    if u<=0 or t<=0: return None,"NO GRADE"
    f=t/u
    return f,("WIN" if f>1.05 else ("TIE" if f>=0.95 else "LOSS"))
def main():
    blocks,cur=[],None
    for line in open(sys.argv[1]):
        line=line.rstrip("\n")
        m=re.match(r"=== (\S+) (\S+) proc=(\d+) reps=(\d+) procs=(\d+) wall_ns=(\d+) ===",line)
        if m:
            cur={"leg":m.group(1),"workload":m.group(2),"proc":int(m.group(3)),"reps":int(m.group(4)),"procs":int(m.group(5)),"wall_ns":int(m.group(6)),"ops":0,"chk":0,"elapsed":0,"events":{}}
            blocks.append(cur); continue
        m=re.match(r"=== idle sleep sample=(\d+) reps=0 ===",line)
        if m:
            cur={"leg":"idle","workload":"sleep","proc":int(m.group(1)),"reps":0,"procs":0,"wall_ns":None,"ops":0,"chk":0,"elapsed":0,"events":{}}
            blocks.append(cur); continue
        m=re.match(r"idle_wall_ns=(\d+)",line)
        if m and cur is not None: cur["wall_ns"]=int(m.group(1)); continue
        if cur is None: continue
        p=line.split(",")
        if len(p)==7 and p[0] in ("glibc","mimiralloc"):
            cur["ops"]+=int(p[2]); cur["chk"]+=int(p[6]); cur["elapsed"]+=int(p[3]); continue
        if "|" in line:
            q=line.split("|")
            if len(q)>=3 and q[0]!="<not supported>":
                try: cur["events"][q[2]]=float(q[0])
                except ValueError: pass
    idle_w=[]
    for b in blocks:
        if b["leg"]=="idle":
            j=b["events"].get("power/energy-pkg/")
            if j is not None and b["wall_ns"]: idle_w.append(j/(b["wall_ns"]/1e9))
    if not idle_w:
        print("Z-114 RESULT: FAIL - no valid idle samples"); return 1
    idle_med=statistics.median(idle_w)
    print(f"IDLE baseline: median {idle_med:.3f} W (min {min(idle_w):.3f}, max {max(idle_w):.3f}, n={len(idle_w)})")
    rows=defaultdict(list); bad=0
    for b in blocks:
        if b["leg"]=="idle": continue
        gross=b["events"].get("power/energy-pkg/")
        if gross is None or b["ops"]==0:
            print(f"BAD BLOCK {(b['leg'],b['workload'])} proc={b['proc']}"); bad+=1; continue
        if b["chk"]!=Z003_CHK[b["workload"]]*b["reps"]*b["procs"]:
            print(f"CHECKSUM FAIL {(b['leg'],b['workload'])} proc={b['proc']} chk={b['chk']}"); bad+=1
        wall=b["wall_ns"]/1e9
        net=gross-idle_med*wall
        rows[(b["leg"],b["workload"])].append((net/b["ops"]*1e9, gross/b["ops"]*1e9, gross/wall, b["ops"]/wall))
    med={}; gmed={}; gw={}; thr={}
    print(f"{'leg':<13} {'workload':<16} {'med_net_nJ/op':>13} {'ops/netJ':>10} {'med_grossW':>10} {'max_grossW':>10} {'agg_ops/s':>12}")
    for key in sorted(rows):
        v=rows[key]; med[key]=statistics.median([r[0] for r in v])
        gmed[key]=statistics.median([r[1] for r in v])
        gw[key]=(statistics.median([r[2] for r in v]), max(r[2] for r in v))
        thr[key]=statistics.median([r[3] for r in v])
        ppw=1e9/med[key] if med[key]>0 else float('nan')
        print(f"{key[0]:<13} {key[1]:<16} {med[key]:>13.2f} {ppw:>10.1f} {gw[key][0]:>10.2f} {gw[key][1]:>10.2f} {thr[key]:>12.0f}")
    print()
    print("=== PERF-PER-WATT VERDICTS - mimir tuned70 vs mimir untuned (ops/net J, 5% band) ===")
    for w in WORKLOADS:
        t=med.get(("mimir_tuned70",w)); u=med.get(("mimir_untuned",w))
        if t is None or u is None:
            print(f"{w}: MISSING LEG"); bad+=1; continue
        if u<=0 or t<=0:
            print(f"{w}: net below resolution (untuned {u:.2f}, tuned70 {t:.2f} nJ/op) - NO RATIO CLAIMED (gross medians: untuned {gmed[('mimir_untuned',w)]:.2f} vs tuned70 {gmed[('mimir_tuned70',w)]:.2f} nJ/op)"); continue
        f,v=grade_ppw(1e9/t,1e9/u)
        print(f"{w}: untuned {1e9/u:.1f} ops/J ({u:.2f} nJ/op) | tuned70 {1e9/t:.1f} ops/J ({t:.2f} nJ/op) | PPW factor {f:.3f}x | {v} | thr untuned {thr[('mimir_untuned',w)]:.0f} -> tuned70 {thr[('mimir_tuned70',w)]:.0f} ops/s | grossW untuned med {gw[('mimir_untuned',w)][0]:.1f} -> tuned70 med {gw[('mimir_tuned70',w)][0]:.1f} max {gw[('mimir_tuned70',w)][1]:.1f}")
    print()
    print("=== CONTEXT - mimir net nJ/op vs glibc untuned (banked energy bands) ===")
    for w in WORKLOADS:
        g=med.get(("glibc",w))
        for leg in ("mimir_untuned","mimir_tuned70"):
            m=med.get((leg,w))
            if m is None or g is None: continue
            if g<=0 or m<=0:
                print(f"{w}: {leg} {m:.2f} vs glibc {g:.2f} nJ/op - below resolution, no ratio (gross {gmed[(leg,w)]:.2f} vs {gmed[('glibc',w)]:.2f})"); continue
            f=m/g; v="WIN" if f<0.95 else ("TIE" if f<=1.05 else "LOSS")
            print(f"{w}: {leg} {m:.2f} vs glibc {g:.2f} nJ/op | factor {f:.2f}x | {v}")
    tw=[gw[k][0] for k in gw if k[0]=="mimir_tuned70"]
    twx=[gw[k][1] for k in gw if k[0]=="mimir_tuned70"]
    if tw:
        print()
        print(f"ENVELOPE: tuned70 median gross package W per workload {['%.1f'%x for x in tw]}; overall max block {max(twx):.1f} W; 70 W envelope {'HELD at median on every workload' if all(x<=70.0 for x in tw) else 'NOT HELD at median on every workload - see table'}")
    if bad:
        print(f"Z-114 RESULT: FAIL - {bad} bad/missing blocks"); return 1
    print("Z-114 RESULT: PASS (checksums true, idle baseline valid)")
    return 0
if __name__=="__main__":
    sys.exit(main())
