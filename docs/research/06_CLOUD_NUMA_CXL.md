# Cloud, NUMA, memory tiers and CXL pools

## Four meanings of “pooled memory”

1. A **process allocator pool** holds blocks inside one address space for reuse.
2. A **node-local pool** manages regions shared by cooperating processes with explicit synchronization and access control.
3. A **CXL-attached or other tiered pool** can expose capacity to host(s) through a platform-specific topology and OS interface.
4. A **networked service** exposes remote data through messages or remote-memory APIs. Its operations, failures and consistency are different from ordinary local loads.

Use precise language: pooled capacity does not by itself provide shared coherent memory accessible simultaneously by every host. Hardware, firmware, switch topology, device capabilities, OS and security policy determine the actual mode.

## NUMA and placement

NUMA means access cost can depend on which CPU and memory node are involved. First-touch policy may place physical pages near the thread that faults them in, subject to OS policies, cpusets and migration. A user-space allocator can expose placement hints or arenas, but the kernel and environment decide actual placement. Measure `numa_maps`/per-node counters where available, thread placement, page migration and access latency. Cloud virtual machines may obscure topology or change host placement; a guest's view is not necessarily physical truth.

Candidates include per-node arenas, allocate-near-consumer, interleave bandwidth-heavy read workloads and migrate long-lived *movable* buffers. Avoid silently moving objects addressed by raw pointers. Cross-node free must remain safe even if a process changes affinity or a node goes offline. Placement preferences need fallback rules for capacity exhaustion and access restrictions.

## CXL research boundary

CXL is a family of protocols and platform capabilities. Linux documentation discusses Type-3 devices and host exposure through system RAM or DAX under supported configurations. Whether a particular system permits capacity partitioning, multi-host sharing or dynamic operations depends on the implementation and configuration. CXL does not turn a general network into transparent coherent RAM, and its memory cost, availability and security must be measured.

Potential MimirAlloc experiment: use a platform-provisioned memory tier as an **explicit** backend for large, handle-based, cold buffers. First inspect the available OS interface and ownership policy; do not attempt firmware reconfiguration from an allocator. Compare local DRAM and tier latency/bandwidth, tail behavior, fault handling, power, price, idle retention and interference. Keep small allocator metadata local until evidence says otherwise. If memory is removed, require a controlled quiescence and fail-safe path appropriate to the platform.

## Cloud/container pressure

A VM or container can have an effective memory limit below host RAM. Monitor accessible cgroup v2 `memory.current`, `memory.high`, `memory.max`, `memory.events` and `memory.pressure`; resolve the process's actual cgroup and mount, which may be namespaced. Some files or permissions may not be available. Hard limits, memory reclaim and orchestrator eviction are distinct. A process allocator cannot override them.

## Placement policy prototype

Inputs: allocation type, size, expected lifetime, access pattern, mobility, locality preference, latency budget and security domain. Outputs: chosen supported backend, quota reservation, explicit failure or fallback. Preserve all properties designated required; a local-DRAM preference can fall back if caller permits, while an exportable-device-handle requirement cannot be silently dropped. Telemetry must report requested and actual placement. Trial migrations only for handle-based resources with all users quiesced, and measure migration cost versus expected remaining benefit.

## Experimental questions

- Do large, cold model weights benefit from a slower tier while hot KV or activations stay local?
- Does moving access-intensive buffers destroy latency or increase energy?
- At what working-set size does memory pooling reduce failures versus increased interference?
- How does a node or tier failure affect consistency and recovery?
- Does the cost/benefit hold after accounting for whole-system energy and capacity?

No cross-host shared-memory claim should be made without a demonstrated supported topology, isolation and consistency model.

## Sources

- [Linux CXL documentation](https://cxl.docs.kernel.org/)
- [Linux CXL device types](https://docs.kernel.org/driver-api/cxl/devices/device-types.html)
- [CXL 3.0 specification](https://computeexpresslink.org/wp-content/uploads/2024/02/CXL-3.0-Specification.pdf)
- [Linux NUMA memory policy](https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html)
- [Linux cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Linux PSI](https://docs.kernel.org/accounting/psi.html)
