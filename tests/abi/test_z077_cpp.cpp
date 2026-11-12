// test_z077_cpp.cpp - Z-077 C++ consumer: links ONLY
// against build/libmimiralloc.so, proving the two Z-077
// declarations sit inside the public header's extern "C"
// guards with their exact C signatures (the assignments
// below fail to compile if either prototype is mangled
// or mis-typed), and that both entry points run from
// C++: a reallocarray round-trip and a trim call.
// Exit 0 iff every check passes.
#include "mimir_alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

static int (*const ref_trim)(std::size_t) = &mimir_trim;
static void *(*const ref_ra)(void *, std::size_t,
                             std::size_t) = &mimir_reallocarray;

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
    unsigned char *p = static_cast<unsigned char *>(
        ref_ra(nullptr, 2, 32));
    CHECK(p != nullptr);
    for (std::size_t i = 0; i < 64; i++) {
        p[i] = static_cast<unsigned char>(i * 3u);
    }
    CHECK(ref_ra(p, SIZE_MAX, 4) == nullptr);
    for (std::size_t i = 0; i < 64; i++) {
        CHECK(p[i] == static_cast<unsigned char>(i * 3u));
    }
    int r = ref_trim(0);
    CHECK(r == 0 || r == 1);
    mimir_free(p);
    std::printf("test_z077_cpp: %s\n",
                fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
