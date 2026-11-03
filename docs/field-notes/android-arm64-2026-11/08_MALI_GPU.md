# 08 — Mali GPU: inventory and real computation

## What ran

An off-screen OpenGL ES diagnostic created an EGL 1.4 display, a 1×1 pbuffer and a GLES 3 context. It reported renderer `Mali-G68` and OpenGL ES 3.2 driver `v1.r38p1-01eac0-mbs2v41_0.0b37450fbb12e582df79a7906d0ad856`. A compute shader processed 16 floats using `x[i] = 2*x[i] + 1`, inserted a buffer-update memory barrier, mapped the buffer and compared the result with the expected odd numbers 1 through 31. All matched and `glGetError` returned zero.

This is evidence of a tiny real GLES GPU dispatch and checked output on this device. It is not an A.E.S.I.R. backend or a model inference measurement. The diagnostic used a Termux Python process and system graphics libraries; it did not add Python to the inference runtime.

A separate inventory created a Vulkan instance and enumerated one Mali-G68 physical device reporting Vulkan 1.3.219 and driver integer 159387648. **No Vulkan command was submitted.** An installed glslang tool later compiled a tiny shader to SPIR-V; shader compilation is not dispatch.

## Memory and timings

One repeated GLES diagnostic took 0.263 seconds and reported 51,984 KiB child high-water RSS. The process high-water mark is not total GPU, service or driver allocation. System MemAvailable before and after was 1,747,520 and 1,735,512 KiB, but other activity makes that delta non-attributable. This short run is not a throughput, thermal, energy or frame-time benchmark.

The Mali device nodes could be opened from the app context. A visible vendor OpenCL library could not be directly loaded because of Android linker namespace rules. Neither node visibility nor library presence proves general GPU API availability. Record exact API, driver, workload and synchronization.

## Next experiments

- Submit a bounded Vulkan buffer copy and verify every output byte after an explicit fence.
- Add small, known-answer math and then representative quantized matrix-vector shapes with tail and alignment checks against a CPU oracle.
- Check buffer ownership, lifetime, coherent/noncoherent memory, synchronization, device loss and cancellation.
- Measure peak memory including staging and driver allocations, then repeat under a defined graphics workload.
- Compare CPU-only and offload under identical inputs, reporting first-token latency, sustained throughput, energy and UI frame times where available.

An accelerator should become a scheduler candidate only after its particular operation and shape are legal, correct, reproducible and beneficial under the selected objective. Unified physical memory does not automatically mean zero-copy.
