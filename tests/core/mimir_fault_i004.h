/* mimir_fault_i004.h - TEST-ONLY declarations for slice I-004.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by I-004. The control surface declared here is
 * the fault-injection shim of docs/PHASE_9_FAILURE_HANDLING.md
 * (INC-I001-01, carried from INC-C007-01 / INC-C008-01):
 * internal test state in the INC-E001-07 shape (b), reachable
 * only by linking the shim object
 * (tests/core/mimir_fault_shim_i004.c) directly, as
 * `make test-i004` does. The shim interposes on the L6
 * platform entries through the linker's --wrap option, so
 * NO production object carries any injection code: the
 * shared library and the `make test` binaries are built
 * exactly as banked, and libmimiralloc.so's dynamic symbol
 * table stays at exactly 4 (proven in
 * tests/core/recorded_run_i004.txt).
 *
 * Semantics (exact, per tests/core/mimir_fault_shim_i004.c):
 * the shim counts every call the core makes to four L6
 * entries -- mimir_platform_map_acquire,
 * mimir_platform_map_release, mimir_platform_global_lock
 * and mimir_platform_page_size -- in the counters below,
 * separating injected failures (produced by the shim) from
 * real failures (the real L6 entry returned the error).
 * Arming is single-shot and counted from the arming call:
 * mimir_fault_arm_acquire(n) with n >= 1 makes the n-th
 * map-acquire call from now fail once with
 * MIMIR_PLATFORM_ERR_MAP, without any OS call (the out
 * parameters are set to the real failure values NULL / 0,
 * exactly as a genuine acquire failure leaves them), and
 * then disarms itself. arm_release / arm_lock / arm_pagesize
 * are the same for map_release (ERR_UNMAP, the real release
 * is NOT performed, so the mapping stays mapped -- exactly
 * the state a genuine release failure leaves), global_lock
 * (ERR_LOCK, the lock is NOT taken) and page_size
 * (ERR_PAGESIZE, no value is stored). Arming with 0 disarms
 * that channel; mimir_fault_disarm() disarms all four.
 * Every other call is forwarded to the real L6 entry
 * unchanged. The shim takes no lock and allocates nothing;
 * allocator-driven calls reach it with the core's global
 * lock already held, which serialises the counters for the
 * single-threaded evidence test.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal test instrumentation only;
 * [UNRESOLVED: U-03] applies as it does to the whole
 * namespaced surface. No production caller may use it, and
 * its presence in a test binary changes no allocator
 * behaviour beyond the failures it is armed to inject.
 */
#ifndef MIMIR_FAULT_I004_H
#define MIMIR_FAULT_I004_H

/* Acquire channel: calls to mimir_platform_map_acquire. */
extern unsigned long mimir_fault_acquire_attempts;
extern unsigned long mimir_fault_acquire_injected;
extern unsigned long mimir_fault_acquire_real_failed;

/* Release channel: calls to mimir_platform_map_release. */
extern unsigned long mimir_fault_release_attempts;
extern unsigned long mimir_fault_release_injected;
extern unsigned long mimir_fault_release_real_failed;

/* Lock channel: calls to mimir_platform_global_lock. */
extern unsigned long mimir_fault_lock_attempts;
extern unsigned long mimir_fault_lock_injected;
extern unsigned long mimir_fault_lock_real_failed;

/* Page-size channel: calls to mimir_platform_page_size. */
extern unsigned long mimir_fault_pagesize_attempts;
extern unsigned long mimir_fault_pagesize_injected;
extern unsigned long mimir_fault_pagesize_real_failed;

void mimir_fault_arm_acquire(unsigned long n);
void mimir_fault_arm_release(unsigned long n);
void mimir_fault_arm_lock(unsigned long n);
void mimir_fault_arm_pagesize(unsigned long n);
void mimir_fault_disarm(void);

#endif /* MIMIR_FAULT_I004_H */
