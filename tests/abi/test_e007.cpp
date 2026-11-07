/* test_e007.cpp - E-007 C++ consumption consumer (Phase 5, Wave 5).
 *
 * This program is a C++ translation unit that consumes the
 * banked C contract exactly as the A-009 implementation-
 * language decision requires: the core is portable C11, the
 * C ABI is the core's own ABI, and C++ consumes that C ABI
 * through the existing extern "C" guards in
 * include/mimir_alloc.h. It links ONLY against
 * build/libmimiralloc.so (the banked E-002 shared library,
 * via make lib) in the R2 dynamic route named by E-004, and
 * exercises the full allocation lifecycle through the four
 * public symbols only: mimir_malloc, mimir_free,
 * mimir_calloc, mimir_realloc.
 *
 * What this proves (with the binding evidence in
 * tests/abi/recorded_run_e007.txt): the header is
 * consumable from C++ with no header change, the four
 * references leave this translation unit UNMANGLED (extern
 * "C" linkage), they bind at load/run time to the C
 * library, and the lifecycle behaves from C++ exactly as
 * the by-analogy contract states.
 *
 * What this does NOT prove or add: no C++ native surface.
 * No operator new / operator delete of any form is defined,
 * declared, or interposed here or in the library (A-008
 * S2.10 family is CONDITIONAL, U-06 open, E-001 non-goal
 * N4); no C++-name entry point exists; the public surface
 * stays exactly the four C symbols. The MimirBuffer class
 * below is a consumer-side RAII wrapper in this test only:
 * it owns a mimir_malloc block and releases it with
 * mimir_free on destruction. It is not a library surface,
 * not a Layer-1 shim deliverable, and it exercises the
 * normal destruction path only - no exception-path claim
 * is made.
 *
 * C++ consumption discipline visible in this file: void*
 * results are converted with static_cast (C++ has no
 * implicit void* conversion), the null pointer is spelled
 * nullptr, and alignment is checked against
 * alignof(std::max_align_t) - the C++ spelling of the
 * D-008 / E-006 operational floor. Zero-size behaviour is
 * NOT checked ([UNRESOLVED: U-09]); errno is NOT checked
 * (U-10). Behaviour is by analogy under
 * [UNRESOLVED: U-03], exactly as the public header states.
 *
 * Standalone on purpose: built and run via make test-e007;
 * NOT part of make test. See
 * docs/PHASE_5_CPP_CONSUMPTION_E007.md.
 */
#include "mimir_alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const char *name)
{
    if (condition) {
        passed++;
        std::printf("PASS %s\n", name);
    } else {
        failed++;
        std::printf("FAIL %s\n", name);
    }
}

/* Consumer-side RAII wrapper (test-local; not a library
 * surface). Owns one mimir_malloc block; the destructor
 * releases it with mimir_free. Copying is disabled. */
class MimirBuffer {
public:
    explicit MimirBuffer(std::size_t n)
        : ptr_(static_cast<unsigned char *>(mimir_malloc(n))),
          size_(n)
    {
    }

    ~MimirBuffer() { mimir_free(ptr_); }

    MimirBuffer(const MimirBuffer &) = delete;
    MimirBuffer &operator=(const MimirBuffer &) = delete;

    unsigned char *data() { return ptr_; }
    std::size_t size() const { return size_; }
    bool valid() const { return ptr_ != nullptr; }

private:
    unsigned char *ptr_;
    std::size_t size_;
};

} // namespace

