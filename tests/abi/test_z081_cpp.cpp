// test_z081_cpp.cpp - Z-081 C++ consumer: links ONLY
// against build/libmimiralloc.so, proving all ten
// public declarations sit inside the header's extern
// "C" guards with their exact C signatures (the
// function-pointer assignments below fail to compile
// if any prototype is mangled or mis-typed), and that
// the surface runs from C++: one exercised call per
// symbol family. Exit 0 iff every check passes.
#include "mimir_alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

static void *(*const ref_malloc)(std::size_t) = &mimir_malloc;
static void (*const ref_free)(void *) = &mimir_free;
static void *(*const ref_calloc)(std::size_t, std::size_t) = &mimir_calloc;
static void *(*const ref_realloc)(void *, std::size_t) = &mimir_realloc;
static void *(*const ref_ra)(void *, std::size_t,
                             std::size_t) = &mimir_reallocarray;
static int (*const ref_trim)(std::size_t) = &mimir_trim;
static void *(*const ref_aligned)(std::size_t,
                                  std::size_t) = &mimir_aligned_alloc;
static int (*const ref_posix)(void **, std::size_t,
                              std::size_t) = &mimir_posix_memalign;
static std::size_t (*const ref_usable)(const void *) = &mimir_usable_size;
static int (*const ref_stats)(mimir_stats_v1 *) = &mimir_stats_snapshot;

static int fails = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fails++;                                                    \
            std::printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);    \
        }                                                               \
    } while (0)

int main()
{
    unsigned char *p = static_cast<unsigned char *>(ref_malloc(64));
    CHECK(p != nullptr);
    for (std::size_t i = 0; i < 64; i++) {
        p[i] = static_cast<unsigned char>(i * 3u);
    }
    unsigned char *q =
        static_cast<unsigned char *>(ref_calloc(2, 32));
    CHECK(q != nullptr);
    CHECK(q[0] == 0 && q[63] == 0);
    ref_free(q);
    p = static_cast<unsigned char *>(ref_realloc(p, 128));
    CHECK(p != nullptr);
    CHECK(p[63] == static_cast<unsigned char>(63 * 3u));
    CHECK(ref_usable(p) >= 128);
    p = static_cast<unsigned char *>(ref_ra(p, 2, 64));
    CHECK(p != nullptr);
    void *a = ref_aligned(64, 40);
    CHECK(a != nullptr &&
          (reinterpret_cast<uintptr_t>(a) % 64) == 0);
    ref_free(a);
    void *m = nullptr;
    CHECK(ref_posix(&m, 64, 40) == 0);
    CHECK(m != nullptr);
    ref_free(m);
    mimir_stats_v1 st;
    CHECK(ref_stats(&st) == 0);
    CHECK(st.struct_version == MIMIR_STATS_VERSION);
    ref_free(p);
    ref_free(nullptr);
    int t = ref_trim(0);
    CHECK(t == 0 || t == 1);
    std::printf("test_z081_cpp: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
