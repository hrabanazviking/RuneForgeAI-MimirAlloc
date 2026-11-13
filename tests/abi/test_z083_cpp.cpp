// test_z083_cpp.cpp - Z-083 C++ consumer: links ONLY
// against build/libmimiralloc.so, proving the three
// memalign-family declarations sit inside the header's
// extern "C" guards (unmangled references, proven in
// the recorded run) and behave from C++: an aligned
// memalign block, a page-aligned valloc block and a
// page-rounded pvalloc block, each freed through
// mimir_free. Exit 0 iff every check passes.
#include "mimir_alloc.h"

#include <cstdint>
#include <cstdio>

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
    void *p = mimir_memalign(128, 200);
    CHECK(p != nullptr);
    if (p != nullptr) {
        CHECK((reinterpret_cast<std::uintptr_t>(p) % 128) == 0);
        mimir_free(p);
    }
    p = mimir_valloc(50);
    CHECK(p != nullptr);
    if (p != nullptr) {
        CHECK((reinterpret_cast<std::uintptr_t>(p) % 4096) == 0);
        mimir_free(p);
    }
    p = mimir_pvalloc(5000);
    CHECK(p != nullptr);
    if (p != nullptr) {
        CHECK(mimir_usable_size(p) >= 8192);
        mimir_free(p);
    }
    CHECK(mimir_memalign(3, 10) == nullptr);
    CHECK(mimir_pvalloc(0) == nullptr);
    std::printf("test_z083_cpp: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
