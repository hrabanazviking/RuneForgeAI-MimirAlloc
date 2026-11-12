// test_z078_cpp.cpp - Z-078 C++ consumer: links ONLY
// against build/libmimiralloc.so, proving the
// allocation-family declarations sit inside the public
// header's extern "C" guards with their exact C
// signatures (the assignments below fail to compile if
// any prototype is mangled or mis-typed), and that the
// D-013 zero-size / errno contract runs from C++: a
// zero-size refusal, a preserving realloc-to-zero, and
// the posix-shaped ENOMEM row with errno untouched.
// Exit 0 iff every check passes.
#include "mimir_alloc.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>

static void *(*const ref_malloc)(std::size_t) = &mimir_malloc;
static void *(*const ref_calloc)(std::size_t, std::size_t) = &mimir_calloc;
static void *(*const ref_realloc)(void *, std::size_t) = &mimir_realloc;
static void *(*const ref_ra)(void *, std::size_t,
                             std::size_t) = &mimir_reallocarray;
static void *(*const ref_aligned)(std::size_t,
                                  std::size_t) = &mimir_aligned_alloc;
static int (*const ref_posix)(void **, std::size_t,
                              std::size_t) = &mimir_posix_memalign;

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
    errno = 77;
    CHECK(ref_malloc(0) == nullptr);
    CHECK(errno == 77);
    CHECK(ref_calloc(4, 0) == nullptr);
    unsigned char *p = static_cast<unsigned char *>(ref_malloc(64));
    CHECK(p != nullptr);
    for (std::size_t i = 0; i < 64; i++) {
        p[i] = static_cast<unsigned char>(i * 3u);
    }
    errno = 77;
    CHECK(ref_realloc(p, 0) == nullptr);
    CHECK(errno == 77);
    for (std::size_t i = 0; i < 64; i++) {
        CHECK(p[i] == static_cast<unsigned char>(i * 3u));
    }
    CHECK(ref_ra(p, 0, 8) == nullptr);
    CHECK(ref_aligned(64, 0) == nullptr);
    void *m = reinterpret_cast<void *>(static_cast<uintptr_t>(1));
    errno = 77;
    CHECK(ref_posix(&m, 8, 0) == ENOMEM);
    CHECK(errno == 77);
    CHECK(m == reinterpret_cast<void *>(static_cast<uintptr_t>(1)));
    mimir_free(p);
    std::printf("test_z078_cpp: %s\n",
                fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
