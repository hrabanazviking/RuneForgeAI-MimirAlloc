#ifndef MIMIR_INTEGRITY_J006_H
#define MIMIR_INTEGRITY_J006_H

/* J-006 test-only surface (INC-E001-07 shape (b), exactly
 * as tests/core/mimir_stats_j002.h): the metadata
 * integrity walk of docs/PHASE_10_METADATA_INTEGRITY.md,
 * declared in src/core/mimir_core.h for the core's own
 * translation units and re-declared here for tests only.
 * Bound local in the shared library (nm -D stays exactly
 * 4); never part of the public ABI. */
int mimir_core_metadata_check(void);

#endif /* MIMIR_INTEGRITY_J006_H */
