/* test_z074_cpp.cpp - Z-074 C++ consumer (E-007 route).
 *
 * A C++ translation unit consuming the two Wave 9 entry
 * points through the public header's extern "C" guards,
 * linked ONLY against build/libmimiralloc.so. It proves
 * the new declarations are consumable from C++ with
 * unmangled references (the runner checks this object's
 * undefined symbols), that the lifecycle behaves from C++
 * as the contract states, and that an aligned block is
 * released through mimir_free from C++ (the RAII wrapper
 * below is consumer-side only, exactly as in E-007; it is
 * not a library surface and no C++ native surface is
 * added or interposed).
 *
 * Standalone: built and run via `make test-z074`; NOT
 * part of `make test`.
 */
#include "mimir_alloc.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

int checks_run = 0;
int checks_failed = 0;

void check(bool cond, const char *expr, int line)
{
    checks_run++;
    if (!cond) {
        checks_failed++;
        std::printf("  [FAIL] %s (line %d)\n", expr, line);
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

class AlignedBlock {
public:
    AlignedBlock(std::size_t alignment, std::size_t size)
        : ptr_(mimir_aligned_alloc(alignment, size))
    {
    }
    ~AlignedBlock() { mimir_free(ptr_); }
    AlignedBlock(const AlignedBlock &) = delete;
    AlignedBlock &operator=(const AlignedBlock &) = delete;
    void *get() const { return ptr_; }

private:
    void *ptr_;
};

} // namespace

int main()
{
    std::printf("Z-074 C++ consumer (libmimiralloc.so only)\n");
    {
        AlignedBlock block(256, 512);
        CHECK(block.get() != nullptr);
        CHECK(reinterpret_cast<std::uintptr_t>(block.get()) %
                  256 ==
              0);
        std::memset(block.get(), 0x5A, 512);
        const auto *bytes =
            static_cast<const unsigned char *>(block.get());
        CHECK(bytes[0] == 0x5A && bytes[511] == 0x5A);
    } /* freed here through mimir_free by the wrapper */
    {
        void *out = nullptr;
        CHECK(mimir_posix_memalign(&out, 8192, 64) == 0);
        CHECK(out != nullptr);
        CHECK(reinterpret_cast<std::uintptr_t>(out) % 8192 == 0);
        /* Cross-entry over the .so from C++: realloc + free. */
        void *grown = mimir_realloc(out, 4096);
        CHECK(grown != nullptr);
        mimir_free(grown != nullptr ? grown : out);
        void *sentinel = reinterpret_cast<void *>(std::uintptr_t{1});
        out = sentinel;
        CHECK(mimir_posix_memalign(&out, 4, 64) == EINVAL);
        CHECK(out == sentinel);
        CHECK(mimir_aligned_alloc(0, 64) == nullptr);
        CHECK(mimir_aligned_alloc(48, 64) == nullptr);
    }
    std::printf("Z074 CPP: %d checks, %d failed\n", checks_run,
                checks_failed);
    std::printf("RESULT: %s\n",
                checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
