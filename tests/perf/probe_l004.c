/* probe_l004.c - L-004 Phase 12 model-loading + GGUF metadata
 * allocation tracking harness.
 *
 * Purpose: track, around a runnable workload, the L-002 class
 * separation (docs/PHASE_12_MEMORY_SEPARATION_TAXONOMY.md) at
 * the load + metadata phases. Every tracked byte lands in
 * exactly one class (W / C1 / C2 / G / R), snapshots are taken
 * only at quiescent points (single-threaded, no allocation in
 * flight), and cross-check totals (rollup Rss/Pss) are checks,
 * never a fourth term (L-002 S1, S2.1, S5.3, S5.4).
 *
 * Workload (this harness's own, defined here BEFORE the run,
 * per L-002 S5.1; it is NOT Aesir's loader - that path cannot
 * run on this Pi, see the slice doc INC-L003-01/02):
 *   Phase A (load): mmap the real TinyLlama GGUF read-only,
 *     shared - the loader route for weights. The mapping is
 *     class W. Residency is faulted by an explicit, recorded
 *     touch policy (every page of the first 32 MiB; one page
 *     per 256 KiB beyond). mmap residency is NOT allocation:
 *     no allocator counter moves in this phase, and the S1/S2
 *     counter block proves it.
 *   Phase B (metadata): walk the GGUF metadata KV table and
 *     the tensor-info table THROUGH the mapping (viewed bytes
 *     stay W, L-002 S1.1 / trap DC-7). Then build the parsed
 *     metadata records twice, once per route:
 *       C1 step: records + a 1 MiB scratch via mimir_malloc /
 *         mimir_calloc (explicit mimir_* adoption). The Phase
 *         10 counters (mimir_core_stats_snapshot) must move by
 *         exactly the harness's requested sum.
 *       C2 step: the same keys + a 1 MiB scratch via plain
 *         malloc (the stdlib route - the route Aesir's own
 *         metadata/tokenizer heap uses, per L-003). The C1
 *         counters must NOT move; the growth is visible only
 *         in /proc anonymous evidence.
 *   Phase C (teardown): free both routes, munmap. W must
 *     return to zero; C1 live objects/mappings must return
 *     to the S0 baseline (retained empty spans may persist:
 *     bytes_cached is reported, per the G-005 posture).
 *
 * Classification rule (L-002 S2.2, applied mechanically to
 * /proc/self/smaps read raw into a static buffer - the reader
 * allocates nothing on the measured heap, L-002 S5.3):
 *   pathname == model path           -> W
 *   pathname empty or '['-label      -> ANON bucket (printed;
 *     split into C1/C2/R only by route evidence: the counter
 *     block for C1, counter-invariance + known code path for
 *     the C2 step, remainder is R and is printed, S5.6)
 *   any other file pathname          -> R (file)
 * G: no device backend exists in this workload; no /dev/dri
 *   or dma-buf mapping is expected and any such mapping would
 *   be printed on the R (file) line for the doc to name. G is
 *   therefore 0 by absence of a backend, stated, not assumed.
 *
 * Exit 0 iff every GATE line prints ok. Gates are the
 * deterministic ones only (magic, walk counts, counter
 * identities, teardown returns); Rss movements are printed
 * as evidence, not gated, because page residency is the
 * kernel's choice, not the allocator's.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mimir_alloc.h"
#include "../core/mimir_stats_j002.h"

#define TOUCH_FULL_BYTES (32u * 1024u * 1024u)
#define TOUCH_STRIDE     (256u * 1024u)
#define SCRATCH_BYTES    (1024u * 1024u)
#define MAX_KV           4096u

static const char *g_model;
static int g_failures;

/* ---------- smaps classification ---------- */

struct class_sums {
    uint64_t size_kb, rss_kb, pss_kb;
};

static char g_smaps_buf[2u * 1024u * 1024u];
static char g_rollup_buf[64u * 1024u];

static ssize_t read_whole(const char *path, char *buf, size_t cap)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    size_t used = 0;
    for (;;) {
        if (used + 1 >= cap)
            break;
        ssize_t r = read(fd, buf + used, cap - used - 1);
        if (r < 0) {
            close(fd);
            return -1;
        }
        if (r == 0)
            break;
        used += (size_t)r;
    }
    close(fd);
    buf[used] = '\0';
    return (ssize_t)used;
}

