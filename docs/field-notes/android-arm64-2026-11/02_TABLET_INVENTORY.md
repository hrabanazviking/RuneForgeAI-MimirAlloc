# 02 — Physical tablet inventory

## Identified device

The Android properties identified a Samsung Galaxy Tab S10 Lite Wi-Fi, model `SM-X400`, device `gts10litewifi`, SoC identifier `s5e8835`, Android 16/API 36, build `X400XXS8CZI2`, security patch 2026-09-05. The reported SoC identifier is associated with Exynos 1380. Samsung's [processor page](https://semiconductor.samsung.com/processor/mobile-processor/exynos-1380/) is useful background; the field measurements below come from the tablet.

`/proc/cpuinfo` exposed eight ARM64 cores: four Cortex-A55 (`0xd05`) and four Cortex-A78 (`0xd41`). The kernel's maximum-frequency files reported 2,002,000 kHz on CPU 0 and 2,400,000 kHz on CPU 4. These are reported maximums, not measured sustained clocks. The system page size was 4,096 bytes in one probe; other Android devices can differ.

## Memory snapshot

| Kernel field | Reading | Meaning |
|---|---:|---|
| MemTotal | 5,841,464 KiB | Kernel-visible total, about 5.57 GiB |
| MemAvailable | 1,525,480 KiB | Initial estimate of available memory, about 1.45 GiB |
| SwapTotal | 8,388,604 KiB | Swap capacity, separate from physical RAM |

These readings change while applications run. `MemAvailable` is an estimate, not a reservation or an Android per-app allocation limit. Swap use can affect responsiveness. A later guarded build observed a minimum 847,196 KiB available before it stopped; that reading includes other activity and is not an attribution of all change to the compiler.

## Accessible interfaces

The app could open `/dev/mali0` and `/dev/dri/renderD128`; opening `/dev/vertex10` and an NPU throughput node was denied. Access to a device node does not itself demonstrate useful computation. The system Vulkan library and Android neural-network library were loadable. The vendor OpenCL library existed, but Android's linker namespace rejected direct loading from the app. The system declared a Samsung `enn` neural-network service.

These findings are scoped to an untrusted-app Termux context and this OS build. A native Android application, a privileged service, a different device, or a different Android build can expose a different interface. Do not weaken Android isolation or infer NPU support from filenames.

## Missing signals

No battery state, measured thermal level, thermal headroom, ActivityManager low-memory threshold, application foreground classification, frame timing, power or repeatable idle baseline was obtained. These omissions matter for future performance or responsiveness claims. A valid run report should capture what is observable, mark denied values explicitly and avoid serial numbers and account identifiers.
