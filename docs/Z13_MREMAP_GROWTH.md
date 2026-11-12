# Z-052 — mremap growth

Slice: Z-052 (docs/MIMIRALLOC_Z13_ROADMAP.md, Wave 6).
Gate: "Audit whether large realloc grows in place; implement
mremap growth where the mapping cache allows, measure vs
glibc. Gate: growth-ladder table before/after; contract
tests green."
Evidence: tests/perf/recorded_run_z13_z052.txt (contract
run, standing gate, growth ladder before/after/glibc).
Code: src/platform/mimir_platform.{c,h} (the L6 primitive),
src/core/mimir_core.c (grow_mremap_locked + the realloc
large branch), tests/core/test_z052_mremap.c (`make
test-z052`, standalone like test-z048), tests/perf/
probe_z052.c + run_z052.sh + summarize_z052.py.

## 1. The audit: did large realloc grow in place?

No — never, in three distinct senses, all verified from
the banked tree (seat f8ee668, Z-001..Z-049):

1. Beyond the recorded size, every large growth was a
   move: allocate a new mapping, memcpy the old requested
   extent, retire the old mapping (B-008 S4.4's honest
   account: "no in-place grow exists in this machine").
2. A growth the retained mapping already served (usable
   slack, e.g. after a shrink) was ALSO a move: growth is
   judged by the requested-size record, not by usable
   extent. test_g004 F6 pins this ("large grow moves
   (64->8192 by record)"), so this slice does not change
   it — see S3(a).
3. Nothing anywhere in the tree named mremap: the L6
   platform layer offered acquire/release only.

The cost of (1) is the growth ladder's before leg: a
34->128 MiB ladder costs 3.77x glibc's price (S5), because
glibc grows its mmapped chunks by mremap and this
allocator memcpy'd every rung.

## 2. Why in-place-only mremap is not implementable here

The literal reading of "grows in place" — mremap with
flags == 0 — was implemented first and measured. It
cannot work for fresh allocator mappings on this host:

- A raw mremap(flags=0) extending a fresh 34 MiB anonymous
  mapping 34->48 MiB fails ENOMEM in a minimal process.
- /proc/self/maps of the same shape shows why: the fresh
  mapping 7e91939ff000-7e9195c00000 ends EXACTLY where
  libc's first segment begins (7e9195c00000). The
  kernel's top-down placement packs every new mapping
  directly against the VMA above it, so a fresh mapping
  is born with zero growth room above it.

The mapping cache does not change this (pooled mappings
are still mapped; if anything they occupy more room).
What the kernel CAN always do is MREMAP_MAYMOVE: extend
in place when the room above is free (proven in the
contract test's freed-neighbour case), otherwise relocate
the whole mapping by moving page-table entries — zero
bytes copied either way. That is the form glibc uses, and
the form this slice implements. The roadmap's words are
honoured in substance: realloc growth is now mremap
growth; "in place" happens exactly where the address
space allows it.

## 3. Where mremap growth applies — the two bounds

grow_mremap_locked attempts mremap only when BOTH hold:

(a) The request outgrows the mapping itself
    (new_size > usable_size). Inside the usable extent
    the banked by-record move semantics stand (S1.2):
    the F6 rule is a recorded design decision of the
    earlier campaign, and no contract row promises
    pointer identity either way — changing it would
    buy nothing measurable (the copy there is bounded
    by the retained mapping, and the old mapping is
    pooled for reuse) while breaking a pinned behaviour.

(b) The grown mapping length exceeds the Z-048 mapping
    cache's byte bound (MIMIR_POOL_MAX_BYTES, 32 MiB).
    This is the roadmap's "where the mapping cache
    allows" made exact. Inside the bound, the banked
    move is the cache's own road: the retired mapping is
    pooled for exact-length reuse, and the I-004 fault
    legs (Leg 4 acquire-atomicity, Leg 5(c) transfer
    rollback, test_j007 L11) pin acquire/copy/retire
    behaviour there — mremap would silently rewrite the
    fault model those legs prove. Beyond the bound a
    retired mapping can never be pooled (it is unmapped
    at retire), so the move pays a full acquire + copy +
    unmap for nothing the cache can reuse; that is
    precisely the growth mremap exists for. The bound is
    also glibc's own mmap-threshold ceiling (32 MiB):
    above it glibc likewise serves allocations from
    individual mmaps and grows them by mremap.

## 4. Mechanics and accounting

- L6 gains one primitive, mimir_platform_map_grow
  (mremap, MREMAP_MAYMOVE), the only facility that can
  change a live mapping's length. Lengths are exact
  (caller owns the checked page arithmetic), admission
  mirrors the layer's discipline, failure leaves the old
  mapping untouched at its old address.
- The core attempt runs under the realloc large branch's
  existing global lock, after descriptor_is_live, with
  alloc_locked's checked round-up (B-007 A5 form) —
  every refusal (bounds, arithmetic, page-size
  discovery, kernel) returns the block untouched and the
  banked S4.5 move runs instead, so failure semantics
  are banked-identical at every size: a growth that
  cannot be satisfied either way is the MA-6.1-REALLOC-02
  failure, NULL with the original preserved.
- A relocation invalidates the descriptor's ADDRESS
  (neighbours in the live registry hold it), so the
  attempt unlinks the descriptor before the call and
  re-links it at the resulting base after. The descriptor
  rides inside the mapping's reserve page (with the
  J-002 owner record), so its contents arrive intact;
  mapping_base / mapping_length / usable_size /
  requested_size are rewritten together and every
  descriptor_is_live relation stays exact (the metadata
  check proves it after every contract case).
- Counters: live bytes move by the mapping delta; the
  J-002 requested/allocated counters by the admitted
  deltas; the peak sampler re-runs. The E-005 audit
  counters do not move — nothing was published and
  nothing retired, and published - retired still equals
  the live mapping count.
- The public surface is unchanged: libmimiralloc.so
  still exports exactly the 4 public symbols; the
  platform primitive is bound local by the version
  script, like every internal entry point.

## 5. Measurement (recorded run, 2026-11-12)

Growth ladder 34->40->48->56->64->80->96->128 MiB,
Z-004 protocol (pinned CPU 2, 3 warm-ups, 7 reps,
interleaved, median; identical checksums on all legs):

| leg          | median ns/op | in-place | vs glibc |
|--------------|-------------:|---------:|----------|
| mimir before | 16,315,428   | 0/7      | LOSS 3.77x |
| mimir after  |  4,345,140   | 5/7      | TIE 1.004x |
| glibc        |  4,329,033   | 5/7      | —          |

After vs before: 0.266x — WIN (73.4% of the ladder price
removed). After vs glibc: TIE. The residual ~4.3 ms/op
on both mremap legs is the workload itself (memset
pattern-touch + page faults over the ladder's extents),
identical across legs by construction. mimir's in-place
count matches glibc's 5/7: the first grows of a fresh
ladder relocate (S2's packing), later grows extend once
the block sits in open address space.

## 6. Verdict

Roadmap gate MET. Large realloc did not grow in place
(audit, S1); mremap growth is implemented exactly where
the mapping cache's economics leave off (S3), with the
banked move preserved — and fault-proven — everywhere
else. Growth-ladder table before/after recorded; the
standing gate is GREEN 4988/0/77 and the Z-052 contract
suite passes 35/0. Grade vs glibc on the ladder: before
LOSS 3.77x, after TIE 1.004x.