enum cls { CLS_W, CLS_ANON, CLS_RFILE };

/* Extract the pathname remainder of a smaps header line:
 * skip 5 whitespace-separated tokens; the rest (trimmed) is
 * the pathname, possibly empty. Returns cls for the entry. */
static enum cls classify_header(const char *line)
{
    const char *p = line;
    for (int tok = 0; tok < 5; tok++) {
        while (*p == ' ' || *p == '\t')
            p++;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n')
            p++;
    }
    while (*p == ' ' || *p == '\t')
        p++;
    size_t len = strcspn(p, "\n");
    if (len == 0 || p[0] == '[')
        return CLS_ANON;
    if (strlen(g_model) == len && strncmp(p, g_model, len) == 0)
        return CLS_W;
    return CLS_RFILE;
}

static void snapshot(const char *label, struct mimir_stats *st,
                     struct class_sums out[3], uint64_t *roll_rss,
                     uint64_t *roll_pss)
{
    struct class_sums sums[3] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };
    if (read_whole("/proc/self/smaps", g_smaps_buf,
                   sizeof g_smaps_buf) < 0) {
        printf("SNAPSHOT %s: smaps read FAILED\n", label);
        g_failures++;
    } else {
        enum cls cur = CLS_RFILE;
        char *save = NULL;
        for (char *line = strtok_r(g_smaps_buf, "\n", &save);
             line; line = strtok_r(NULL, "\n", &save)) {
            unsigned long long lo, hi;
            char perm[8];
            if (sscanf(line, "%llx-%llx %7s", &lo, &hi, perm) == 3
                && (perm[0] == 'r' || perm[0] == '-')) {
                cur = classify_header(line);
                continue;
            }
            char key[32];
            unsigned long long val;
            if (sscanf(line, "%31[^:]: %llu", key, &val) == 2) {
                if (strcmp(key, "Size") == 0)
                    sums[cur].size_kb += val;
                else if (strcmp(key, "Rss") == 0)
                    sums[cur].rss_kb += val;
                else if (strcmp(key, "Pss") == 0)
                    sums[cur].pss_kb += val;
            }
        }
    }
    uint64_t r_rss = 0, r_pss = 0;
    if (read_whole("/proc/self/smaps_rollup", g_rollup_buf,
                   sizeof g_rollup_buf) >= 0) {
        char *save = NULL;
        for (char *line = strtok_r(g_rollup_buf, "\n", &save);
             line; line = strtok_r(NULL, "\n", &save)) {
            char key[32];
            unsigned long long val;
            if (sscanf(line, "%31[^:]: %llu", key, &val) == 2) {
                if (strcmp(key, "Rss") == 0)
                    r_rss = val;
                else if (strcmp(key, "Pss") == 0)
                    r_pss = val;
            }
        }
    }
    if (mimir_core_stats_snapshot(st) != 0) {
        printf("SNAPSHOT %s: stats snapshot FAILED\n", label);
        g_failures++;
        memset(st, 0, sizeof *st);
    }
    memcpy(out, sums, sizeof sums);
    *roll_rss = r_rss;
    *roll_pss = r_pss;
    printf("SNAPSHOT %s\n", label);
    printf("  W       Size=%llu Rss=%llu Pss=%llu kB\n",
           (unsigned long long)sums[CLS_W].size_kb,
           (unsigned long long)sums[CLS_W].rss_kb,
           (unsigned long long)sums[CLS_W].pss_kb);
    printf("  ANON    Size=%llu Rss=%llu Pss=%llu kB\n",
           (unsigned long long)sums[CLS_ANON].size_kb,
           (unsigned long long)sums[CLS_ANON].rss_kb,
           (unsigned long long)sums[CLS_ANON].pss_kb);
    printf("  R(file) Size=%llu Rss=%llu Pss=%llu kB\n",
           (unsigned long long)sums[CLS_RFILE].size_kb,
           (unsigned long long)sums[CLS_RFILE].rss_kb,
           (unsigned long long)sums[CLS_RFILE].pss_kb);
    printf("  rollup  Rss=%llu Pss=%llu kB (cross-check only)\n",
           (unsigned long long)r_rss, (unsigned long long)r_pss);
    printf("  C1 counters: requested=%llu allocated=%llu mapped=%llu"
           " live_map_bytes=%llu live_span_bytes=%llu live_small=%llu"
           " live_mappings=%llu cached=%llu released=%llu failures=%llu\n",
           (unsigned long long)st->bytes_requested,
           (unsigned long long)st->bytes_allocated,
           (unsigned long long)st->bytes_mapped,
           (unsigned long long)st->live_mapping_bytes,
           (unsigned long long)st->live_span_bytes,
           (unsigned long long)st->live_small_objects,
           (unsigned long long)st->live_mappings,
           (unsigned long long)st->bytes_cached,
           (unsigned long long)st->bytes_released,
           (unsigned long long)st->alloc_failures);
}

