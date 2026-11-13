/* trace_capture_z095.c - Z-095 trace capture shim (MimirAlloc Z13).
 *
 * An LD_PRELOAD interposer that forwards the malloc family to
 * the host glibc (dlsym RTLD_NEXT) and logs every operation to
 * a binary file, so a REAL program's allocation stream on this
 * host can be replayed later by probe_z095.c. This is Z-095's
 * own capture road, built because Z-094's canonical traces are
 * not yet banked; Z-095's tuning must be re-validated against
 * Z-094's traces when they bank (recorded as OWED).
 *
 * Log format (fixed 32-byte records, native endian):
 *   struct { uint8_t op; uint8_t pad[7];
 *            uint64_t size; uint64_t ptr; uint64_t ptr2; }
 *   op 'A': allocation of `size` bytes, result in `ptr`
 *           (malloc and every memalign-family entry; calloc is
 *           folded to its total size before logging)
 *   op 'R': realloc: old block `ptr`, new size `size`,
 *           result in `ptr2`
 *   op 'F': free of `ptr` (size unused)
 * A record is written only for a non-NULL result (alloc) or a
 * non-NULL argument (free/realloc). Failed allocations are not
 * replayable events and are not logged.
 *
 * Environment:
 *   MIMIR_TRACE_OUT  output path (default /tmp/z095_trace.bin)
 *   MIMIR_TRACE_MAX  stop logging after N records (default
 *                    4000000); the program keeps running,
 *                    forwarded, just unlogged past the cap.
 *
 * Discipline: no stdio, no allocation inside the logging path
 * (raw write(2) from a static 1 MiB buffer), a spinlock makes
 * records atomic across threads, a thread-local guard defeats
 * recursion (dlsym's own calloc is served from a static
 * bootstrap arena and never logged). Record ORDER is the
 * order records reached the buffer, which under concurrency
 * is one valid interleaving, not a per-thread program order;
 * the converter replays that order single-threaded and the
 * recorded run says so.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>

struct rec { uint8_t op; uint8_t pad[7];
             uint64_t size; uint64_t ptr; uint64_t ptr2; };

static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void (*real_free)(void *);
static void *(*real_memalign)(size_t, size_t);
static int (*real_posix_memalign)(void **, size_t, size_t);
static void *(*real_aligned_alloc)(size_t, size_t);
static void *(*real_valloc)(size_t);
static void *(*real_pvalloc)(size_t);

static int trace_fd = -1;
static uint64_t trace_max = 4000000;
static uint64_t trace_count;
static volatile int trace_lock;
static char trace_buf[1 << 20];
static size_t trace_buf_len;
static __thread int in_hook;
static int resolving;

/* Bootstrap arena for dlsym-time calloc (never logged). */
static char boot_arena[65536];
static size_t boot_off;

static void resolve(void)
{
    if (real_malloc) return;
    if (resolving) return;
    resolving = 1;
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    real_free = dlsym(RTLD_NEXT, "free");
    real_memalign = dlsym(RTLD_NEXT, "memalign");
    real_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
    real_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");
    real_valloc = dlsym(RTLD_NEXT, "valloc");
    real_pvalloc = dlsym(RTLD_NEXT, "pvalloc");
    resolving = 0;
}

