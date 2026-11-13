/* probe_z097.c - Z-097 short-lived process storm driver
 * (MimirAlloc Z13).
 *
 * Runs ONE fork+exec storm: COUNT sequential children of
 * the Z-097 payload (tests/perf/child_z097.c), each child
 * run to completion before the next is forked - the
 * shell-script / build-farm shape, where a process is
 * created, does a little work, and dies. The storm is
 * timed as a whole with CLOCK_MONOTONIC; per-process cost
 * is elapsed / COUNT. RUSAGE_CHILDREN deltas over the
 * storm give the user/sys CPU split and the minor-fault
 * count the children paid.
 *
 * Worlds (argv[3]):
 *   plain    children exec with the driver's own
 *            environment (glibc world).
 *   preload  children exec with LD_PRELOAD=<ipolib>
 *            added to (or replacing in) the environment:
 *            the Z-082 interposing build
 *            (build/libmimiralloc_interpose.so). The
 *            driver itself is NEVER preloaded - it links
 *            no MimirAlloc artifact and its own allocator
 *            is not part of the measurement; only the
 *            exec'd children live in the measured world.
 *
 * Method (Z-004 protocol shape): ONE storm per process;
 * repetitions are separate driver processes driven by
 * run_z097.sh (taskset-pinned to one core, worlds
 * interleaved rep by rep, median taken by
 * summarize_z097.py). Warm-up storms are the runner's
 * business and are discarded there.
 *
 * Every child must exit 0 - the payload's timed modes
 * exit 0 only when the checksum over the bytes read
 * back from its allocations matches the arithmetic
 * checksum, so a silent allocator fault in a short-lived
 * child shows here as a failed child, not a wrong time.
 *
 * Usage: probe_z097 <child> <mode> <world> <count>
 *        <ipolib-or-dash>
 *   mode: none | first | multi | churn (see child_z097.c)
 *
 * Prints one CSV line:
 *   world,mode,count,elapsed_ns,ns_per_child,user_ns,
 *   sys_ns,minflt_delta,ok_children
 * Exit 0 iff every child exited 0.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static uint64_t ts_ns(const struct timespec *t)
{
    return (uint64_t)t->tv_sec * UINT64_C(1000000000)
         + (uint64_t)t->tv_nsec;
}

static uint64_t tv_ns(const struct timeval *t)
{
    return (uint64_t)t->tv_sec * UINT64_C(1000000000)
         + (uint64_t)t->tv_usec * UINT64_C(1000);
}

int main(int argc, char **argv)
{
    const char *child, *mode, *world, *ipolib;
    long count, i;
    char **envp = environ;
    char *preload_kv = NULL;
    char **newenv = NULL;
    struct rusage ru0, ru1;
    struct timespec t0, t1;
    long ok = 0, failed = 0;

    if (argc != 6) {
        fprintf(stderr,
            "usage: probe_z097 <child> <mode> <world> "
            "<count> <ipolib-or-dash>\n");
        return 2;
    }
    child = argv[1];
    mode = argv[2];
    world = argv[3];
    count = strtol(argv[4], NULL, 10);
    ipolib = argv[5];
    if (count <= 0) {
        fprintf(stderr, "probe_z097: count must be > 0\n");
        return 2;
    }
    if (strcmp(world, "preload") == 0) {
        /* Build the child environment once: the driver's
         * environ with LD_PRELOAD set to the interposing
         * .so (replacing any inherited value). */
        size_t n = 0, k = 0, j;
        int replaced = 0;
        size_t kvlen = strlen("LD_PRELOAD=") + strlen(ipolib) + 1;
        preload_kv = (char *)malloc(kvlen);
        if (preload_kv == NULL) return 2;
        snprintf(preload_kv, kvlen, "LD_PRELOAD=%s", ipolib);
        while (environ[n] != NULL) n++;
        newenv = (char **)malloc((n + 2) * sizeof(char *));
        if (newenv == NULL) return 2;
        for (j = 0; j < n; j++) {
            if (strncmp(environ[j], "LD_PRELOAD=", 11) == 0) {
                if (!replaced) {
                    newenv[k++] = preload_kv;
                    replaced = 1;
                }
                continue;
            }
            newenv[k++] = environ[j];
        }
        if (!replaced) newenv[k++] = preload_kv;
        newenv[k] = NULL;
        envp = newenv;
    } else if (strcmp(world, "plain") != 0) {
        fprintf(stderr, "probe_z097: world must be plain "
                        "or preload\n");
        return 2;
    }

    if (getrusage(RUSAGE_CHILDREN, &ru0) != 0) return 2;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) return 2;
    for (i = 0; i < count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            failed++;
            break;
        }
        if (pid == 0) {
            char *const cargv[4] = {
                (char *)child, (char *)mode, NULL, NULL
            };
            execve(child, cargv, envp);
            _exit(127);
        }
        {
            int st = 0;
            pid_t w;
            do {
                w = waitpid(pid, &st, 0);
            } while (w < 0 && errno == EINTR);
            if (w == pid && WIFEXITED(st)
                && WEXITSTATUS(st) == 0) {
                ok++;
            } else {
                failed++;
            }
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) return 2;
    if (getrusage(RUSAGE_CHILDREN, &ru1) != 0) return 2;

    {
        uint64_t elapsed = ts_ns(&t1) - ts_ns(&t0);
        uint64_t user = tv_ns(&ru1.ru_utime)
                      - tv_ns(&ru0.ru_utime);
        uint64_t sys = tv_ns(&ru1.ru_stime)
                     - tv_ns(&ru0.ru_stime);
        long minflt = ru1.ru_minflt - ru0.ru_minflt;
        printf("%s,%s,%ld,%llu,%llu,%llu,%llu,%ld,%ld\n",
               world, mode, count,
               (unsigned long long)elapsed,
               (unsigned long long)(elapsed
                   / (uint64_t)count),
               (unsigned long long)user,
               (unsigned long long)sys,
               minflt, ok);
        fflush(stdout);
    }
    free(preload_kv);
    free(newenv);
    return failed == 0 && ok == count ? 0 : 1;
}
