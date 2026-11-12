/* mimir_trim_policy_z050.h - TEST-ONLY declarations for
 * slice Z-050 (trim policy under pressure,
 * docs/Z13_TRIM_POLICY.md).
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by Z-050. The declarations here mirror
 * src/core/mimir_core.h (the defining translation unit sees
 * its own prototypes there, M-003 discipline): internal core
 * entry points in the INC-E001-07 shape (b), reachable only
 * by linking the library OBJECTS directly, as
 * `make test-z050` does. The shared library compiles the
 * core object with hidden visibility and the version script
 * binds every non-public name local, so none of them appears
 * in libmimiralloc.so's dynamic symbol table (nm -D stays
 * exactly 4).
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_pressure_parse(text, out): pure parser for the
 *   two-line /proc/pressure/memory format. 0 on success;
 *   -1 (out untouched) on NULL, missing fields, or text
 *   lacking the PSI avg300 marker.
 * - mimir_core_pressure_read(out): fopen/fread of the live
 *   /proc/pressure/memory, then the parser. -1 on any
 *   failure; the policy treats failure as fail-calm.
 * - mimir_trim_decision_for(sample, idle_seconds): the pure
 *   decision table of docs/Z13_TRIM_POLICY.md S2.
 *   HIGH (some_avg10 >= 5.00 or full_avg10 >= 2.50) -> FULL
 *   at any idle; RAISED (some_avg10 >= 1.00) -> FULL at
 *   idle >= 10 s, else PARTIAL; CALM -> PARTIAL at
 *   idle >= 60 s, else NONE. NULL sample -> NONE.
 * - mimir_core_trim_policy_apply(sample, idle, out_dec):
 *   executes the decision through the I-003 trim surface
 *   (FULL = trim_retained, PARTIAL = trim_retained_some of
 *   half the retained spans rounded up, NONE = nothing)
 *   and returns the spans actually released.
 * - mimir_core_trim_policy_tick(report): one live
 *   evaluation -- PSI read + idle derivation from the J-002
 *   activity counters + apply + before/after Q-RETAINED.
 *   0 on success; -1 (nothing trimmed) on read failure.
 */
#ifndef MIMIR_TRIM_POLICY_Z050_H
#define MIMIR_TRIM_POLICY_Z050_H

#include <stddef.h>
#include <stdint.h>

struct mimir_pressure_sample {
    double some_avg10;
    double full_avg10;
    uint64_t some_total_us;
    uint64_t full_total_us;
};

#define MIMIR_TRIM_DECISION_NONE   0
#define MIMIR_TRIM_DECISION_PARTIAL 1
#define MIMIR_TRIM_DECISION_FULL   2

struct mimir_trim_policy_report {
    struct mimir_pressure_sample sample;
    uint64_t idle_seconds;
    int decision;
    size_t released_spans;
    size_t retained_before;
    size_t retained_after;
};

int mimir_pressure_parse(const char *text,
                         struct mimir_pressure_sample *out);
int mimir_core_pressure_read(struct mimir_pressure_sample *out);
int mimir_trim_decision_for(const struct mimir_pressure_sample *sample,
                            uint64_t idle_seconds);
size_t mimir_core_trim_policy_apply(
    const struct mimir_pressure_sample *sample,
    uint64_t idle_seconds, int *out_decision);
int mimir_core_trim_policy_tick(struct mimir_trim_policy_report *report);

#endif /* MIMIR_TRIM_POLICY_Z050_H */
