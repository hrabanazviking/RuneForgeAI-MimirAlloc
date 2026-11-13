/* probe_z104.c - Z-104 sanitizer detection controls.
 *
 * The matrix (run_z104.sh) builds the full suite under
 * ASan, UBSan and ASan+UBSan and requires zero sanitizer
 * reports. A clean suite alone does not prove the
 * sanitizers can see anything in this tree, so this probe
 * provides the positive and boundary controls:
 *
 *   asan-detect   (ASan build) a one-byte heap overflow on
 *                 a glibc-malloc block MUST be reported
 *                 (heap-buffer-overflow) with a nonzero
 *                 exit. If ASan cannot see even this, the
 *                 clean suite proves nothing.
 *   ubsan-detect  (UBSan build, -fno-sanitize-recover=all)
 *                 a signed integer overflow MUST be
 *                 reported (runtime error) with a nonzero
 *                 exit.
 *   coverage      (ASan build) prints the ASan poison
 *                 state of (a) the byte just past a glibc
 *                 block (expected poisoned: ASan redzone)
 *                 and (b) the byte just past a MimirAlloc
 *                 block (expected NOT poisoned: mimir
 *                 blocks are carved from the allocator's
 *                 own mmap spans, which ASan does not
 *                 redzone). This is the slice's coverage
 *                 boundary, measured, not asserted: ASan
 *                 watches the allocator's code and its
 *                 stack/global/glibc-heap traffic; client
 *                 blocks served from mimir spans are
 *                 outside ASan's view and remain the job
 *                 of the MIMIR_DEBUG redzones (J-004) and
 *                 the Wave-8 metadata detection.
 *
 * Exit codes: asan-detect/ubsan-detect return 0 only if
 * the overflow is NOT caught (the runner treats exit 0 as
 * CONTROL FAILED); the sanitizer abort is the pass.
 * coverage returns 0 when both poison readings match the
 * expected boundary, 1 otherwise, 2 when built without
 * ASan (runner does not use it then).
 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"

#if defined(__SANITIZE_ADDRESS__)
# include <sanitizer/asan_interface.h>
# define Z104_HAVE_ASAN 1
#elif defined(__has_feature)
# if __has_feature(address_sanitizer)
#  include <sanitizer/asan_interface.h>
#  define Z104_HAVE_ASAN 1
# endif
#endif

static int mode_asan_detect(void)
{
    char *p = (char *)malloc(16);
    if (!p) { printf("CONTROL asan-detect: malloc failed\n"); return 2; }
    memset(p, 0x41, 16);
    /* One byte past the block: ASan redzone. A correct
     * ASan runtime reports heap-buffer-overflow here and
     * never returns. The volatile store keeps the write
     * from being optimised away. */
    volatile char *past = (volatile char *)(p + 16);
    *past = 0x42;
    printf("CONTROL asan-detect: NOT DETECTED (write past "
           "glibc block completed)\n");
    free(p);
    return 0;
}

static int mode_ubsan_detect(void)
{
    volatile int x = INT_MAX;
    /* Signed overflow: undefined behaviour. UBSan with
     * -fno-sanitize-recover=all reports it and aborts. */
    int y = x + 1;
    printf("CONTROL ubsan-detect: NOT DETECTED (INT_MAX+1 "
           "== %d, no report)\n", y);
    return 0;
}

static int mode_coverage(void)
{
#ifdef Z104_HAVE_ASAN
    char *g = (char *)malloc(32);
    void *m = mimir_malloc(32);
    if (!g || !m) { printf("COVERAGE: allocation failed\n"); return 2; }
    memset(g, 0x47, 32);
    memset(m, 0x4d, 32);
    int g_in = __asan_address_is_poisoned(g + 31);
    int g_past = __asan_address_is_poisoned(g + 32);
    int m_in = __asan_address_is_poisoned((char *)m + 31);
    int m_past = __asan_address_is_poisoned((char *)m + 32);
    printf("COVERAGE glibc block: in-block poisoned=%d "
           "byte-past poisoned=%d (expect 0/1)\n", g_in, g_past);
    printf("COVERAGE mimir block: in-block poisoned=%d "
           "byte-past poisoned=%d (expect 0/0)\n", m_in, m_past);
    free(g);
    mimir_free(m);
    if (g_in == 0 && g_past != 0 && m_in == 0 && m_past == 0) {
        printf("COVERAGE RESULT: boundary as expected - "
               "ASan redzones cover glibc heap, not "
               "mimir-served spans\n");
        return 0;
    }
    printf("COVERAGE RESULT: boundary DIFFERS from the "
           "recorded expectation\n");
    return 1;
#else
    printf("COVERAGE: built without ASan; not applicable\n");
    return 2;
#endif
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: probe_z104 <asan-detect|ubsan-detect|coverage>\n");
        return 2;
    }
    if (strcmp(argv[1], "asan-detect") == 0) return mode_asan_detect();
    if (strcmp(argv[1], "ubsan-detect") == 0) return mode_ubsan_detect();
    if (strcmp(argv[1], "coverage") == 0) return mode_coverage();
    printf("unknown mode: %s\n", argv[1]);
    return 2;
}
