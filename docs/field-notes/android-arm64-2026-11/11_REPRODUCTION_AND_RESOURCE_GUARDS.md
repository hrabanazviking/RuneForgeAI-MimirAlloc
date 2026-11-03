# 11 — Reproduction, monitoring and resource guards

## Baseline record

Record date/time, source revision and build identity; device model without serial number; OS build, ABI, libc, kernel, page size, compiler and linker versions; available storage; initial memory and swap; and whether the app is foreground. Record temperature, thermal state, battery, pressure and frame timing when available. Mark inaccessible signals unknown. Repeat measurements after the workload and retain failures.

The following Linux-style commands are illustrative and depend on the device's permissions and tools:

```sh
uname -m
cat /proc/meminfo | head -n 8
getconf PAGESIZE
```

On the tablet, executing the Android `getprop` wrapper was denied from this context, although a bionic property API was available. Android access checks and available commands vary. Never bypass platform access controls to fill a report.

## Guarded builds

A real watchdog should run the workload at reduced priority, use one job initially, sample the relevant memory and disk signals, write a raw log, and terminate the entire workload when it crosses explicit limits. It must also handle sampling errors and enforce a wall-clock limit. Confirm processes have exited and record the final state. A guard can reduce interference, but it cannot guarantee that the system remains responsive: sampling has latency, allocations can spike, and other apps change memory use.

The field build used 850 MiB `MemAvailable`, 2 GiB free storage and 180 seconds as experimental cutoffs. Those numbers are not portable defaults, Android's low-memory threshold or validated UX budgets. Do not weaken a stop condition merely to finish a build. Preserve caches for resumption only after the device recovers and the constraints are re-evaluated.

## Address-space probe

Use a small `PROT_NONE` anonymous mapping and a true no-replace flag supported by the kernel. Check both the return address and errno, immediately unmap successful requests, and never use a flag that can replace existing mappings. Sampling one address tests one address: an occupied range, unsupported flags or policy can change interpretation. A rejected high address alone does not reveal every memory-layout property.

## Privacy and provenance

Log software versions, source hashes, exact commands, exit status and relevant output. Remove usernames, private paths, credentials, tokens, serial numbers, chat contents and device identifiers not needed for reproduction. Publish hashes and public links only when they refer to actual artifacts. If a result came from a local unpushed branch, say so. Distinguish a model publisher's checksum from a locally verified digest.