int main()
{
    std::printf("E-007 C++ consumption consumer (dynamic, libmimiralloc.so)\n");

    /* Two live malloc blocks: distinct, aligned, patterns
     * intact. Sizes and patterns differ from E-004's C
     * consumer so this evidence is independent. */
    unsigned char *a = static_cast<unsigned char *>(mimir_malloc(128));
    check(a != nullptr, "mimir_malloc(128) returns non-null");
    unsigned char *b = static_cast<unsigned char *>(mimir_malloc(128));
    check(b != nullptr, "second mimir_malloc(128) returns non-null");
    if (a != nullptr) {
        check((reinterpret_cast<std::uintptr_t>(a) %
               alignof(std::max_align_t)) == 0,
              "mimir_malloc(128) aligned to alignof(std::max_align_t)");
    }
    if (a != nullptr && b != nullptr) {
        check(a != b, "two live malloc blocks are distinct");
        std::memset(a, 0xA5, 128);
        std::memset(b, 0x5A, 128);
        bool ok = true;
        for (std::size_t i = 0; i < 128; i++) {
            if (a[i] != 0xA5 || b[i] != 0x5A) { ok = false; }
        }
        check(ok, "live malloc blocks hold distinct patterns undisturbed");
    }

    /* calloc: zeroed at return, then writable. */
    unsigned char *z =
        static_cast<unsigned char *>(mimir_calloc(16, 16)); /* 256 bytes */
    check(z != nullptr, "mimir_calloc(16, 16) returns non-null");
    unsigned char *g = nullptr;
    if (z != nullptr) {
        bool ok = true;
        for (std::size_t i = 0; i < 256; i++) {
            if (z[i] != 0) { ok = false; }
        }
        check(ok, "mimir_calloc(16, 16) storage reads zero at return");
        for (std::size_t i = 0; i < 256; i++) {
            z[i] = static_cast<unsigned char>(0xFFu - (i & 0xFFu));
        }

        /* realloc grow: prefix preserved byte-for-byte. */
        g = static_cast<unsigned char *>(mimir_realloc(z, 768));
        check(g != nullptr, "mimir_realloc(block, 768) grow returns non-null");
        if (g != nullptr) {
            ok = true;
            for (std::size_t i = 0; i < 256; i++) {
                if (g[i] != static_cast<unsigned char>(0xFFu - (i & 0xFFu))) {
                    ok = false;
                }
            }
            check(ok, "mimir_realloc grow preserves first 256 bytes");
            for (std::size_t i = 256; i < 768; i++) { g[i] = 0x6B; }

            /* realloc shrink: surviving prefix preserved. */
            unsigned char *s =
                static_cast<unsigned char *>(mimir_realloc(g, 48));
            check(s != nullptr,
                  "mimir_realloc(block, 48) shrink returns non-null");
            if (s != nullptr) {
                ok = true;
                for (std::size_t i = 0; i < 48; i++) {
                    if (s[i] !=
                        static_cast<unsigned char>(0xFFu - (i & 0xFFu))) {
                        ok = false;
                    }
                }
                check(ok, "mimir_realloc shrink preserves first 48 bytes");
                mimir_free(s);
            } else {
                mimir_free(g);
            }
        } else {
            mimir_free(z);
        }
    }

    /* realloc(nullptr, n) acts as malloc (the C++ spelling
     * of the E-004 realloc(NULL, n) case). */
    unsigned char *n =
        static_cast<unsigned char *>(mimir_realloc(nullptr, 64));
    check(n != nullptr, "mimir_realloc(nullptr, 64) returns non-null");
    if (n != nullptr) {
        std::memset(n, 0x91, 64);
        check(n[0] == 0x91 && n[63] == 0x91,
              "mimir_realloc(nullptr, 64) storage writable over full extent");
        mimir_free(n);
    }

    /* C++ class consumption: an RAII wrapper object owns a
     * block through the C surface; on scope exit its
     * destructor releases the block with mimir_free. */
    {
        MimirBuffer buf(96);
        check(buf.valid(), "MimirBuffer(96) holds non-null storage");
        check(buf.size() == 96, "MimirBuffer(96) reports size 96");
        bool ok = false;
        if (buf.valid()) {
            for (std::size_t i = 0; i < buf.size(); i++) {
                buf.data()[i] = static_cast<unsigned char>(i ^ 0x3Cu);
            }
            ok = true;
            for (std::size_t i = 0; i < buf.size(); i++) {
                if (buf.data()[i] !=
                    static_cast<unsigned char>(i ^ 0x3Cu)) {
                    ok = false;
                }
            }
        }
        check(ok, "MimirBuffer storage writable and read back over 96 bytes");
    } /* buf destroyed here: mimir_free called by ~MimirBuffer */

    unsigned char *after =
        static_cast<unsigned char *>(mimir_malloc(80));
    check(after != nullptr,
          "mimir_malloc(80) after MimirBuffer destruction returns non-null");
    if (after != nullptr) {
        std::memset(after, 0x2D, 80);
        check(after[0] == 0x2D && after[79] == 0x2D,
              "mimir_malloc(80) storage writable over full extent");
        mimir_free(after);
    }

    /* Oversize and calloc-overflow refusals: exactly null. */
    check(mimir_malloc(static_cast<std::size_t>(-1)) == nullptr,
          "mimir_malloc(SIZE_MAX) returns null (oversize refusal)");
    check(mimir_calloc(static_cast<std::size_t>(-1), 2) == nullptr,
          "mimir_calloc(SIZE_MAX, 2) returns null (overflow refusal)");

    /* free(nullptr) no-op; free live blocks; allocator
     * healthy after. */
    mimir_free(nullptr);
    check(true, "mimir_free(nullptr) returns normally (defined no-op)");
    if (a != nullptr) { mimir_free(a); }
    if (b != nullptr) { mimir_free(b); }
    check(true, "mimir_free of live malloc blocks returns normally");
    unsigned char *again =
        static_cast<unsigned char *>(mimir_malloc(72));
    check(again != nullptr, "mimir_malloc(72) after frees returns non-null");
    if (again != nullptr) {
        std::memset(again, 0xC3, 72);
        check(again[0] == 0xC3 && again[71] == 0xC3,
              "mimir_malloc(72) storage writable over full extent");
        mimir_free(again);
    }

    std::printf("E007 %d/%d checks passed\n", passed, passed + failed);
    std::printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
