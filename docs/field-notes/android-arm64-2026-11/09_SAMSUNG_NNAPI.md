# 09 — Samsung neural-network interface

## Observed public interface

The tablet had the Android neural-networks library, a manifest entry for an `enn` service and Samsung ENN-related libraries. The app could not open `/dev/vertex10` or an NPU throughput node. Presence and permissions are inventory observations, not proof of a specific chip executing a graph.

A standalone native C diagnostic used Android's NNAPI device APIs to enumerate two devices:

| Device | Reported type | Reported version | FP32 ADD |
|---|---:|---|---|
| `enn` | 4, accelerator | `E0.0.1` | Supported, compiled, returned expected output |
| `nnapi-reference` | 2, CPU | `X400XXS8CZI2` | Supported, compiled, returned expected output |

Each device was selected individually for compilation. The tiny graph added `[1, -2, 0.5, 10]` and `[2, 4, -0.25, -3]`; both returned `[3, 2, 0.25, 7]` with success. One repeat took 0.089 seconds; it was not a statistically useful benchmark.

## What selection establishes

Explicitly selecting `enn` establishes that the public NNAPI path accepted and executed the graph through the selected vendor device interface. It prevents NNAPI from choosing the separate reference device for that compilation. It does **not** establish which physical engine the vendor service used internally, whether it fell back internally, or whether a large transformer graph is supported. No physical NPU trace was obtained.

A service's reported device type, a compiler library, a driver manifest and a successful small graph are different evidence levels. Q4_K_M GGUF tensors are not automatically an NNAPI or LiteRT graph. Conversion and quantization can change model representation and outputs.

## Follow-up protocol

1. Identify a public, licensed runtime and precise vendor support for this Android build.
2. Query operation and quantization support on the explicit device; avoid hidden CPU fallback where the API allows.
3. Use known-answer tests across shapes, boundary conditions and numerical tolerances.
4. Obtain a supported trace or other independent evidence of physical NPU dispatch.
5. Measure compile/setup time, data movement, peak memory, sustained execution, energy and foreground impact.
6. Only then assess a Qwen subgraph, dynamic KV, conversions and end-to-end quality.

If physical dispatch or licensing cannot be established, record the path as blocked and keep other work independent. Android's [NNAPI documentation](https://developer.android.com/ndk/guides/neuralnetworks) is the primary API starting point; availability and recommendations can change across Android releases.
