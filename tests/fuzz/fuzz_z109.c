/* fuzz_z109.c - Z-109 (Wave 13 reserve, pool choice: API fuzz
 * at scale): a libFuzzer-style battery over the FULL public
 * surface of include/mimir_alloc.h (13 entry points), driven
 * by a byte-stream interpreter.
 *
 * The interpreter consumes a byte stream as an operation
 * program: each op byte selects an entry point and the
 * following bytes select its arguments (slot, size class,
 * alignment class, failure shape). The same interpreter is
 * fed three ways:
 *   1. seeded mode - a xorshift64star PRNG generates the
 *      stream from a fixed seed (same seed -> same stream,
 *      proven by the runner's two-run diff);
 *   2. corpus mode - each file in a directory is one stream
 *      (the libFuzzer corpus shape);
 *   3. libFuzzer mode - compiled with -DZ109_LIBFUZZER and
 *      -fsanitize=fuzzer, LLVMFuzzerTestOneInput runs one
 *      stream per input (no main in that build).
 *
 * Observables compared are defined behaviour only (the
 * M-004 discipline, extended to the whole surface): pattern
 * integrity of every live block, pairwise disjointness of
 * live requested extents, alignment of aligned-entry
 * results, calloc zero-fill, realloc preserved prefix,
 * failure atomicity (a failed realloc/reallocarray leaves
 * the original live and byte-identical), the D-013
 * zero-size contract (exact NULL / ENOMEM shapes), the
 * D-012 usable-size law (live block: > 0 and >= requested;
 * NULL: 0), posix_memalign return-code shapes with
 * *memptr unchanged on failure, stats-snapshot shape
 * (version/size/return codes) and the live-count equality
 * live_small_objects + live_mappings == model live count
 * through the PUBLIC mimir_stats_snapshot. Pointer identity
 * and timing are never oracles. Undefined behaviour is
 * never invoked: the model only ever frees/reallocs live
 * family pointers it owns, exactly once; invalid
 * alignments and overflow sizes are used only where the
 * header defines the refusal.
 *
 * Injection build (-DZ109_INJECT, linked with the I-004
 * fault shim under -Wl,--wrap=mimir_platform_map_acquire):
 * before alloc-family ops the driver arms a single-shot
 * mmap-refusal with a short stream-chosen countdown (the
 * D-09 coverage gap named in the Z-108 verdict S6). A NULL
 * (or ENOMEM) for a lawful request is then lawful ONLY if
 * the same request succeeds on an immediate disarmed
 * retry; the original-block checks apply unchanged. The
 * run must observe injected failures (shim counters) or it
 * has proven nothing and reports FAIL.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "mimir_alloc.h"
#ifdef Z109_INJECT
#include "../core/mimir_fault_i004.h"
#endif

#define MAXLIVE 192
#define BATCH 16

typedef struct {
  uint64_t checks, fails;
  uint64_t seed;
  long op;
  int id;              /* thread id (0 = main/single) */
  int mt;              /* multithread mode: no global stats eq */
  int stop;            /* a FINDING fired: stop this stream */
  uint64_t injected_nulls; /* inject build: NULLs excused by retry */
} fz_ctx;

static void fail(fz_ctx *c, const char *what, const char *detail) {
  c->fails++;
  c->stop = 1;
  printf("FINDING ctx=%d seed=%llu op=%ld %s %s\n", c->id,
         (unsigned long long)c->seed, c->op, what, detail);
  fflush(stdout);
}
#define CHECK(c, cond, what, detail) do { \
    (c)->checks++; if (!(cond)) fail((c), (what), (detail)); \
  } while (0)

/* ---------------- byte stream ---------------- */
typedef struct {
  const uint8_t *buf;
  size_t len, pos;
} stream_t;
static int rd8(stream_t *s, uint8_t *out) {
  if (s->pos >= s->len) return 0;
  *out = s->buf[s->pos++];
  return 1;
}
static uint64_t rd64(stream_t *s) {
  uint64_t v = 0; int i;
  for (i = 0; i < 8; i++) {
    uint8_t b = 0;
    if (!rd8(s, &b)) return v;
    v |= (uint64_t)b << (8 * i);
  }
  return v;
}
static size_t pick(stream_t *s, size_t n) {
  uint8_t b = 0;
  (void)rd8(s, &b);
  return n ? (size_t)(b % n) : 0;
}

/* ---------------- ownership model ---------------- */
typedef struct {
  void *p; size_t sz; size_t usable; uint8_t pat;
  size_t align; int live; uint64_t serial;
} blk_t;
typedef struct {
  blk_t blk[MAXLIVE];
  uint64_t serial;
  uint64_t ops[20];
} model_t;

static int model_live_count(const model_t *m) {
  int n = 0, i;
  for (i = 0; i < MAXLIVE; i++) if (m->blk[i].live) n++;
  return n;
}
static int model_free_slot(const model_t *m) {
  int i;
  for (i = 0; i < MAXLIVE; i++) if (!m->blk[i].live) return i;
  return -1;
}
static int model_pick_live(const model_t *m, stream_t *s) {
  int n = model_live_count(m), k, i;
  if (!n) return -1;
  k = (int)pick(s, (size_t)n);
  for (i = 0; i < MAXLIVE; i++)
    if (m->blk[i].live && k-- == 0) return i;
  return -1;
}
static void fill_blk(blk_t *b) { memset(b->p, b->pat, b->sz); }
static int verify_blk(const blk_t *b) {
  size_t i; const unsigned char *p = (const unsigned char *)b->p;
  for (i = 0; i < b->sz; i++) if (p[i] != b->pat) return 0;
  return 1;
}
static int cmp_ptr(const void *a, const void *b) {
  uintptr_t x = (uintptr_t)*(void *const *)a;
  uintptr_t y = (uintptr_t)*(void *const *)b;
  return (x > y) - (x < y);
}

