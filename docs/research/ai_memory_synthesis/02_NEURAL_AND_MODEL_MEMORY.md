# Neural, recurrent and model-state memory

## Learned parameters and inference state

A language model's parameters retain statistical structure acquired during training. They are not a file system whose contents can be enumerated, reliably edited or deleted one record at a time. Context tokens supply immediate information; they are not inherently durable after the runtime discards them. Attention KV and recurrent state are intermediate computation needed to continue a particular sequence. External retrieval provides a separate, updateable source of evidence.

These mechanisms can collaborate. For example, retrieve a versioned document, insert selected text into a prompt, then use KV state to avoid recomputing compatible prefixes. Deleting a retrieved document from a source store does not automatically remove every derived cache, session state, log or training influence; deletion behavior must be implemented per domain and documented.

## Memory-augmented neural systems

[Neural Turing Machines](https://arxiv.org/abs/1410.5401) combine a neural controller with a differentiable memory-like resource and learned addressing. They demonstrated small algorithmic tasks in their experimental setting. This is useful conceptual inspiration for learnable addressing, but a differentiable read/write matrix is not automatically a safe byte-addressed heap, a persistent database or an exact emulator.

Research question: can an optional controller learn a good *ranking* of managed objects or retrieval candidates from workload histories? The answer must be tested against simple deterministic policies; controller errors must not compromise object ownership or access rights.

## Long-context recurrence and selective state

[Transformer-XL](https://arxiv.org/abs/1901.02860) uses segment-level recurrence and positional handling to model dependencies across segments. [Mamba](https://arxiv.org/abs/2312.00752) studies input-dependent selective state-space mechanisms and a hardware-aware implementation. These are model-architecture choices, not generic wrappers that can be bolted onto any checkpoint. State shapes, update rules, precision and runtime version form part of compatibility.

A runtime that supports state snapshots should record the model hash, architecture, tokenizer/input format, state schema, position, dtype, backend version and all required state components. Restore only when the model-specific contract allows it. A mathematically equivalent kernel can still produce numeric differences; distinguish reproducible-within-tolerance from bit-exact where relevant.

## Long context is not guaranteed recall

[Lost in the Middle](https://arxiv.org/abs/2307.03172) found position-dependent degradation on its studied models and tasks. It is evidence that advertised context capacity and useful information access are different measurements; it is not proof every later model has identical behavior. Test each target with relevant facts at multiple positions, conflicting versions and answerable/unanswerable queries.

## Proposed MimirAlloc role

- Treat model state as an explicitly typed runtime-owned resource with a declared budget, location, completion and snapshot contract.
- Keep CPU heap management separate from state semantics. An allocator can reserve/release buffers but cannot decide which recurrent states are interchangeable.
- Expose optional accounting categories: checkpoint weights, attention KV, recurrent state, activation/workspace, retrieval context, trace and snapshots.
- Permit placement hints only through runtime-supported APIs; measure transfer and synchronization.
- Support checkpoint operations only with safe points, schema/version checks, integrity and confidentiality controls.

## Experiments

1. Compare the same supported model/session with cold state, valid restored state and full recomputation. Check logits/outputs or state according to the stated precision contract.
2. Sweep context length and relevant-fact position; report accuracy, latency and peak host/device memory. Include missing evidence and contradictory updates.
3. Record what happens on model revision, adapter, tokenizer, dtype or backend change. Incompatible states should be rejected or rebuilt rather than silently reused.
4. For a recurrent model, measure snapshot size and state reset/restore separately from attention-KV metrics. Do not extrapolate from one architecture to another.

## Sources

- [Neural Turing Machines](https://arxiv.org/abs/1410.5401)
- [Transformer-XL](https://arxiv.org/abs/1901.02860)
- [Mamba](https://arxiv.org/abs/2312.00752)
- [Lost in the Middle](https://arxiv.org/abs/2307.03172)