static void trace_open(void)
{
    const char *path = getenv("MIMIR_TRACE_OUT");
    const char *mx = getenv("MIMIR_TRACE_MAX");
    if (!path) path = "/tmp/z095_trace.bin";
    if (mx) {
        uint64_t v = 0;
        while (*mx >= '0' && *mx <= '9') { v = v * 10 + (uint64_t)(*mx - '0'); mx++; }
        if (v) trace_max = v;
    }
    trace_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

static void lock(void) { while (__atomic_exchange_n(&trace_lock, 1, __ATOMIC_ACQUIRE)) { } }
static void unlock(void) { __atomic_store_n(&trace_lock, 0, __ATOMIC_RELEASE); }

static void emit(uint8_t op, uint64_t size, uint64_t ptr, uint64_t ptr2)
{
    struct rec r;
    if (in_hook) return;
    in_hook = 1;
    lock();
    if (trace_fd < 0) trace_open();
    if (trace_fd >= 0 && trace_count < trace_max) {
        memset(&r, 0, sizeof r);
        r.op = op; r.size = size; r.ptr = ptr; r.ptr2 = ptr2;
        if (trace_buf_len + sizeof r > sizeof trace_buf) {
            ssize_t w = write(trace_fd, trace_buf, trace_buf_len);
            (void)w;
            trace_buf_len = 0;
        }
        memcpy(trace_buf + trace_buf_len, &r, sizeof r);
        trace_buf_len += sizeof r;
        trace_count++;
    }
    unlock();
    in_hook = 0;
}

__attribute__((destructor)) static void trace_flush(void)
{
    if (trace_fd >= 0 && trace_buf_len) {
        ssize_t w = write(trace_fd, trace_buf, trace_buf_len);
        (void)w;
        trace_buf_len = 0;
    }
}

void *malloc(size_t size)
{
    void *p;
    resolve();
    if (!real_malloc) return NULL;
    p = real_malloc(size);
    if (p) emit('A', size, (uint64_t)(uintptr_t)p, 0);
    return p;
}

void *calloc(size_t nmemb, size_t size)
{
    void *p;
    size_t total;
    resolve();
    if (!real_calloc) {
        /* dlsym bootstrap: serve from the static arena. */
        total = nmemb * size;
        if (total == 0) total = 16;
        total = (total + 15) & ~(size_t)15;
        if (boot_off + total > sizeof boot_arena) return NULL;
        p = boot_arena + boot_off;
        boot_off += total;
        memset(p, 0, total);
        return p;
    }
    p = real_calloc(nmemb, size);
    if (p) emit('A', nmemb * size, (uint64_t)(uintptr_t)p, 0);
    return p;
}

void *realloc(void *old, size_t size)
{
    void *p;
    resolve();
    if (!real_realloc) return NULL;
    p = real_realloc(old, size);
    if (p) emit('R', size, (uint64_t)(uintptr_t)old, (uint64_t)(uintptr_t)p);
    else if (!old && size) emit('R', size, 0, 0); /* unreachable in practice */
    return p;
}

void free(void *p)
{
    resolve();
    if (!p) return;
    if ((char *)p >= boot_arena && (char *)p < boot_arena + sizeof boot_arena) return;
    emit('F', 0, (uint64_t)(uintptr_t)p, 0);
    if (real_free) real_free(p);
}

void *memalign(size_t align, size_t size)
{
    void *p;
    resolve();
    if (!real_memalign) return NULL;
    p = real_memalign(align, size);
    if (p) emit('A', size, (uint64_t)(uintptr_t)p, 0);
    return p;
}

void *aligned_alloc(size_t align, size_t size)
{
    void *p;
    resolve();
    if (!real_aligned_alloc) return NULL;
    p = real_aligned_alloc(align, size);
    if (p) emit('A', size, (uint64_t)(uintptr_t)p, 0);
    return p;
}

void *valloc(size_t size)
{
    void *p;
    resolve();
    if (!real_valloc) return NULL;
    p = real_valloc(size);
    if (p) emit('A', size, (uint64_t)(uintptr_t)p, 0);
    return p;
}

void *pvalloc(size_t size)
{
    void *p;
    resolve();
    if (!real_pvalloc) return NULL;
    p = real_pvalloc(size);
    if (p) emit('A', size, (uint64_t)(uintptr_t)p, 0);
    return p;
}

int posix_memalign(void **memptr, size_t align, size_t size)
{
    int rc;
    resolve();
    if (!real_posix_memalign) return ENOMEM;
    rc = real_posix_memalign(memptr, align, size);
    if (rc == 0 && *memptr) emit('A', size, (uint64_t)(uintptr_t)*memptr, 0);
    return rc;
}
