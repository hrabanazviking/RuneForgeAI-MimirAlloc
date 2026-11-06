# MimirAlloc - Phase 3 build (slices C-001..C-003).
# Plain Makefile per D-006: portable C11 core, C ABI.
# C-001: public header, stub, contract smoke test.
# C-002: L6 platform layer (src/platform/), exercised by its
#        own standalone test under tests/platform/.
# C-003: real mapping-backed core (src/core/) behind the L1
#        boundary (src/api/); `make test` builds the API, core
#        and platform objects, runs the contract smoke test
#        and the core test under tests/core/.

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc
BUILD   = build

LIBOBJS = $(BUILD)/mimir_alloc.o $(BUILD)/mimir_core.o \
          $(BUILD)/mimir_platform.o

all: $(BUILD)/test_smoke $(BUILD)/test_core

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

$(BUILD)/test_smoke: $(LIBOBJS) $(BUILD)/test_smoke.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_smoke.o -o $@

$(BUILD)/test_core: $(LIBOBJS) $(BUILD)/test_core.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_core.o -o $@

test: $(BUILD)/test_smoke $(BUILD)/test_core
	./$(BUILD)/test_smoke
	./$(BUILD)/test_core

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
