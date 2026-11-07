/* probe_l006.c - L-006 Phase 12 prompt prefill / decode /
 * teardown allocation profile harness.
 *
 * Follows the L-002 S5 measurement protocol
 * (docs/PHASE_12_MEMORY_SEPARATION_TAXONOMY.md) and the L-004
 * harness pattern (tests/perf/probe_l004.c). Snapshots only at
 * quiescent points (single-threaded, no allocation in flight):
 * S0 baseline, S1 after mmap, S2 after metadata walk + weight
 * touch, S3 after KV allocation, S4 after prefill (C1 route),
 * S4C2 after prefill stdlib workspace (C2 route), S5 steady
 * decode (after a fixed 32 steps), S6 after teardown.
 *
 * Classes (L-002 S2.2, applied mechanically to
 * /proc/self/smaps read raw into a static buffer - the reader
 * allocates nothing on the measured heap, L-002 S5.3):
 * pathname == model path -> W; empty or bracket label -> ANON
 * bucket (split into C1/C2/R only by route evidence: the
 * Phase 10 counter block for C1, counter-invariance across a
 * known stdlib step for C2, remainder stays R and is printed,
 * S5.6); any other file pathname -> R (file). G: no device
 * backend exists in this workload, so G = 0 by absence of a
 * backend, stated, not assumed.
 *
 * Workload (this harness's own, defined BEFORE the run per
 * L-002 S5.1; it is an EXPLICIT-ADOPTION harness: every C1
 * byte is a mimir_* call made by this harness itself. It is
 * NOT Aesir prefill/decode - the full Aesir engine cannot
 * build or run on this Pi (INC-L003-01/02) and no Aesir
 * inference-path component builds natively (recorded in the
 * slice doc). The phase SHAPES are the L-001 S2.3 hypotheses
 * made concrete with sizes derived from the real GGUF's own
 * metadata: KV bytes/token = block_count x head_count_kv x
 * head_dim x 2 (K and V) x 4 (f32) = 22 x 4 x 64 x 2 x 4 =
 * 45,056 B, computed in code from the parsed dims and gated
 * against the parsed rope dimension count):
 *   KV allocation: one mimir_malloc slice per prompt token
 *     (32 slices), each fully touched - the KV cache the
 *     prefill phase fills.
 *   Prefill: prompt token-id buffer + prompt embedding
 *     buffer (P x embedding_length x 2 B, f16) + a burst of
 *     4 activation workspace buffers via mimir_* (C1); then
 *     a stdlib workspace + id copy via plain malloc (C2 -
 *     the route Aesir's own implicit heap takes, per L-003).
 *   Decode: 32 steps; each step appends ONE KV slice via
 *     mimir_malloc (KV growth, held), plus transient churn:
 *     one mimir_malloc scratch + one mimir_malloc logits
 *     buffer (vocab x 4 B, vocab parsed from the GGUF
 *     tokenizer token array), both touched and freed in the
 *     step, and one stdlib scratch alloc/free (C2 churn).
 *   Teardown: free every C1 and C2 block, munmap. W must
 *     return to zero; C1 live objects/mappings must return
 *     to the S0 baseline (retained empty spans are reported,
 *     per the G-005 posture, not gated).
 *
 * Exit 0 iff every GATE prints ok. Gates are deterministic
 * only (magic, walk counts, parsed dims, exact counter
 * deltas for each phase, teardown returns). Rss movements
 * are evidence, never gates: residency is the kernel's
 * choice, not the allocator's.
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
#define PROMPT_TOKENS    32u
#define DECODE_STEPS     32u
#define PREFILL_WS_COUNT 4u
#define PREFILL_WS_BYTES (256u * 1024u)
#define C2_WS_BYTES      (512u * 1024u)
#define STEP_SCRATCH     8192u
#define MAX_KV           4096u

static const char *g_model;
static int g_failures;

struct class_sums { uint64_t size_kb, rss_kb, pss_kb; };
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
    buf[used] = 0;
    return (ssize_t)used;
}

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
                     struct class_sums out[3], uint64_t *roll_rss,
                     uint64_t *roll_pss)
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
    if (!ok) g_failures++;
}

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

static int walk_value(uint32_t t, uint64_t *cur, char *dst, size_t dstcap)
{
    if (t == 8) {
        uint64_t len;
        if (rd_u64(*cur, &len) != 0 || len > (1u << 26)) return -1;
        *cur += 8;
        if (*cur > g_fsize || len > g_fsize - *cur) return -1;
        if (dst && dstcap) {
            size_t c = len < dstcap - 1 ? (size_t)len : dstcap - 1;
            memcpy(dst, g_base + *cur, c);
            dst[c] = 0;
        }
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
                if (walk_value(8, cur, NULL, 0) != 0) return -1;
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

static void touch_range(uint8_t *p, uint64_t n, uint8_t v)
{
    for (uint64_t off = 0; off < n; off += 4096) p[off] = v;
    if (n) p[n - 1] = v;
}

struct kv_stage { char key[128]; uint32_t type; };

int main(int argc, char **argv)
{
    if (argc != 2) { printf("usage: probe_l006 <model.gguf>\n"); return 2; }
    g_model = argv[1];
    printf("probe_l006 - L-006 prefill/decode/teardown profile harness\n");
    printf("model: %s\n", g_model);
    struct kv_stage *stage = calloc(MAX_KV, sizeof *stage);
    if (!stage) { printf("staging alloc failed\n"); return 2; }
    struct mimir_stats st0, st1, st2, st3, st4, st4c, st5, st6;
    struct class_sums s0[3], s1[3], s2[3], s3[3], s4[3], s4c[3], s5[3], s6[3];
    uint64_t rr, rp;
    snapshot("S0 baseline (pre-load)", &st0, s0, &rr, &rp);

    int fd = open(g_model, O_RDONLY);
    if (fd < 0) { printf("open model FAILED\n"); return 1; }
    struct stat sb;
    if (fstat(fd, &sb) != 0) { printf("fstat FAILED\n"); return 1; }
    g_fsize = (uint64_t)sb.st_size;
    uint8_t *map = mmap(NULL, (size_t)g_fsize, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { printf("mmap FAILED\n"); return 1; }
    g_base = map;
    char magic[4];
    if (rd_bytes(0, magic, 4) != 0) { printf("header read FAILED\n"); return 1; }
    gate(memcmp(magic, "GGUF", 4) == 0, "gguf_magic");
    uint32_t version = 0, alignment = 32;
    uint64_t tensor_count = 0, kv_count = 0;
    rd_u32(4, &version);
    rd_u64(8, &tensor_count);
    rd_u64(16, &kv_count);
    printf("GGUF version=%u tensors=%llu kv=%llu file_bytes=%llu\n",
           version, (unsigned long long)tensor_count,
           (unsigned long long)kv_count, (unsigned long long)g_fsize);
    gate(kv_count > 0 && kv_count <= MAX_KV, "kv_count_sane");
    snapshot("S1 after mmap (W Size jumps, residency ~0)", &st1, s1, &rr, &rp);
    gate(s1[CLS_W].size_kb * 1024 >= g_fsize
         && s1[CLS_W].size_kb * 1024 < g_fsize + 1024 * 1024,
         "W_size_equals_file_at_S1");
    gate(st1.bytes_requested == st0.bytes_requested
         && st1.bytes_mapped == st0.bytes_mapped,
         "mmap_moves_no_allocator_counter");

    uint64_t cur = 24;
    char arch[128] = "", gname[256] = "";
    uint32_t d_layers = 0, d_embd = 0, d_heads = 0, d_kvheads = 0;
    uint32_t d_rope = 0, d_ctx = 0;
    uint64_t vocab = 0, walked = 0;
    int walk_ok = 1;
    for (uint64_t i = 0; i < kv_count; i++) {
        uint64_t klen;
        if (rd_u64(cur, &klen) != 0 || klen == 0 || klen > 127) { walk_ok = 0; break; }
        cur += 8;
        if (cur > g_fsize || klen > g_fsize - cur) { walk_ok = 0; break; }
        memcpy(stage[i].key, g_base + cur, (size_t)klen);
        stage[i].key[klen] = 0;
        cur += klen;
        uint32_t vt;
        if (rd_u32(cur, &vt) != 0) { walk_ok = 0; break; }
        cur += 4;
        stage[i].type = vt;
        uint32_t u32v = 0;
        if (vt == 4) rd_u32(cur, &u32v);
        if (vt == 9) {
            uint32_t et = 0; uint64_t n = 0;
            rd_u32(cur, &et); rd_u64(cur + 4, &n);
            if (et == 8 && strcmp(stage[i].key, "tokenizer.ggml.tokens") == 0)
                vocab = n;
        }
        if (strcmp(stage[i].key, "llama.block_count") == 0) d_layers = u32v;
        else if (strcmp(stage[i].key, "llama.embedding_length") == 0) d_embd = u32v;
        else if (strcmp(stage[i].key, "llama.attention.head_count") == 0) d_heads = u32v;
        else if (strcmp(stage[i].key, "llama.attention.head_count_kv") == 0) d_kvheads = u32v;
        else if (strcmp(stage[i].key, "llama.rope.dimension_count") == 0) d_rope = u32v;
        else if (strcmp(stage[i].key, "llama.context_length") == 0) d_ctx = u32v;
        char *dst = NULL; size_t dcap = 0;
        if (strcmp(stage[i].key, "general.architecture") == 0) { dst = arch; dcap = sizeof arch; }
        else if (strcmp(stage[i].key, "general.name") == 0) { dst = gname; dcap = sizeof gname; }
        else if (strcmp(stage[i].key, "general.alignment") == 0 && vt == 4) alignment = u32v;
        if (walk_value(vt, &cur, dst, dcap) != 0) { walk_ok = 0; break; }
        walked++;
    }
    gate(walk_ok && walked == kv_count, "metadata_kv_walk_complete");
    printf("metadata: architecture=%s name=%s alignment=%u\n", arch, gname, alignment);
    uint64_t tensors_walked = 0;
    int tw_ok = 1;
    for (uint64_t t = 0; t < tensor_count; t++) {
        uint64_t nlen;
        if (rd_u64(cur, &nlen) != 0 || nlen > (1u << 20)) { tw_ok = 0; break; }
        cur += 8 + nlen;
        uint32_t nd;
        if (rd_u32(cur, &nd) != 0 || nd > 16) { tw_ok = 0; break; }
        cur += 4 + (uint64_t)nd * 8 + 4 + 8;
        if (cur > g_fsize) { tw_ok = 0; break; }
        tensors_walked++;
    }
    gate(tw_ok && tensors_walked == tensor_count, "tensor_info_walk_complete");
    uint64_t data_start = (cur + alignment - 1) / alignment * alignment;
    printf("metadata_end=%llu data_start=%llu\n",
           (unsigned long long)cur, (unsigned long long)data_start);
    gate(d_layers > 0 && d_embd > 0 && d_heads > 0 && d_kvheads > 0
         && d_rope == d_embd / d_heads && vocab > 0, "gguf_dims_parsed");
    uint64_t kv_per_token = (uint64_t)d_layers * d_kvheads * d_rope * 2 * 4;
    printf("dims: layers=%u embd=%u heads=%u kv_heads=%u head_dim=%u ctx=%u vocab=%llu\n",
           d_layers, d_embd, d_heads, d_kvheads, d_rope, d_ctx,
           (unsigned long long)vocab);
    printf("kv_per_token_bytes=%llu (layers x kv_heads x head_dim x 2 x 4, f32 K+V)\n",
           (unsigned long long)kv_per_token);
    gate(kv_per_token == 45056, "kv_per_token_formula");

    volatile uint64_t sink = 0;
    for (uint64_t off = 0; off < g_fsize; ) {
        sink += g_base[off];
        off += (off < TOUCH_FULL_BYTES) ? 4096u : TOUCH_STRIDE;
    }
    printf("touch sink=%llu (policy: full 32 MiB head, 256 KiB stride beyond)\n",
           (unsigned long long)sink);
    snapshot("S2 after metadata walk + weight touch", &st2, s2, &rr, &rp);
    gate(st2.bytes_requested == st0.bytes_requested
         && st2.bytes_mapped == st0.bytes_mapped,
         "metadata_view_and_touch_move_no_counter");

    /* --- KV allocation (C1): one slice per prompt token --- */
    void *kv[PROMPT_TOKENS + DECODE_STEPS];
    uint64_t kv_req_prompt = 0;
    int kv_ok = 1;
    for (uint32_t i = 0; i < PROMPT_TOKENS; i++) {
        kv[i] = mimir_malloc(kv_per_token);
        if (!kv[i]) { kv_ok = 0; break; }
        touch_range((uint8_t *)kv[i], kv_per_token, (uint8_t)(0x30 + i));
        kv_req_prompt += kv_per_token;
    }
    gate(kv_ok, "kv_prompt_slices_alloc");
    snapshot("S3 after KV allocation (prompt slices, C1)", &st3, s3, &rr, &rp);
    gate(st3.bytes_requested - st2.bytes_requested == kv_req_prompt,
         "KV_counter_delta_equals_prompt_slices");

    /* --- Prefill (C1): prompt buffers + activation burst --- */
    uint64_t pf_req = 0;
    uint32_t *prompt_ids = mimir_malloc(PROMPT_TOKENS * 4);
    if (prompt_ids) { for (uint32_t i = 0; i < PROMPT_TOKENS; i++) prompt_ids[i] = 1000 + i; pf_req += PROMPT_TOKENS * 4; }
    else gate(0, "prefill_prompt_ids_alloc");
    uint64_t embed_bytes = (uint64_t)PROMPT_TOKENS * d_embd * 2;
    uint8_t *prompt_embed = mimir_malloc(embed_bytes);
    if (prompt_embed) { touch_range(prompt_embed, embed_bytes, 0xA5); pf_req += embed_bytes; }
    else gate(0, "prefill_embed_alloc");
    uint8_t *ws[PREFILL_WS_COUNT];
    int ws_ok = 1;
    for (uint32_t i = 0; i < PREFILL_WS_COUNT; i++) {
        ws[i] = mimir_malloc(PREFILL_WS_BYTES);
        if (!ws[i]) { ws_ok = 0; break; }
        touch_range(ws[i], PREFILL_WS_BYTES, (uint8_t)(0x50 + i));
        pf_req += PREFILL_WS_BYTES;
    }
    gate(ws_ok, "prefill_workspace_alloc");
    snapshot("S4 after prefill (C1 prompt buffers + workspace burst)", &st4, s4, &rr, &rp);
    gate(st4.bytes_requested - st3.bytes_requested == pf_req,
         "prefill_C1_delta_equals_harness_requested_sum");
    printf("prefill C1 requested sum=%llu (ids %u + embed %llu + ws %u x %u)\n",
           (unsigned long long)pf_req, PROMPT_TOKENS * 4,
           (unsigned long long)embed_bytes, PREFILL_WS_COUNT, PREFILL_WS_BYTES);

    /* --- Prefill (C2): stdlib workspace, counters invariant --- */
    uint8_t *c2_ws = malloc(C2_WS_BYTES);
    if (c2_ws) touch_range(c2_ws, C2_WS_BYTES, 0x5A); else gate(0, "prefill_c2_ws_alloc");
    uint32_t *c2_ids = malloc(PROMPT_TOKENS * 4);
    if (c2_ids) { for (uint32_t i = 0; i < PROMPT_TOKENS; i++) c2_ids[i] = 1000 + i; }
    else gate(0, "prefill_c2_ids_alloc");
    snapshot("S4C2 after prefill stdlib workspace (C2)", &st4c, s4c, &rr, &rp);
    gate(st4c.bytes_requested == st4.bytes_requested
         && st4c.bytes_allocated == st4.bytes_allocated
         && st4c.bytes_mapped == st4.bytes_mapped
         && st4c.live_small_objects == st4.live_small_objects
         && st4c.live_mappings == st4.live_mappings,
         "C2_prefill_step_moves_no_C1_counter");

    /* --- Decode: 32 steps, KV growth + transient churn --- */
    uint64_t logits_bytes = vocab * 4;
    uint64_t dec_req = 0;
    int dec_ok = 1;
    for (uint32_t step = 0; step < DECODE_STEPS; step++) {
        uint32_t idx = PROMPT_TOKENS + step;
        kv[idx] = mimir_malloc(kv_per_token);
        if (!kv[idx]) { dec_ok = 0; break; }
        touch_range((uint8_t *)kv[idx], kv_per_token, (uint8_t)(0x60 + step));
        dec_req += kv_per_token;
        uint8_t *scratch = mimir_malloc(STEP_SCRATCH);
        if (!scratch) { dec_ok = 0; break; }
        touch_range(scratch, STEP_SCRATCH, 0x77);
        dec_req += STEP_SCRATCH;
        uint8_t *logits = mimir_malloc(logits_bytes);
        if (!logits) { mimir_free(scratch); dec_ok = 0; break; }
        touch_range(logits, 4096, 0x11);
        dec_req += logits_bytes;
        mimir_free(logits);
        mimir_free(scratch);
        uint8_t *c2s = malloc(STEP_SCRATCH);
        if (c2s) { touch_range(c2s, STEP_SCRATCH, 0x22); free(c2s); }
    }
    gate(dec_ok, "decode_steps_complete");
    snapshot("S5 steady decode (after 32 steps)", &st5, s5, &rr, &rp);
    gate(st5.bytes_requested - st4c.bytes_requested == dec_req,
         "decode_C1_delta_equals_harness_requested_sum");
    printf("decode C1 requested sum=%llu over %u steps (per step: kv %llu + scratch %u + logits %llu)\n",
           (unsigned long long)dec_req, DECODE_STEPS,
           (unsigned long long)kv_per_token, STEP_SCRATCH,
           (unsigned long long)logits_bytes);
    printf("KV live at S5: %u slices x %llu B = %llu B held (prompt %u + decode %u)\n",
           PROMPT_TOKENS + DECODE_STEPS, (unsigned long long)kv_per_token,
           (unsigned long long)(PROMPT_TOKENS + DECODE_STEPS) * kv_per_token,
           PROMPT_TOKENS, DECODE_STEPS);

    /* --- Teardown: free both routes, munmap --- */
    free(c2_ids);
    free(c2_ws);
    for (uint32_t i = 0; i < PREFILL_WS_COUNT; i++) mimir_free(ws[i]);
    mimir_free(prompt_embed);
    mimir_free(prompt_ids);
    for (uint32_t i = 0; i < PROMPT_TOKENS + DECODE_STEPS; i++) mimir_free(kv[i]);
    munmap(map, (size_t)g_fsize);
    close(fd);
    snapshot("S6 after teardown", &st6, s6, &rr, &rp);
    gate(s6[CLS_W].size_kb == 0 && s6[CLS_W].rss_kb == 0, "W_returns_to_zero");
    gate(st6.live_small_objects == st0.live_small_objects
         && st6.live_mappings == st0.live_mappings,
         "C1_live_returns_to_baseline");
    printf("teardown: bytes_released delta=%llu (retained mapped at S6=%llu, cached=%llu, G-005 posture, reported not gated)\n",
           (unsigned long long)(st6.bytes_released - st0.bytes_released),
           (unsigned long long)st6.bytes_mapped,
           (unsigned long long)st6.bytes_cached);
    free(stage);
    printf("L006 RESULT: %s (failures=%d)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
