/* mimir_trace_z094.c - MimirAlloc Z-094: the audit-hook
 * trace capture library (Wave 11, real workload traces).
 *
 * An LD_PRELOAD interposition shim: it forwards every
 * malloc-family call it interposes to the real glibc entry
 * (dlsym RTLD_NEXT) unchanged, and appends one compact
 * text record per call to a per-process raw trace file.
 * This is the "audit-hook route" the roadmap names for
 * Z-094: the allocator boundary itself is the hook, the
 * traced program runs on plain glibc, and MimirAlloc is
 * not in the capture path at all - traces record what
 * real programs ASK FOR, leg-independently. The replay
 * harness (tests/perf/replay_z094.py) later re-runs a
 * canonical trace against any allocator leg.
 *
 * Interposed set (exactly these eight): malloc, free,
 * calloc, realloc, reallocarray, aligned_alloc,
 * posix_memalign, memalign. Everything else (strdup and
 * kin, __libc_* internal allocations, mmap-served
 * requests inside other libraries) is OUTSIDE the trace
 * boundary by construction and is named as such in the
 * slice doc - those calls still work, served by glibc;
 * they are simply not PLT-boundary calls from the traced
 * program and its non-libc libraries.
 *
 * Raw record grammar (one per line, shim-local ids):
 *   M <id> <size>            malloc; id 0 = returned NULL
 *   C <id> <nmemb> <size>    calloc; id 0 = returned NULL
 *   A <id> <align> <size>    aligned_alloc / memalign /
 *                            posix_memalign success;
 *                            id 0 = failure / refusal
 *   R <old> <new> <size>     realloc(-array) success;
 *                            new==old = in place; the
 *                            retire form is R <old> 0 0
 *                            (size 0: glibc freed the
 *                            block and returned NULL)
 *   E <old> <size>           realloc(-array) failure:
 *                            NULL returned, old block
 *                            retained live and unchanged
 *   F <id>                   free; id 0 = pointer the
 *                            shim never issued (foreign:
 *                            still forwarded to real free)
 * Ids are per-process, monotone from 1, and identify a
 * POINTER (a realloc move retires the old id's pointer
 * and issues a fresh id for the new pointer; the
 * summariser folds moves back to one logical identity).
 *
 * Safety engineering (this library is preloaded into
 * real programs; a tracer bug must never become a
 * workload bug):
 * - All interposed calls serialise on one spinlock, held
 *   across the real call, the id-map update and the log
 *   append, so the raw stream's order is a true serial
 *   order of completed calls and the map can never race.
 *   The price is throughput under malloc contention;
 *   Z-094 traces are correctness artefacts, not timings,
 *   and no number from a traced run is a benchmark.
 * - Symbol resolution is lazy via dlsym(RTLD_NEXT); a
 *   calloc arriving mid-resolution is served from a
 *   static bootstrap arena and never logged (it is the
 *   resolver's own allocation, not the program's), and
 *   freeing an arena pointer is a no-op.
 * - Fork: a pthread_atfork child handler resets ALL
 *   per-process shim state (id map, id counter, log
 *   fd and buffer, op counter, spinlock), so every
 *   process raw stream is self-contained. A child
 *   freeing a pointer its parent allocated logs F 0
 *   (foreign to the child stream - exactly what it
 *   is) instead of an inherited shim id the child
 *   stream never issued; without this, fork-heavy
 *   workloads (the git churn capture) produced
 *   unknown-free streams the summariser rightly
 *   refused. vfork is out of scope: a vfork child
 *   may only exec/_exit before touching the heap.
 * - Flushes are record-aligned (see log_flush): a
 *   killed process loses at most its final buffered
 *   whole records, never half a record.
 * - Logging uses only write(2) on an fd opened from
 *   MIMIR_TRACE_DIR (file trace.<pid>.raw), a static
 *   buffer, and hand-rolled integer formatting: no stdio,
 *   no allocation, no PLT call that could re-enter the
 *   shim. A fork child detects the pid change, drops the
 *   inherited buffer and opens its own file.
 * - If MIMIR_TRACE_DIR is unset the shim is a pure
 *   pass-through (no file, no map, no logging).
 * - A per-process op cap (100M records) stops logging
 *   (one T record) rather than fill a disk; the
 *   summariser refuses a capped stream, so a truncated
 *   trace can never pose as complete.
 * Compiled -fno-builtin (Makefile Z-094 block) so the
 * compiler never rewrites these definitions with builtin
 * malloc-family assumptions. Not part of `make test`;
 * the standing gate is untouched by this slice.
 */
#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

/* ---- real entry points (resolved lazily) ---- */
static void *(*real_malloc)(size_t);
static void (*real_free)(void *);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void *(*real_reallocarray)(void *, size_t, size_t);
static void *(*real_aligned_alloc)(size_t, size_t);
static void *(*real_memalign)(size_t, size_t);
static int (*real_posix_memalign)(void **, size_t, size_t);

