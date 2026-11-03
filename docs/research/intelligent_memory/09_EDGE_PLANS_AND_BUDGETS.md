# Edge deployment, model overhead and device budgets

## Physical resources versus advertised capacities

A laptop with shared CPU/GPU memory can report host RAM and device-visible capacity backed by the same physical pages. A unified-memory pool must not add those figures together unless independent backing is established. Driver reservations, the OS, display, page cache, weights and competing applications also consume capacity. Separate physical backing, virtual address ranges, capacity available to an API and current budget.

For Volmarr's planned AMD Ryzen AI MAX+ 395 machine, treat the published system memory as an upper-level specification, not a guarantee that all bytes are simultaneously available to an allocator or to every GPU/NPU API. Verify the actual device, operating system, firmware, available drivers and framework operations. AMD NPU, integrated GPU and CPU support for any proposed model or memory sharing is not established by this document.

## Budget examples

For a host: `effective budget = measured available capacity - system reserve - active models - backend reservations - transient peak margin`. Use the budget appropriate to the process/container, not nominal host RAM. If the operating system reports shared pages in multiple counters, reconcile before adding them. Budget controller model, its inference runtime and KV separately from the main model.

For the advisor: model weights + context/KV + runtime code/data + scratch + retrieval/index + IPC/logging + transient initialization. Quantization metadata and copies vary. 135M parameters at ideal 4-bit storage are approximately 64.4 MiB for raw weights; actual runtime usage can be higher. 0.6B parameters at ideal 4-bit are about 286 MiB for raw weights alone. These figures are arithmetic examples, not performance measurements or claims about specific available quantized model files.

## Device profiles to investigate

| Profile | Primary research questions |
|---|---|
| Android tablet, ARM64 | bionic/Termux/proot differences, 4/16 KiB pages, OS process lifecycle, advisor overhead, native accelerator API availability |
| Raspberry Pi-class Linux | actual RAM and VA/page configuration, CPU inference latency, swap/storage wear, energy and thermal effects |
| Unified-memory AMD laptop | runtime support, actual physical accounting, device synchronization, model and workspace coexistence |
| Discrete GPU laptop | GPU reservation vs host RAM, PCIe transfer cost, thermal/power impact and device-runtime integration |
| Cloud/server | cgroups, NUMA/tier placement, security boundaries, fairness and instance cost |
| Model-contained virtual machine | guest arena vs host tensor budget, precise state, host bridge and tiny memory map |

Profiles are investigation targets, not tested support. Device availability and software compatibility must be verified on the real system.

## Controller scheduling

A practical first design runs advisory inference at a low, configurable frequency or at phase boundaries, never on each allocation. A short feature window might summarize recent memory use, reclaim pressure, allocation distribution and workload phase without raw content. Set explicit limits on advisor RAM, CPU/GPU, token length, work queue and duty cycle. Pause under critical pressure and preserve deterministic fallback. Test whether the telemetry capture itself outweighs the benefit.

## Memory budget policy

1. Application declares hard minimum, preferred capacity and optional degradation choices.
2. Deterministic admission control verifies available quotas and current committed reservations.
3. Optional advisor suggests bounded cache or placement settings using current epoch data.
4. Guard validates action; applying changes never invalidates live objects.
5. If pressure rises, release eligible caches, pause advisor, reduce new admissions or notify the application. Context shortening, model unload or reduced quality requires an explicit application-level contract.
6. Report requested and actual capacity, active views, transfer costs and uncertainty.

## Thermal and energy measurement

Measure full job energy where a trustworthy sensor or external power meter exists; record charging state, screen, background apps, ambient temperature, runtime and sampling limitations. Compare baseline and advisor across repeated alternating trials. Report duration, operations completed, watt-hours/joules if measured, peak temperatures and idle wakeups. Do not infer energy from a smaller parameter count alone.

## Sources

- [Android 16 KB page guidance](https://developer.android.com/guide/practices/page-sizes)
- [Linux PSI](https://docs.kernel.org/accounting/psi.html)
- [SmolLM2 model](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct)
- [Qwen3 0.6B](https://huggingface.co/Qwen/Qwen3-0.6B)
- [Vulkan embedded memory guidance](https://docs.vulkan.org/guide/latest/embedded_programming.html)