/* Batch check: patterns, overlap, (single-thread) public
 * stats live-count equality. */
static void batch_check(fz_ctx *c, model_t *m) {
  int i, n = model_live_count(m), k = 0;
  void *ptrs[MAXLIVE];
  for (i = 0; i < MAXLIVE; i++) if (m->blk[i].live) {
    CHECK(c, verify_blk(&m->blk[i]), "pattern",
          "live block corrupt");
    ptrs[k++] = m->blk[i].p;
    CHECK(c, m->blk[i].usable >= m->blk[i].sz && m->blk[i].usable > 0,
          "usable", "live usable extent violates D-012");
    if (c->stop) return;
  }
  qsort(ptrs, (size_t)k, sizeof(void *), cmp_ptr);
  for (i = 1; i < k; i++) {
    size_t psz = 0; int j;
    for (j = 0; j < MAXLIVE; j++)
      if (m->blk[j].live && m->blk[j].p == ptrs[i - 1])
        psz = m->blk[j].sz;
    CHECK(c, (uintptr_t)ptrs[i - 1] + psz <= (uintptr_t)ptrs[i],
          "overlap", "live requested extents overlap");
    if (c->stop) return;
  }
  if (!c->mt) {
    struct mimir_stats_v1 st;
    memset(&st, 0, sizeof st);
    CHECK(c, mimir_stats_snapshot(&st) == 0, "stats",
          "snapshot failed");
    if (c->stop) return;
    CHECK(c, st.struct_version == MIMIR_STATS_VERSION &&
          st.struct_size == sizeof(struct mimir_stats_v1),
          "stats", "version/size shape wrong");
    CHECK(c, st.live_small_objects + st.live_mappings ==
          (uint64_t)n, "live-count",
          "model != public stats live count");
  }
  (void)n;
}

/* ---------------- argument tables ---------------- */
/* Lawful sizes: span classes, page boundaries, mapping path. */
static const size_t SIZES[] = {
  1, 2, 3, 8, 16, 24, 32, 48, 64, 80, 96, 128, 160, 192, 256,
  320, 384, 512, 768, 1024, 1025, 2048, 4095, 4096, 4097,
  8192, 16384, 32768, 65536, 131072, 262144, 1048576
};
#define NSIZES (sizeof(SIZES) / sizeof(SIZES[0]))
/* Failure-shaped sizes: representable-unsatisfiable and
 * overflowing. Used only where NULL is the defined result. */
static const size_t BADSIZES[] = {
  (size_t)-1, (size_t)-100, (size_t)-4096,
  ((size_t)-1) / 2 + 1, ((size_t)-1) / 2
};
#define NBAD (sizeof(BADSIZES) / sizeof(BADSIZES[0]))
static const size_t ALIGNS_OK[] = {
  1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 4096, 16384,
  65536, 1048576
};
#define NALIGN (sizeof(ALIGNS_OK) / sizeof(ALIGNS_OK[0]))
static const size_t ALIGNS_BAD[] = {
  0, 3, 5, 6, 7, 9, 12, 24, 48, 96, 100, 4095, (size_t)-1
};
#define NBALIGN (sizeof(ALIGNS_BAD) / sizeof(ALIGNS_BAD[0]))

/* ---------------- injection arming ---------------- */
static void maybe_arm(fz_ctx *c, stream_t *s) {
  (void)c;
#ifdef Z109_INJECT
  uint8_t b = 0;
  (void)rd8(s, &b);
  if (b % 4 == 0)
    mimir_fault_arm_acquire(1 + (unsigned long)(rd64(s) % 24));
#else
  (void)c; (void)s; if (0) (void)rd64(s);
#endif
}
/* After a lawful request returned NULL in the inject build:
 * disarm and retry once. Returns the retry result; the
 * caller treats a second NULL as a FINDING. In the plain
 * build a lawful NULL is a FINDING immediately. */
static void *lawful_retry(fz_ctx *c, int kind, size_t a, size_t b,
                          void *orig) {
  /* kind: 0 malloc(a) 1 calloc(a,b) 2 aligned(a,b)
   *       3 memalign(a,b) 4 valloc(a) 5 pvalloc(a) */
#ifdef Z109_INJECT
  void *p;
  mimir_fault_disarm();
  switch (kind) {
    case 0: p = mimir_malloc(a); break;
    case 1: p = mimir_calloc(a, b); break;
    case 2: p = mimir_aligned_alloc(a, b); break;
    case 3: p = mimir_memalign(a, b); break;
    case 4: p = mimir_valloc(a); break;
    default: p = mimir_pvalloc(a); break;
  }
  (void)orig;
  if (p) c->injected_nulls++;
  return p;
#else
  (void)c; (void)kind; (void)a; (void)b; (void)orig;
  return NULL;
#endif
}

/* Register a fresh block in the model and audit it. */
static void adopt(fz_ctx *c, model_t *m, void *p, size_t sz,
                  size_t align) {
  int s = model_free_slot(m);
  blk_t *b;
  size_t us;
  if (s < 0) { mimir_free(p); return; }
  b = &m->blk[s];
  b->p = p; b->sz = sz; b->align = align;
  b->pat = (uint8_t)(0x3Cu ^ (uint8_t)(++m->serial));
  b->live = 1; b->serial = m->serial;
  if (align > 1)
    CHECK(c, (uintptr_t)p % align == 0, "align",
          "aligned entry result misaligned");
  us = mimir_usable_size(p);
  CHECK(c, us >= sz && us > 0, "usable",
        "usable extent violates D-012 at adoption");
  b->usable = us;
  fill_blk(b);
}

