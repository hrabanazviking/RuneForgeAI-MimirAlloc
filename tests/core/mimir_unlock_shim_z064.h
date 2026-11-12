/* mimir_unlock_shim_z064.h - TEST-ONLY declarations for
 * slice Z-064 (lock-failure observability, D-01).
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document. The control surface declared here is
 * the unlock fault-injection shim of
 * docs/MIMIRALLOC_Z13_ROADMAP.md Z-064: the I-004 shim
 * heritage (tests/core/mimir_fault_i004.h) extended to the
 * one L6 entry I-004 did not wrap,
 * mimir_platform_global_unlock. Internal test state in the
 * INC-E001-07 shape (b), reachable only by linking the shim
 * object (tests/core/mimir_unlock_shim_z064.c) directly, as
 * `make test-z064` does. The shim interposes through the
 * linker's --wrap option, so NO production object carries
 * any injection code.
 *
 * Semantics (exact, per tests/core/mimir_unlock_shim_z064.c):
 * the shim counts every call the core makes to
 * mimir_platform_global_unlock. Two single-shot injection
 * modes, each armed with arm(n) to fire on the n-th unlock
 * call from arming:
 * - REPORT (mimir_unlock_arm_report): the real unlock IS
 *   performed and its real status discarded; the shim then
 *   returns MIMIR_PLATFORM_ERR_LOCK to the core. This models
 *   the caller-visible half of a failed unlock -- the core
 *   receives a failure status at an unlock site -- while
 *   the mutex itself is genuinely released, so the process
 *   can go on to prove the failure was counted and reported
 *   and that allocator work continues.
 * - TRUE (mimir_unlock_arm_true): the real unlock is NOT
 *   performed and MIMIR_PLATFORM_ERR_LOCK is returned --
 *   exactly the state a genuine pthread_mutex_unlock
 *   failure leaves (the mutex stays locked). Any later
 *   lock-taking call in that process wedges, by the nature
 *   of the failure itself; the Z-064 test therefore runs
 *   this mode only in a forked child that reads the tally
 *   through the lock-free getter and exits without taking
 *   the lock again.
 * Arming one mode disarms the other; arming with 0 disarms
 * that mode; mimir_unlock_disarm() disarms both. Every
 * non-injected call forwards to the real entry and counts
 * a real failure when the real entry returns one. The shim
 * takes no lock and allocates nothing.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal test instrumentation only;
 * [UNRESOLVED: U-03] applies as it does to the whole
 * namespaced surface. No production caller may use it.
 */
#ifndef MIMIR_UNLOCK_SHIM_Z064_H
#define MIMIR_UNLOCK_SHIM_Z064_H

extern unsigned long mimir_unlock_attempts;
extern unsigned long mimir_unlock_injected_report;
extern unsigned long mimir_unlock_injected_true;
extern unsigned long mimir_unlock_real_failed;

void mimir_unlock_arm_report(unsigned long n);
void mimir_unlock_arm_true(unsigned long n);
void mimir_unlock_disarm(void);

#endif /* MIMIR_UNLOCK_SHIM_Z064_H */
