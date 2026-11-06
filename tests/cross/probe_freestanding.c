/* MimirAlloc D-005 freestanding x86-64 probe.
 *
 * Purpose: prove, end to end, the parts of an x86-64 cross
 * road that exist on the Raspberry Pi today - clang x86-64
 * code generation and an x86-64 link - and, via Box64, the
 * execution of an x86-64 binary on this ARM64 host.
 *
 * This program is FREESTANDING: raw Linux syscalls only,
 * no libc, no headers beyond what the compiler provides.
 * It is NOT the allocator and NOT the portable test suite.
 * Its success proves nothing about running MimirAlloc on
 * x86-64; it proves the codegen + link + emulated-run
 * legs of the road, and isolates the missing leg (an
 * x86-64 libc sysroot) as the only blocker. See
 * docs/PHASE_4_CROSS_COMPILATION_AND_CI.md.
 *
 * Behaviour: writes "D005-X64-PROBE-OK\n" to stdout and
 * exits with status 42 (a distinctive code, so a harness
 * can tell a completed run from a crash or a no-op).
 */
static long sys3(long n, long a, long b, long c)
{
    long r;
    __asm__ volatile("syscall" : "=a"(r)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return r;
}

void _start(void)
{
    static const char msg[] = "D005-X64-PROBE-OK\n";
    sys3(1, 1, (long)msg, (long)sizeof(msg) - 1); /* write */
    sys3(60, 42, 0, 0);                          /* exit  */
    for (;;) {
    }
}
