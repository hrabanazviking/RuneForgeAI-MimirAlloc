#!/bin/sh
# batch_z114.sh - Z-114 parallel batch launcher (MimirAlloc Z13).
# Launches P concurrent probe_z007 processes, one per physical
# core (even CPUs 0,2,4,...), waits for all, then cats their CSV
# lines to stdout. Usage: batch_z114.sh P BIN WORKLOAD REPS OUTDIR
P=$1; BIN=$2; W=$3; R=$4; OUT=$5
mkdir -p $OUT
rm -f $OUT/part_*.txt
i=0
while [ $i -lt $P ]; do
  cpu=$(( i * 2 ))
  taskset -c $cpu $BIN $W $R > $OUT/part_$i.txt 2>/dev/null &
  i=$((i+1))
done
wait
cat $OUT/part_*.txt
