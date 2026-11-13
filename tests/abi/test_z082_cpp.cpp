/* test_z082_cpp.cpp - Z-082 C++ consumer: links ONLY
 * against build/libmimiralloc_interpose.so. Exercises
 * the standard names from C++ and -- the real
 * interposition evidence -- C++ new/delete, which
 * libstdc++ routes through the public malloc/free:
 * under this library those calls land on the interposed
 * definitions (the binding is proven in the recorded
 * run by the consumer's DT_NEEDED and by the D-013
 * zero-size answer below, which glibc would not give).
 * Exit 0 iff every check passes. */
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <new>

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
    /* Standard names from C++. */
    unsigned char *p = static_cast<unsigned char *>(std::malloc(64));
    CHECK(p != nullptr);
    for (std::size_t i = 0; i < 64; i++) {
        p[i] = static_cast<unsigned char>(i + 11u);
    }
    p = static_cast<unsigned char *>(std::realloc(p, 128));
    CHECK(p != nullptr);
    for (std::size_t i = 0; i < 64; i++) {
        CHECK(p[i] == static_cast<unsigned char>(i + 11u));
    }
    /* D-013 under the standard name: malloc(0) is NULL
     * here; glibc would return a distinct pointer. */
    errno = 77; CHECK(std::malloc(0) == nullptr); CHECK(errno == 77);
    std::free(p);

    /* new/delete ride on the interposed malloc/free. */
    int *one = new int(41);
    CHECK(*one == 41);
    *one += 1;
    CHECK(*one == 42);
    delete one;

    int *arr = new int[16];
    for (int i = 0; i < 16; i++) {
        arr[i] = i * i;
    }
    for (int i = 0; i < 16; i++) {
        CHECK(arr[i] == i * i);
    }
    delete[] arr;

    std::printf("test_z082_cpp: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
