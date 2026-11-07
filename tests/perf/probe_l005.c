/* probe_l005.c - L-005 Phase 12 tokenizer + KV cache profile.
 *
 * TWO DIFFERENT THINGS are profiled; the doc keeps them apart:
 *
 * (T) TOKENIZER - MEASURED over real data. The real TinyLlama
 *   GGUF is mmap'd (class W, the L-004 route) and its
 *   tokenizer.ggml.* metadata is parsed THROUGH the mapping.
 *   The parsed structures a tokenizer holds in memory (token
 *   string blob, id array, score array, type array - the
 *   RuneWeaver fields of Aesir loader/tokenizer.mojo: vocab
 *   List[String], token_to_id Dict, scores List[Float32],
 *   token_types List[Int]) are then built twice: once via
 *   mimir_* (C1, counter-gated) and once via malloc (C2, the
 *   stdlib route Aesir's own tokenizer heap uses, per L-003;
 *   counter-invariance-gated). The Aesir component's own
 *   tokenizer leg runs separately in run_l005.sh (real Mojo
 *   code, child peak RSS, C2) - it is NOT this harness.
 *
 * (K) KV CACHE - MODELLED SHAPE, harness-measured. Aesir's
 *   engine cannot run on this Pi (INC-L003-01/02), so NO
 *   figure here is Aesir running. The harness reproduces the
 *   allocation SHAPE described in Aesir source
 *   (/home/pi/c2/aesir_engine/core/mimir_well.mojo):
 *     KVCache (line 824): ONE allocation each for K and V of
 *       num_layers * max_seq_len * hidden_dim f16 elements,
 *       drawn from the Well (stdlib route, C2 in Aesir).
 *     PagedKVCache (line 962): block_size tokens per page
 *       (default 16); page_table / block_owner /
 *       block_logical_index List[Int] of
 *       max_sequences * ceil(max_seq_len/block_size);
 *       sequence_lengths[max_sequences];
 *       layer_lengths[max_sequences*num_layers]; physical
 *       pool = physical_blocks * block_size tokens backing
 *       the base KVCache. Concrete source test instance:
 *       tests/test_kv_cache.mojo PagedKVCache(8,4,well,2,2,
 *       2,3) - 2-token pages, 3 physical blocks, 2 seqs.
 *   Here: TinyLlama dims parsed from the GGUF, 1 sequence,
 *   physical_blocks=64, prefill 256 tokens then 64 decode
 *   steps (positions appended in order, pages touched in
 *   logical order, as append() does). Run in a C2 (malloc)
 *   leg and a C1 (mimir_*) leg so the profile shows what
 *   MimirAlloc WOULD carry under explicit adoption.
 *
 * Classes per L-002: one class per byte (W/C1/C2/G/R);
 * snapshots only at quiescent points; rollup is a cross-
 * check, never a term. G=0: no device backend runs.
 * Exit 0 iff every GATE prints ok.
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

static const char *g_model;
static int g_failures;
static char g_smaps_buf[2u * 1024u * 1024u];
static char g_rollup_buf[64u * 1024u];

static ssize_t read_whole(const char *path, char *buf, size_t cap)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    size_t used = 0;
    for (;;) {
        if (used + 1 >= cap) break;
        ssize_t r = read(fd, buf + used, cap - used - 1);
        if (r < 0) { close(fd); return -1; }
        if (r == 0) break;
        used += (size_t)r;
    }
    close(fd);
    buf[used] = '\0';
    return (ssize_t)used;
}

struct class_sums { uint64_t size_kb, rss_kb, pss_kb; };
enum cls { CLS_W, CLS_ANON, CLS_RFILE };

static enum cls classify_header(const char *line)
{
    const char *p = line;
    for (int tok = 0; tok < 5; tok++) {
        while (*p == ' ' || *p == '\t') p++;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    while (*p == ' ' || *p == '\t') p++;
    size_t len = strcspn(p, "\n");
    if (len == 0 || p[0] == '[') return CLS_ANON;
    if (strlen(g_model) == len && strncmp(p, g_model, len) == 0)
        return CLS_W;
    return CLS_RFILE;
}

static void snapshot(const char *label, struct mimir_stats *st,
                     struct class_sums out[3])
{
    struct class_sums sums[3] = { {0,0,0}, {0,0,0}, {0,0,0} };
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
                if (strcmp(key, "Size") == 0) sums[cur].size_kb += val;
                else if (strcmp(key, "Rss") == 0) sums[cur].rss_kb += val;
                else if (strcmp(key, "Pss") == 0) sums[cur].pss_kb += val;
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
                if (strcmp(key, "Rss") == 0) r_rss = val;
                else if (strcmp(key, "Pss") == 0) r_pss = val;
            }
        }
    }
    if (mimir_core_stats_snapshot(st) != 0) {
        printf("SNAPSHOT %s: stats snapshot FAILED\n", label);
        g_failures++;
        memset(st, 0, sizeof *st);
    }
    memcpy(out, sums, sizeof sums);
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
    if (!ok) g_failures++;
}

/* ---------- GGUF walking (through the mapping: class W) --- */
static const uint8_t *g_base;
static uint64_t g_fsize;