/* ---------------- ops ---------------- */
/* op classes (op byte value):
 *  0 malloc        1 calloc       2 realloc      3 reallocarray
 *  4 aligned_alloc 5 posix_memalign 6 memalign   7 valloc
 *  8 pvalloc       9 free        10 defined no-ops / queries
 * 11 write/verify 12 usable audit 13 trim       14 stats audit
 * 15 zero-size battery           16 invalid-alignment battery
 * 17 overflow battery            18 cross-thread transfer+free
 * 19 batch check  (also automatic every BATCH ops)          */

static void op_malloc(fz_ctx *c, model_t *m, stream_t *s) {
  size_t sz; void *p;
  m->ops[0]++;
  if (model_free_slot(m) < 0) return;
  maybe_arm(c, s);
  sz = SIZES[pick(s, NSIZES)];
  p = mimir_malloc(sz);
  if (!p) p = lawful_retry(c, 0, sz, 0, NULL);
  CHECK(c, p != NULL, "malloc", "NULL for lawful size");
  if (!p) return;
  adopt(c, m, p, sz, 1);
}
static void op_calloc(fz_ctx *c, model_t *m, stream_t *s) {
  size_t nm, sz, i; unsigned char *p; int ok = 1;
  m->ops[1]++;
  if (model_free_slot(m) < 0) return;
  maybe_arm(c, s);
  nm = 1 + pick(s, 24);
  sz = 1 + pick(s, 96);
  p = mimir_calloc(nm, sz);
  if (!p) p = (unsigned char *)lawful_retry(c, 1, nm, sz, NULL);
  CHECK(c, p != NULL, "calloc", "NULL for lawful product");
  if (!p) return;
  for (i = 0; i < nm * sz; i++) if (p[i] != 0) ok = 0;
  CHECK(c, ok, "calloc", "not zero-filled");
  if (c->stop) { mimir_free(p); return; }
  adopt(c, m, p, nm * sz, 1);
}
static void do_free_idx(fz_ctx *c, model_t *m, int s) {
  blk_t *b = &m->blk[s];
  CHECK(c, verify_blk(b), "free", "pattern corrupt before free");
  mimir_free(b->p);
  b->live = 0; b->p = NULL;
}
static void op_realloc(fz_ctx *c, model_t *m, stream_t *s,
                       int array_form) {
  int si = model_pick_live(m, s);
  blk_t *b; size_t oldsz, newsz, minsz, na = 0, nb = 0;
  void *np; unsigned kind;
  m->ops[array_form ? 3 : 2]++;
  if (si < 0) { op_malloc(c, m, s); return; }
  b = &m->blk[si];
  oldsz = b->sz;
  kind = (unsigned)pick(s, 12);
  maybe_arm(c, s);
  if (kind == 0) {           /* zero-size: D-013 contract */
    np = array_form ? mimir_reallocarray(b->p, 0, 8)
                    : mimir_realloc(b->p, 0);
    CHECK(c, np == NULL, "realloc-zero",
          "zero-size realloc not NULL (D-013)");
    CHECK(c, verify_blk(b), "realloc-zero",
          "original damaged by zero-size realloc");
    return;
  }
  if (kind == 1) {           /* failure-shaped size */
    size_t bad = BADSIZES[pick(s, NBAD)];
    if (array_form) { na = 2; nb = bad / 2 + 1;
      np = mimir_reallocarray(b->p, na, nb);
    } else {
      np = mimir_realloc(b->p, bad);
    }
    CHECK(c, np == NULL, "realloc-fail",
          "failure-shaped realloc not NULL");
    CHECK(c, verify_blk(b), "realloc-fail",
          "original damaged by failed realloc");
    CHECK(c, mimir_usable_size(b->p) == b->usable, "realloc-fail",
          "usable extent changed by failed realloc");
    return;
  }
  newsz = SIZES[pick(s, NSIZES)];
  if (array_form) {
    na = 1 + pick(s, 8); nb = newsz / na; if (nb == 0) nb = 1;
    newsz = na * nb;
    np = mimir_reallocarray(b->p, na, nb);
  } else {
    np = mimir_realloc(b->p, newsz);
  }
  if (!np) {
    /* lawful failure: only injection may excuse it, and the
     * original must be intact; retry disarmed. */
    CHECK(c, verify_blk(b), "realloc",
          "original damaged by failed lawful realloc");
#ifdef Z109_INJECT
    mimir_fault_disarm();
    np = array_form ? mimir_reallocarray(b->p, na, nb)
                    : mimir_realloc(b->p, newsz);
    if (np) c->injected_nulls++;
#endif
    CHECK(c, np != NULL, "realloc",
          "NULL for lawful size without injection");
    if (!np) return;
  }
  minsz = oldsz < newsz ? oldsz : newsz;
  { size_t i; const unsigned char *p = (const unsigned char *)np;
    int ok = 1;
    for (i = 0; i < minsz; i++) if (p[i] != b->pat) ok = 0;
    CHECK(c, ok, "realloc", "preserved prefix corrupt"); }
  if (c->stop) return;
  b->p = np; b->sz = newsz; b->align = 1;
  b->usable = mimir_usable_size(np);
  CHECK(c, b->usable >= newsz && b->usable > 0, "usable",
        "usable extent violates D-012 after realloc");
  fill_blk(b);
}
static void op_aligned(fz_ctx *c, model_t *m, stream_t *s) {
  size_t al, sz; void *p;
  m->ops[4]++;
  if (model_free_slot(m) < 0) return;
  maybe_arm(c, s);
  al = ALIGNS_OK[pick(s, NALIGN)];
  sz = SIZES[pick(s, NSIZES)];
  p = mimir_aligned_alloc(al, sz);
  if (!p) p = lawful_retry(c, 2, al, sz, NULL);
  CHECK(c, p != NULL, "aligned_alloc",
        "NULL for lawful alignment+size");
  if (!p) return;
  adopt(c, m, p, sz, al);
}
static void op_posix(fz_ctx *c, model_t *m, stream_t *s) {
  size_t al, sz; void *out; int rc;
  static const void *SENT = (const void *)(uintptr_t)0x1BADB002u;
  m->ops[5]++;
  al = ALIGNS_OK[pick(s, NALIGN)];
  sz = SIZES[pick(s, NSIZES)];
  out = (void *)SENT;
  maybe_arm(c, s);
  rc = mimir_posix_memalign(&out, al, sz);
  if (al % sizeof(void *) != 0) {
    CHECK(c, rc == EINVAL, "posix",
          "non-multiple-of-void* alignment not EINVAL");
    CHECK(c, out == SENT, "posix", "*memptr changed on EINVAL");
    return;
  }
  if (rc != 0) {
#ifdef Z109_INJECT
    /* ENOMEM shape: *memptr unchanged, retry disarmed. */
    CHECK(c, out == SENT, "posix", "*memptr changed on ENOMEM");
    CHECK(c, rc == ENOMEM, "posix",
          "failure return is not ENOMEM");
    mimir_fault_disarm();
    rc = mimir_posix_memalign(&out, al, sz);
    if (rc == 0) c->injected_nulls++;
#endif
    CHECK(c, rc == 0, "posix",
          "ENOMEM for lawful request without injection");
    if (rc != 0) return;
  }
  if (model_free_slot(m) < 0) { mimir_free(out); return; }
  adopt(c, m, out, sz, al);
}
static void op_memalign(fz_ctx *c, model_t *m, stream_t *s) {
  size_t al, sz; void *p;
  m->ops[6]++;
  if (model_free_slot(m) < 0) return;
  maybe_arm(c, s);
  al = ALIGNS_OK[pick(s, NALIGN)];
  sz = SIZES[pick(s, NSIZES)];
  p = mimir_memalign(al, sz);
  if (!p) p = lawful_retry(c, 3, al, sz, NULL);
  CHECK(c, p != NULL, "memalign", "NULL for lawful request");
  if (!p) return;
  adopt(c, m, p, sz, al);
}
static void op_valloc(fz_ctx *c, model_t *m, stream_t *s,
                      int pv) {
  size_t sz, want; void *p;
  m->ops[pv ? 8 : 7]++;
  if (model_free_slot(m) < 0) return;
  maybe_arm(c, s);
  sz = SIZES[pick(s, NSIZES)];
  p = pv ? mimir_pvalloc(sz) : mimir_valloc(sz);
  if (!p) p = lawful_retry(c, pv ? 5 : 4, sz, 0, NULL);
  CHECK(c, p != NULL, pv ? "pvalloc" : "valloc",
        "NULL for lawful request");
  if (!p) return;
  want = pv ? ((sz + 4095u) & ~(size_t)4095u) : sz;
  { int s2 = model_free_slot(m); blk_t *b;
    size_t us = mimir_usable_size(p);
    CHECK(c, us >= want && us > 0, "usable",
          "valloc-family usable extent violates D-012");
    if (s2 < 0) { mimir_free(p); return; }
    b = &m->blk[s2];
    b->p = p; b->sz = sz; b->align = 4096;
    b->pat = (uint8_t)(0x3Cu ^ (uint8_t)(++m->serial));
    b->live = 1; b->serial = m->serial; b->usable = us;
    CHECK(c, (uintptr_t)p % 4096 == 0, "align",
          "valloc-family result not page-aligned");
    fill_blk(b);
  }
}
typedef struct { void *p; uint8_t pat; size_t sz; int ok; } xfer_t;
static void *xfer_thread(void *arg) {
  xfer_t *x = (xfer_t *)arg;
  const unsigned char *p = (const unsigned char *)x->p;
  size_t i; x->ok = 1;
  for (i = 0; i < x->sz; i++) if (p[i] != x->pat) x->ok = 0;
  mimir_free(x->p);
  return NULL;
}
static void op_xfer(fz_ctx *c, model_t *m, stream_t *s) {
  int si = model_pick_live(m, s);
  blk_t *b; xfer_t x; pthread_t th;
  m->ops[18]++;
  if (si < 0) { op_malloc(c, m, s); return; }
  b = &m->blk[si];
  x.p = b->p; x.pat = b->pat; x.sz = b->sz; x.ok = 0;
  if (pthread_create(&th, NULL, xfer_thread, &x) != 0) {
    CHECK(c, 0, "xfer", "pthread_create failed");
    return;
  }
  pthread_join(th, NULL);
  CHECK(c, x.ok, "xfer", "pattern corrupt in freeing thread");
  b->live = 0; b->p = NULL;
}
static void op_battery_zero(fz_ctx *c, model_t *m, stream_t *s) {
  void *out; int rc; size_t al;
  static const void *SENT = (const void *)(uintptr_t)0x1BADB002u;
  (void)s;
  m->ops[15]++;
  /* D-013: every zero-size shape is the contract NULL,
   * never a failure, never a live block. */
  CHECK(c, mimir_malloc(0) == NULL, "zero", "malloc(0) not NULL");
  CHECK(c, mimir_calloc(0, 8) == NULL, "zero",
        "calloc(0,8) not NULL");
  CHECK(c, mimir_calloc(8, 0) == NULL, "zero",
        "calloc(8,0) not NULL");
  CHECK(c, mimir_calloc(0, 0) == NULL, "zero",
        "calloc(0,0) not NULL");
  CHECK(c, mimir_realloc(NULL, 0) == NULL, "zero",
        "realloc(NULL,0) not NULL");
  CHECK(c, mimir_reallocarray(NULL, 0, 8) == NULL, "zero",
        "reallocarray(NULL,0,8) not NULL");
  CHECK(c, mimir_aligned_alloc(16, 0) == NULL, "zero",
        "aligned_alloc(16,0) not NULL");
  CHECK(c, mimir_memalign(16, 0) == NULL, "zero",
        "memalign(16,0) not NULL");
  CHECK(c, mimir_valloc(0) == NULL, "zero", "valloc(0) not NULL");
  CHECK(c, mimir_pvalloc(0) == NULL, "zero",
        "pvalloc(0) not NULL");
  al = 16;
  out = (void *)SENT;
  rc = mimir_posix_memalign(&out, al, 0);
  CHECK(c, rc == ENOMEM, "zero",
        "posix_memalign(valid,0) not ENOMEM");
  CHECK(c, out == SENT, "zero",
        "*memptr changed on zero-size posix_memalign");
  CHECK(c, mimir_usable_size(NULL) == 0, "zero",
        "usable_size(NULL) not 0");
  batch_check(c, m);
}
static void op_battery_badalign(fz_ctx *c, model_t *m,
                                stream_t *s) {
  size_t al; void *out; int rc;
  static const void *SENT = (const void *)(uintptr_t)0x1BADB002u;
  m->ops[16]++;
  al = ALIGNS_BAD[pick(s, NBALIGN)];
  CHECK(c, mimir_aligned_alloc(al, 64) == NULL, "badalign",
        "aligned_alloc accepted invalid alignment");
  CHECK(c, mimir_memalign(al, 64) == NULL, "badalign",
        "memalign accepted invalid alignment");
  out = (void *)SENT;
  rc = mimir_posix_memalign(&out, al, 64);
  CHECK(c, rc == EINVAL, "badalign",
        "posix_memalign invalid alignment not EINVAL");
  CHECK(c, out == SENT, "badalign",
        "*memptr changed on EINVAL posix_memalign");
  rc = mimir_posix_memalign(NULL, 16, 64);
  CHECK(c, rc == EINVAL, "badalign",
        "posix_memalign(NULL memptr) not EINVAL");
  (void)m;
}
static void op_battery_overflow(fz_ctx *c, model_t *m,
                                stream_t *s) {
  int si; size_t bad;
  m->ops[17]++;
  bad = BADSIZES[pick(s, NBAD)];
  CHECK(c, mimir_malloc(bad) == NULL, "overflow",
        "malloc(bad size) not NULL");
  CHECK(c, mimir_calloc(((size_t)-1) / 2 + 1, 2) == NULL,
        "overflow", "calloc product overflow not NULL");
  CHECK(c, mimir_calloc(2, ((size_t)-1) / 2 + 1) == NULL,
        "overflow", "calloc swapped overflow not NULL");
  CHECK(c, mimir_pvalloc((size_t)-100) == NULL, "overflow",
        "pvalloc round-up overflow not NULL");
  CHECK(c, mimir_valloc(bad) == NULL, "overflow",
        "valloc(bad size) not NULL");
  si = model_pick_live(m, s);
  if (si >= 0) {
    blk_t *b = &m->blk[si];
    CHECK(c, mimir_reallocarray(b->p, ((size_t)-1) / 2 + 1, 2)
          == NULL, "overflow",
          "reallocarray product overflow not NULL");
    CHECK(c, verify_blk(b), "overflow",
          "original damaged by overflow reallocarray");
  }
  batch_check(c, m);
}

