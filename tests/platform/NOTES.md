# C-002 notes — inconsistencies and observations recorded during the slice

1. glibc feature-test fact (found by compile failure, fixed in-slice):
   under a strict `#define _POSIX_C_SOURCE 200809L`, glibc 2.36
   (Debian 12, this Pi) does NOT declare MAP_ANONYMOUS, even though
   MAP_ANONYMOUS is POSIX.1-2024. mimir_platform.c therefore also
   defines _DEFAULT_SOURCE. Any later L6 code compiled with strict
   POSIX feature macros on this target will hit the same wall.
   This is a libc exposure fact, not a design conflict: B-006 S5's
   chosen acquire form (MAP_PRIVATE | MAP_ANONYMOUS) is unchanged.

2. B-007 A.5 records an open inconsistency in B-006's total-mapping
   formula (the exact relation of `metadata_reserve` in S6 to
   `usable_offset` in S7.1 is not settled). C-002 takes no position
   on it and is unaffected: this layer rounds only the size it is
   handed up to a whole number of system pages, with B-007 A5/A6
   checked arithmetic. Whichever reconciliation of A.5 lands, it
   composes above this layer (L5 hands L6 one checked length).

3. B-006 S9.1 recommends a POSIX pthread mutex over C11 mtx_t for
   the simple lock; C-002 implements the recommended choice
   (statically initialised pthread_mutex_t). No inconsistency.
