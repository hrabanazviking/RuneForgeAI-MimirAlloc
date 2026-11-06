# MimirAlloc - Phase 3 build (slices C-001..C-007).
# Plain Makefile per D-006: portable C11 core, C ABI.
# C-001: public header, stub, contract smoke test.
# C-002: L6 platform layer (src/platform/), exercised by its
#        own standalone test under tests/platform/.
# C-003: real mapping-backed core (src/core/) behind the L1
#        boundary (src/api/); `make test` builds the API, core
#        and platform objects, runs the contract smoke test
#        and the core test under tests/core/.
# C-004: B-007 completeness audit - the audit test under
#        tests/core/ (test_b007.c) runs in `make test`;
#        the audit itself is tests/core/AUDIT_B007_C004.md.
# C-005: calloc/realloc per B-008 - the evidence test under
#        tests/core/ (test_c005.c) runs in `make test`;
#        the slice record is tests/core/NOTES_C005.md.
# C-006: section 6.3 startup / error-path audit - the
#        evidence test under tests/core/ (test_c006.c) runs
#        in `make test`; the audit itself is
#        tests/core/AUDIT_6_3_C006.md. No src/ change.
# C-007: portable unit/contract suite per B-009 - the suite
#        under tests/contract/ (test_c007.c) runs in `make test`
#        and standalone via `make test-c007`; the slice record
#        is tests/contract/NOTES_C007.md.
# C-008: B-008 unsupported-behaviour audit + roadmap S9.3
#        controlled-failure tests - the evidence test under
#        tests/core/ (test_c008.c) runs in `make test`; the
#        audit itself is tests/core/AUDIT_B008_C008.md and
#        the slice record is tests/core/NOTES_C008.md.
#        No src/ change.

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc
BUILD   = build

LIBOBJS = $(BUILD)/mimir_alloc.o $(BUILD)/mimir_core.o \
          $(BUILD)/mimir_platform.o

all: $(BUILD)/test_smoke $(BUILD)/test_core $(BUILD)/test_b007 $(BUILD)/test_c005 $(BUILD)/test_c006 $(BUILD)/test_c007 $(BUILD)/test_c008

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/mimir_alloc.o: src/api/mimir_alloc.c include/mimir_alloc.h src/core/mimir_core.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/api/mimir_alloc.c -o $@

$(BUILD)/mimir_core.o: src/core/mimir_core.c src/core/mimir_core.h src/platform/mimir_platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/core/mimir_core.c -o $@

$(BUILD)/mimir_platform.o: src/platform/mimir_platform.c src/platform/mimir_platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/platform/mimir_platform.c -o $@

$(BUILD)/test_smoke.o: tests/contract/test_smoke.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/contract/test_smoke.c -o $@

$(BUILD)/test_core.o: tests/core/test_core.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_core.c -o $@

$(BUILD)/test_b007.o: tests/core/test_b007.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_b007.c -o $@

$(BUILD)/test_c005.o: tests/core/test_c005.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_c005.c -o $@

$(BUILD)/test_c006.o: tests/core/test_c006.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_c006.c -o $@

$(BUILD)/test_c007.o: tests/contract/test_c007.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/contract/test_c007.c -o $@

$(BUILD)/test_c008.o: tests/core/test_c008.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_c008.c -o $@

$(BUILD)/test_smoke: $(LIBOBJS) $(BUILD)/test_smoke.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_smoke.o -o $@

$(BUILD)/test_core: $(LIBOBJS) $(BUILD)/test_core.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_core.o -o $@

$(BUILD)/test_b007: $(LIBOBJS) $(BUILD)/test_b007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_b007.o -o $@

$(BUILD)/test_c005: $(LIBOBJS) $(BUILD)/test_c005.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_c005.o -o $@

$(BUILD)/test_c006: $(LIBOBJS) $(BUILD)/test_c006.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_c006.o -o $@

$(BUILD)/test_c007: $(LIBOBJS) $(BUILD)/test_c007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_c007.o -o $@

$(BUILD)/test_c008: $(LIBOBJS) $(BUILD)/test_c008.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_c008.o -o $@

test: $(BUILD)/test_smoke $(BUILD)/test_core $(BUILD)/test_b007 $(BUILD)/test_c005 $(BUILD)/test_c006 $(BUILD)/test_c007 $(BUILD)/test_c008
	./$(BUILD)/test_smoke
	./$(BUILD)/test_core
	./$(BUILD)/test_b007
	./$(BUILD)/test_c005
	./$(BUILD)/test_c006
	./$(BUILD)/test_c007
	./$(BUILD)/test_c008

test-c007: $(BUILD)/test_c007
	./$(BUILD)/test_c007

clean:
	rm -rf $(BUILD)

.PHONY: all test test-c007 clean
