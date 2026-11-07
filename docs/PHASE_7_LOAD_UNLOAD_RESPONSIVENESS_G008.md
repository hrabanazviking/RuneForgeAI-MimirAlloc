# Phase 7 — Repeated Load/Unload + Multitasking Responsiveness Record (G-008)

Slice G-008, Wave 7 (Phase 7: mobile-friendly small objects).
Base: `92f9ceb` (G-007 banked). Host: Raspberry Pi
purpleviking1 (aarch64, page size 16384), Samtal live on
port 8787 throughout; nothing was stopped, configured, or
installed. All workloads in this slice are **synthetic**.
Full evidence: `tests/core/recorded_run_g008.txt`
(1009 lines; every exit code recorded). Harness:
`tests/core/test_g008.c`; responsiveness probe:
`tests/core/run_g008_probe.sh`; standalone via
`make test-g008` (not part of `make test`).

## 1. Method

**Load/unload leg.** `test_g008.c` links only `-ldl` —
never the library objects — and consumes
`build/libmimiralloc.so` exactly as an outside consumer
does: 50 bounded cycles of `dlopen(RTLD_NOW|RTLD_LOCAL)`
→ `dlsym` ×4 → synthetic workload (32 × 256 B + 8 ×
1024 B small-class blocks + 1 × 8192 B calloc on the
large mapping path + one realloc grow 256 → 512 per
cycle; fill, verify every pattern byte, free all) →
`dlclose`. Per cycle it records open/symbol/close
success, the `mimir_malloc` address, and — after each
`dlclose` — the `libmimiralloc.so` image mappings still
present in `/proc/self/maps`, total anonymous rw bytes
mapped, and VmRSS. An **orphan probe** then leaks one
256 B block across a final `dlclose` and checks from
`maps` whether its page survives, and whether its
contents still read back intact.