/* ---------------- stream interpreter ---------------- */
static void run_stream(fz_ctx *c, model_t *m, stream_t *s,
                       long max_ops) {
  long i;
  memset(m, 0, sizeof *m);
  for (i = 0; i < max_ops; i++) {
    uint8_t opb;
    c->op = i;
    if (!rd8(s, &opb)) break;
    switch (opb % 20) {
      case 0: op_malloc(c, m, s); break;
      case 1: op_calloc(c, m, s); break;
      case 2: op_realloc(c, m, s, 0); break;
      case 3: op_realloc(c, m, s, 1); break;
      case 4: op_aligned(c, m, s); break;
      case 5: op_posix(c, m, s); break;
      case 6: op_memalign(c, m, s); break;
      case 7: op_valloc(c, m, s, 0); break;
      case 8: op_valloc(c, m, s, 1); break;
      case 9: { int si = model_pick_live(m, s);
        m->ops[9]++;
        if (si < 0) op_malloc(c, m, s); else do_free_idx(c, m, si);
        break; }
      case 10: m->ops[10]++;
        mimir_free(NULL);
        CHECK(c, mimir_stats_snapshot(NULL) == -1, "noop",
              "stats_snapshot(NULL) not -1");
        break;
      case 11: { int si = model_pick_live(m, s);
        m->ops[11]++;
        if (si < 0) { op_malloc(c, m, s); break; }
        CHECK(c, verify_blk(&m->blk[si]), "write",
              "pattern corrupt before rewrite");
        m->blk[si].pat = (uint8_t)(m->blk[si].pat + 0x11u);
        fill_blk(&m->blk[si]);
        break; }
      case 12: { int si = model_pick_live(m, s);
        m->ops[12]++;
        if (si < 0) { op_malloc(c, m, s); break; }
        CHECK(c, mimir_usable_size(m->blk[si].p) ==
              m->blk[si].usable, "usable",
              "usable extent changed under no mutation");
        break; }
      case 13: { int r; size_t pad;
        m->ops[13]++;
        pad = (pick(s, 4) == 0) ? 0 : SIZES[pick(s, NSIZES)];
        r = mimir_trim(pad);
        CHECK(c, r == 0 || r == 1, "trim",
              "trim returned a value other than 0/1");
        batch_check(c, m);
        break; }
      case 14: { struct mimir_stats_v1 st;
        m->ops[14]++;
        memset(&st, 0, sizeof st);
        CHECK(c, mimir_stats_snapshot(&st) == 0, "stats",
              "snapshot failed");
        CHECK(c, st.page_size_bytes == 4096, "stats",
              "page_size_bytes not this host's 4096");
        break; }
      case 15: op_battery_zero(c, m, s); break;
      case 16: op_battery_badalign(c, m, s); break;
      case 17: op_battery_overflow(c, m, s); break;
      case 18: op_xfer(c, m, s); break;
      default: m->ops[19]++; batch_check(c, m); break;
    }
    if (c->stop) break;
    if (i % BATCH == BATCH - 1) batch_check(c, m);
    if (c->stop) break;
  }
  /* final drain */
  { int j;
    for (j = 0; j < MAXLIVE; j++)
      if (m->blk[j].live) do_free_idx(c, m, j);
  }
  (void)mimir_trim(0);
  if (!c->mt) batch_check(c, m);
  CHECK(c, model_live_count(m) == 0, "final",
        "model not empty after drain");
}

