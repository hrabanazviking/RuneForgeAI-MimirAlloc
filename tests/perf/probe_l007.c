/* probe_l007.c - L-007 Phase 12 Qwen3 comparison PROTOCOL gate
 * validator. It does NOT run any model. It validates the
 * protocol's control-variable capture (docs/
 * PHASE_12_QWEN3_COMPARISON_PROTOCOL.md Section 4):
 *  - parses a run-header file for each arm (baseline /
 *    comparison) carrying the six Phase 12 exit controls:
 *    model_sha256, prompt_sha256, context, threads,
 *    backend_sha256, temp_start_c + throttled;
 *  - GATEs: all six identical across arms; start temps differ
 *    by at most 2.0 C; neither arm starts throttled;
 *  - correctness GATE: the two arms' output files, when
 *    given, are byte-identical;
 *  - LIVE capture: reads this host's SoC temperature from
 *    sysfs exactly as a real run header must record it.
 * Exit 0 iff every GATE prints ok. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_failures;
static void gate(int ok, const char *name)
{
    printf("GATE %s: %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) g_failures++;
}

struct header {
    char model_sha[128], prompt_sha[128], backend_sha[128];
    long context, threads, throttled;
    double temp_c;
    int have_model, have_prompt, have_backend, have_ctx,
        have_thr, have_temp, have_throttled;
};

static void set_str(char *dst, size_t cap, const char *v, int *have)
{
    snprintf(dst, cap, "%s", v); *have = 1;
}
static int parse_header(const char *path, struct header *h)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    memset(h, 0, sizeof *h);
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = '\0';
        if (strcmp(line, "model_sha256") == 0)
            set_str(h->model_sha, sizeof h->model_sha, v, &h->have_model);
        else if (strcmp(line, "prompt_sha256") == 0)
            set_str(h->prompt_sha, sizeof h->prompt_sha, v, &h->have_prompt);
        else if (strcmp(line, "backend_sha256") == 0)
            set_str(h->backend_sha, sizeof h->backend_sha, v, &h->have_backend);
        else if (strcmp(line, "context") == 0) { h->context = atol(v); h->have_ctx = 1; }
        else if (strcmp(line, "threads") == 0) { h->threads = atol(v); h->have_thr = 1; }
        else if (strcmp(line, "throttled") == 0) { h->throttled = atol(v); h->have_throttled = 1; }
        else if (strcmp(line, "temp_start_c") == 0) { h->temp_c = atof(v); h->have_temp = 1; }
    }
    fclose(f);
    return 0;
}

static int files_identical(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    int same = 1;
    for (;;) {
        int ca = fgetc(fa), cb = fgetc(fb);
        if (ca != cb) { same = 0; break; }
        if (ca == EOF) break;
    }
    fclose(fa); fclose(fb);
    return same;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 5) {
        printf("usage: probe_l007 <baseline.hdr> <comparison.hdr> [out_base out_comp]\n");
        return 2;
    }
    printf("probe_l007 - L-007 comparison protocol gate validator\n");
    long milli = -1;
    FILE *tf = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (tf) { if (fscanf(tf, "%ld", &milli) != 1) milli = -1; fclose(tf); }
    printf("LIVE temp_capture: %s", milli >= 0 ? "" : "UNAVAILABLE\n");
    if (milli >= 0) printf("%.1f C (thermal_zone0)\n", milli / 1000.0);
    gate(milli >= 0, "live_temp_capture_works");
    struct header b, c;
    gate(parse_header(argv[1], &b) == 0, "baseline_header_parses");
    gate(parse_header(argv[2], &c) == 0, "comparison_header_parses");
    gate(b.have_model && b.have_prompt && b.have_backend && b.have_ctx
         && b.have_thr && b.have_temp && b.have_throttled
         && c.have_model && c.have_prompt && c.have_backend
         && c.have_ctx && c.have_thr && c.have_temp && c.have_throttled,
         "all_six_controls_present_both_arms");
    gate(strcmp(b.model_sha, c.model_sha) == 0, "same_model_hash");
    gate(strcmp(b.prompt_sha, c.prompt_sha) == 0, "same_prompt");
    gate(b.context == c.context, "same_context");
    gate(b.threads == c.threads, "same_thread_count");
    gate(strcmp(b.backend_sha, c.backend_sha) == 0, "same_backend");
    gate(fabs(b.temp_c - c.temp_c) <= 2.0, "thermal_start_within_2C");
    gate(b.throttled == 0 && c.throttled == 0, "neither_arm_throttled_at_start");
    if (argc == 5) {
        int same = files_identical(argv[3], argv[4]);
        gate(same == 1, "outputs_byte_identical");
    }
    printf("L007 PROBE RESULT: %s (failures=%d)\n",
           g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