static void gate(int ok, const char *name)
{
    printf("GATE %s: %s\n", name, ok ? "ok" : "FAIL");
    if (!ok)
        g_failures++;
}

/* ---------- GGUF walking (through the mapping: class W) --- */

static const uint8_t *g_base;
static uint64_t g_fsize;

static int rd_bytes(uint64_t off, void *dst, uint64_t len)
{
    if (off > g_fsize || len > g_fsize - off)
        return -1;
    memcpy(dst, g_base + off, (size_t)len);
    return 0;
}
static int rd_u32(uint64_t off, uint32_t *v)
{
    return rd_bytes(off, v, 4);
}
static int rd_u64(uint64_t off, uint64_t *v)
{
    return rd_bytes(off, v, 8);
}

static uint64_t gguf_type_size(uint32_t t)
{
    switch (t) {
    case 0: case 1: case 7: return 1;   /* u8,i8,bool */
    case 2: case 3: return 2;           /* u16,i16 */
    case 4: case 5: case 6: return 4;   /* u32,i32,f32 */
    case 10: case 11: case 12: return 8; /* u64,i64,f64 */
    default: return 0;
    }
}

/* Walk one value of type t at *cur; advance *cur past it.
 * If want_str and t is STRING, copy into dst (truncated). */
static int walk_value(uint32_t t, uint64_t *cur, char *dst, size_t dstcap)
{
    if (t == 8) { /* STRING */
        uint64_t len;
        if (rd_u64(*cur, &len) != 0 || len > (1u << 26))
            return -1;
        *cur += 8;
        if (*cur > g_fsize || len > g_fsize - *cur)
            return -1;
        if (dst && dstcap) {
            size_t c = len < dstcap - 1 ? (size_t)len : dstcap - 1;
            memcpy(dst, g_base + *cur, c);
            dst[c] = '\0';
        }
        *cur += len;
        return 0;
    }
    if (t == 9) { /* ARRAY */
        uint32_t et;
        uint64_t n;
        if (rd_u32(*cur, &et) != 0)
            return -1;
        *cur += 4;
        if (rd_u64(*cur, &n) != 0 || n > (1u << 30))
            return -1;
        *cur += 8;
        if (et == 8) {
            for (uint64_t i = 0; i < n; i++)
                if (walk_value(8, cur, NULL, 0) != 0)
                    return -1;
            return 0;
        }
        uint64_t es = gguf_type_size(et);
        if (es == 0 || n > (g_fsize - *cur) / es)
            return -1;
        *cur += n * es;
        return 0;
    }
    uint64_t sz = gguf_type_size(t);
    if (sz == 0 || *cur > g_fsize || sz > g_fsize - *cur)
        return -1;
    *cur += sz;
    return 0;
}

/* ---------- main ---------- */

