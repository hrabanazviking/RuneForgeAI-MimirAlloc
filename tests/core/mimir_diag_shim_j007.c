/*
 * mimir_diag_shim_j007.c -- counting interposer for slice
 * J-007 (docs/PHASE_10_DIAGNOSTICS.md), the proof instrument
 * for the channel's two hard claims:
 *
 *  (1) ZERO ALLOCATION ON THE REPORT PATH. --wrap binds the
 *      linked objects' malloc / calloc / realloc / free
 *      references to the counters below. The J-007 channel
 *      (and the whole core) calls none of them; the test
 *      opens a counting window around report emissions --
 *      including an emission while the I-004 shim is armed
 *      to fail every backing acquire -- and asserts every
 *      delta is exactly 0.
 *
 *  (2) NO RECURSION. --wrap also binds write: when the test
 *      arms mimir_diag_wrap_write_reenter, the first write
 *      on fd 2 inside a report attempts ONE nested
 *      mimir_core_diag_report (a report triggered from
 *      inside a report's own emission). The channel's guard
 *      must suppress it: the test asserts the suppressed
 *      counter moves by exactly 1, the emitted counter by
 *      exactly 1, and max depth stays 1.
 *
 * Every wrapper forwards to the __real_ entry unchanged
 * (allocation behaviour of the test binary is the libc
 * behaviour); counting is the only addition, plus the one
 * armed nested attempt. This file is a TEST-ONLY object,
 * linked only into build/test_j007; no production object
 * carries any of it, and the shared library is the banked
 * build (recorded run: nm -D stays exactly 4).
 */

#include "mimir_diag_shim_j007.h"

#include <stddef.h>
#include <unistd.h>

#include "../../src/core/mimir_core.h"

void *__real_malloc(size_t size);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);
ssize_t __real_write(int fd, const void *buf, size_t count);

unsigned long mimir_diag_wrap_malloc_calls;
unsigned long mimir_diag_wrap_calloc_calls;
unsigned long mimir_diag_wrap_realloc_calls;
unsigned long mimir_diag_wrap_free_calls;
unsigned long mimir_diag_wrap_write_calls;
int mimir_diag_wrap_write_reenter;

void *__wrap_malloc(size_t size)
{
    mimir_diag_wrap_malloc_calls++;
    return __real_malloc(size);
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
    mimir_diag_wrap_calloc_calls++;
    return __real_calloc(nmemb, size);
}

void *__wrap_realloc(void *ptr, size_t size)
{
    mimir_diag_wrap_realloc_calls++;
    return __real_realloc(ptr, size);
}

void __wrap_free(void *ptr)
{
    mimir_diag_wrap_free_calls++;
    __real_free(ptr);
}

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    mimir_diag_wrap_write_calls++;
    if (mimir_diag_wrap_write_reenter && fd == STDERR_FILENO) {
        /* One nested report attempt, from inside the outer
         * report's own write: the recursion-proof trigger.
         * Disarm first so the attempt happens exactly once. */
        mimir_diag_wrap_write_reenter = 0;
        mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                               MIMIR_DIAG_CAUSE_BACKING, 777);
    }
    return __real_write(fd, buf, count);
}