#ifndef Z109_LIBFUZZER
/* ---------------- PRNG stream generation ---------------- */
typedef struct { uint64_t s; } rng_t;
static uint64_t rng_next(rng_t *r) {
  uint64_t x = r->s;
  x ^= x >> 12; x ^= x << 25; x ^= x >> 27; r->s = x;
  return x * 0x2545F4914F6CDD1DULL;
}
static uint8_t *gen_stream(uint64_t seed, size_t bytes) {
  uint8_t *buf = (uint8_t *)malloc(bytes);
  rng_t r; size_t i = 0;
  if (!buf) return NULL;
  r.s = seed ? seed : 1;
  while (i < bytes) {
    uint64_t v = rng_next(&r); int k;
    for (k = 0; k < 8 && i < bytes; k++)
      buf[i++] = (uint8_t)(v >> (8 * k));
  }
  return buf;
}
static void print_seed_line(fz_ctx *c, model_t *m,
                            const char *tag, long ops_done) {
  int i;
  printf("SEED %llu %s ops=%ld", (unsigned long long)c->seed,
         tag, ops_done);
  for (i = 0; i < 20; i++)
    printf(" op%d=%llu", i, (unsigned long long)m->ops[i]);
  printf(" checks=%llu fails=%llu injected_nulls=%llu live_end=%d\n",
         (unsigned long long)c->checks,
         (unsigned long long)c->fails,
         (unsigned long long)c->injected_nulls,
         model_live_count(m));
  fflush(stdout);
}
static long run_seed(uint64_t seed, long ops, int mt, int id) {
  size_t bytes = (size_t)ops * 10 + 64;
  uint8_t *buf = gen_stream(seed, bytes);
  fz_ctx c; model_t m; stream_t s;
  if (!buf) { printf("FINDING seed gen OOM\n"); return 1; }
  memset(&c, 0, sizeof c);
  c.seed = seed; c.mt = mt; c.id = id;
  s.buf = buf; s.len = bytes; s.pos = 0;
  run_stream(&c, &m, &s, ops);
  print_seed_line(&c, &m, mt ? "MT-PART" : "STREAM",
                  c.op + 1);
#ifdef Z109_INJECT
  printf("FAULT acquire_attempts=%lu injected=%lu real_failed=%lu release_attempts=%lu lock_attempts=%lu pagesize_attempts=%lu\n", mimir_fault_acquire_attempts, mimir_fault_acquire_injected, mimir_fault_acquire_real_failed, mimir_fault_release_attempts, mimir_fault_lock_attempts, mimir_fault_pagesize_attempts);
#endif
  free(buf);
  return c.fails ? 1 : 0;
}

