/* f005_audit.c - MimirAlloc slice F-005 test-only audit
 * getters, static leg (E-005 shape (b) on the Mojo path).
 *
 * This file lives under tests/mojo/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document. It wraps the six internal E-005 audit
 * counters (declared for tests in tests/abi/
 * mimir_audit_e005.h) in parameterless getter functions,
 * because Mojo's std.ffi external_call calls functions -
 * it cannot read a C global variable directly.
 *
 * The getters are meaningful only in the F-005 static leg:
 * build/test_f005_audit links this file's object together
 * with the library OBJECTS (build/mimir_alloc.o,
 * build/mimir_core.o, build/mimir_platform.o) into the
 * generated Mojo executable, so the counters read here are
 * the very counters the executable's own mimir_* calls
 * move - the E-005 static leg's construction, carried to
 * the Mojo path. Linked against libmimiralloc.so instead,
 * these references cannot resolve: the counters are local
 * in the .so (proven in the recorded run), which is
 * exactly why the two legs are separate builds.
 */
extern unsigned long mimir_audit_call_malloc;
extern unsigned long mimir_audit_call_calloc;
extern unsigned long mimir_audit_call_realloc;
extern unsigned long mimir_audit_call_free;
extern unsigned long mimir_audit_alloc_published;
extern unsigned long mimir_audit_mapping_retired;

unsigned long f005_audit_call_malloc(void)
{
    return __atomic_load_n(&mimir_audit_call_malloc,
                           __ATOMIC_RELAXED);
}

unsigned long f005_audit_call_calloc(void)
{
    return __atomic_load_n(&mimir_audit_call_calloc,
                           __ATOMIC_RELAXED);
}

unsigned long f005_audit_call_realloc(void)
{
    return __atomic_load_n(&mimir_audit_call_realloc,
                           __ATOMIC_RELAXED);
}

unsigned long f005_audit_call_free(void)
{
    return __atomic_load_n(&mimir_audit_call_free,
                           __ATOMIC_RELAXED);
}

unsigned long f005_audit_published(void)
{
    return __atomic_load_n(&mimir_audit_alloc_published,
                           __ATOMIC_RELAXED);
}

unsigned long f005_audit_retired(void)
{
    return __atomic_load_n(&mimir_audit_mapping_retired,
                           __ATOMIC_RELAXED);
}

/* G-004 re-teach (INC-G004-05): the G-003 span/object
 * counters (tests/core/mimir_small_g003.h semantics),
 * exposed the same way so the audit leg's Mojo script
 * can assert the two-registry expectations: this
 * script's sizes are all <= 1024, so its publications
 * and retirements land HERE, not in the mapping
 * counters above - exactly the re-teach the C static
 * leg (tests/abi/test_e005.c) received in G-003. */
extern unsigned long mimir_audit_small_alloc;
extern unsigned long mimir_audit_small_free;

unsigned long f005_audit_small_alloc(void)
{
    return __atomic_load_n(&mimir_audit_small_alloc,
                           __ATOMIC_RELAXED);
}

unsigned long f005_audit_small_free(void)
{
    return __atomic_load_n(&mimir_audit_small_free,
                           __ATOMIC_RELAXED);
}
