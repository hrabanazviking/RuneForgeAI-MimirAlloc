/* mimir_debug_j004.h -- test-only redeclaration of the
 * J-004 debug verification entry point (INC-E001-07 shape
 * (b), exactly like mimir_stats_j002.h): declared in
 * src/core/mimir_core.h under MIMIR_DEBUG only, defined in
 * src/core/mimir_core.c under MIMIR_DEBUG only, bound local
 * in the shared library (nm -D stays exactly 4 in both
 * flavours). In a production build the symbol does not
 * exist and this header declares nothing. */
#ifndef MIMIR_DEBUG_J004_H
#define MIMIR_DEBUG_J004_H

#ifdef MIMIR_DEBUG
int mimir_core_debug_verify_all(void);
#endif

#endif /* MIMIR_DEBUG_J004_H */