/* ---------------- multithread leg ---------------- */
#define MT_THREADS 4
#define INBOX_SLOTS 64
typedef struct {
  blk_t blk[INBOX_SLOTS]; int n; pthread_mutex_t mu;
} inbox_t;
static inbox_t g_inbox;
typedef struct { int id; uint64_t seed; long ops; long rc; } mt_arg_t;
/* The MT loop is run_stream's twin with inbox exchange and
 * no global batch check (the public stats live count is a
 * whole-allocator observable; main checks it once after the
 * join, when it must be exactly 0). Per-thread pattern and
 * overlap checks run every BATCH ops on the thread's own
 * model. Trim and snapshot stay lawful concurrent calls;
 * their in-stream global checks are substituted. */
static void mt_run(fz_ctx *c, model_t *m, stream_t *s,
                   long max_ops) {
  long i;
  for (i = 0; i < max_ops; i++) {
    uint8_t opb;
    c->op = i;
    if (!rd8(s, &opb)) break;
    switch (opb % 20) {
      case 0: op_malloc(c, m, s); break;
      case 1: op_calloc(c, m, s); break;
      case 2: op_realloc(c, m, s, 0); break;
      case 3: op_realloc(c, m, s, 1); break;
      case 4: op_aligned(c, m, s); break;
      case 5: op_posix(c, m, s); break;
      case 6: op_memalign(c, m, s); break;
      case 7: op_valloc(c, m, s, 0); break;
      case 8: op_valloc(c, m, s, 1); break;
      case 9: { int si = model_pick_live(m, s);
        m->ops[9]++;
        if (si < 0) op_malloc(c, m, s);
        else do_free_idx(c, m, si);
        break; }
      case 10: m->ops[10]++;
        mimir_free(NULL);
        CHECK(c, mimir_stats_snapshot(NULL) == -1, "noop",
              "stats_snapshot(NULL) not -1");
        break;
      case 11: { int si = model_pick_live(m, s);
        m->ops[11]++;
        if (si < 0) { op_malloc(c, m, s); break; }
        CHECK(c, verify_blk(&m->blk[si]), "write",
              "pattern corrupt before rewrite");
        m->blk[si].pat = (uint8_t)(m->blk[si].pat + 0x11u);
        fill_blk(&m->blk[si]);
        break; }
      case 12: { int si = model_pick_live(m, s);
        m->ops[12]++;
        if (si < 0) { op_malloc(c, m, s); break; }
        CHECK(c, mimir_usable_size(m->blk[si].p) ==
              m->blk[si].usable, "usable",
              "usable extent changed under no mutation");
        break; }
      case 13: { int r = mimir_trim(0);
        m->ops[13]++;
        CHECK(c, r == 0 || r == 1, "trim",
              "trim returned a value other than 0/1");
        break; }
      case 14: { struct mimir_stats_v1 st;
        m->ops[14]++;
        memset(&st, 0, sizeof st);
        CHECK(c, mimir_stats_snapshot(&st) == 0, "stats",
              "snapshot failed under concurrency");
        break; }
      case 15: m->ops[15]++;
        CHECK(c, mimir_malloc(0) == NULL, "zero",
              "malloc(0) not NULL");
        CHECK(c, mimir_calloc(0, 8) == NULL, "zero",
              "calloc(0,8) not NULL");
        CHECK(c, mimir_realloc(NULL, 0) == NULL, "zero",
              "realloc(NULL,0) not NULL");
        CHECK(c, mimir_valloc(0) == NULL, "zero",
              "valloc(0) not NULL");
        break;
      case 16: op_battery_badalign(c, m, s); break;
      case 17: m->ops[17]++;
        CHECK(c, mimir_malloc((size_t)-1) == NULL, "overflow",
              "malloc(SIZE_MAX) not NULL");
        CHECK(c, mimir_calloc(((size_t)-1) / 2 + 1, 2) == NULL,
              "overflow", "calloc overflow not NULL");
        break;
      case 18: op_xfer(c, m, s); break;
      default: m->ops[19]++; break; /* no global check in MT */
    }
    if (c->stop) break;
    if (i % BATCH == BATCH - 1) {
      int j, k = 0; void *ptrs[MAXLIVE];
      for (j = 0; j < MAXLIVE; j++) if (m->blk[j].live) {
        CHECK(c, verify_blk(&m->blk[j]), "pattern",
              "live block corrupt (MT)");
        ptrs[k++] = m->blk[j].p;
        if (c->stop) break;
      }
      if (c->stop) break;
      qsort(ptrs, (size_t)k, sizeof(void *), cmp_ptr);
      for (j = 1; j < k; j++) {
        size_t psz = 0; int t;
        for (t = 0; t < MAXLIVE; t++)
          if (m->blk[t].live && m->blk[t].p == ptrs[j - 1])
            psz = m->blk[t].sz;
        CHECK(c, (uintptr_t)ptrs[j - 1] + psz <=
              (uintptr_t)ptrs[j], "overlap",
              "live extents overlap (MT)");
        if (c->stop) break;
      }
      if (c->stop) break;
    }
    /* inbox exchange: every 401 ops post one, every 601 pop */
    if (i % 401 == 400) {
      int si = model_pick_live(m, s);
      if (si >= 0) {
        pthread_mutex_lock(&g_inbox.mu);
        if (g_inbox.n < INBOX_SLOTS) {
          blk_t *d = &g_inbox.blk[g_inbox.n++];
          *d = m->blk[si];
          m->blk[si].live = 0; m->blk[si].p = NULL;
        }
        pthread_mutex_unlock(&g_inbox.mu);
      }
    }
    if (i % 601 == 600) {
      blk_t got; int have = 0;
      memset(&got, 0, sizeof got);
      pthread_mutex_lock(&g_inbox.mu);
      if (g_inbox.n > 0) { got = g_inbox.blk[--g_inbox.n]; have = 1; }
      pthread_mutex_unlock(&g_inbox.mu);
      if (have) {
        CHECK(c, verify_blk(&got), "inbox",
              "inbox block corrupt on receipt");
        mimir_free(got.p);
      }
    }
    if (c->stop) break;
  }
  { int j;
    for (j = 0; j < MAXLIVE; j++)
      if (m->blk[j].live) do_free_idx(c, m, j);
  }
  CHECK(c, model_live_count(m) == 0, "final",
        "MT model not empty after drain");
}
static void *mt_thread(void *arg) {
  mt_arg_t *a = (mt_arg_t *)arg;
  size_t bytes = (size_t)a->ops * 10 + 64;
  uint8_t *buf = gen_stream(a->seed, bytes);
  fz_ctx c; model_t m; stream_t s;
  if (!buf) { a->rc = 1; return NULL; }
  memset(&c, 0, sizeof c);
  memset(&m, 0, sizeof m);
  c.seed = a->seed; c.mt = 1; c.id = a->id;
  s.buf = buf; s.len = bytes; s.pos = 0;
  mt_run(&c, &m, &s, a->ops);
  print_seed_line(&c, &m, "MT", c.op + 1);
  a->rc = c.fails ? 1 : 0;
  free(buf);
  return NULL;
}
static long run_mt(uint64_t seed0, long ops) {
  pthread_t th[MT_THREADS];
  mt_arg_t args[MT_THREADS];
  int i; long rc = 0;
  memset(&g_inbox, 0, sizeof g_inbox);
  pthread_mutex_init(&g_inbox.mu, NULL);
  for (i = 0; i < MT_THREADS; i++) {
    args[i].id = i + 1;
    args[i].seed = seed0 + (uint64_t)i * 7919u;
    args[i].ops = ops; args[i].rc = 0;
    if (pthread_create(&th[i], NULL, mt_thread, &args[i]) != 0) {
      printf("FINDING mt pthread_create failed\n");
      return 1;
    }
  }
  for (i = 0; i < MT_THREADS; i++) {
    pthread_join(th[i], NULL);
    rc |= args[i].rc;
  }
  /* main adopts and frees whatever the inbox still holds */
  { fz_ctx c; int j;
    memset(&c, 0, sizeof c); c.seed = seed0; c.id = 0;
    for (j = 0; j < g_inbox.n; j++) {
      CHECK(&c, verify_blk(&g_inbox.blk[j]), "inbox",
            "leftover inbox block corrupt");
      mimir_free(g_inbox.blk[j].p);
    }
    printf("MT-INBOX-LEFTOVER %d checks=%llu fails=%llu\n",
           g_inbox.n, (unsigned long long)c.checks,
           (unsigned long long)c.fails);
    rc |= c.fails ? 1 : 0;
  }
  (void)mimir_trim(0);
  { struct mimir_stats_v1 st;
    memset(&st, 0, sizeof st);
    if (mimir_stats_snapshot(&st) != 0 ||
        st.live_small_objects + st.live_mappings != 0) {
      printf("FINDING mt final live count not zero\n");
      rc = 1;
    } else {
      printf("MT-FINAL live=0 PASS\n");
    }
  }
  pthread_mutex_destroy(&g_inbox.mu);
  return rc;
}

