# 07 — Memory budgeting for inference

The long-term A.E.S.I.R. goal was Qwen3-1.7B Q4_K_M CPU generation while sharing the tablet responsibly. No model was downloaded, imported, loaded or run in this investigation. MimirAlloc does not implement inference or a resource governor.

## Candidate identity

The Hugging Face API reported the following for [`bartowski/Qwen_Qwen3-1.7B-GGUF`](https://huggingface.co/bartowski/Qwen_Qwen3-1.7B-GGUF/tree/dcb19155b962dbb6389f4691a982043a8e651022):

- Revision: `dcb19155b962dbb6389f4691a982043a8e651022`
- File: `Qwen_Qwen3-1.7B-Q4_K_M.gguf`
- Reported size: 1,282,439,584 bytes
- Published LFS SHA-256: `72c5c3cb38fa32d5256e2fe30d03e7a64c6c79e668ad84057e3bd66e250b24fb`

This is remote metadata, not a digest of a file verified on the tablet. Confirm licensing and distribution conditions, preserve the exact revision and independently hash the bytes actually loaded. The [Qwen model card](https://huggingface.co/Qwen/Qwen3-1.7B) describes the model family; the GGUF file's own metadata and tensors must determine a particular runtime's geometry.

## Why the file size is not the working set

Initial `MemAvailable` was 1,525,480 KiB. The model file is approximately 1,252,382 KiB, leaving an arithmetic difference of approximately 273,098 KiB. That difference is not a safe reserve. A real session also needs resident model pages, KV cache, tokenizer, runtime, scratch buffers, process overhead, OS headroom and concurrent applications. File-backed pages can be reclaimed and may be shared; do not count shared memory twice, but do count real copies and peaks.

An illustrative roadmap calculation, assuming 28 layers, 8 KV heads, 128 dimensions and F16 keys and values, is:

```text
2 × 28 × 8 × 128 × 2 bytes = 114,688 bytes per token
1,024 tokens × 114,688 bytes = 112 MiB
2,048 tokens × 114,688 bytes = 224 MiB
```

This is not a measured allocation for the candidate file. Check the actual head dimension, precision, layout, padding and scale overhead before using the estimate. Scratch and prefill peaks can exceed steady decode. Quantized KV introduces its own accuracy and layout questions.

## Proposed admission contract

A future application should read current Android memory and thermal signals, account for the model and bounded context with checked arithmetic, keep a documented safety margin, and reject or offer a smaller context when required values are unknown or insufficient. Do not silently truncate text or claim a model fits from total RAM alone. A low-priority worker and bounded prefill can reduce contention, but must be measured against an actual foreground workload.

Correctness requires the exact model template, tokenizer, architecture details, stopping behavior and identical-input reference comparisons. Memory admission, output accuracy, cancellation, recovery and user experience each need separate gates.
