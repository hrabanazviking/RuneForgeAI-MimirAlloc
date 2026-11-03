# 03 — Virtual addresses and the Mojo failure

## First failure

The Linux ARM64 Mojo 1.2.0 package was installed in a separate glibc Python environment. Its launcher could not directly execute the compiler because the ELF requested `/lib/ld-linux-aarch64.so.1`, which was not present at that path in this Android environment. Invoking the installed Termux glibc loader directly got farther: before `--version`, the compiler aborted with a TCMalloc `MmapAligned()` failure and a diagnostic saying it assumed a 48-bit virtual address space. A tested 1.3.0.dev2026110205 nightly similarly aborted.

That diagnostic's 1 GiB reservation was a **virtual address range request**, not proof that 1 GiB of physical RAM was allocated. Virtual-address capacity, resident memory, swap and file storage are distinct resources. A virtual reservation can fail despite available physical memory.

## Device probe

A small anonymous 4 KiB `mmap` test used fixed-no-replace semantics and released successful mappings. On this device a request at `1 << 38` returned that address; requests at `1 << 39` and `1 << 47` returned ENOMEM. This is consistent with a 39-bit user virtual-address boundary, but a handful of probes is not a complete kernel configuration audit or a cross-device guarantee. Address placement, security policy, existing mappings and kernel behavior can affect a probe.

The diagnostic and probe together support the hypothesis that allocator address assumptions prevent startup here. They do not prove that all failures called “out of memory” share this cause. They do not show that a changed allocator automatically makes the compiler, generated code, or Android ABI work.

## Why one constant matters

In the investigated TCMalloc source, address width feeds more than an upper-bound check: it participates in memory-tag placement and page-map geometry. Changing a hint while leaving deallocation/tag logic or compiled consumers inconsistent risks invalid classification or corruption. A process must not mix incompatible allocator configuration assumptions. The error's suggestion of `TCMALLOC_ADDRESS_BITS` did not match a configurable ARM64 constant in the pinned source revision investigated.

A width should be a deliberate property of the built artifact and target environment. A 39-bit setting is not universal: an address-space width, page size, ABI and supported kernel need their own tests. A broad support claim requires multiple representative devices and failure cases.

## Public source versus package

Public Modular source inspected at `99ab7e90258db16bda9261b19bcb59d729ee3429` selects jemalloc for the Linux Mojo driver. The separately downloaded nightly still printed the TCMalloc error. The package's exact relationship to that public commit was not established. Build and test the actual source artifact rather than assuming a source setting describes a wheel.

Primary reading: [Mojo system requirements](https://mojolang.org/docs/requirements/), [Mojo compilation targets](https://mojolang.org/docs/tools/compilation/), and [TCMalloc source](https://github.com/google/tcmalloc). These are external references, not MimirAlloc dependencies.
