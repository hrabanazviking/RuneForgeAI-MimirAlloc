/* test_e005_audit_link_probe.c - E-005 negative link probe.
 *
 * This program is EXPECTED TO FAIL TO LINK against
 * libmimiralloc.so: it references one of the internal
 * audit counters (mimir_audit_call_malloc) that slice
 * E-005 deliberately keeps out of the shared library's
 * dynamic symbol table (INC-E001-07 shape (b)). The
 * recorded run (tests/abi/recorded_run_e005.txt) builds
 * it against the .so only and records the link failure
 * and its exit code as evidence that the audit hook did
 * not expand the public ABI. It is never built by any
 * Makefile target and never run.
 */
#include <stdio.h>

extern unsigned long mimir_audit_call_malloc;

int main(void)
{
    printf("%lu\n", mimir_audit_call_malloc);
    return 0;
}