static int rd_bytes(uint64_t off, void *dst, uint64_t len)
{
    if (off > g_fsize || len > g_fsize - off) return -1;
    memcpy(dst, g_base + off, (size_t)len);
    return 0;
}
static int rd_u32(uint64_t off, uint32_t *v) { return rd_bytes(off, v, 4); }
static int rd_u64(uint64_t off, uint64_t *v) { return rd_bytes(off, v, 8); }

static uint64_t gguf_type_size(uint32_t t)
{
    switch (t) {
    case 0: case 1: case 7: return 1;
    case 2: case 3: return 2;
    case 4: case 5: case 6: return 4;
    case 10: case 11: case 12: return 8;
    default: return 0;
    }
}

/* Tokenizer extraction state (offsets into the mapping). */
#define MAX_VOCAB 200000u
static uint64_t *g_tok_off;   /* per-token string data offset */
static uint32_t *g_tok_len;   /* per-token string length */
static uint64_t g_vocab = 0;
static uint64_t g_tok_str_bytes = 0;  /* sum of token string lens */
static uint64_t g_scores_off = 0, g_scores_n = 0;
static uint64_t g_types_off = 0, g_types_n = 0;
static uint32_t g_layers = 0, g_hidden = 0, g_ctx = 0;
static char g_tok_model[64] = "";

static int walk_scalar_or_array(uint32_t t, uint64_t *cur)
{
    if (t == 8) {
        uint64_t len;
        if (rd_u64(*cur, &len) != 0 || len > (1u << 26)) return -1;
        *cur += 8;
        if (*cur > g_fsize || len > g_fsize - *cur) return -1;
        *cur += len;
        return 0;
    }
    if (t == 9) {
        uint32_t et;
        uint64_t n;
        if (rd_u32(*cur, &et) != 0) return -1;
        *cur += 4;
        if (rd_u64(*cur, &n) != 0 || n > (1u << 30)) return -1;
        *cur += 8;
        if (et == 8) {
            for (uint64_t i = 0; i < n; i++)
                if (walk_scalar_or_array(8, cur) != 0) return -1;
            return 0;
        }
        uint64_t es = gguf_type_size(et);
        if (es == 0 || n > (g_fsize - *cur) / es) return -1;
        *cur += n * es;
        return 0;
    }
    uint64_t sz = gguf_type_size(t);
    if (sz == 0 || *cur > g_fsize || sz > g_fsize - *cur) return -1;
    *cur += sz;
    return 0;
}