static __thread int resolving;

/* ---- bootstrap arena (resolver-time calloc) ---- */
static char arena[65536];
static size_t arena_off;
static int in_arena(const void *p)
{
    return p >= (const void *)arena &&
           p < (const void *)(arena + sizeof arena);
}
static void *arena_alloc(size_t n, size_t align)
{
    uintptr_t cur = (uintptr_t)(arena + arena_off);
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t off = (size_t)(aligned - (uintptr_t)arena);
    if (off + n > sizeof arena)
        return NULL;
    arena_off = off + n;
    return (void *)aligned;
}

/* ---- spinlock ---- */
static volatile int shim_lock;
static void lock(void)
{
    while (__sync_lock_test_and_set(&shim_lock, 1)) {
        /* spin: critical sections are one real call */
    }
}
static void unlock(void)
{
    __sync_lock_release(&shim_lock);
}

/* ---- id map: pointer -> shim id (open addressing) ---- */
#define MAP_BITS 22
#define MAP_SIZE ((size_t)1 << MAP_BITS)
#define MAP_TOMB ((uint64_t) ~(uint64_t)0)
struct mapent { void *ptr; uint64_t id; };
static struct mapent idmap[MAP_SIZE];
static uint64_t next_id = 1;
static size_t map_used;

static size_t map_hash(const void *p)
{
    uint64_t x = (uint64_t)((uintptr_t)p >> 4);
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (size_t)x & (MAP_SIZE - 1);
}
static uint64_t map_lookup(const void *p)
{
    size_t i = map_hash(p);
    while (idmap[i].ptr) {
        if (idmap[i].ptr == p)
            return idmap[i].id == MAP_TOMB ? 0 : idmap[i].id;
        i = (i + 1) & (MAP_SIZE - 1);
    }
    return 0;
}
static uint64_t map_insert(void *p)
{
    size_t i = map_hash(p);
    uint64_t id;
    if (map_used > (MAP_SIZE / 4) * 3)
        return 0; /* saturated: record as unissued */
    while (idmap[i].ptr && idmap[i].ptr != p &&
           idmap[i].id != MAP_TOMB)
        i = (i + 1) & (MAP_SIZE - 1);
    if (!idmap[i].ptr)
        map_used++;
    id = next_id++;
    idmap[i].ptr = p;
    idmap[i].id = id;
    return id;
}
static uint64_t map_remove(const void *p)
{
    size_t i = map_hash(p);
    while (idmap[i].ptr) {
        if (idmap[i].ptr == p) {
            uint64_t id = idmap[i].id == MAP_TOMB ? 0 : idmap[i].id;
            idmap[i].id = MAP_TOMB;
            return id;
        }
        i = (i + 1) & (MAP_SIZE - 1);
    }
    return 0;
}

/* ---- logging ---- */
static int log_fd = -2; /* -2 = untried, -1 = disabled */
static char log_buf[65536];
static size_t log_len;
static pid_t log_pid;
static uint64_t op_count;
static int op_capped;
#define OP_CAP ((uint64_t)100000000)

