// test_z075_cpp.cpp - Z-075 C++ consumer (E-series
// style). Linked ONLY against build/libmimiralloc.so: it
// proves the Z-075 declaration sits inside the public
// header's extern "C" guards with its exact prototype
// (a size_t return and a const void * parameter -- the
// recorded D-012 signature: a pure query) and answers
// exactly from C++. Standalone: built and run via
// `make test-z075`; NOT part of `make test`.
#include "mimir_alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

int checks_run = 0;
int checks_failed = 0;

void check(bool ok, const char *expr, int line)
{
    checks_run++;
    if (!ok) {
        checks_failed++;
        std::printf("  [FAIL] %s (line %d)\n", expr, line);
    }
}

/* Consumer-side RAII wrapper: owns one block, answers its
 * usable size through a const pointer on purpose. */
struct Block {
    void *p;
    explicit Block(std::size_t n) : p(mimir_malloc(n)) {}
    ~Block() { mimir_free(p); }
    Block(const Block &) = delete;
    Block &operator=(const Block &) = delete;
    std::size_t usable() const { return mimir_usable_size(p); }
};

} // namespace

#define CHECK(cond) check((cond), #cond, __LINE__)

int main()
{
    const std::size_t ps =
        static_cast<std::size_t>(sysconf(_SC_PAGESIZE));

    std::printf("Z-075 C++ consumer (libmimiralloc.so only)\n");
    CHECK(mimir_usable_size(nullptr) == 0);
    {
        Block b(200);
        CHECK(b.p != nullptr);
        CHECK(b.usable() == 224);
        const void *cp = b.p;
        CHECK(mimir_usable_size(cp) == 224);
    }
    {
        void *p = mimir_malloc(5000);
        CHECK(p != nullptr);
        CHECK(mimir_usable_size(p) ==
              ((5000 + ps - 1) & ~(ps - 1)));
        mimir_free(p);
        CHECK(mimir_usable_size(p) == 0);
    }
    std::printf("Z075 CPP: %d checks, %d failed\n", checks_run,
                checks_failed);
    std::printf("RESULT: %s\n",
                checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