/* Walk the KV table; extract model dims + tokenizer arrays. */
static int walk_kv(uint64_t kv_count)
{
    uint64_t cur = 24;
    for (uint64_t i = 0; i < kv_count; i++) {
        uint64_t klen;
        if (rd_u64(cur, &klen) != 0 || klen == 0 || klen > 127) return -1;
        cur += 8;
        if (cur > g_fsize || klen > g_fsize - cur) return -1;
        char key[128];
        memcpy(key, g_base + cur, (size_t)klen);
        key[klen] = '\0';
        cur += klen;
        uint32_t vt;
        if (rd_u32(cur, &vt) != 0) return -1;
        cur += 4;
        if (strcmp(key, "llama.block_count") == 0 && vt == 4)
            rd_u32(cur, &g_layers);
        else if (strcmp(key, "llama.embedding_length") == 0 && vt == 4)
            rd_u32(cur, &g_hidden);
        else if (strcmp(key, "llama.context_length") == 0 && vt == 4)
            rd_u32(cur, &g_ctx);
        else if (strcmp(key, "tokenizer.ggml.model") == 0 && vt == 8) {
            uint64_t len;
            if (rd_u64(cur, &len) == 0 && len < sizeof g_tok_model) {
                memcpy(g_tok_model, g_base + cur + 8, (size_t)len);
                g_tok_model[len] = '\0';
            }
        } else if (strcmp(key, "tokenizer.ggml.tokens") == 0 && vt == 9) {
            uint32_t et; uint64_t n, p;
            if (rd_u32(cur, &et) != 0 || et != 8) return -1;
            if (rd_u64(cur + 4, &n) != 0 || n == 0 || n > MAX_VOCAB) return -1;
            p = cur + 12;
            g_vocab = n;
            for (uint64_t j = 0; j < n; j++) {
                uint64_t len;
                if (rd_u64(p, &len) != 0 || len > (1u << 20)) return -1;
                p += 8;
                if (p > g_fsize || len > g_fsize - p) return -1;
                g_tok_off[j] = p;
                g_tok_len[j] = (uint32_t)len;
                g_tok_str_bytes += len;
                p += len;
            }
            cur = p;
            continue;
        } else if (strcmp(key, "tokenizer.ggml.scores") == 0 && vt == 9) {
            uint32_t et;
            if (rd_u32(cur, &et) != 0 || et != 6) return -1;
            if (rd_u64(cur + 4, &g_scores_n) != 0) return -1;
            g_scores_off = cur + 12;
        } else if (strcmp(key, "tokenizer.ggml.token_type") == 0 && vt == 9) {
            uint32_t et;
            if (rd_u32(cur, &et) != 0 || et != 5) return -1;
            if (rd_u64(cur + 4, &g_types_n) != 0) return -1;
            g_types_off = cur + 12;
        }
        if (walk_scalar_or_array(vt, &cur) != 0) return -1;
    }
    return 0;
}

/* ---------- parsed tokenizer structures (T legs) ---------- */
struct tok_structs {
    uint8_t *blob;    /* token strings, NUL-separated copies */
    int32_t *ids;     /* token_to_id values (identity here) */
    float *scores;    /* copied from the mapping */
    int32_t *types;   /* copied from the mapping */
    uint64_t blob_bytes, ids_bytes, scores_bytes, types_bytes;
    uint64_t total;
    uint64_t checksum;
};

static void tok_fill(struct tok_structs *t)
{
    uint64_t pos = 0;
    uint64_t sum = 0;
    for (uint64_t j = 0; j < g_vocab; j++) {
        memcpy(t->blob + pos, g_base + g_tok_off[j], g_tok_len[j]);
        pos += g_tok_len[j];
        t->blob[pos++] = '\0';
        t->ids[j] = (int32_t)j;
        sum += (uint8_t)t->blob[pos - 2 < pos ? pos - 1 : pos];
    }
    memcpy(t->scores, g_base + g_scores_off,
           (size_t)(g_scores_n * 4));
    memcpy(t->types, g_base + g_types_off,
           (size_t)(g_types_n * 4));
    for (uint64_t j = 0; j < g_scores_n; j++) {
        uint32_t u;
        memcpy(&u, &t->scores[j], 4);
        sum += u & 0xff;
    }
    for (uint64_t j = 0; j < g_types_n; j++)
        sum += (uint32_t)t->types[j];
    t->checksum = sum;
}

/* ---------- KV modelled shape (K legs) ---------- */
#define KV_BLOCK 16u
#define KV_PHYS_BLOCKS 64u
#define KV_PREFILL 256u
#define KV_DECODE 64u