**Responsiveness leg.** `run_g008_probe.sh` samples two
probes, 15 samples each, baseline (no churn) vs churn:
P1 Samtal `GET http://127.0.0.1:8787/` (HTTP code +
`time_total` via curl, read-only); P2 `/bin/true`
fork+exec wall time (`date +%s%N`). The churn is
`test_g008 … churn 12 2`: the library loaded once, 2
worker threads (G-004's thread bound is 4) churning
small allocate/fill/verify/free loops for 12 s. Timing
is reported as observed fact, never as a guaranteed
bound (INC-G008-05).

**Bring-up note.** One harness defect was found and
fixed during bring-up: the first workload verify loop
checked all 512 bytes of the realloc-grown block,
including the grow tail whose contents are unspecified
(B-008); corrected to the preserved 256 bytes. The
recorded run is the fixed harness. No core source was
changed by this slice; no allocator defect was found.

## 2. Load/unload record (measured, recorded run)

- All 50 cycles: dlopen + dlsym ×4 succeed, workload
  verify failures 0, dlclose returns 0 — **214 checks,
  0 failures, RESULT: PASS**, `make test-g008` exit 0.
- **The library image fully unmaps on every dlclose:**
  image mappings while loaded = 4 lines / 147,456
  bytes; `img_after_close=0` in all 50 cycles and at
  the end of the record. The library's own static
  state (registries, counters, `.bss`) is destroyed
  with the unmap: a fresh load starts from a fresh
  registry (each cycle's workload verifies clean
  against it, and the orphan-reload leg verifies clean
  on a fresh load).
- **`mimir_malloc` address is not stable across
  cycles:** first load `0x7fff3fd10924`; of the 49
  later cycles, 4 landed at the same address and 45 at
  a different one (INC-G008-04).
- **Retained spans do NOT unmap at dlclose — they
  orphan.** Anonymous rw bytes after each close grow
  by **exactly 49,152 bytes = 3 pages per cycle**,
  every cycle: 98,304 after cycle 0 → 2,506,752 after
  cycle 49; growth cycle 1→50 = 2,408,448 bytes = 147
  pages = 49 × 3. VmRSS grows in step (+48 KiB per
  cycle; start 1360 KiB → end 3824 KiB, delta
  +2464 KiB including the orphan probe and reload
  legs). The 3 pages are exactly the retained spans of
  the 3 small classes the workload warms (256, 512 via
  the realloc grow, 1024 — one span published and
  retained per class in this workload, under the RP-2
  bound of 2). The 8192 B large block contributes
  nothing: its mapping is retired at free, as in
  Phase 3. Mechanism, by recorded inspection: `src/`
  registers **no destructor** (the only
  `destructor|constructor` grep hits in `src/` +
  `include/` are comments stating there is none) and
  no atexit/atfork/thread-local cleanup, so `dlclose`
  unmaps the image but nothing ever munmaps the span
  mappings the image's code created. They stay mapped
  until process exit (INC-G008-02).
- **Orphan probe:** the leaked 256 B block's page is
  still mapped after dlclose and its 0xA5 pattern
  reads back byte-intact. The block is intentionally
  never freed: after the unmap the registry that owned
  it no longer exists, and freeing it through a later
  load would be undefined behaviour per the public
  header (INC-G008-03).

**Counter/registry carry-over, stated honestly.** The
E-005/G-003 audit counters are hidden in the `.so`
(`nm -D` = exactly 4, recorded), so no counter value
can be read through a dlopen'd load at all
(INC-G008-01). Carry-over is therefore established by
the mapping evidence above: registry *state* does not
survive (fresh `.bss` per load; workloads verify
against a fresh registry), while retained span
*mappings* do survive, orphaned, at exactly the
measured 3 pages per warmed cycle for this workload.
Derived, not measured: a consumer that warms all 20
classes to the RP-2 bound in each load would orphan
the full RP-5 worst case — 40 spans = 640 KiB — per
load/unload cycle.

## 3. Responsiveness record (measured, recorded run)

All 60 Samtal samples returned HTTP 200.

| Probe | Leg | min | p50 | max | mean (ms) |
|---|---|---|---|---|---|
| Samtal GET / | baseline | 0.820 | 0.896 | 8.770 | 1.537 |
| Samtal GET / | churn | 0.819 | 0.996 | 4.969 | 1.903 |
| /bin/true | baseline | 1.089 | 1.281 | 6.500 | 1.880 |
| /bin/true | churn | 1.072 | 1.230 | 4.321 | 1.596 |

Churn leg: **14,795,488 ops, 0 verify failures** in
12 s on 2 threads, churn RSS 1680 KiB at exit, churn
exit 0. Post-churn Samtal health: HTTP 200 in
1.393 ms. Reading, carefully: Samtal's p50 moved
+0.100 ms and its mean +0.366 ms under churn, while
`/bin/true`'s p50 and mean moved *down* — i.e. at this
small, bounded churn level the medians show no
material responsiveness change in either direction,
and the largest single-sample spikes in the record
occur in the **baseline** legs (8.770 ms Samtal,
6.500 ms true, with no churn running): this host has
background noise of that order with or without the
allocator churning (INC-G005-03's setting — a working
home, not a quiet bench). These are single-run
observations, not bounds (INC-G008-05).

## 4. Comparison vs the G-001 baseline, where shapes match

- **Retained-after-free shape.** G-001's Phase 3
  mapping-per-allocation allocator: `retained_after_
  free_kb=0`, RSS flat under churn — it returned
  everything at free. The Phase 7 allocator's answer
  is deliberately different *while loaded*: warmed
  classes retain up to the RP-2 bound (G-005 measured
  the full 40-span / 640 KiB worst case) as the price
  of reuse latency. G-008 adds the load/unload edge of
  that shape: across a `dlclose`, retention is not
  returned at all — it orphans (Section 2). The
  G-001 zero-retention property therefore holds for
  the Phase 7 allocator only at process exit, not at
  free (G-005) and not at unload (G-008).
- **Latency shape.** G-001 measured per-op allocation
  latency directly (Phase 3 p50 ≈ 4.9–5.2 µs); G-008
  does not re-measure per-op latency (G-003/G-005 own
  that shape) and claims no ratio. The comparable
  G-008 shape is *device* responsiveness while the
  allocator works: Samtal p50 ≈ 0.9–1.0 ms in both
  legs (Section 3).

## 5. Gaps, surprises, and owed work (INC-G008-NN)

- **INC-G008-01 (counters unobservable via the .so —
  by design).** The audit counters are hidden-visibility
  (INC-E001-07 shape (b)); a dlopen consumer cannot
  read them, so this slice's carry-over verdicts rest
  on `/proc/self/maps` + VmRSS measurement and the
  recorded `src/` inspection, never on counter reads.
- **INC-G008-02 (dlclose orphans retained spans —
  measured finding, not fixed here).** No destructor
  exists, so every load/unload cycle that warms small
  classes permanently leaves their retained spans
  mapped until process exit (measured: exactly
  3 pages/cycle for this workload; derived worst case
  640 KiB/cycle at full RP-5 warming). This is the
  load/unload face of INC-G005-03's no-reaper design.
  Per this slice's charter — record the facts around
  that property, do not silently change it — **no core
  change is made**; a future contract slice may add an
  unload-time release (destructor or an explicit
  trim/shutdown entry point) or document that
  dlopen/dlclose consumers must expect per-cycle
  retention. Flagged for the G-009 exit packet.
- **INC-G008-03 (live allocations at dlclose are
  unrecoverable — measured + inspection).** A block
  still live at dlclose keeps its span and contents
  (orphan probe: mapped, byte-intact) but no defined
  path can ever free or reuse it: the registry is
  destroyed with the unmap and the header makes
  foreign-pointer free undefined. Consumers must free
  everything before dlclose; the library offers no
  shutdown call to do it for them.
- **INC-G008-04 (function-pointer instability —
  observed fact).** `mimir_malloc` resolved to the
  first load's address in only 4 of 49 later cycles.
  Nothing may be cached across a dlclose — standard
  dlclose discipline, recorded because this record
  measured it.
- **INC-G008-05 (responsiveness numbers are
  single-run observations on a shared host).** One
  15-sample run per leg on a working home; baseline
  spikes (8.770 ms) exceed every churn-leg median
  shift. No latency bound is claimed for Phase 7 exit;
  if G-009 wants a tighter statement it is owed
  repeated runs, ideally at a quieter hour.
- **INC-G008-06 (probe coverage is read-path only —
  scope).** The Samtal probe is `GET /` only; write
  paths and long-lived connections under churn are
  not covered by this slice and are not claimed.

## 6. Verification summary (all on the Pi, recorded)

- `make clean` exit 0; `make test` exit 0 — 4590
  checks, 0 failed (banked totals unchanged).
- `make lib` exit 0; `nm -D` exactly 4 dynamic
  symbols.
- `make test-g008` exit 0: load/unload 214 checks /
  0 failures; churn 14,795,488 ops / 0 failures;
  probe exit 0; Samtal HTTP 200 before, during
  (60/60 samples), and after.
- README.md and `include/mimir_alloc.h` sha256-
  identical to base `92f9ceb` (recorded in the run
  file). Makefile change additive only (+26 lines,
  the `test-g008` target). No `src/` change.