struct kv_stage {
    char key[128];
    uint32_t type;
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: probe_l004 <model.gguf>\n");
        return 2;
    }
    g_model = argv[1];
    printf("probe_l004 - L-004 load + metadata tracking harness\n");
    printf("model: %s\n", g_model);

    /* Staging lives BEFORE S0 so it is inside the baseline. */
    struct kv_stage *stage = calloc(MAX_KV, sizeof *stage);
    if (!stage) {
        printf("staging alloc failed\n");
        return 2;
    }

    struct mimir_stats st0, st1, st2, st3, st4, st6;
    struct class_sums s0[3], s1[3], s2[3], s3[3], s4[3], s6[3];
    uint64_t rr, rp;

    snapshot("S0 baseline (pre-load)", &st0, s0, &rr, &rp);

    /* --- Phase A: mmap load (W) --- */
    int fd = open(g_model, O_RDONLY);
    if (fd < 0) {
        printf("open model FAILED\n");
        return 1;
    }
    struct stat sb;
    if (fstat(fd, &sb) != 0) {
        printf("fstat FAILED\n");
        return 1;
    }
    g_fsize = (uint64_t)sb.st_size;
    uint8_t *map = mmap(NULL, (size_t)g_fsize, PROT_READ,
                        MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        printf("mmap FAILED\n");
        return 1;
    }
    g_base = map;
    char magic[4];
    if (rd_bytes(0, magic, 4) != 0) {
        printf("header read FAILED\n");
        return 1;
    }
    gate(memcmp(magic, "GGUF", 4) == 0, "gguf_magic");
    uint32_t version = 0, alignment = 32;
    uint64_t tensor_count = 0, kv_count = 0;
    rd_u32(4, &version);
    rd_u64(8, &tensor_count);
    rd_u64(16, &kv_count);
    printf("GGUF version=%u tensors=%llu kv=%llu file_bytes=%llu\n",
           version, (unsigned long long)tensor_count,
           (unsigned long long)kv_count,
           (unsigned long long)g_fsize);
    gate(kv_count > 0 && kv_count <= MAX_KV, "kv_count_sane");
    snapshot("S1 after mmap (W Size jumps, residency ~0)", &st1,
             s1, &rr, &rp);
    gate(s1[CLS_W].size_kb * 1024 >= g_fsize
         && s1[CLS_W].size_kb * 1024 < g_fsize + 1024 * 1024,
         "W_size_equals_file_at_S1");
    gate(st1.bytes_requested == st0.bytes_requested
         && st1.bytes_mapped == st0.bytes_mapped,
         "mmap_moves_no_allocator_counter");

    /* --- Phase B part 1: metadata walk through the mapping --- */
    uint64_t cur = 24;
    char arch[128] = "", gname[256] = "";
    uint64_t walked = 0;
    int walk_ok = 1;
    for (uint64_t i = 0; i < kv_count; i++) {
        uint64_t klen;
        if (rd_u64(cur, &klen) != 0 || klen == 0 || klen > 127) {
            walk_ok = 0;
            break;
        }
        cur += 8;
        if (cur > g_fsize || klen > g_fsize - cur) {
            walk_ok = 0;
            break;
        }
        memcpy(stage[i].key, g_base + cur, (size_t)klen);
        stage[i].key[klen] = '\0';
        cur += klen;
        uint32_t vt;
        if (rd_u32(cur, &vt) != 0) {
            walk_ok = 0;
            break;
        }
        cur += 4;
        stage[i].type = vt;
        char *dst = NULL;
        size_t dcap = 0;
        if (strcmp(stage[i].key, "general.architecture") == 0) {
            dst = arch;
            dcap = sizeof arch;
        } else if (strcmp(stage[i].key, "general.name") == 0) {
            dst = gname;
            dcap = sizeof gname;
        } else if (strcmp(stage[i].key, "general.alignment") == 0
                   && vt == 4) {
            rd_u32(cur, &alignment);
        }
        if (walk_value(vt, &cur, dst, dcap) != 0) {
            walk_ok = 0;
            break;
        }
        walked++;
    }
    gate(walk_ok && walked == kv_count, "metadata_kv_walk_complete");
    printf("metadata: architecture=%s name=%s alignment=%u\n",
           arch, gname, alignment);
    uint64_t tensors_walked = 0;
    int tw_ok = 1;
    for (uint64_t t = 0; t < tensor_count; t++) {
        uint64_t nlen;
        if (rd_u64(cur, &nlen) != 0 || nlen > (1u << 20)) {
            tw_ok = 0;
            break;
        }
        cur += 8 + nlen;
        uint32_t nd;
        if (rd_u32(cur, &nd) != 0 || nd > 16) {
            tw_ok = 0;
            break;
        }
        cur += 4 + (uint64_t)nd * 8 + 4 + 8; /* dims, type, offset */
        if (cur > g_fsize) {
            tw_ok = 0;
            break;
        }
        tensors_walked++;
    }
    gate(tw_ok && tensors_walked == tensor_count,
         "tensor_info_walk_complete");
    uint64_t data_start = (cur + alignment - 1) / alignment * alignment;
    printf("metadata_end=%llu data_start=%llu\n",
           (unsigned long long)cur,
           (unsigned long long)data_start);

    /* Touch policy (recorded): every page of the first 32 MiB,
     * one page per 256 KiB beyond. Faults W residency only. */
    volatile uint64_t sink = 0;
    for (uint64_t off = 0; off < g_fsize; ) {
        sink += g_base[off];
        off += (off < TOUCH_FULL_BYTES) ? 4096u : TOUCH_STRIDE;
    }
    printf("touch sink=%llu (policy: full 32 MiB head, 256 KiB"
           " stride beyond)\n", (unsigned long long)sink);
    snapshot("S2 after metadata walk + weight touch", &st2, s2,
             &rr, &rp);
    gate(st2.bytes_requested == st0.bytes_requested
         && st2.bytes_mapped == st0.bytes_mapped,
         "metadata_view_and_touch_move_no_counter");

    /* --- Phase B part 2 (C1): parsed records via mimir_* --- */
    void **c1_keys = calloc(kv_count, sizeof(void *));
    void *c1_recs = mimir_calloc(kv_count, 64);
    uint64_t c1_requested = kv_count * 64;
    if (!c1_recs)
        gate(0, "c1_records_alloc");
    for (uint64_t i = 0; i < kv_count; i++) {
        size_t kl = strlen(stage[i].key) + 1;
        c1_keys[i] = mimir_malloc(kl);
        if (c1_keys[i])
            memcpy(c1_keys[i], stage[i].key, kl);
        c1_requested += kl;
    }
    uint8_t *c1_scratch = mimir_malloc(SCRATCH_BYTES);
    if (c1_scratch) {
        for (uint64_t off = 0; off < SCRATCH_BYTES; off += 4096)
            c1_scratch[off] = 0xA5;
        c1_requested += SCRATCH_BYTES;
    } else {
        gate(0, "c1_scratch_alloc");
    }
    snapshot("S3 after C1 step (mimir_* metadata records)", &st3,
             s3, &rr, &rp);
    gate(st3.bytes_requested - st2.bytes_requested == c1_requested,
         "C1_counter_delta_equals_harness_requested_sum");

    /* --- Phase B part 3 (C2): same records via stdlib malloc --- */
    void **c2_keys = calloc(kv_count, sizeof(void *));
    for (uint64_t i = 0; i < kv_count; i++) {
        size_t kl = strlen(stage[i].key) + 1;
        c2_keys[i] = malloc(kl);
        if (c2_keys[i])
            memcpy(c2_keys[i], stage[i].key, kl);
    }
    uint8_t *c2_scratch = malloc(SCRATCH_BYTES);
    if (c2_scratch) {
        for (uint64_t off = 0; off < SCRATCH_BYTES; off += 4096)
            c2_scratch[off] = 0x5A;
    } else {
        gate(0, "c2_scratch_alloc");
    }
    snapshot("S4 after C2 step (stdlib metadata records)", &st4,
             s4, &rr, &rp);
    gate(st4.bytes_requested == st3.bytes_requested
         && st4.bytes_allocated == st3.bytes_allocated
         && st4.bytes_mapped == st3.bytes_mapped
         && st4.live_small_objects == st3.live_small_objects,
         "C2_step_moves_no_C1_counter");

    /* --- Phase C: teardown --- */
    free(c2_scratch);
    for (uint64_t i = 0; i < kv_count; i++)
        free(c2_keys[i]);
    free(c2_keys);
    mimir_free(c1_scratch);
    for (uint64_t i = 0; i < kv_count; i++)
        mimir_free(c1_keys[i]);
    free(c1_keys);
    mimir_free(c1_recs);
    munmap(map, (size_t)g_fsize);
    close(fd);
    snapshot("S6 after teardown", &st6, s6, &rr, &rp);
    gate(s6[CLS_W].size_kb == 0 && s6[CLS_W].rss_kb == 0,
         "W_returns_to_zero");
    gate(st6.live_small_objects == st0.live_small_objects
         && st6.live_mappings == st0.live_mappings,
         "C1_live_returns_to_baseline");
    printf("teardown: bytes_released delta=%llu (retained cached"
           " bytes at S6=%llu, G-005 posture, reported not gated)\n",
           (unsigned long long)(st6.bytes_released
                                - st0.bytes_released),
           (unsigned long long)st6.bytes_cached);
    free(stage);
    printf("L004 RESULT: %s (failures=%d)\n",
           g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