/* ---------------- corpus mode ---------------- */
static long run_corpus_file(const char *path, uint64_t seed) {
  FILE *f = fopen(path, "rb");
  uint8_t *buf; long n; fz_ctx c; model_t m; stream_t s;
  if (!f) { printf("FINDING corpus cannot open %s\n", path);
    return 1; }
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  buf = (uint8_t *)malloc(n > 0 ? (size_t)n : 1);
  if (!buf) { fclose(f); return 1; }
  if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    fclose(f); free(buf); return 1;
  }
  fclose(f);
  memset(&c, 0, sizeof c);
  c.seed = seed; c.id = 0;
  s.buf = buf; s.len = (size_t)(n > 0 ? n : 0); s.pos = 0;
  run_stream(&c, &m, &s, 100000);
  printf("CORPUS %s bytes=%ld", path, n);
  printf(" checks=%llu fails=%llu live_end=%d\n",
         (unsigned long long)c.checks,
         (unsigned long long)c.fails, model_live_count(&m));
  fflush(stdout);
  free(buf);
  return c.fails ? 1 : 0;
}

#endif /* Z109_LIBFUZZER drivers */
/* ---------------- libFuzzer entry ---------------- */
#ifdef Z109_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fz_ctx c; model_t m; stream_t s;
  memset(&c, 0, sizeof c);
  c.seed = 0; c.id = 0;
  s.buf = data; s.len = size; s.pos = 0;
  run_stream(&c, &m, &s, 500);
  return 0;
}
#endif

