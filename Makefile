# MimirAlloc - Phase 3 foundation build (slice C-001).
# Plain Makefile per D-006: portable C11 core, C ABI.
# Builds only the stub TU and the contract smoke test.

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -Iinclude
BUILD   = build

all: $(BUILD)/test_smoke

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/mimir_alloc.o: src/api/mimir_alloc.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/api/mimir_alloc.c -o $@

$(BUILD)/test_smoke.o: tests/contract/test_smoke.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/contract/test_smoke.c -o $@

$(BUILD)/test_smoke: $(BUILD)/mimir_alloc.o $(BUILD)/test_smoke.o
	$(CC) $(CFLAGS) $(BUILD)/mimir_alloc.o $(BUILD)/test_smoke.o -o $@

test: $(BUILD)/test_smoke
	./$(BUILD)/test_smoke

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