struct kv_shape {
    uint64_t layers, hidden, ctx;
    uint64_t phys_tokens;      /* KV_PHYS_BLOCKS * KV_BLOCK */
    uint64_t pool_bytes;       /* ONE of K or V */
    uint64_t meta_bytes;       /* page tables + lengths */
    uint64_t total;            /* 2*pool + meta */
};

static void kv_describe(struct kv_shape *s)
{
    s->layers = g_layers;
    s->hidden = g_hidden;
    s->ctx = g_ctx;
    s->phys_tokens = KV_PHYS_BLOCKS * KV_BLOCK;
    s->pool_bytes = s->layers * s->phys_tokens * s->hidden * 2;
    uint64_t logical = (s->ctx + KV_BLOCK - 1) / KV_BLOCK;
    s->meta_bytes = 3 * logical * 8 + 8 + s->layers * 8;
    s->total = 2 * s->pool_bytes + s->meta_bytes;
}

/* Touch (write+read back) the rows for positions [p0,p1) in
 * every layer, one byte per 4096-byte page of each row, in
 * logical order - the append() access shape. */
static uint64_t kv_touch(uint8_t *pool, const struct kv_shape *s,
                         uint64_t p0, uint64_t p1)
{
    uint64_t row_bytes = s->hidden * 2;
    uint64_t sum = 0;
    for (uint64_t layer = 0; layer < s->layers; layer++) {
        for (uint64_t pos = p0; pos < p1; pos++) {
            uint8_t *row = pool + (layer * s->phys_tokens + pos)
                           * row_bytes;
            for (uint64_t off = 0; off < row_bytes; off += 4096) {
                row[off] = (uint8_t)(layer + pos);
                sum += row[off];
            }
        }
    }
    return sum;
}

