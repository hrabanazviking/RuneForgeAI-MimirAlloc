/* mimir_diag_shim_j007.h - TEST-ONLY declarations for the
 * J-007 counting interposer (tests/core/
 * mimir_diag_shim_j007.c). Linked ONLY into build/test_j007
 * via --wrap on malloc / calloc / realloc / free / write.
 * The counters count calls made by the linked objects
 * (core, platform, test, shims) while the test's windows
 * are open; the test reads deltas, never absolutes.
 * mimir_diag_wrap_write_reenter, when set to 1 by the test,
 * makes the next __wrap_write on fd 2 attempt ONE nested
 * mimir_core_diag_report before forwarding -- the
 * recursion-proof trigger. It disarms itself.
 */
#ifndef MIMIR_DIAG_SHIM_J007_H
#define MIMIR_DIAG_SHIM_J007_H

extern unsigned long mimir_diag_wrap_malloc_calls;
extern unsigned long mimir_diag_wrap_calloc_calls;
extern unsigned long mimir_diag_wrap_realloc_calls;
extern unsigned long mimir_diag_wrap_free_calls;
extern unsigned long mimir_diag_wrap_write_calls;
extern int mimir_diag_wrap_write_reenter;

#endif /* MIMIR_DIAG_SHIM_J007_H */
