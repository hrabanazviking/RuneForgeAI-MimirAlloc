/* test_e008_mixed.c - E-008 mixed-allocator ownership:
 * the SAFE probes, in this separate child process.
 *
 * The contract posture (include/mimir_alloc.h, by analogy
 * to MA-6.1-FREE-03/-04 and MA-6.1-REALLOC-05): freeing
 * or reallocating a FOREIGN pointer - one allocated by
 * libc malloc, say - through mimir_free / mimir_realloc
 * is UNDEFINED BEHAVIOUR with no defined error, return
 * or recovery. There is NO promised runtime rejection,
 * and this slice implements none. The Wave 5 queue's
 * phrase "mixed-allocator ownership rejected per
 * contract" is therefore discharged honestly as: the
 * rejection IS the contract rule itself (a pointer must
 * be released by the allocator family that allocated it,
 * through the same library instance) plus a test-design
 * prohibition - no test in this slice ever passes a
 * foreign pointer to a mimir_* entry point. The gap
 * between the queue wording and the contract's UB
 * posture is recorded as INC-E008-01.
 *
 * What CAN be observed safely, and is proven here:
 * - Call-path separation (E-005 audit counters, static
 *   leg): a full libc malloc/calloc/realloc/free cycle
 *   in this process moves NO MimirAlloc counter - libc
 *   ownership never enters MimirAlloc's books, so there
 *   is no ownership transfer for MimirAlloc to accept.
 * - A MimirAlloc block allocated and freed through
 *   mimir_* moves the counters exactly (+1 call, +1
 *   published, +1 call-free, +1 retired) - ownership
 *   exists only inside the family that created it.
 * - Mapping separation (construction observation, NOT a
 *   contract claim): the MimirAlloc block is page-aligned
 *   because every MimirAlloc allocation is its own OS
 *   mapping (B-006 mapping-per-allocation baseline); no
 *   property of the libc block is asserted.
 *
 * This binary links the library OBJECTS (like
 * test_e005.c) precisely so it can read the internal
 * audit counters declared in tests/abi/
 * mimir_audit_e005.h; those counters are bound local in
 * libmimiralloc.so and unreadable through it - which is
 * why the dynamic-route proof in test_e008_main.c rests
 * on binding + pointer + contents evidence instead
 * (INC-E008-04). NO foreign free is attempted here or
 * anywhere in this slice: a crash-or-not outcome would
 * prove nothing about the contract and is not evidence.
 *
 * Standalone via make test-e008; NOT part of make test.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (cond) {                                                     \
            printf("  [ok] %s\n", #cond);                               \
        } else {                                                        \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

static unsigned long load(const unsigned long *ctr)
{
    return __atomic_load_n(ctr, __ATOMIC_RELAXED);
}

static int counters_are(unsigned long cm, unsigned long cc,
                        unsigned long cr, unsigned long cf,
                        unsigned long pub, unsigned long ret)
{
    return load(&mimir_audit_call_malloc) == cm &&
           load(&mimir_audit_call_calloc) == cc &&
           load(&mimir_audit_call_realloc) == cr &&
           load(&mimir_audit_call_free) == cf &&
           load(&mimir_audit_alloc_published) == pub &&
           load(&mimir_audit_mapping_retired) == ret;
}

static void audit_line(const char *phase)
{
    printf("AUDIT %s call_malloc=%lu call_calloc=%lu "
           "call_realloc=%lu call_free=%lu published=%lu "
           "retired=%lu live=%lu\n",
           phase,
           load(&mimir_audit_call_malloc),
           load(&mimir_audit_call_calloc),
           load(&mimir_audit_call_realloc),
           load(&mimir_audit_call_free),
           load(&mimir_audit_alloc_published),
           load(&mimir_audit_mapping_retired),
           load(&mimir_audit_alloc_published) -
               load(&mimir_audit_mapping_retired));
}

int main(void)
{
    long page;
    unsigned char *x;
    unsigned char *y;
    unsigned char *z;
    unsigned char *m;

    printf("E-008 mixed-allocator safe probes (separate child "
           "process, audit-counter leg)\n");
    printf("POSTURE: freeing a foreign pointer via mimir_free / "
           "mimir_realloc is UNDEFINED BEHAVIOUR per contract "
           "(MA-6.1-FREE-03/-04 by analogy); no runtime rejection "
           "is promised or implemented.\n");
    printf("PROHIBITION: this process NEVER passes a libc pointer "
           "to a mimir_* entry point and never passes a MimirAlloc "
           "pointer to libc free.\n");

    audit_line("start");
    CHECK(counters_are(0, 0, 0, 0, 0, 0));

    /* libc cycle: full ownership life inside libc only. */
    x = (unsigned char *)malloc(64);
    y = (unsigned char *)calloc(16, 8);
    z = (unsigned char *)malloc(32);
    CHECK(x != NULL && y != NULL && z != NULL);
    if (z != NULL) {
        unsigned char *z2 = (unsigned char *)realloc(z, 96);
        CHECK(z2 != NULL);
        if (z2 != NULL) {
            z = z2;
        }
    }
    free(x);
    free(y);
    free(z);
    audit_line("after-libc-cycle");
    CHECK(counters_are(0, 0, 0, 0, 0, 0));

    /* MimirAlloc cycle: ownership exists only here. */
    m = (unsigned char *)mimir_malloc(64);
    CHECK(m != NULL);
    audit_line("after-mimir-malloc");
    CHECK(counters_are(1, 0, 0, 0, 1, 0));
    page = sysconf(_SC_PAGESIZE);
    CHECK(page > 0);
    if (m != NULL && page > 0) {
        CHECK(((uintptr_t)m % (uintptr_t)page) == 0);
        printf("OBSERVED: mimir block page-aligned (mapping-per-"
               "allocation construction fact, not a contract claim); "
               "no property of any libc block is asserted.\n");
        memset(m, 0x3C, 64);
    }
    mimir_free(m);
    audit_line("after-mimir-free");
    CHECK(counters_are(1, 0, 0, 1, 1, 1));

    printf("CHECKS: %d run, %d failed\n", checks_run, checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