static void log_flush(void)
{
    /* Record-aligned flush: write only up to the last
     * newline in the buffer and keep any partial tail
     * record buffered. Appends are record-atomic under
     * the spinlock, so the buffer always holds whole
     * records plus at most one in-progress tail record
     * (only while log_putc is mid-record at a buffer
     * boundary). A process killed at any instant - the
     * Ollama capture kills its server by design - thus
     * leaves a stream of whole records: a true prefix
     * of its op stream, never a torn record. The first
     * version flushed byte-wise and the killed Ollama
     * server streams ended mid-record (M 24 / M 34423
     * tails at exact 64 KiB multiples); the summariser
     * now also refuses any stream not ending at a
     * record boundary, so a torn stream can never pose
     * as complete even if some future path emits one. */
    size_t end = log_len, off = 0;
    while (end && log_buf[end - 1] != '\n')
        end--;
    if (end == 0)
        return; /* nothing record-complete buffered yet */
    while (off < end) {
        ssize_t w = write(log_fd, log_buf + off, end - off);
        if (w <= 0) {
            /* Stream broken (e.g. ENOSPC): stop logging
             * rather than risk a torn record. What is
             * already in the file stays a true prefix. */
            log_fd = -1;
            log_len = 0;
            return;
        }
        off += (size_t)w;
    }
    log_len -= end;
    if (log_len)
        memmove(log_buf, log_buf + end, log_len);
}
static void log_open(void)
{
    const char *dir = getenv("MIMIR_TRACE_DIR");
    char path[1024];
    size_t n = 0;
    const char *s;
    pid_t pid = getpid();
    if (!dir || !*dir) {
        log_fd = -1;
        return;
    }
    for (s = dir; *s && n + 32 < sizeof path; s++)
        path[n++] = *s;
    for (s = "/trace."; *s; s++)
        path[n++] = *s;
    {
        char num[24];
        int ni = 0;
        unsigned int v = (unsigned int)pid;
        if (v == 0)
            num[ni++] = '0';
        while (v) {
            num[ni++] = (char)('0' + (v % 10));
            v /= 10;
        }
        while (ni)
            path[n++] = num[--ni];
    }
    for (s = ".raw"; *s; s++)
        path[n++] = *s;
    path[n] = '\0';
    log_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    log_pid = pid;
    log_len = 0;
}
static void log_check_pid(void)
{
    if (log_fd == -2)
        log_open();
    if (log_fd >= 0 && getpid() != log_pid) {
        /* Fork child whose atfork handler did not run
         * (defensive path; the handler is the primary
         * mechanism): never write the parent's buffer,
         * and drop the inherited id map for the same
         * reason the handler does. */
        memset(idmap, 0, sizeof idmap);
        next_id = 1;
        map_used = 0;
        op_count = 0;
        op_capped = 0;
        log_len = 0;
        log_fd = -2;
        log_open();
    }
}
static void log_putc(char c)
{
    if (log_len == sizeof log_buf)
        log_flush();
    if (log_len < sizeof log_buf)
        log_buf[log_len++] = c;
}
static void log_u64(uint64_t v)
{
    char num[24];
    int ni = 0;
    if (v == 0)
        num[ni++] = '0';
    while (v) {
        num[ni++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (ni)
        log_putc(num[--ni]);
}
static void log_rec1(char op, uint64_t a)
{
    log_putc(op); log_putc(' '); log_u64(a); log_putc('\n');
}
static void log_rec2(char op, uint64_t a, uint64_t b)
{
    log_putc(op); log_putc(' '); log_u64(a); log_putc(' ');
    log_u64(b); log_putc('\n');
}
static void log_rec3(char op, uint64_t a, uint64_t b, uint64_t c)
{
    log_putc(op); log_putc(' '); log_u64(a); log_putc(' ');
    log_u64(b); log_putc(' '); log_u64(c); log_putc('\n');
}
/* returns 1 if this record may be written */
static int log_gate(void)
{
    log_check_pid();
    if (log_fd < 0)
        return 0;
    if (op_capped)
        return 0;
    if (op_count >= OP_CAP) {
        op_capped = 1;
        log_rec1('T', op_count);
        log_flush();
        return 0;
    }
    op_count++;
    return 1;
}

__attribute__((destructor)) static void trace_fini(void)
{
    if (log_fd >= 0 && log_len)
        log_flush();
}

/* ---- fork handling: per-process stream isolation ---- */
static void trace_fork_child(void)
{
    /* Runs in the fork child before fork() returns,
     * with only the calling thread present: safe to
     * reset every piece of inherited shim state
     * outright. Pointers the parent allocated are
     * foreign to this process own stream from here
     * on (see header). */
    shim_lock = 0;
    memset(idmap, 0, sizeof idmap);
    next_id = 1;
    map_used = 0;
    log_fd = -2;
    log_len = 0;
    log_pid = 0;
    op_count = 0;
    op_capped = 0;
}

__attribute__((constructor)) static void trace_init(void)
{
    (void)pthread_atfork(NULL, NULL, trace_fork_child);
}

/* ---- resolution ---- */
static void resolve_all(void)
{
    if (real_malloc)
        return;
    resolving = 1;
    real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
    real_calloc = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    real_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    real_reallocarray = (void *(*)(void *, size_t, size_t))
        dlsym(RTLD_NEXT, "reallocarray");
    real_aligned_alloc = (void *(*)(size_t, size_t))
        dlsym(RTLD_NEXT, "aligned_alloc");
    real_memalign = (void *(*)(size_t, size_t))
        dlsym(RTLD_NEXT, "memalign");
    real_posix_memalign = (int (*)(void **, size_t, size_t))
        dlsym(RTLD_NEXT, "posix_memalign");
    resolving = 0;
}

/* ---- the interposed eight ---- */
void *malloc(size_t size)
{
    void *r;
    uint64_t id = 0;
    if (resolving)
        return arena_alloc(size ? size : 16, 16);
    lock();
    resolve_all();
    r = real_malloc(size);
    if (r)
        id = map_insert(r);
    if (log_gate())
        log_rec2('M', id, (uint64_t)size);
    unlock();
    return r;
}

void free(void *ptr)
{
    uint64_t id;
    if (!ptr)
        return;
    if (in_arena(ptr))
        return;
    if (resolving) {
        /* cannot forward safely mid-resolution; a pointer
         * freed here was arena- or libc-born; leaking it
         * during the resolver's own window is the safe
         * failure mode and is confined to that window. */
        return;
    }
    lock();
    resolve_all();
    id = map_remove(ptr);
    if (log_gate())
        log_rec1('F', id);
    real_free(ptr);
    unlock();
}

void *calloc(size_t nmemb, size_t size)
{
    void *r;
    uint64_t id = 0;
    if (resolving) {
        size_t total = nmemb * size;
        void *p = arena_alloc(total ? total : 16, 16);
        if (p)
            memset(p, 0, total);
        return p;
    }
    lock();
    resolve_all();
    r = real_calloc(nmemb, size);
    if (r)
        id = map_insert(r);
    if (log_gate())
        log_rec3('C', id, (uint64_t)nmemb, (uint64_t)size);
    unlock();
    return r;
}

static void *realloc_common(void *ptr, size_t size, int is_array,
                            size_t nmemb)
{
    void *r;
    uint64_t oldid = 0, newid = 0;
    if (resolving)
        return NULL;
    lock();
    resolve_all();
    if (ptr && !in_arena(ptr))
        oldid = map_lookup(ptr);
    if (is_array)
        r = real_reallocarray(ptr, nmemb, size);
    else
        r = real_realloc(ptr, size);
    if (ptr && in_arena(ptr)) {
        /* arena block being grown into the real heap:
         * copy out, present as a fresh allocation */
        if (r && size)
            memcpy(r, ptr, size < 65536 ? size : 65536);
        unlock();
        return r;
    }
    if (!r && size == 0 && ptr) {
        /* glibc retire form: block freed, NULL returned */
        if (oldid)
            map_remove(ptr);
        if (log_gate())
            log_rec3('R', oldid, 0, 0);
        unlock();
        return NULL;
    }
    if (!r && ptr) {
        /* genuine failure: old block retained */
        if (log_gate()) {
            uint64_t total = (uint64_t)size;
            if (is_array) {
                __uint128_t t = (__uint128_t)nmemb * size;
                total = t > SIZE_MAX ? SIZE_MAX : (uint64_t)t;
            }
            log_rec2('E', oldid, total);
        }
        unlock();
        return NULL;
    }
    if (!r && !ptr) {
        /* realloc(NULL, size) that failed = malloc failure */
        if (log_gate()) {
            uint64_t total = (uint64_t)size;
            if (is_array) {
                __uint128_t t = (__uint128_t)nmemb * size;
                total = t > SIZE_MAX ? SIZE_MAX : (uint64_t)t;
                log_rec3('C', 0, (uint64_t)nmemb, (uint64_t)size);
                unlock();
                return NULL;
            }
            log_rec2('M', 0, total);
        }
        unlock();
        return NULL;
    }
    /* success */
    if (r == ptr) {
        newid = oldid;
    } else {
        if (oldid)
            map_remove(ptr);
        newid = map_insert(r);
    }
    if (log_gate()) {
        uint64_t total = (uint64_t)size;
        if (is_array) {
            __uint128_t t = (__uint128_t)nmemb * size;
            total = t > SIZE_MAX ? SIZE_MAX : (uint64_t)t;
        }
        if (ptr)
            log_rec3('R', oldid, newid, total);
        else if (is_array)
            log_rec3('C', newid, (uint64_t)nmemb, (uint64_t)size);
        else
            log_rec2('M', newid, total);
    }
    unlock();
    return r;
}

void *realloc(void *ptr, size_t size)
{
    return realloc_common(ptr, size, 0, 0);
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
    return realloc_common(ptr, size, 1, nmemb);
}

void *aligned_alloc(size_t alignment, size_t size)
{
    void *r;
    uint64_t id = 0;
    if (resolving)
        return NULL;
    lock();
    resolve_all();
    r = real_aligned_alloc(alignment, size);
    if (r)
        id = map_insert(r);
    if (log_gate())
        log_rec3('A', id, (uint64_t)alignment, (uint64_t)size);
    unlock();
    return r;
}

void *memalign(size_t alignment, size_t size)
{
    void *r;
    uint64_t id = 0;
    if (resolving)
        return NULL;
    lock();
    resolve_all();
    r = real_memalign(alignment, size);
    if (r)
        id = map_insert(r);
    if (log_gate())
        log_rec3('A', id, (uint64_t)alignment, (uint64_t)size);
    unlock();
    return r;
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    int rc;
    uint64_t id = 0;
    if (resolving)
        return ENOMEM;
    lock();
    resolve_all();
    rc = real_posix_memalign(memptr, alignment, size);
    if (rc == 0 && *memptr)
        id = map_insert(*memptr);
    if (log_gate())
        log_rec3('A', id, (uint64_t)alignment, (uint64_t)size);
    unlock();
    return rc;
}