/* ---------------- main ---------------- */
#ifndef Z109_LIBFUZZER
int main(int argc, char **argv) {
  long rc = 0;
  printf("FUZZ-Z109 harness v1 (trace version 1) "
         "libFuzzer-style, full public surface"
#ifdef Z109_INJECT
         " INJECT build (mmap-refusal armed)"
#endif
         "\n");
  if (argc >= 4 && strcmp(argv[1], "seed") == 0) {
    rc = run_seed(strtoull(argv[2], NULL, 0), atol(argv[3]),
                  0, 0);
    printf("RESULT: %s\n", rc ? "FAIL" : "PASS");
    return (int)rc;
  }
  if (argc >= 4 && strcmp(argv[1], "mt") == 0) {
    rc = run_mt(strtoull(argv[2], NULL, 0), atol(argv[3]));
    printf("RESULT: %s\n", rc ? "FAIL" : "PASS");
    return (int)rc;
  }
  if (argc >= 3 && strcmp(argv[1], "corpus") == 0) {
    rc = run_corpus_file(argv[2], 0);
    printf("RESULT: %s\n", rc ? "FAIL" : "PASS");
    return (int)rc;
  }
  if (argc >= 5 && strcmp(argv[1], "gen") == 0) {
    /* gen <seed> <bytes> <path>: write a corpus file */
    uint64_t seed = strtoull(argv[2], NULL, 0);
    size_t bytes = (size_t)atol(argv[3]);
    uint8_t *buf = gen_stream(seed, bytes);
    FILE *f = fopen(argv[4], "wb");
    if (!buf || !f) return 1;
    fwrite(buf, 1, bytes, f);
    fclose(f); free(buf);
    return 0;
  }
  fprintf(stderr,
    "usage: %s seed <seed> <ops> | mt <seed0> <ops-per-thread>"
    " | corpus <file> | gen <seed> <bytes> <path>\n", argv[0]);
  return 2;
}
#endif