int main(int argc, char **argv)
{
    if (argc != 2) { printf("usage: probe_l005 <model.gguf>\n"); return 2; }
    g_model = argv[1];
    printf("probe_l005 - L-005 tokenizer + KV cache profile harness\n");
    printf("model: %s\n", g_model);
    g_tok_off = calloc(MAX_VOCAB, sizeof(uint64_t));
    g_tok_len = calloc(MAX_VOCAB, sizeof(uint32_t));
    if (!g_tok_off || !g_tok_len) { printf("staging failed\n"); return 2; }

    struct mimir_stats st0, stA, stB, stC, stD, stE, stF, stZ;
    struct class_sums s_[3];
    snapshot("S0 baseline", &st0, s_);

    int fd = open(g_model, O_RDONLY);
    if (fd < 0) { printf("open FAILED\n"); return 1; }
    struct stat sb;
    if (fstat(fd, &sb) != 0) { printf("fstat FAILED\n"); return 1; }
    g_fsize = (uint64_t)sb.st_size;
    uint8_t *map = mmap(NULL, (size_t)g_fsize, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { printf("mmap FAILED\n"); return 1; }
    g_base = map;
    char magic[4];
    rd_bytes(0, magic, 4);
    gate(memcmp(magic, "GGUF", 4) == 0, "gguf_magic");
    uint64_t tensors = 0, kvs = 0;
    rd_u64(8, &tensors);
    rd_u64(16, &kvs);
    printf("GGUF tensors=%llu kv=%llu file_bytes=%llu\n",
           (unsigned long long)tensors, (unsigned long long)kvs,
           (unsigned long long)g_fsize);
    gate(walk_kv(kvs) == 0, "kv_walk_complete");
    printf("dims: layers=%u hidden=%u ctx=%u\n", g_layers, g_hidden, g_ctx);
    gate(g_layers > 0 && g_hidden > 0 && g_ctx > 0, "model_dims_parsed");
    printf("tokenizer: model=%s vocab=%llu token_str_bytes=%llu"
           " scores=%llu types=%llu\n", g_tok_model,
           (unsigned long long)g_vocab,
           (unsigned long long)g_tok_str_bytes,
           (unsigned long long)g_scores_n,
           (unsigned long long)g_types_n);
    gate(g_vocab > 0 && g_scores_n == g_vocab && g_types_n == g_vocab,
         "tokenizer_arrays_complete");
    snapshot("S1 after mmap + tokenizer parse (views stay W)", &stA, s_);
    gate(stA.bytes_requested == st0.bytes_requested
         && stA.bytes_mapped == st0.bytes_mapped,
         "parse_moves_no_allocator_counter");

    /* --- T-C1 leg: parsed structures via mimir_* --- */
    struct tok_structs tc1;
    memset(&tc1, 0, sizeof tc1);
    tc1.blob_bytes = g_tok_str_bytes + g_vocab;
    tc1.ids_bytes = g_vocab * 4;
    tc1.scores_bytes = g_scores_n * 4;
    tc1.types_bytes = g_types_n * 4;
    tc1.total = tc1.blob_bytes + tc1.ids_bytes + tc1.scores_bytes
              + tc1.types_bytes;
    tc1.blob = mimir_malloc(tc1.blob_bytes);
    tc1.ids = mimir_malloc(tc1.ids_bytes);
    tc1.scores = mimir_malloc(tc1.scores_bytes);
    tc1.types = mimir_malloc(tc1.types_bytes);
    if (!tc1.blob || !tc1.ids || !tc1.scores || !tc1.types) {
        printf("T-C1 alloc FAILED\n");
        return 1;
    }
    tok_fill(&tc1);
    snapshot("S2 after T-C1 (mimir_* tokenizer structures)", &stB, s_);
    printf("T-C1 total_requested=%llu checksum=%llu\n",
           (unsigned long long)tc1.total,
           (unsigned long long)tc1.checksum);
    gate(stB.bytes_requested - stA.bytes_requested == tc1.total,
         "T_C1_counter_delta_equals_requested");

    /* --- T-C2 leg: same structures via malloc (stdlib) --- */
    struct tok_structs tc2;
    memset(&tc2, 0, sizeof tc2);
    tc2.blob_bytes = tc1.blob_bytes;
    tc2.ids_bytes = tc1.ids_bytes;
    tc2.scores_bytes = tc1.scores_bytes;
    tc2.types_bytes = tc1.types_bytes;
    tc2.total = tc1.total;
    tc2.blob = malloc(tc2.blob_bytes);
    tc2.ids = malloc(tc2.ids_bytes);
    tc2.scores = malloc(tc2.scores_bytes);
    tc2.types = malloc(tc2.types_bytes);
    if (!tc2.blob || !tc2.ids || !tc2.scores || !tc2.types) {
        printf("T-C2 alloc FAILED\n");
        return 1;
    }
    tok_fill(&tc2);
    snapshot("S3 after T-C2 (malloc tokenizer structures)", &stC, s_);
    printf("T-C2 checksum=%llu\n", (unsigned long long)tc2.checksum);
    gate(tc2.checksum == tc1.checksum, "T_legs_checksums_equal");
    gate(stC.bytes_requested == stB.bytes_requested
         && stC.bytes_mapped == stB.bytes_mapped,
         "T_C2_moves_no_C1_counter");
    mimir_free(tc1.blob); mimir_free(tc1.ids);
    mimir_free(tc1.scores); mimir_free(tc1.types);
    free(tc2.blob); free(tc2.ids); free(tc2.scores); free(tc2.types);
    snapshot("S4 after tokenizer teardown", &stD, s_);
    gate(stD.live_small_objects == st0.live_small_objects
         && stD.live_mapping_bytes == st0.live_mapping_bytes,
         "T_teardown_returns_to_baseline");

    /* --- K legs: MODELLED paged-KV shape (see header) --- */
    struct kv_shape ks;
    kv_describe(&ks);
    printf("KV shape (modelled from mimir_well.mojo KVCache:824 +"
           " PagedKVCache:962): layers=%llu hidden=%llu ctx=%llu"
           " block=%u phys_blocks=%u phys_tokens=%llu"
           " pool_K_bytes=%llu pool_V_bytes=%llu meta_bytes=%llu"
           " total_bytes=%llu\n",
           (unsigned long long)ks.layers, (unsigned long long)ks.hidden,
           (unsigned long long)ks.ctx, KV_BLOCK, KV_PHYS_BLOCKS,
           (unsigned long long)ks.phys_tokens,
           (unsigned long long)ks.pool_bytes,
           (unsigned long long)ks.pool_bytes,
           (unsigned long long)ks.meta_bytes,
           (unsigned long long)ks.total);
    uint64_t logical_blocks = (ks.ctx + KV_BLOCK - 1) / KV_BLOCK;
    uint64_t prefill_blocks = (KV_PREFILL + KV_BLOCK - 1) / KV_BLOCK;
    uint64_t total_blocks = ((KV_PREFILL + KV_DECODE) + KV_BLOCK - 1)
                            / KV_BLOCK;
    printf("KV lifetime: prefill %u tokens (%llu blocks), decode %u"
           " steps (%llu blocks total of %u physical / %llu logical)\n",
           KV_PREFILL, (unsigned long long)prefill_blocks, KV_DECODE,
           (unsigned long long)total_blocks, KV_PHYS_BLOCKS,
           (unsigned long long)logical_blocks);

    /* K-C2 leg: stdlib route (the route Aesir's Well uses). */
    uint8_t *k2 = malloc(ks.pool_bytes);
    uint8_t *v2 = malloc(ks.pool_bytes);
    uint8_t *m2 = malloc(ks.meta_bytes);
    if (!k2 || !v2 || !m2) { printf("K-C2 alloc FAILED\n"); return 1; }
    memset(m2, 0, (size_t)ks.meta_bytes);
    snapshot("S5 K-C2 pools allocated (malloc, modelled shape)", &stE, s_);
    gate(stE.bytes_requested == stD.bytes_requested
         && stE.bytes_mapped == stD.bytes_mapped,
         "K_C2_moves_no_C1_counter");
    uint64_t sum_pre = kv_touch(k2, &ks, 0, KV_PREFILL)
                     + kv_touch(v2, &ks, 0, KV_PREFILL);
    uint64_t sum_dec = kv_touch(k2, &ks, KV_PREFILL,
                                KV_PREFILL + KV_DECODE)
                     + kv_touch(v2, &ks, KV_PREFILL,
                                KV_PREFILL + KV_DECODE);
    printf("K-C2 touch checksums: prefill=%llu decode=%llu\n",
           (unsigned long long)sum_pre, (unsigned long long)sum_dec);
    gate(sum_pre > 0 && sum_dec > 0, "K_C2_touch_live");
    snapshot("S6 K-C2 after prefill+decode touches", &stE, s_);
    free(k2); free(v2); free(m2);

    /* K-C1 leg: explicit mimir_* adoption of the same shape. */
    uint8_t *k1 = mimir_malloc(ks.pool_bytes);
    uint8_t *v1 = mimir_malloc(ks.pool_bytes);
    uint8_t *m1 = mimir_malloc(ks.meta_bytes);
    if (!k1 || !v1 || !m1) { printf("K-C1 alloc FAILED\n"); return 1; }
    memset(m1, 0, (size_t)ks.meta_bytes);
    uint64_t csum1 = kv_touch(k1, &ks, 0, KV_PREFILL + KV_DECODE)
                   + kv_touch(v1, &ks, 0, KV_PREFILL + KV_DECODE);
    printf("K-C1 touch checksum (prefill+decode combined)=%llu\n",
           (unsigned long long)csum1);
    gate(csum1 == sum_pre + sum_dec, "K_legs_touch_checksums_equal");
    snapshot("S7 K-C1 pools live (mimir_*, modelled shape)", &stF, s_);
    gate(stF.bytes_requested - stD.bytes_requested == ks.total,
         "K_C1_counter_delta_equals_shape_total");
    mimir_free(k1); mimir_free(v1); mimir_free(m1);
    snapshot("S8 after KV teardown", &stZ, s_);
    gate(stZ.live_mapping_bytes == st0.live_mapping_bytes
         && stZ.live_small_objects == st0.live_small_objects,
         "K_teardown_returns_to_baseline");
    munmap(map, (size_t)g_fsize);
    close(fd);
    printf("L005 RESULT: %s (failures=%d)\n",
           g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
