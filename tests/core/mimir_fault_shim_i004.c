/*
 * mimir_fault_shim_i004.c -- fault-injection shim for slice
 * I-004 (docs/PHASE_9_FAILURE_HANDLING.md), the instrument
 * whose absence is INC-I001-01 (carried from INC-C007-01 /
 * INC-C008-01).
 *
 * Road chosen (the least invasive honest one in this tree):
 * link-time interposition. This file is a TEST-ONLY object,
 * linked only into build/test_i004, where the Makefile's
 * test-i004 rule passes -Wl,--wrap=<name> for the four L6
 * platform entries below. The linker then resolves the
 * core object's calls to those names to the __wrap_ forms
 * here, and the __real_ forms to the genuine platform
 * object. No file under src/ or include/ is changed, no
 * production binary contains this code, and the shared
 * library is bit-for-bit the banked build (the recorded
 * run proves nm -D --defined-only stays at exactly 4 and
 * that none of this file's symbols is in the .so at all).
 *
 * Each wrapper counts the call, and when its channel's
 * single-shot countdown (armed through the control
 * functions declared in tests/core/mimir_fault_i004.h)
 * reaches zero on this call, it returns exactly the error
 * the real entry returns for the corresponding genuine
 * failure, WITHOUT performing the operation:
 * - acquire: ERR_MAP, *out_base = NULL, *out_length = 0
 *   (the all-or-nothing failure state of the real entry);
 *   nothing is mapped, exactly as when mmap fails.
 * - release: ERR_UNMAP and the real munmap is NOT called,
 *   so the mapping stays mapped -- precisely the state a
 *   genuine release failure leaves, which is what the
 *   core's F-06 rollback is written for.
 * - lock: ERR_LOCK and the mutex is NOT taken, the state a
 *   genuine lock failure leaves (F-07 posture).
 * - page_size: ERR_PAGESIZE and nothing is stored, the
 *   state a genuine discovery failure leaves (F-11).
 * Every non-injected call forwards to the real entry and
 * counts a real failure when the real entry returns one,
 * so the counters separate injected failures from genuine
 * OS failures exactly (I-002 FC-4's separation, for the
 * OS-failure kind; posture NULLs never reach any of these
 * entries and move no counter here, FC-6; budget refusals
 * do not exist in this tree, INC-I001-14).
 *
 * The shim allocates nothing, takes no lock of its own and
 * never calls back into the allocator: the failure path is
 * a counter decrement and a return (I-002 FC-3: no retry,
 * no recursion, no allocation inside the error path).
 */

#include "mimir_fault_i004.h"

#include "../../src/platform/mimir_platform.h"

/* The genuine L6 entries, bound by the linker under the
 * --wrap convention (only meaningful in the test-i004
 * link; this object is never linked anywhere else). */
int __real_mimir_platform_map_acquire(size_t requested_bytes,
                                      void **out_base,
                                      size_t *out_length);
int __real_mimir_platform_map_release(void *base, size_t length);
int __real_mimir_platform_global_lock(void);
int __real_mimir_platform_page_size(size_t *out_page_size);

unsigned long mimir_fault_acquire_attempts;
unsigned long mimir_fault_acquire_injected;
unsigned long mimir_fault_acquire_real_failed;
unsigned long mimir_fault_release_attempts;
unsigned long mimir_fault_release_injected;
unsigned long mimir_fault_release_real_failed;
unsigned long mimir_fault_lock_attempts;
unsigned long mimir_fault_lock_injected;
unsigned long mimir_fault_lock_real_failed;
unsigned long mimir_fault_pagesize_attempts;
unsigned long mimir_fault_pagesize_injected;
unsigned long mimir_fault_pagesize_real_failed;

/* Single-shot countdowns: 0 = disarmed; otherwise the
 * number of calls still to let through before the failing
 * one (arm(n) fails the n-th call from arming). */
static unsigned long countdown_acquire;
static unsigned long countdown_release;
static unsigned long countdown_lock;
static unsigned long countdown_pagesize;

void mimir_fault_arm_acquire(unsigned long n)
{
    countdown_acquire = n;
}

void mimir_fault_arm_release(unsigned long n)
{
    countdown_release = n;
}

void mimir_fault_arm_lock(unsigned long n)
{
    countdown_lock = n;
}

void mimir_fault_arm_pagesize(unsigned long n)
{
    countdown_pagesize = n;
}

void mimir_fault_disarm(void)
{
    countdown_acquire = 0;
    countdown_release = 0;
    countdown_lock = 0;
    countdown_pagesize = 0;
}

/* Returns 1 exactly once, on the armed call: decrements a
 * non-zero countdown and reports whether this call is the
 * one to fail. */
static int this_call_fails(unsigned long *countdown)
{
    if (*countdown == 0) {
        return 0;
    }
    *countdown -= 1;
    return *countdown == 0;
}

int __wrap_mimir_platform_map_acquire(size_t requested_bytes,
                                      void **out_base,
                                      size_t *out_length)
{
    int status;

    mimir_fault_acquire_attempts += 1;
    if (this_call_fails(&countdown_acquire)) {
        mimir_fault_acquire_injected += 1;
        if (out_base != NULL) {
            *out_base = NULL;
        }
        if (out_length != NULL) {
            *out_length = 0;
        }
        return MIMIR_PLATFORM_ERR_MAP;
    }
    status = __real_mimir_platform_map_acquire(requested_bytes,
                                               out_base,
                                               out_length);
    if (status != MIMIR_PLATFORM_OK) {
        mimir_fault_acquire_real_failed += 1;
    }
    return status;
}

int __wrap_mimir_platform_map_release(void *base, size_t length)
{
    int status;

    mimir_fault_release_attempts += 1;
    if (this_call_fails(&countdown_release)) {
        mimir_fault_release_injected += 1;
        return MIMIR_PLATFORM_ERR_UNMAP;
    }
    status = __real_mimir_platform_map_release(base, length);
    if (status != MIMIR_PLATFORM_OK) {
        mimir_fault_release_real_failed += 1;
    }
    return status;
}

int __wrap_mimir_platform_global_lock(void)
{
    int status;

    mimir_fault_lock_attempts += 1;
    if (this_call_fails(&countdown_lock)) {
        mimir_fault_lock_injected += 1;
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    status = __real_mimir_platform_global_lock();
    if (status != MIMIR_PLATFORM_OK) {
        mimir_fault_lock_real_failed += 1;
    }
    return status;
}

int __wrap_mimir_platform_page_size(size_t *out_page_size)
{
    int status;

    mimir_fault_pagesize_attempts += 1;
    if (this_call_fails(&countdown_pagesize)) {
        mimir_fault_pagesize_injected += 1;
        return MIMIR_PLATFORM_ERR_PAGESIZE;
    }
    status = __real_mimir_platform_page_size(out_page_size);
    if (status != MIMIR_PLATFORM_OK) {
        mimir_fault_pagesize_real_failed += 1;
    }
    return status;
}
