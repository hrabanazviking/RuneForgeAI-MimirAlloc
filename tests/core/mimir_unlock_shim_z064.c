/*
 * mimir_unlock_shim_z064.c -- unlock fault-injection shim
 * for slice Z-064 (lock-failure observability, D-01).
 *
 * Road: the I-004 shim's own road (link-time interposition,
 * tests/core/mimir_fault_shim_i004.c), extended to the one
 * L6 entry I-004 did not wrap: mimir_platform_global_unlock.
 * This file is a TEST-ONLY object, linked only into
 * build/test_z064_unlock, where the Makefile's test-z064
 * rule passes -Wl,--wrap=mimir_platform_global_unlock. No
 * file under src/ or include/ is changed by the shim, no
 * production binary contains this code, and the shared
 * library is the banked build (the Z-064 recorded run
 * proves nm -D --defined-only stays at exactly 4).
 *
 * Two single-shot injection modes (the header has the full
 * semantics): REPORT performs the real unlock and then
 * returns ERR_LOCK (the core sees a failed unlock; the
 * mutex is really released, so the run continues and can
 * prove counting + reporting + continued service); TRUE
 * returns ERR_LOCK without performing the unlock (a
 * genuine failure's exact state: the mutex stays locked;
 * the test runs it only in a forked child that reads the
 * tally lock-free and exits). Every other call forwards to
 * the real entry and counts a real failure when the real
 * entry returns one, so injected and genuine failures are
 * separated exactly, as in I-004.
 *
 * The shim allocates nothing, takes no lock of its own and
 * never calls back into the allocator.
 */

#include "mimir_unlock_shim_z064.h"

#include "../../src/platform/mimir_platform.h"

/* The genuine L6 entry, bound by the linker under the
 * --wrap convention (only meaningful in the test-z064
 * link; this object is never linked anywhere else). */
int __real_mimir_platform_global_unlock(void);

unsigned long mimir_unlock_attempts;
unsigned long mimir_unlock_injected_report;
unsigned long mimir_unlock_injected_true;
unsigned long mimir_unlock_real_failed;

/* Single-shot countdowns: 0 = disarmed; otherwise the
 * number of calls still to let through before the failing
 * one (arm(n) fails the n-th call from arming). */
static unsigned long countdown_report;
static unsigned long countdown_true;

void mimir_unlock_arm_report(unsigned long n)
{
    countdown_report = n;
    countdown_true = 0;
}

void mimir_unlock_arm_true(unsigned long n)
{
    countdown_true = n;
    countdown_report = 0;
}

void mimir_unlock_disarm(void)
{
    countdown_report = 0;
    countdown_true = 0;
}

/* Returns 1 exactly once, on the armed call. */
static int this_call_fails(unsigned long *countdown)
{
    if (*countdown == 0) {
        return 0;
    }
    *countdown -= 1;
    return *countdown == 0;
}

int __wrap_mimir_platform_global_unlock(void)
{
    int status;

    mimir_unlock_attempts += 1;
    if (this_call_fails(&countdown_true)) {
        mimir_unlock_injected_true += 1;
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    if (this_call_fails(&countdown_report)) {
        (void)__real_mimir_platform_global_unlock();
        mimir_unlock_injected_report += 1;
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    status = __real_mimir_platform_global_unlock();
    if (status != MIMIR_PLATFORM_OK) {
        mimir_unlock_real_failed += 1;
    }
    return status;
}
