/* test_z076_cpp.cpp - Z-076 C++ consumer (E-series style).
 *
 * Linked ONLY against build/libmimiralloc.so: proves the
 * public statistics declarations are usable from C++
 * (extern "C" guards cover the Z-076 additions, the struct
 * layout is a plain aggregate) and behave through the
 * shared library. Standalone via `make test-z076`; NOT
 * part of `make test`.
 */
#include "mimir_alloc.h"

#include <cstdio>
#include <cstring>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (!(cond)) {                                                  \
            checks_failed++;                                            \
            std::printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);    \
        }                                                               \
    } while (0)

int main()
{
    mimir_stats_v1 v;
    void *p;

    std::printf("Z-076 C++ consumer (libmimiralloc.so only)\n");
    std::memset(&v, 0, sizeof v);
    CHECK(mimir_stats_snapshot(&v) == 0);
    CHECK(v.struct_version == MIMIR_STATS_VERSION);
    CHECK(v.struct_size == sizeof(mimir_stats_v1));
    CHECK(v.lock_contention_measured == 1);
    CHECK(v.live_requested_measured == 0);
    p = mimir_malloc(64);
    CHECK(p != nullptr);
    std::memset(p, 0x5A, 64);
    mimir_free(p);
    std::memset(&v, 0, sizeof v);
    CHECK(mimir_stats_snapshot(&v) == 0);
    CHECK(v.live_small_objects == 0);
    std::printf("Z076 CPP: %d checks, %d failed\n", checks_run,
                checks_failed);
    std::printf("RESULT: %s\n",
                checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
