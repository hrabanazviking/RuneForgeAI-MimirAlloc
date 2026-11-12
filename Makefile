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
# D-003: page-size portability proof - the evidence test
#        under tests/platform/ (test_d003.c) runs standalone
#        via `make test-d003`; the audit itself is
#        docs/PAGE_SIZE_PORTABILITY.md and the recorded run
#        is tests/platform/recorded_run_d003.txt.
# C-008: B-008 unsupported-behaviour audit + roadmap S9.3
#        controlled-failure tests - the evidence test under
#        tests/core/ (test_c008.c) runs in `make test`; the
#        audit itself is tests/core/AUDIT_B008_C008.md and
#        the slice record is tests/core/NOTES_C008.md.
#        No src/ change.
# D-004: physical multithread allocate/fill/free test -
#        the evidence test under tests/concurrency/
#        (test_d004.c) runs in `make test` and standalone
#        via `make test-d004`; the summary is
#        docs/PHASE_4_MULTITHREAD_RUN.md and the recorded
#        run is tests/concurrency/recorded_run_d004.txt.

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -flto
# K-004 (Phase 11): -O2 added to the shipped CFLAGS on measured
# evidence only (docs/PHASE_11_PERFORMANCE_K004.md; bench
# tests/core/bench_k004.c, recorded run tests/core/
# recorded_run_k004.txt): small64 median 168->86 ns/op,
# burst256 1488->661 ns/op, large4096 flat (mmap-bound).
# This discharges the INC-K002-02 comparison leg by making
# the optimised flavour the shipped flavour. No source,
# ABI or flag other than -O2 changes.
# Z-035 (Z13 line): -flto added to the shipped CFLAGS on measured
# evidence only (tests/perf/recorded_run_z13_z035.txt; runner
# tests/perf/run_z035.sh, bench tests/perf/bench_k002.c mimir
# leg, allocator objects only, bench TU -O2 in every leg):
# small_churn median 34.18->27.87 ns/op (+18.5%), mixed_batch
# 223.22->148.03 (+33.7%), mixed_lifecycle +11.2%, large_batch
# TIE; replicated in a second interleaved run. -O3 and
# -march=native measured TIE on every workload in both runs
# and are NOT adopted. The four .so link lines also gain -flto
# so the shared libraries link the same LTO objects they are
# compiled from. Note: the static archive now holds LTO
# objects - link it with -flto in CFLAGS (every in-tree
# consumer does); Z-096 re-trials the flag set on the
# near-final core. No source or ABI change.
# J-004 debug flavour (docs/PHASE_10_FOUNDATION_PLAN.md S5.1;
# docs/PHASE_10_DEBUG_REDZONES.md): `make DEBUG=1` adds
# -DMIMIR_DEBUG=1 and directs ALL objects to build-debug/,
# so a debug object can never be linked into a production
# artifact by stale-file accident. Default (no DEBUG) is
# production, byte-for-byte the banked build: BUILD=build
# and CFLAGS unchanged. No runtime flag exists in either
# flavour. `make DEBUG=1 test-debug` runs the full suite
# plus test-j004 under the debug build.
ifeq ($(DEBUG),1)
BUILD   = build-debug
CFLAGS += -DMIMIR_DEBUG=1
else
BUILD   = build
endif

LIBOBJS = $(BUILD)/mimir_alloc.o $(BUILD)/mimir_core.o \
          $(BUILD)/mimir_platform.o

all: $(BUILD)/test_smoke $(BUILD)/test_core $(BUILD)/test_b007 $(BUILD)/test_c005 $(BUILD)/test_c006 $(BUILD)/test_c007 $(BUILD)/test_c008 $(BUILD)/test_d004

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

$(BUILD)/test_d004.o: tests/concurrency/test_d004.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/concurrency/test_d004.c -o $@

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

$(BUILD)/test_d004: $(LIBOBJS) $(BUILD)/test_d004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_d004.o -o $@

test: $(BUILD)/test_smoke $(BUILD)/test_core $(BUILD)/test_b007 $(BUILD)/test_c005 $(BUILD)/test_c006 $(BUILD)/test_c007 $(BUILD)/test_c008 $(BUILD)/test_d004
	./$(BUILD)/test_smoke
	./$(BUILD)/test_core
	./$(BUILD)/test_b007
	./$(BUILD)/test_c005
	./$(BUILD)/test_c006
	./$(BUILD)/test_c007
	./$(BUILD)/test_c008
	./$(BUILD)/test_d004

test-c007: $(BUILD)/test_c007
	./$(BUILD)/test_c007

clean:
	rm -rf $(BUILD)

.PHONY: all test test-c007 test-d003 test-d004 test-d005 test-d006 test-d007 test-d008 clean

$(BUILD)/test_d003.o: tests/platform/test_d003.c include/mimir_alloc.h src/platform/mimir_platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/platform/test_d003.c -o $@

$(BUILD)/test_d003: $(LIBOBJS) $(BUILD)/test_d003.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_d003.o -o $@

test-d003: $(BUILD)/test_d003
	./$(BUILD)/test_d003

test-d004: $(BUILD)/test_d004
	./$(BUILD)/test_d004

# D-005: cross-compilation probe - builds and runs the
# freestanding x86-64 probe (tests/cross/probe_freestanding.c)
# via clang --target=x86_64-linux-gnu + ld.gold, executed
# under Box64/binfmt (scripts/cross/build_x86_64_
# freestanding_probe.sh). Standalone on purpose: it is NOT
# part of `make test` and proves nothing about running the
# allocator itself on x86-64 - no x86-64 libc sysroot exists
# on the Pi. See docs/PHASE_4_CROSS_COMPILATION_AND_CI.md.
test-d005:
	./scripts/cross/build_x86_64_freestanding_probe.sh

# D-006: Android toolchain readiness probe - runs the
# read-only probe (scripts/android/probe_android_
# toolchain.sh) and reports PRESENT/ABSENT for every
# component of an Android NDK build road on this host.
# Standalone on purpose: it is NOT part of `make test`
# and proves nothing about building or running the
# allocator for Android - no Android NDK exists on the
# Pi. See docs/PHASE_4_ANDROID_BUILD_PLAN.md.
test-d006:
	./scripts/android/probe_android_toolchain.sh

# D-007: Android physical-device test program - the
# on-device test under tests/android/ (test_d007.c),
# built and run NATIVELY here as a logic check only.
# Standalone on purpose: it is NOT part of `make test`
# and a native PASS is NOT an Android result - the
# on-device run on the Samsung SM-X400 is OWED until an
# NDK build road (D-006) and a device road exist. See
# docs/PHASE_4_ANDROID_DEVICE_PACKET.md.
$(BUILD)/test_d007.o: tests/android/test_d007.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/android/test_d007.c -o $@

$(BUILD)/test_d007: $(LIBOBJS) $(BUILD)/test_d007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_d007.o -o $@

test-d007: $(BUILD)/test_d007
	./$(BUILD)/test_d007

# D-008: libc / alignment compatibility probe - the
# evidence test under tests/compat/ (test_d008.c) measures
# returned-pointer alignment for the four public symbols
# across odd, boundary and large sizes and the calloc /
# realloc paths, checked against _Alignof(max_align_t).
# Standalone on purpose via `make test-d008`: it is NOT
# part of `make test`. See
# docs/LIBC_ALIGNMENT_COMPATIBILITY.md and the recorded
# run tests/compat/recorded_run_d008.txt.
$(BUILD)/test_d008.o: tests/compat/test_d008.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/compat/test_d008.c -o $@

$(BUILD)/test_d008: $(LIBOBJS) $(BUILD)/test_d008.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_d008.o -o $@

test-d008: $(BUILD)/test_d008
	./$(BUILD)/test_d008

# E-002: shared-library build with controlled symbol
# visibility - libmimiralloc.so is built from DEDICATED PIC
# objects under build/pic/ (the banked objects are compiled
# without -fPIC; see INC-D002-03) and linked with the export
# contract in src/api/mimir_alloc.map. The API object is
# compiled with -fvisibility=default (its only definitions
# are the four public entry points); the core and platform
# objects are compiled with -fvisibility=hidden, and the
# version script additionally binds every non-public symbol
# local, so the defined dynamic exports are exactly
# mimir_malloc, mimir_free, mimir_calloc and mimir_realloc.
# The consumer evidence test under tests/abi/ (test_e002.c)
# links against the .so and runs standalone via
# `make test-e002`; the recorded inspection is
# tests/abi/recorded_run_e002.txt and the slice record is
# docs/SHARED_LIBRARY_BUILD.md. Standalone on purpose: `lib`
# and `test-e002` are NOT part of `make test`, whose
# composition is unchanged by this slice.
PICDIR  = $(BUILD)/pic
PICOBJS = $(PICDIR)/mimir_alloc_pic.o $(PICDIR)/mimir_core_pic.o \
          $(PICDIR)/mimir_platform_pic.o
MAPFILE = src/api/mimir_alloc.map

$(PICDIR):
	mkdir -p $(PICDIR)

$(PICDIR)/mimir_alloc_pic.o: src/api/mimir_alloc.c include/mimir_alloc.h src/core/mimir_core.h | $(PICDIR)
	$(CC) $(CFLAGS) -fPIC -fvisibility=default -c src/api/mimir_alloc.c -o $@

$(PICDIR)/mimir_core_pic.o: src/core/mimir_core.c src/core/mimir_core.h src/platform/mimir_platform.h | $(PICDIR)
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -c src/core/mimir_core.c -o $@

$(PICDIR)/mimir_platform_pic.o: src/platform/mimir_platform.c src/platform/mimir_platform.h | $(PICDIR)
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -c src/platform/mimir_platform.c -o $@

$(BUILD)/libmimiralloc.so: $(PICOBJS) $(MAPFILE) | $(BUILD)
	$(CC) -flto -shared -Wl,--version-script=$(MAPFILE) -o $@ $(PICOBJS) -pthread

lib: $(BUILD)/libmimiralloc.so

$(BUILD)/test_e002.o: tests/abi/test_e002.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e002.c -o $@

$(BUILD)/test_e002: $(BUILD)/libmimiralloc.so $(BUILD)/test_e002.o
	$(CC) $(CFLAGS) $(BUILD)/test_e002.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

test-e002: $(BUILD)/test_e002
	./$(BUILD)/test_e002

.PHONY: lib test-e002

# E-003: static library + static/dynamic link-route proof -
# the archive target under build/libmimiralloc.a packages
# the three banked library objects (LIBOBJS) with ar. The
# consumer under tests/abi/ (test_e003.c) exercises the
# full allocation lifecycle through the four public
# symbols only. R1 (static) is proven by the standalone
# `make test-e003` target and the recorded run
# tests/abi/recorded_run_e003.txt; R2 (dynamic) is OWED
# to E-002's shared library - no .so target exists at
# this base. Standalone on purpose: `make test`
# composition is unchanged. See
# docs/PHASE_5_LINK_ROUTES_E003.md.
STATICLIB = $(BUILD)/libmimiralloc.a

$(BUILD)/libmimiralloc.a: $(LIBOBJS)
	ar rcs $@ $(LIBOBJS)

static-lib: $(BUILD)/libmimiralloc.a

$(BUILD)/test_e003_static: tests/abi/test_e003.c $(BUILD)/libmimiralloc.a | $(BUILD)
	$(CC) $(CFLAGS) tests/abi/test_e003.c $(BUILD)/libmimiralloc.a -o $@ -Wl,-Map,$(BUILD)/test_e003_static.map

test-e003: $(BUILD)/test_e003_static
	./$(BUILD)/test_e003_static

.PHONY: static-lib test-e003

# E-004: independent test executable binding proof -
# the consumer under tests/abi/ (test_e004.c) is a new,
# independent program linked ONLY against the shared
# library built by `make lib` (build/libmimiralloc.so).
# It proves at run time that its undefined mimir_*
# references bind to libmimiralloc.so (nm/readelf, ldd
# and LD_DEBUG=bindings evidence in
# tests/abi/recorded_run_e004.txt) and passes the full
# allocation lifecycle through that dynamically bound
# library. This slice also closes INC-E003-01 by running
# the R2 proving commands E-003 recorded. Standalone via
# `make test-e004`; NOT part of `make test`, whose
# composition is unchanged. See
# docs/PHASE_5_BINDING_PROOF_E004.md.
$(BUILD)/test_e004.o: tests/abi/test_e004.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e004.c -o $@

$(BUILD)/test_e004: $(BUILD)/libmimiralloc.so $(BUILD)/test_e004.o
	$(CC) $(CFLAGS) $(BUILD)/test_e004.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

test-e004: $(BUILD)/test_e004
	./$(BUILD)/test_e004

.PHONY: test-e004

# E-005: visible allocation counter / explicit audit hook -
# the internal audit counters in src/api/mimir_alloc.c
# (L1 call counters) and src/core/mimir_core.c (mapping
# published/retired counters) are INC-E001-07 shape (b):
# readable only through a test-only build, never exported
# from libmimiralloc.so (proven in the recorded run by
# nm -D staying at exactly 4 and by an audit-reference
# link probe against the .so failing). Two standalone
# legs: `make test-e005` links the library OBJECTS into
# tests/abi/test_e005.c and checks the counters exactly
# across a lifecycle script with a libc-bypass negative
# phase; `make test-e005-dyn` links tests/abi/
# test_e005_dyn.c against the .so only and proves the
# same call path from outside via descriptor validation
# and mapped-bytes accounting (shape (a) leg). The slice
# record is docs/PHASE_5_AUDIT_HOOK_E005.md and the
# recorded run is tests/abi/recorded_run_e005.txt.
# Standalone on purpose: neither target is part of
# `make test`, whose composition is unchanged.
$(BUILD)/test_e005.o: tests/abi/test_e005.c tests/abi/mimir_audit_e005.h include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e005.c -o $@

$(BUILD)/test_e005: $(LIBOBJS) $(BUILD)/test_e005.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_e005.o -o $@

test-e005: $(BUILD)/test_e005
	./$(BUILD)/test_e005

$(BUILD)/test_e005_dyn.o: tests/abi/test_e005_dyn.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e005_dyn.c -o $@

$(BUILD)/test_e005_dyn: $(BUILD)/libmimiralloc.so $(BUILD)/test_e005_dyn.o
	$(CC) $(CFLAGS) $(BUILD)/test_e005_dyn.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

test-e005-dyn: $(BUILD)/test_e005_dyn
	./$(BUILD)/test_e005_dyn

.PHONY: test-e005 test-e005-dyn

# E-006: aligned-allocation surface assessment -
# the evidence test under tests/abi/ (test_e006.c) is
# linked ONLY against the shared library built by
# `make lib` (build/libmimiralloc.so), in the E-004
# route, and measures the actual alignment of pointers
# returned by the four public symbols across sizes and
# through the calloc/realloc lifecycles: the contract
# floor (_Alignof(max_align_t)) as checks, the page
# modulus as printed observations only (INC-D008-08).
# Standalone via `make test-e006`; NOT part of
# `make test`, whose composition is unchanged. No
# aligned-allocation entry point is added by this
# slice (hard law: no silent ABI expansion). See
# docs/PHASE_5_ALIGNED_SURFACE_E006.md and the recorded
# run tests/abi/recorded_run_e006.txt.
$(BUILD)/test_e006.o: tests/abi/test_e006.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e006.c -o $@

$(BUILD)/test_e006: $(BUILD)/libmimiralloc.so $(BUILD)/test_e006.o
	$(CC) $(CFLAGS) $(BUILD)/test_e006.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

test-e006: $(BUILD)/test_e006
	./$(BUILD)/test_e006

.PHONY: test-e006

# E-007: C++ consumption variants - the consumer under
# tests/abi/ (test_e007.cpp) is a C++ translation unit
# that includes include/mimir_alloc.h under its existing
# extern "C" guards and exercises the full allocation
# lifecycle through the four public symbols only, linked
# ONLY against the shared library built by `make lib`
# (build/libmimiralloc.so), in the R2 dynamic route named
# by E-004. Its undefined mimir_* references leave the
# C++ object UNMANGLED and bind to the C library (binding
# evidence in tests/abi/recorded_run_e007.txt). The two
# header-only TUs (header_only_e007.cpp / .c) are compiled
# to objects by the same target as the standing header
# C++-clean / C11-clean proof. No C++ native surface is
# added: no operator new/delete, no C++-name entry point;
# the dynamic exports stay exactly the four C symbols.
# Standalone via `make test-e007`; NOT part of `make test`,
# whose composition is unchanged. See
# docs/PHASE_5_CPP_CONSUMPTION_E007.md.
CXX      = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -pthread -Iinclude

$(BUILD)/test_e007.o: tests/abi/test_e007.cpp include/mimir_alloc.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c tests/abi/test_e007.cpp -o $@

$(BUILD)/test_e007: $(BUILD)/libmimiralloc.so $(BUILD)/test_e007.o
	$(CXX) $(CXXFLAGS) $(BUILD)/test_e007.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

$(BUILD)/header_only_e007_cpp.o: tests/abi/header_only_e007.cpp include/mimir_alloc.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c tests/abi/header_only_e007.cpp -o $@

$(BUILD)/header_only_e007_c.o: tests/abi/header_only_e007.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/header_only_e007.c -o $@

test-e007: $(BUILD)/test_e007 $(BUILD)/header_only_e007_cpp.o $(BUILD)/header_only_e007_c.o
	./$(BUILD)/test_e007

.PHONY: test-e007

# E-008: startup ordering + cross-module ownership -
# the peer module under tests/abi/ (test_e008_peer.c) is
# built as its OWN shared library, build/libe008peer.so,
# linked against build/libmimiralloc.so, so the main
# consumer (test_e008_main.c) and the peer are two
# distinct ELF modules whose mimir_* references both
# bind to the one libmimiralloc.so instance (DT_NEEDED
# on both, LD_DEBUG evidence in
# tests/abi/recorded_run_e008.txt). The peer's
# constructor allocates through mimir_malloc before
# main; its destructor verifies and frees that block
# after main. Three companion programs complete the
# slice: test_e008_first.c (allocation as the very
# first MimirAlloc call of a program),
# test_e008_threads.c (first calls raced from 8
# threads, foundation plan T8's second startup case),
# and test_e008_mixed.c (the mixed-allocator SAFE
# probes: E-005 audit counters prove libc calls move
# no MimirAlloc counter; NO foreign pointer is ever
# passed to a mimir_* entry point - foreign free is
# contract UB with no promised rejection, INC-E008-01).
# Standalone via `make test-e008`; NOT part of
# `make test`, whose composition is unchanged. See
# docs/PHASE_5_STARTUP_OWNERSHIP_E008.md.
PEERLIB = $(BUILD)/libe008peer.so

$(PICDIR)/test_e008_peer_pic.o: tests/abi/test_e008_peer.c tests/abi/test_e008_peer.h include/mimir_alloc.h | $(PICDIR)
	$(CC) $(CFLAGS) -fPIC -c tests/abi/test_e008_peer.c -o $@

$(PEERLIB): $(BUILD)/libmimiralloc.so $(PICDIR)/test_e008_peer_pic.o
	$(CC) -shared $(PICDIR)/test_e008_peer_pic.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

$(BUILD)/test_e008_main.o: tests/abi/test_e008_main.c tests/abi/test_e008_peer.h include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e008_main.c -o $@

$(BUILD)/test_e008: $(PEERLIB) $(BUILD)/test_e008_main.o
	$(CC) $(CFLAGS) $(BUILD)/test_e008_main.o -L$(BUILD) -le008peer -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

$(BUILD)/test_e008_first.o: tests/abi/test_e008_first.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e008_first.c -o $@

$(BUILD)/test_e008_first: $(BUILD)/libmimiralloc.so $(BUILD)/test_e008_first.o
	$(CC) $(CFLAGS) $(BUILD)/test_e008_first.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

$(BUILD)/test_e008_threads.o: tests/abi/test_e008_threads.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e008_threads.c -o $@

$(BUILD)/test_e008_threads: $(BUILD)/libmimiralloc.so $(BUILD)/test_e008_threads.o
	$(CC) $(CFLAGS) $(BUILD)/test_e008_threads.o -L$(BUILD) -lmimiralloc -Wl,-rpath,'$$ORIGIN' -o $@

$(BUILD)/test_e008_mixed.o: tests/abi/test_e008_mixed.c tests/abi/mimir_audit_e005.h include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/abi/test_e008_mixed.c -o $@

$(BUILD)/test_e008_mixed: $(LIBOBJS) $(BUILD)/test_e008_mixed.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_e008_mixed.o -o $@

test-e008: $(BUILD)/test_e008_first $(BUILD)/test_e008_threads $(BUILD)/test_e008_mixed $(BUILD)/test_e008
	./$(BUILD)/test_e008_first
	./$(BUILD)/test_e008_threads
	./$(BUILD)/test_e008_mixed
	./$(BUILD)/test_e008

.PHONY: test-e008

# F-003: Mojo boundary wiring (B2+B1) - the generated Mojo
# program under tests/mojo/ (test_f003.mojo) calls the four
# namespaced mimir_* symbols explicitly from Mojo via
# std.ffi external_call, with build/libmimiralloc.so placed
# on its link line through mojo build's documented -Xlinker
# flags (F-001 boundary B2). Standalone via
# `make test-f003`; NOT part of `make test`, whose
# composition is unchanged, and `make test` does not depend
# on Mojo being present - this target is the only one that
# invokes mojo, using the `mojo` found on PATH (on the Pi,
# the pixi wrapper). See docs/PHASE_6_BOUNDARY_WIRING_F003.md
# and tests/mojo/recorded_run_f003.txt.
MOJO ?= mojo

$(BUILD)/test_f003: tests/mojo/test_f003.mojo $(BUILD)/libmimiralloc.so | $(BUILD)
	$(MOJO) build -o $@ tests/mojo/test_f003.mojo -Xlinker -L$(BUILD) -Xlinker -lmimiralloc -Xlinker -rpath -Xlinker '$$ORIGIN'

test-f003: $(BUILD)/test_f003
	./$(BUILD)/test_f003

.PHONY: test-f003

# F-004: minimal Mojo program run (Phase 6, T3) - the
# evidence program under tests/mojo/ (test_f004.mojo) is
# built with `mojo build` and run natively via the
# standalone target `make test-f004-mojo`. Mojo-guarded on
# purpose: when no `mojo` is on PATH the target prints a
# skip line and exits 0, so the C suite never depends on
# Mojo. Standalone on purpose: it is NOT part of
# `make test`, whose composition is unchanged. The WITH
# (MimirAlloc-wired) leg is OWED to F-003's banked boundary
# (see docs/PHASE_6_MINIMAL_MOJO_RUN_F004.md); this target
# runs the WITHOUT baseline leg only. The recorded run is
# tests/mojo/recorded_run_f004.txt.
test-f004-mojo:
	@if command -v mojo >/dev/null 2>&1; then \
		mkdir -p $(BUILD); \
		timeout 180 mojo build tests/mojo/test_f004.mojo -o $(BUILD)/test_f004_mojo && ./$(BUILD)/test_f004_mojo; \
	else \
		echo "test-f004-mojo: mojo not on PATH - SKIPPED (Mojo-guarded standalone target)"; \
	fi

.PHONY: test-f004-mojo

# F-005: allocator-use proof on the Mojo path (binding
# grade) - the two generated Mojo programs under
# tests/mojo/ upgrade F-003's verdict+linkage proof to
# binding grade, adapting E-004's binding proof and
# E-005's two legs to the Mojo path. DYNAMIC leg:
# tests/mojo/test_f005.mojo is linked against
# build/libmimiralloc.so (the F-003 B2 boundary) plus the
# test-only observer build/libf005probe.so (from
# tests/mojo/f005_probe.c: descriptor validation +
# /proc/self/maps mapped-bytes accounting, E-005 dynamic
# leg); the recorded run adds LD_DEBUG=bindings evidence
# (E-004's shape). STATIC leg:
# tests/mojo/test_f005_audit.mojo links the library
# OBJECTS and the getter object build/f005_audit.o (from
# tests/mojo/f005_audit.c) directly, so the program reads
# the six internal E-005 audit counters its own mimir_*
# calls move, exactly as `make test-e005` does in C.
# Standalone via `make test-f005`; NOT part of `make
# test`, whose composition is unchanged, and `make test`
# does not depend on Mojo being present - this target is
# (with test-f003) one of the only ones that invoke mojo,
# using the `mojo` found on PATH. See
# docs/PHASE_6_ALLOCATOR_USE_PROOF_F005.md and
# tests/mojo/recorded_run_f005.txt.
$(BUILD)/libf005probe.so: tests/mojo/f005_probe.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -fPIC -shared tests/mojo/f005_probe.c -o $@

$(BUILD)/test_f005: tests/mojo/test_f005.mojo $(BUILD)/libmimiralloc.so $(BUILD)/libf005probe.so | $(BUILD)
	$(MOJO) build -o $@ tests/mojo/test_f005.mojo -Xlinker -L$(BUILD) -Xlinker -lmimiralloc -Xlinker -lf005probe -Xlinker -rpath -Xlinker '$$ORIGIN'

$(BUILD)/f005_audit.o: tests/mojo/f005_audit.c | $(BUILD)
	$(CC) $(CFLAGS) -c tests/mojo/f005_audit.c -o $@

$(BUILD)/test_f005_audit: tests/mojo/test_f005_audit.mojo $(LIBOBJS) $(BUILD)/f005_audit.o | $(BUILD)
	$(MOJO) build -o $@ tests/mojo/test_f005_audit.mojo -Xlinker $(BUILD)/mimir_alloc.o -Xlinker $(BUILD)/mimir_core.o -Xlinker $(BUILD)/mimir_platform.o -Xlinker $(BUILD)/f005_audit.o -Xlinker -lpthread

test-f005: $(BUILD)/test_f005 $(BUILD)/test_f005_audit
	./$(BUILD)/test_f005
	./$(BUILD)/test_f005_audit

.PHONY: test-f005

# F-006: representative program repeated-compile comparison
# (Phase 6, tasks T4+T6) - the harness under tests/mojo/
# (run_f006_compare.sh) runs a bounded repeated series
# (5 builds + 5 runs per leg) for the WITHOUT leg
# (tests/mojo/test_f004.mojo, F-004 baseline, no boundary)
# and the WITH leg (tests/mojo/test_f005.mojo dynamic leg,
# the F-003 B2 boundary as proven binding-grade by F-005),
# on this tree's revision, and prints the full record.
# Standalone via `make test-f006`; NOT part of `make test`,
# whose composition is unchanged, and `make test` does not
# depend on Mojo being present - this target is (with
# test-f003/test-f004-mojo/test-f005) one of the only ones
# that invoke mojo, using the `mojo` found on PATH. The
# recorded run is tests/mojo/recorded_run_f006.txt; the
# slice record is
# docs/PHASE_6_REPEATED_COMPILE_COMPARISON_F006.md.
test-f006: $(BUILD)/libmimiralloc.so $(BUILD)/libf005probe.so
	bash tests/mojo/run_f006_compare.sh

.PHONY: test-f006

# F-007: relevant upstream test execution record (Phase 6,
# task T5) - the probe under tests/mojo/
# (run_f007_upstream_probe.sh) re-verifies, read-only and in
# seconds, what F-007 established by execution: the installed
# package ships no upstream tests; Mojo 1.1.0 has no `mojo
# test` subcommand but `mojo run` (with the documented -I
# package path for test_utils) executes a relevant subset of
# the 1.2.0.dev stdlib test files from the operator-local
# checkout against the installed 1.1.0 stdlib (112 tests, all
# passed at F-007); the rest of the subset is compile-blocked
# by version skew, and the lit / C++ unit suites cannot run
# (no lit, no same-source built toolchain). The probe prints
# PRESENT/ABSENT-style verdicts per component and exits 0
# when the probe itself completes (D-006 probe shape).
# Standalone via `make test-f007`; NOT part of `make test`,
# whose composition is unchanged. The full execution record
# is tests/mojo/recorded_run_f007.txt; the slice record is
# docs/PHASE_6_UPSTREAM_TESTS_F007.md.
test-f007:
	bash tests/mojo/run_f007_upstream_probe.sh

.PHONY: test-f007

# G-001: Phase 3 baseline micro-probe - the probe under
# tests/perf/ (probe_g001.c) measures allocation latency
# distribution and retained memory (VmRSS) for the banked
# mapping-per-allocation allocator (mimir leg) and for the
# platform allocator in the identical shape (libc leg).
# Standalone via make test-g001; NOT part of make test,
# whose composition is unchanged. Recorded run:
# tests/perf/recorded_run_g001.txt; slice record:
# docs/PHASE_7_FOUNDATION_PLAN.md.
$(BUILD)/probe_g001.o: tests/perf/probe_g001.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_g001.c -o $@

$(BUILD)/probe_g001: $(LIBOBJS) $(BUILD)/probe_g001.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_g001.o -o $@

test-g001: $(BUILD)/probe_g001
	./$(BUILD)/probe_g001 mimir
	./$(BUILD)/probe_g001 libc

.PHONY: test-g001

# G-003: Phase 7 small-object path evidence test - the
# test under tests/core/ (test_g003.c) exercises the
# size-class span path of docs/PHASE_7_SIZE_CLASSES_G002.md
# as implemented in src/core/mimir_core.c: R-1 routing via
# the E-005 mapping counters and the G-003 span/object
# counters (tests/core/mimir_small_g003.h), alignment,
# pattern/non-overlap, bounded reuse, the two-registry
# free path (R-4), calloc class-extent zeroing (R-2),
# realloc cases (R-3 / B-008), and a G-001-shaped latency
# leg. Standalone via `make test-g003`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_g003.txt; slice record:
# docs/PHASE_7_SMALL_OBJECT_PATH_G003.md.
$(BUILD)/test_g003.o: tests/core/test_g003.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_g003.c -o $@

$(BUILD)/test_g003: $(LIBOBJS) $(BUILD)/test_g003.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_g003.o -o $@

test-g003: $(BUILD)/test_g003
	./$(BUILD)/test_g003

.PHONY: test-g003

# G-004: Phase 7 central fallback integration evidence
# test - the test under tests/core/ (test_g004.c) audits
# the integration of the G-003 size-class span path with
# the Phase 3 mapping-per-allocation central fallback:
# R-1 boundary routing via both counter sets, span
# exhaustion, retirement beyond the retention bound, the
# two-registry free path (R-4) interleaved and under
# posture probes, the realloc matrix (R-3), calloc (R-2)
# and defined failures. Standalone via `make test-g004`;
# NOT part of `make test`, whose composition is
# unchanged. Recorded run: tests/core/
# recorded_run_g004.txt; slice record:
# docs/PHASE_7_CENTRAL_FALLBACK_G004.md.
$(BUILD)/test_g004.o: tests/core/test_g004.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_g004.c -o $@

$(BUILD)/test_g004: $(LIBOBJS) $(BUILD)/test_g004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_g004.o -o $@

test-g004: $(BUILD)/test_g004
	./$(BUILD)/test_g004

.PHONY: test-g004

# G-005: Phase 7 reclaim policy + idle-thread retained-
# memory measurement - the evidence test under tests/core/
# (test_g005.c) verifies the reclaim policy defined in
# docs/PHASE_7_RECLAIM_POLICY_G005.md (RP-1..RP-5) as
# implemented by G-003 in src/core/mimir_core.c: per-class
# empty-span retention bound (2), synchronous release of
# the span that exceeds the bound at free time, churn
# stability, and the idle-thread case (no thread-local
# cache exists, so idle threads retain nothing beyond the
# global per-class bound). It reads the G-003 span/object
# counters via tests/core/mimir_small_g003.h and VmRSS as
# a process-level cross-check only. Standalone via
# `make test-g005`; NOT part of `make test`, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_g005.txt.
$(BUILD)/test_g005.o: tests/core/test_g005.c include/mimir_alloc.h tests/core/mimir_small_g003.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_g005.c -o $@

$(BUILD)/test_g005: $(LIBOBJS) $(BUILD)/test_g005.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_g005.o -o $@

test-g005: $(BUILD)/test_g005
	./$(BUILD)/test_g005

.PHONY: test-g005

# G-006: Phase 7 thread exit handling - the evidence
# test under tests/core/ (test_g006.c) verifies the
# thread-exit posture scoped by INC-G005-04: the core
# holds no per-thread state (no _Thread_local / __thread
# / pthread-key state in src/), so thread exit drains
# nothing and allocations outlive their allocating
# thread in the global registries. It proves by
# execution: exit-while-holding small + large blocks
# freed afterwards from the main thread via the
# two-registry free path, exits concurrent with other
# threads allocating/freeing, many short-lived churning
# threads, and pthread_exit vs return equivalence. It
# reads the E-005 mapping counters and the G-003
# span/object counters at quiescent (post-join) points
# only. Standalone via `make test-g006`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_g006.txt; slice record:
# docs/PHASE_7_THREAD_EXIT_G006.md.
$(BUILD)/test_g006.o: tests/core/test_g006.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_g006.c -o $@

$(BUILD)/test_g006: $(LIBOBJS) $(BUILD)/test_g006.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_g006.o -o $@

test-g006: $(BUILD)/test_g006
	./$(BUILD)/test_g006

.PHONY: test-g006

# G-007: Phase 7 tuning parameters evidence test - the
# test under tests/core/ (test_g007.c) verifies the
# tuning surface of docs/PHASE_7_TUNING_PARAMETERS_G007.md:
# the G-005 RP-2 per-class empty-span retention bound is
# configurable through the test-only setter/getter pair of
# tests/core/mimir_tune_g007.h (INC-E001-07 shape (b)),
# with invalid values refused and the previous value kept,
# the effective value readable back exactly, and the
# unconfigured default (2) reproducing the G-005 P1 shape
# exactly, all counted via the G-003 span counters
# (tests/core/mimir_small_g003.h). Standalone via
# `make test-g007`; NOT part of `make test`, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_g007.txt.
$(BUILD)/test_g007.o: tests/core/test_g007.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_tune_g007.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_g007.c -o $@

$(BUILD)/test_g007: $(LIBOBJS) $(BUILD)/test_g007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_g007.o -o $@

test-g007: $(BUILD)/test_g007
	./$(BUILD)/test_g007

.PHONY: test-g007

# G-008: Phase 7 repeated load/unload + multitasking
# responsiveness record - the harness under tests/core/
# (test_g008.c) consumes build/libmimiralloc.so ONLY via
# dlopen/dlsym/dlclose (it links -ldl, never the library
# objects), running 50 bounded load/unload cycles with a
# small synthetic workload per cycle, /proc/self/maps +
# VmRSS state-carry-over evidence, and an orphan probe;
# its churn mode is driven by the responsiveness probe
# script tests/core/run_g008_probe.sh (Samtal HTTP +
# /bin/true latency, baseline vs 2-thread bounded churn).
# Standalone via `make test-g008`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_g008.txt; slice record:
# docs/PHASE_7_LOAD_UNLOAD_RESPONSIVENESS_G008.md.
$(BUILD)/test_g008.o: tests/core/test_g008.c | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_g008.c -o $@

$(BUILD)/test_g008: $(BUILD)/libmimiralloc.so $(BUILD)/test_g008.o
	$(CC) $(CFLAGS) $(BUILD)/test_g008.o -o $@ -ldl

test-g008: $(BUILD)/test_g008
	./$(BUILD)/test_g008 $(BUILD)/libmimiralloc.so
	bash tests/core/run_g008_probe.sh

.PHONY: test-g008

# H-001: Phase 7 unload drain evidence test - the test
# under tests/core/ (test_h001.c) verifies the drain of
# docs/PHASE_7_UNLOAD_DRAIN_H001.md (commissioned fix for
# INC-G008-02) in two legs in one binary: a DLOPEN leg
# consuming build/libmimiralloc.so only via
# dlopen/dlsym/dlclose in the G-008 harness shape
# (50 cycles, same workload), which must now record
# anonymous rw growth of exactly 0 B per cycle and
# img_after_close=0 throughout, plus the live-at-unload
# probe (INC-G008-03: a block still live at dlclose is
# deliberately NOT drained; its span and pattern
# survive); and a DIRECT leg linking the library objects
# (LIBOBJS), which reads the G-003 span counters
# (tests/core/mimir_small_g003.h), the E-005 mapping
# counters (tests/abi/mimir_audit_e005.h) and the drain
# entry point itself (tests/core/mimir_drain_h001.h) to
# prove the drain semantics exactly: only EMPTY spans
# released, live blocks and mappings untouched,
# idempotent, allocator usable afterwards. Standalone
# via `make test-h001`; NOT part of `make test`, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_h001.txt; slice record:
# docs/PHASE_7_UNLOAD_DRAIN_H001.md.
$(BUILD)/test_h001.o: tests/core/test_h001.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_drain_h001.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_h001.c -o $@

$(BUILD)/test_h001: $(LIBOBJS) $(BUILD)/libmimiralloc.so $(BUILD)/test_h001.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_h001.o -o $@ -ldl

test-h001: $(BUILD)/test_h001
	./$(BUILD)/test_h001 $(BUILD)/libmimiralloc.so

.PHONY: test-h001

# H-002: Phase 8 cross-thread/contention baseline probe -
# standalone via make test-h002; NOT part of make test.
# Recorded run: tests/perf/recorded_run_h002.txt.
$(BUILD)/probe_h002.o: tests/perf/probe_h002.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_h002.c -o $@

$(BUILD)/probe_h002: $(LIBOBJS) $(BUILD)/probe_h002.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_h002.o -o $@

test-h002: $(BUILD)/probe_h002
	./$(BUILD)/probe_h002

.PHONY: test-h002

# Z-013: THP under madvise probe - standalone via make
# test-z013; NOT part of make test. Host/kernel probe
# (no allocator under test). Recorded run:
# tests/perf/recorded_run_z13_thp.txt.
$(BUILD)/probe_z013: tests/perf/probe_z013.c | $(BUILD)
	$(CC) $(CFLAGS) tests/perf/probe_z013.c -o $@

test-z013: $(BUILD)/probe_z013
	./$(BUILD)/probe_z013

.PHONY: test-z013

# Z-017: alignment audit probe - dual-leg (bench_k002
# pattern): glibc leg (-DLEG_GLIBC) and MimirAlloc leg
# (production LIBOBJS) built from tests/perf/probe_z017.c.
# Standalone via make test-z017; NOT part of make test.
# Recorded run: tests/perf/recorded_run_z13_z017.txt.
$(BUILD)/probe_z017_glibc.o: tests/perf/probe_z017.c | $(BUILD)
	$(CC) $(CFLAGS) -DLEG_GLIBC -c tests/perf/probe_z017.c -o $@

$(BUILD)/probe_z017_glibc: $(BUILD)/probe_z017_glibc.o
	$(CC) $(CFLAGS) $(BUILD)/probe_z017_glibc.o -o $@

$(BUILD)/probe_z017_mimir.o: tests/perf/probe_z017.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_z017.c -o $@

$(BUILD)/probe_z017_mimir: $(LIBOBJS) $(BUILD)/probe_z017_mimir.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_z017_mimir.o -o $@

test-z017: $(BUILD)/probe_z017_glibc $(BUILD)/probe_z017_mimir
	./$(BUILD)/probe_z017_glibc
	./$(BUILD)/probe_z017_mimir

.PHONY: test-z017

# Z-025: calloc and realloc anatomy probe - dual-leg
# (probe_z017 pattern): glibc leg (-DLEG_GLIBC) and
# MimirAlloc leg (production LIBOBJS) built from
# tests/perf/probe_z025.c. Standalone via make test-z025;
# NOT part of make test. Recorded run:
# tests/perf/recorded_run_z13_z025.txt. Runner:
# tests/perf/run_z025.sh (Z-004 protocol).
$(BUILD)/probe_z025_glibc.o: tests/perf/probe_z025.c | $(BUILD)
	$(CC) $(CFLAGS) -DLEG_GLIBC -c tests/perf/probe_z025.c -o $@

$(BUILD)/probe_z025_glibc: $(BUILD)/probe_z025_glibc.o
	$(CC) $(CFLAGS) $(BUILD)/probe_z025_glibc.o -o $@

$(BUILD)/probe_z025_mimir.o: tests/perf/probe_z025.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_z025.c -o $@

$(BUILD)/probe_z025_mimir: $(LIBOBJS) $(BUILD)/probe_z025_mimir.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_z025_mimir.o -o $@

test-z025: $(BUILD)/probe_z025_glibc $(BUILD)/probe_z025_mimir
	./$(BUILD)/probe_z025_glibc > /dev/null
	./$(BUILD)/probe_z025_mimir > /dev/null

.PHONY: test-z025

# Z-030: cross-thread frees into TLS-owned blocks - the
# remote-free queue suite under tests/concurrency/
# (test_z030_remote_queue.c + mimir_remote_queue_z030.h),
# built against the production LIBOBJS (test-only static
# link, like run_z028.sh). Standalone via make test-z030;
# NOT part of make test (standing gate counts untouched).
# Recorded run: tests/perf/recorded_run_z13_z030.txt.
$(BUILD)/test_z030_remote_queue.o: tests/concurrency/test_z030_remote_queue.c tests/concurrency/mimir_remote_queue_z030.h include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -Itests/concurrency -c tests/concurrency/test_z030_remote_queue.c -o $@

$(BUILD)/test_z030_remote_queue: $(LIBOBJS) $(BUILD)/test_z030_remote_queue.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_z030_remote_queue.o -o $@

test-z030: $(BUILD)/test_z030_remote_queue
	./$(BUILD)/test_z030_remote_queue

.PHONY: test-z030

# Z-043: producer/consumer free storm - the storm suite
# under tests/concurrency/ (test_z043_free_storm.c),
# built against the production LIBOBJS (test-only static
# link, like run_z030.sh). Standalone via make test-z043;
# NOT part of make test (standing gate counts untouched).
# Recorded run: tests/perf/recorded_run_z13_z043.txt.
$(BUILD)/test_z043_free_storm.o: tests/concurrency/test_z043_free_storm.c tests/concurrency/mimir_remote_queue_z030.h include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -Itests/concurrency -c tests/concurrency/test_z043_free_storm.c -o $@

$(BUILD)/test_z043_free_storm: $(LIBOBJS) $(BUILD)/test_z043_free_storm.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_z043_free_storm.o -o $@

test-z043: $(BUILD)/test_z043_free_storm
	./$(BUILD)/test_z043_free_storm

.PHONY: test-z043

# H-003: Phase 8 cross-thread frees evidence test -
# the test under tests/core/ (test_h003.c) verifies
# cross-thread frees on the span and mapping paths,
# cross-thread calloc/realloc, and the H-001 drain
# interplay, with exact audit-counter reconciliation
# after every phase (E-005 discipline). Standalone
# via make test-h003; NOT part of make test, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_h003.txt; slice record:
# docs/PHASE_8_CROSS_THREAD_FREES_H003.md.
$(BUILD)/test_h003.o: tests/core/test_h003.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_drain_h001.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_h003.c -o $@

$(BUILD)/test_h003: $(LIBOBJS) $(BUILD)/test_h003.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_h003.o -o $@

test-h003: $(BUILD)/test_h003
	./$(BUILD)/test_h003

.PHONY: test-h003

# H-004: Phase 8 contention controls probe - test-only
# harness under tests/perf/ (probe_h004.c) measuring the
# status quo (real allocator, H-002 Phase B shape) against
# two lock/queue MODELS (striped shards, bounded remote-
# free queue) over a fixed slot pool. Standalone via
# `make test-h004`; NOT part of `make test`, whose
# composition is unchanged. No src/ change. Recorded run:
# tests/perf/recorded_run_h004.txt; slice record:
# docs/PHASE_8_CONTENTION_CONTROLS_H004.md.
$(BUILD)/probe_h004.o: tests/perf/probe_h004.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_h004.c -o $@

$(BUILD)/probe_h004: $(LIBOBJS) $(BUILD)/probe_h004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_h004.o -o $@

test-h004: $(BUILD)/probe_h004
	./$(BUILD)/probe_h004

.PHONY: test-h004

# H-005: Phase 8 owner-death evidence test -
# the test under tests/core/ (test_h005.c) verifies
# owner death under contention: victims die (return
# and pthread_exit) mid-churn holding span and mapping
# blocks while churners work, and survivors verify and
# free the dead blocks cross-thread under renewed
# contention, with exact audit-counter reconciliation
# at quiescent points. Standalone via make test-h005;
# NOT part of make test, whose composition is
# unchanged. Recorded run:
# tests/core/recorded_run_h005.txt; slice record:
# docs/PHASE_8_OWNER_DEATH_H005.md.
$(BUILD)/test_h005.o: tests/core/test_h005.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_h005.c -o $@

$(BUILD)/test_h005: $(LIBOBJS) $(BUILD)/test_h005.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_h005.o -o $@

test-h005: $(BUILD)/test_h005
	./$(BUILD)/test_h005

.PHONY: test-h005

# H-006: Phase 8 thread cancellation + teardown evidence
# test - tests/core/test_h006.c exercises cancellation
# between allocator calls, cancellation with consumer
# cleanup handlers that free allocator blocks, teardown
# shapes (leftovers freed cross-thread, churn, H-001
# drain) and fork probes in disposable child processes
# (quiescent fork, fork+exec, mid-churn fork and async
# cancellation under parent timeouts), plus a dlclose
# teardown leg against the .so. Standalone via
# `make test-h006`; NOT part of `make test`, whose
# composition is unchanged. No src/ change. Recorded run:
# tests/core/recorded_run_h006.txt; slice record:
# docs/PHASE_8_CANCELLATION_TEARDOWN_H006.md.
$(BUILD)/test_h006.o: tests/core/test_h006.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_drain_h001.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_h006.c -o $@

$(BUILD)/test_h006: $(LIBOBJS) $(BUILD)/libmimiralloc.so $(BUILD)/test_h006.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_h006.o -o $@ -ldl

test-h006: $(BUILD)/test_h006
	./$(BUILD)/test_h006 $(BUILD)/libmimiralloc.so

.PHONY: test-h006
# H-007: Phase 8 stress probe - tests/perf/probe_h007.c
# Standalone via make test-h007; NOT part of make test.
# Recorded run: tests/perf/recorded_run_h007.txt.
$(BUILD)/probe_h007.o: tests/perf/probe_h007.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_h007.c -o $@

$(BUILD)/probe_h007: $(LIBOBJS) $(BUILD)/probe_h007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_h007.o -o $@

test-h007: $(BUILD)/probe_h007
	./$(BUILD)/probe_h007

.PHONY: test-h007

# H-008: Phase 8 latency + memory-under-contention probe -
# tests/perf/probe_h008.c (per-op percentiles, H-002 Phase B
# shape; held-set memory reconciliation at quiescent barrier
# points against the E-005/G-003 counters). Standalone via
# make test-h008; NOT part of make test, whose composition
# is unchanged. No src/ change. Recorded run:
# tests/perf/recorded_run_h008.txt; slice record:
# docs/PHASE_8_LATENCY_MEMORY_H008.md.
$(BUILD)/probe_h008.o: tests/perf/probe_h008.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/perf/probe_h008.c -o $@

$(BUILD)/probe_h008: $(LIBOBJS) $(BUILD)/probe_h008.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/probe_h008.o -o $@

test-h008: $(BUILD)/probe_h008
	./$(BUILD)/probe_h008

.PHONY: test-h008

# I-003: Phase 9 trim / cache-release evidence test -
# the test under tests/core/ (test_i003.c) verifies the
# on-demand trim of docs/PHASE_9_TRIM_IMPLEMENTATION.md
# (semantics per I-002 Section 5) against the I-003
# test-only surface (tests/core/mimir_trim_i003.h):
# retained bytes before trim, exact released counts
# reconciled at quiescent points against Q-RETAINED /
# Q-MAPPED, the G-003/E-005 counters and the OS-side
# anonymous figure, live blocks byte-identical after
# trim, class-fair partial trims, trim-again no-op, and
# the H-001 drain unregressed on the shared walk.
# Standalone via `make test-i003`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_i003.txt; slice record:
# docs/PHASE_9_TRIM_IMPLEMENTATION.md.
$(BUILD)/test_i003.o: tests/core/test_i003.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_trim_i003.h tests/core/mimir_tune_g007.h tests/core/mimir_drain_h001.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_i003.c -o $@

$(BUILD)/test_i003: $(LIBOBJS) $(BUILD)/test_i003.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_i003.o -o $@

test-i003: $(BUILD)/test_i003
	./$(BUILD)/test_i003

.PHONY: test-i003

# I-004: Phase 9 genuine allocation-failure evidence test -
# the test under tests/core/ (test_i004.c) verifies the
# failure-handling contract of docs/PHASE_9_FAILURE_HANDLING.md
# (contract per I-002 Section 6.3, FC-1..FC-6) against the
# test-only fault-injection shim
# (tests/core/mimir_fault_shim_i004.c, declared in
# tests/core/mimir_fault_i004.h): the shim object is linked
# ONLY into this binary, with -Wl,--wrap on the four L6
# platform entries, so the Nth mapping / release / lock /
# page-size call can be failed on demand (roadmap S9.3).
# No production object is changed and no other binary is
# wrapped. Standalone via `make test-i004`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_i004.txt; slice record:
# docs/PHASE_9_FAILURE_HANDLING.md.
$(BUILD)/mimir_fault_shim_i004.o: tests/core/mimir_fault_shim_i004.c tests/core/mimir_fault_i004.h src/platform/mimir_platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/mimir_fault_shim_i004.c -o $@

$(BUILD)/test_i004.o: tests/core/test_i004.c include/mimir_alloc.h tests/core/mimir_fault_i004.h tests/core/mimir_small_g003.h tests/core/mimir_trim_i003.h tests/core/mimir_tune_g007.h tests/core/mimir_drain_h001.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_i004.c -o $@

$(BUILD)/test_i004: $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/test_i004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/test_i004.o -Wl,--wrap=mimir_platform_map_acquire -Wl,--wrap=mimir_platform_map_release -Wl,--wrap=mimir_platform_global_lock -Wl,--wrap=mimir_platform_page_size -o $@

test-i004: $(BUILD)/test_i004
	./$(BUILD)/test_i004

.PHONY: test-i004

# I-005: Phase 9 constrained-memory evidence test -
# the test under tests/core/ (test_i005.c) runs the
# designed Phase 9 constrained runs of
# docs/PHASE_9_CONSTRAINED_RUNS.md (exit item X5):
# forked children calibrate a per-process RLIMIT_AS from
# their own measured VmSize baseline plus a headroom in
# exact page arithmetic, then prove clean-NULL refusal,
# non-overlap, smaller-request service, realloc failure
# atomicity, trim under pressure (I-003 surface) and a
# raw-mmap fragmentation leg under the real OS limit,
# with a 30 s per-child wall bound enforced by the
# parent. Standalone via `make test-i005`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_i005.txt; slice record:
# docs/PHASE_9_CONSTRAINED_RUNS.md.
$(BUILD)/test_i005.o: tests/core/test_i005.c include/mimir_alloc.h tests/core/mimir_trim_i003.h tests/core/mimir_tune_g007.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_i005.c -o $@

$(BUILD)/test_i005: $(LIBOBJS) $(BUILD)/test_i005.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_i005.o -o $@

test-i005: $(BUILD)/test_i005
	./$(BUILD)/test_i005

.PHONY: test-i005

# I-006: Phase 9 fragmentation and reclaim-overhead
# measurement test - the test under tests/core/
# (test_i006.c) measures fragmentation and reclaim
# overhead against the definitions of
# docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md S6.2
# (F-1..F-6), using the I-003 test-only trim surface
# (tests/core/mimir_trim_i003.h): a stated fragmented
# state across classes 64 / 256 / 1024, F-3 / F-1(b) /
# F-2 figures and the R-EXT / R-INT ratios from measured
# quantities only, cross-class unusability proof,
# free-path / partial / full / no-op reclaim costs with
# exact span and byte deltas (wall time reported as
# timing-never-an-oracle), hit-vs-miss follow-on cost,
# live blocks byte-identical across every reclaim step.
# Standalone via `make test-i006`; NOT part of
# `make test`, whose composition is unchanged. Recorded
# run: tests/core/recorded_run_i006.txt; slice record:
# docs/PHASE_9_FRAGMENTATION_AND_RECLAIM.md.
$(BUILD)/test_i006.o: tests/core/test_i006.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_trim_i003.h tests/core/mimir_tune_g007.h tests/core/mimir_drain_h001.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_i006.c -o $@

$(BUILD)/test_i006: $(LIBOBJS) $(BUILD)/test_i006.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_i006.o -o $@

test-i006: $(BUILD)/test_i006
	./$(BUILD)/test_i006

.PHONY: test-i006

# I-007: Phase 9 telemetry separation proof -
# the test under tests/core/ (test_i007.c) proves the
# I-002 separation of allocator-managed quantities
# (Q-MAPPED / Q-RETAINED, locked snapshots via the I-003
# test-only surface) from process-wide figures (VmSize /
# VmRSS / smaps_rollup / maps) with measured legs in both
# directions and a quiescent reconciliation. Standalone
# via `make test-i007`; NOT part of `make test`, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_i007.txt; slice record:
# docs/PHASE_9_TELEMETRY_SEPARATION.md.
$(BUILD)/test_i007.o: tests/core/test_i007.c include/mimir_alloc.h tests/core/mimir_small_g003.h tests/core/mimir_trim_i003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_i007.c -o $@

$(BUILD)/test_i007: $(LIBOBJS) $(BUILD)/test_i007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_i007.o -o $@

test-i007: $(BUILD)/test_i007
	./$(BUILD)/test_i007

.PHONY: test-i007

# I-008: Phase 9 repeated pressure cycles +
# responsiveness under pressure test - the test under
# tests/core/ (test_i008.c) runs 10 stated pressure
# cycles in one RLIMIT_AS-calibrated child (I-005 form,
# 128-page headroom, parent-enforced 30 s wall bound):
# each cycle allocates exactly 1200 x 64 B (5 spans),
# frees to exactly 5 retained spans, fills large 4096 B
# blocks to the clean NULL (exactly 61), probes
# responsiveness under pressure (small alloc + local
# health check, timing-never-an-oracle), releases and
# trims back to an exactly reconciled quiescent point;
# all 10 cycle tuples must be identical (no ratchet).
# The parent samples Samtal HTTP before, during and
# after the cycles (G-008 house form). Standalone via
# `make test-i008`; NOT part of `make test`, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_i008.txt; slice record:
# docs/PHASE_9_PRESSURE_CYCLES.md.
$(BUILD)/test_i008.o: tests/core/test_i008.c include/mimir_alloc.h tests/core/mimir_trim_i003.h tests/core/mimir_tune_g007.h tests/core/mimir_small_g003.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_i008.c -o $@

$(BUILD)/test_i008: $(LIBOBJS) $(BUILD)/test_i008.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_i008.o -o $@

test-i008: $(BUILD)/test_i008
	./$(BUILD)/test_i008

.PHONY: test-i008

# J-002: Phase 10 statistics counters evidence test -
# the test under tests/core/ (test_j002.c) verifies the
# Phase 10 statistics surface of
# docs/PHASE_10_STATISTICS.md against the J-002 test-only
# header (tests/core/mimir_stats_j002.h): ONE locked
# snapshot whose counters move by exactly the scripted
# amounts for stated small / large / calloc / realloc
# workloads (definitions written before the run),
# Q-MAPPED / Q-RETAINED cross-reads against the I-003
# surface, a failure leg through the I-004 --wrap shim
# proving alloc_failures counts genuine failures while
# posture NULLs are counted separately, span occupancy
# per class, cross-thread frees (PARTIAL: mapping path),
# lock contention honestly ABSENT, and snapshot
# invariants under concurrent churn. The I-004 shim
# object is linked ONLY into this binary, exactly as in
# test-i004. Standalone via `make test-j002`; NOT part
# of `make test`, whose composition is unchanged.
# Recorded run: tests/core/recorded_run_j002.txt;
# slice record: docs/PHASE_10_STATISTICS.md.
$(BUILD)/test_j002.o: tests/core/test_j002.c include/mimir_alloc.h tests/core/mimir_stats_j002.h tests/core/mimir_trim_i003.h tests/core/mimir_drain_h001.h tests/core/mimir_small_g003.h tests/core/mimir_fault_i004.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_j002.c -o $@

$(BUILD)/test_j002: $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/test_j002.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/test_j002.o -Wl,--wrap=mimir_platform_map_acquire -Wl,--wrap=mimir_platform_map_release -Wl,--wrap=mimir_platform_global_lock -Wl,--wrap=mimir_platform_page_size -o $@

test-j002: $(BUILD)/test_j002
	./$(BUILD)/test_j002

.PHONY: test-j002

# J-003: Phase 10 statistics BALANCE PROOF -- the test
# under tests/core/ (test_j003.c) proves the J-001
# contract identities R1-R12 (docs/
# PHASE_10_FOUNDATION_PLAN.md S4.4) EXACTLY at quiescent
# points against the J-002 locked snapshot, across
# workloads: small/large churn with calloc/realloc,
# per-entry-point call accounting (R9), a fault leg
# through the I-004 --wrap shim (incl. a rolled-back
# release), I-003-shape trim cycles, cross-thread frees
# incl. the disclosed realloc transfer-retire narrowing
# asserted as documented, an exact peak high-water leg,
# and an I-008-style RLIMIT_AS pressure cycle in a
# forked child. Identities that cannot be asserted
# exactly (D-LOCK, D-FAIL-BUDGET, R10 free-slots,
# Tier-2 cross-thread) are recorded NOT-PROVABLE in the
# slice doc, never silently skipped. The I-004 shim
# object is linked ONLY into this binary, exactly as in
# test-j002. Standalone via `make test-j003`; NOT part
# of `make test`, whose composition is unchanged.
# Recorded run: tests/core/recorded_run_j003.txt;
# slice record: docs/PHASE_10_BALANCE_PROOF.md.
$(BUILD)/test_j003.o: tests/core/test_j003.c include/mimir_alloc.h tests/core/mimir_stats_j002.h tests/core/mimir_trim_i003.h tests/core/mimir_drain_h001.h tests/core/mimir_small_g003.h tests/core/mimir_fault_i004.h tests/core/mimir_tune_g007.h tests/abi/mimir_audit_e005.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_j003.c -o $@

$(BUILD)/test_j003: $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/test_j003.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/test_j003.o -Wl,--wrap=mimir_platform_map_acquire -Wl,--wrap=mimir_platform_map_release -Wl,--wrap=mimir_platform_global_lock -Wl,--wrap=mimir_platform_page_size -o $@

test-j003: $(BUILD)/test_j003
	./$(BUILD)/test_j003

.PHONY: test-j003

# J-004: Phase 10 debug-only red zones + canaries evidence
# test - the test under tests/core/ (test_j004.c) verifies
# the debug flavour of docs/PHASE_10_DEBUG_REDZONES.md:
# in disposable child processes it overwrites a post-zone
# and a pre-zone by a stated amount and proves detection
# fires (SIGABRT + the fixed fd-2 record), proves the
# realloc leg, the on-demand verify-all leg, counter
# semantics unchanged (J-002 snapshot), debug span
# capacities at the exact layout arithmetic, and a bench
# leg (timing-never-an-oracle). Run via
# `make DEBUG=1 test-j004` (objects in build-debug/);
# `make DEBUG=1 test-debug` runs the whole `make test`
# suite plus this test under the debug build. In a
# production build the binary runs only the flavour leg
# (production capacities, no debug entry point) and the
# committed recorded run is the DEBUG=1 one. Standalone;
# NOT part of `make test`, whose composition is unchanged.
# Recorded run: tests/core/recorded_run_j004.txt; slice
# record: docs/PHASE_10_DEBUG_REDZONES.md.
$(BUILD)/test_j004.o: tests/core/test_j004.c include/mimir_alloc.h tests/core/mimir_debug_j004.h tests/core/mimir_stats_j002.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_j004.c -o $@

$(BUILD)/test_j004: $(LIBOBJS) $(BUILD)/test_j004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_j004.o -o $@

test-j004: $(BUILD)/test_j004
	./$(BUILD)/test_j004

# test-debug runs every `make test` binary plus test-j004
# under the current flavour's build dir and REPORTS each
# binary's exit status without masking any of them (the
# loop's own exit is the echo's). Grading of the debug
# flavour lives in tests/core/recorded_run_j004.txt and
# docs/PHASE_10_DEBUG_REDZONES.md INC-J004-02: three banked
# checks encode production layout arithmetic exactly and
# diverge under debug BY DESIGN (plan S5.2); every other
# check must pass and no run may abort.
test-debug: $(BUILD)/test_smoke $(BUILD)/test_core $(BUILD)/test_b007 $(BUILD)/test_c005 $(BUILD)/test_c006 $(BUILD)/test_c007 $(BUILD)/test_c008 $(BUILD)/test_d004 $(BUILD)/test_j004
	@for t in test_smoke test_core test_b007 test_c005 test_c006 test_c007 test_c008 test_d004 test_j004; do ./$(BUILD)/$$t >/dev/null 2>&1; echo "test-debug $(BUILD)/$$t exit=$$?"; done

.PHONY: test-j004 test-debug

# J-005: Phase 10 debug flavour -- poisoning + quarantine
# of freed blocks (docs/PHASE_10_POISON_QUARANTINE.md;
# plan docs/PHASE_10_FOUNDATION_PLAN.md S5.1). The debug
# flavour is the ONE flag MIMIR_DEBUG: every debug object
# is compiled with -DMIMIR_DEBUG=1 into the SEPARATE
# directory build-debug/, so no debug object can be
# linked into a production artifact by stale-file
# accident, and the production targets above are
# untouched (plain make / make test / make lib build
# exactly the banked production objects; `make DEBUG=1
# test-j005` is accepted and identical, the flavour
# being carried by these targets themselves). The debug
# shared library (make lib-debug) is built from PIC
# objects under build-debug/pic/ with the same version
# script, so nm -D is exactly 4 in both flavours.
# Standalone via `make test-j005`; NOT part of
# `make test`, whose composition is unchanged.
# Recorded run: tests/core/recorded_run_j005.txt.
DBUILD  = build-debug
DCFLAGS = $(CFLAGS) -DMIMIR_DEBUG=1
DLIBOBJS = $(DBUILD)/mimir_alloc.o $(DBUILD)/mimir_core.o \
          $(DBUILD)/mimir_platform.o

$(DBUILD):
	mkdir -p $(DBUILD)

$(DBUILD)/mimir_alloc.o: src/api/mimir_alloc.c include/mimir_alloc.h src/core/mimir_core.h | $(DBUILD)
	$(CC) $(DCFLAGS) -c src/api/mimir_alloc.c -o $@

$(DBUILD)/mimir_core.o: src/core/mimir_core.c src/core/mimir_core.h src/platform/mimir_platform.h tests/core/mimir_quarantine_j005.h | $(DBUILD)
	$(CC) $(DCFLAGS) -c src/core/mimir_core.c -o $@

$(DBUILD)/mimir_platform.o: src/platform/mimir_platform.c src/platform/mimir_platform.h | $(DBUILD)
	$(CC) $(DCFLAGS) -c src/platform/mimir_platform.c -o $@

$(DBUILD)/test_j005.o: tests/core/test_j005.c include/mimir_alloc.h tests/core/mimir_quarantine_j005.h tests/core/mimir_stats_j002.h tests/core/mimir_trim_i003.h tests/core/mimir_drain_h001.h | $(DBUILD)
	$(CC) $(DCFLAGS) -c tests/core/test_j005.c -o $@

$(DBUILD)/test_j005: $(DLIBOBJS) $(DBUILD)/test_j005.o
	$(CC) $(DCFLAGS) $(DLIBOBJS) $(DBUILD)/test_j005.o -o $@

test-j005: $(DBUILD)/test_j005
	./$(DBUILD)/test_j005

DPICDIR  = $(DBUILD)/pic
DPICOBJS = $(DPICDIR)/mimir_alloc_pic.o $(DPICDIR)/mimir_core_pic.o \
          $(DPICDIR)/mimir_platform_pic.o

$(DPICDIR): | $(DBUILD)
	mkdir -p $(DPICDIR)

$(DPICDIR)/mimir_alloc_pic.o: src/api/mimir_alloc.c include/mimir_alloc.h src/core/mimir_core.h | $(DPICDIR)
	$(CC) $(DCFLAGS) -fPIC -fvisibility=default -c src/api/mimir_alloc.c -o $@

$(DPICDIR)/mimir_core_pic.o: src/core/mimir_core.c src/core/mimir_core.h src/platform/mimir_platform.h | $(DPICDIR)
	$(CC) $(DCFLAGS) -fPIC -fvisibility=hidden -c src/core/mimir_core.c -o $@

$(DPICDIR)/mimir_platform_pic.o: src/platform/mimir_platform.c src/platform/mimir_platform.h | $(DPICDIR)
	$(CC) $(DCFLAGS) -fPIC -fvisibility=hidden -c src/platform/mimir_platform.c -o $@

$(DBUILD)/libmimiralloc.so: $(DPICOBJS) $(MAPFILE) | $(DBUILD)
	$(CC) -flto -shared -Wl,--version-script=$(MAPFILE) -o $@ $(DPICOBJS) -pthread

lib-debug: $(DBUILD)/libmimiralloc.so

.PHONY: test-j005 lib-debug

# J-006: Phase 10 metadata integrity + corrupted-metadata
# posture evidence test - the test under tests/core/
# (test_j006.c) inventories the allocator's metadata
# structures (in-mapping descriptor, span header,
# free lists, registries, counters) and probes each
# corruption class in DISPOSABLE CHILD PROCESSES ONLY:
# lookup-validation refusals, the test-only integrity
# walk (tests/core/mimir_integrity_j006.h), bounded
# registry/free-list walks, and the honestly silent
# classes (consistent field corruption, free_head naming
# a live object) recorded as INC-J006 items in
# docs/PHASE_10_METADATA_INTEGRITY.md. Standalone via
# `make test-j006`; NOT part of `make test`, whose
# composition is unchanged. Recorded run:
# tests/core/recorded_run_j006.txt; slice record:
# docs/PHASE_10_METADATA_INTEGRITY.md.
$(BUILD)/test_j006.o: tests/core/test_j006.c include/mimir_alloc.h tests/core/mimir_stats_j002.h tests/core/mimir_drain_h001.h tests/core/mimir_integrity_j006.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_j006.c -o $@

$(BUILD)/test_j006: $(LIBOBJS) $(BUILD)/test_j006.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/test_j006.o -o $@

test-j006: $(BUILD)/test_j006
	./$(BUILD)/test_j006

.PHONY: test-j006

# J-007: Phase 10 allocation-free diagnostics evidence
# test - the test under tests/core/ (test_j007.c) verifies
# the diagnostics channel of docs/PHASE_10_DIAGNOSTICS.md
# against the J-007 test-only header
# (tests/core/mimir_diag_j007.h): fixed-format field-by-
# field usefulness on captured fd 2 bytes, ZERO malloc-
# family and ZERO L6 calls on the report path (counting
# interposer tests/core/mimir_diag_shim_j007.c, --wrap
# malloc/calloc/realloc/free/write, plus the I-004 fault
# shim's counters), recursion proof (a report attempted
# from inside a report's own write is suppressed; a
# report from inside an allocation failure adds zero
# acquires; max depth 1), wiring at the real failure
# points (calloc overflow, oversize, injected backing /
# lock / page-size, realloc grow backing), and silence
# for posture NULLs, successes and invalid-pointer
# realloc, with J-002 counter semantics intact. The two
# shim objects are linked ONLY into this binary, exactly
# as in test-j002. Standalone via `make test-j007`; NOT
# part of `make test`, whose composition is unchanged.
# Recorded run: tests/core/recorded_run_j007.txt;
# slice record: docs/PHASE_10_DIAGNOSTICS.md.
$(BUILD)/mimir_diag_shim_j007.o: tests/core/mimir_diag_shim_j007.c tests/core/mimir_diag_shim_j007.h src/core/mimir_core.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/mimir_diag_shim_j007.c -o $@

$(BUILD)/test_j007.o: tests/core/test_j007.c include/mimir_alloc.h tests/core/mimir_stats_j002.h tests/core/mimir_diag_j007.h tests/core/mimir_fault_i004.h tests/core/mimir_diag_shim_j007.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/core/test_j007.c -o $@

$(BUILD)/test_j007: $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/mimir_diag_shim_j007.o $(BUILD)/test_j007.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/mimir_fault_shim_i004.o $(BUILD)/mimir_diag_shim_j007.o $(BUILD)/test_j007.o -Wl,--wrap=mimir_platform_map_acquire -Wl,--wrap=mimir_platform_map_release -Wl,--wrap=mimir_platform_global_lock -Wl,--wrap=mimir_platform_page_size -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free -Wl,--wrap=write -o $@

test-j007: $(BUILD)/test_j007
	./$(BUILD)/test_j007

.PHONY: test-j007

# J-008: hardened build flavour + overflow/overhead evidence -
# a SEPARATE build directory (build-hardened/) so no hardened
# object can be linked into a production artefact by stale-
# file accident (the J-001 S5.1 directory-and-macro rule,
# carried to hardening). Flags probed on this Pi's toolchain
# (gcc 12.2 / glibc 2.36); per-flag verdicts and artefact
# evidence: docs/PHASE_10_HARDENED_BUILD.md. The default
# production build is untouched: no default target, flag or
# composition changes. Standalone via `make lib-hardened`,
# `make test-j008`, `make bench-j008`; NOT part of `make test`.
HARD    = build-hardened
HCFLAGS = $(CFLAGS) -O2 -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fstack-clash-protection -Wformat -Wformat-security -Werror=format-security
HARDLINK = -Wl,-z,relro,-z,now,-z,noexecstack
HARDOBJS = $(HARD)/mimir_alloc.o $(HARD)/mimir_core.o $(HARD)/mimir_platform.o
HARDPIC  = $(HARD)/pic/mimir_alloc_pic.o $(HARD)/pic/mimir_core_pic.o $(HARD)/pic/mimir_platform_pic.o

$(HARD):
	mkdir -p $(HARD)

$(HARD)/pic: | $(HARD)
	mkdir -p $(HARD)/pic

$(HARD)/mimir_alloc.o: src/api/mimir_alloc.c include/mimir_alloc.h src/core/mimir_core.h | $(HARD)
	$(CC) $(HCFLAGS) -c src/api/mimir_alloc.c -o $@

$(HARD)/mimir_core.o: src/core/mimir_core.c src/core/mimir_core.h src/platform/mimir_platform.h | $(HARD)
	$(CC) $(HCFLAGS) -c src/core/mimir_core.c -o $@

$(HARD)/mimir_platform.o: src/platform/mimir_platform.c src/platform/mimir_platform.h | $(HARD)
	$(CC) $(HCFLAGS) -c src/platform/mimir_platform.c -o $@

$(HARD)/pic/mimir_alloc_pic.o: src/api/mimir_alloc.c include/mimir_alloc.h src/core/mimir_core.h | $(HARD)/pic
	$(CC) $(HCFLAGS) -fPIC -fvisibility=default -c src/api/mimir_alloc.c -o $@

$(HARD)/pic/mimir_core_pic.o: src/core/mimir_core.c src/core/mimir_core.h src/platform/mimir_platform.h | $(HARD)/pic
	$(CC) $(HCFLAGS) -fPIC -fvisibility=hidden -c src/core/mimir_core.c -o $@

$(HARD)/pic/mimir_platform_pic.o: src/platform/mimir_platform.c src/platform/mimir_platform.h | $(HARD)/pic
	$(CC) $(HCFLAGS) -fPIC -fvisibility=hidden -c src/platform/mimir_platform.c -o $@

$(HARD)/libmimiralloc.so: $(HARDPIC) $(MAPFILE) | $(HARD)
	$(CC) -flto -shared $(HARDLINK) -Wl,--version-script=$(MAPFILE) -o $@ $(HARDPIC) -pthread

lib-hardened: $(HARD)/libmimiralloc.so

$(HARD)/test_j008.o: tests/core/test_j008.c include/mimir_alloc.h tests/core/mimir_stats_j002.h | $(HARD)
	$(CC) $(HCFLAGS) -fPIE -c tests/core/test_j008.c -o $@

$(HARD)/test_j008: $(HARDOBJS) $(HARD)/test_j008.o
	$(CC) $(HCFLAGS) -fPIE -pie $(HARDLINK) $(HARDOBJS) $(HARD)/test_j008.o -o $@

test-j008: $(HARD)/test_j008 $(HARD)/libmimiralloc.so
	./$(HARD)/test_j008

$(BUILD)/bench_j008.o: tests/core/bench_j008.c include/mimir_alloc.h tests/core/mimir_stats_j002.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/core/bench_j008.c -o $@

$(BUILD)/bench_j008: $(LIBOBJS) $(BUILD)/bench_j008.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_j008.o -o $@

$(HARD)/bench_j008.o: tests/core/bench_j008.c include/mimir_alloc.h tests/core/mimir_stats_j002.h | $(HARD)
	$(CC) $(HCFLAGS) -c tests/core/bench_j008.c -o $@

$(HARD)/bench_j008: $(HARDOBJS) $(HARD)/bench_j008.o
	$(CC) $(HCFLAGS) $(HARDOBJS) $(HARD)/bench_j008.o -o $@

bench-j008: $(BUILD)/bench_j008 $(HARD)/bench_j008
	@echo production; ./$(BUILD)/bench_j008
	@echo production+stats; ./$(BUILD)/bench_j008 stats
	@echo hardened; ./$(HARD)/bench_j008
	@echo hardened+stats; ./$(HARD)/bench_j008 stats

.PHONY: lib-hardened test-j008 bench-j008

# K-002: Phase 11 baseline benchmark suite - one harness
# source (tests/perf/bench_k002.c) built twice: glibc leg
# (-DLEG_GLIBC, stdlib malloc/free) and MimirAlloc leg
# (banked LIBOBJS). Standalone via `make bench-k002`:
# it is NOT part of `make test`. The driver object is
# compiled -O2 (bench_j008 precedent); the allocator
# objects stay exactly the production build. The runner
# executes >= 5 repetitions per leg as separate processes.
$(BUILD)/bench_k002_glibc.o: tests/perf/bench_k002.c | $(BUILD)
	$(CC) $(CFLAGS) -O2 -DLEG_GLIBC -c tests/perf/bench_k002.c -o $@

$(BUILD)/bench_k002_glibc: $(BUILD)/bench_k002_glibc.o
	$(CC) $(CFLAGS) -O2 $(BUILD)/bench_k002_glibc.o -o $@

$(BUILD)/bench_k002_mimir.o: tests/perf/bench_k002.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/perf/bench_k002.c -o $@

$(BUILD)/bench_k002_mimir: $(LIBOBJS) $(BUILD)/bench_k002_mimir.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_k002_mimir.o -o $@

bench-k002: $(BUILD)/bench_k002_glibc $(BUILD)/bench_k002_mimir
	./tests/perf/run_bench_k002.sh /tmp/k002_raw.txt 7
	python3 tests/perf/summarize_k002.py /tmp/k002_raw.txt

.PHONY: bench-k002

# K-004: build-flavour bench (docs/PHASE_11_PERFORMANCE_
# K004.md). Standalone via `make bench-k004`; NOT part of
# `make test`. Bench TU compiled -O2 (bench_j008 precedent);
# allocator objects carry the shipped CFLAGS.
$(BUILD)/bench_k004.o: tests/core/bench_k004.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/core/bench_k004.c -o $@
$(BUILD)/bench_k004: $(LIBOBJS) $(BUILD)/bench_k004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_k004.o -o $@
bench-k004: $(BUILD)/bench_k004
	./$(BUILD)/bench_k004
.PHONY: bench-k004

# K-005: locality bench - one harness (tests/perf/bench_k005.c)
# linked against banked LIBOBJS (-O0 shipped flavour; harness
# -O2, bench_k002 precedent). Standalone, NOT in `make test`.
$(BUILD)/bench_k005.o: tests/perf/bench_k005.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/perf/bench_k005.c -o $@
$(BUILD)/bench_k005: $(LIBOBJS) $(BUILD)/bench_k005.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_k005.o -o $@
bench-k005: $(BUILD)/bench_k005
	./tests/perf/run_bench_k005.sh /tmp/k005_raw.txt 7
	python3 tests/perf/summarize_k005.py /tmp/k005_raw.txt
.PHONY: bench-k005

# K-006: false-sharing + concurrency-scaling probe
# (docs/PHASE_11_FALSE_SHARING_SCALING.md). One source, two
# binaries (bench_k002 pattern): -DLEG_GLIBC and the shipped
# -O2 mimir leg. Standalone via `make bench-k006` (7 fresh-
# process reps); `make test-k006` is the 1-rep smoke form.
# Neither is part of `make test`.
$(BUILD)/bench_k006_glibc.o: tests/perf/probe_k006.c | $(BUILD)
	$(CC) $(CFLAGS) -O2 -DLEG_GLIBC -c tests/perf/probe_k006.c -o $@

$(BUILD)/bench_k006_glibc: $(BUILD)/bench_k006_glibc.o
	$(CC) $(CFLAGS) -O2 $(BUILD)/bench_k006_glibc.o -o $@

$(BUILD)/bench_k006_mimir.o: tests/perf/probe_k006.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/perf/probe_k006.c -o $@

$(BUILD)/bench_k006_mimir: $(LIBOBJS) $(BUILD)/bench_k006_mimir.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_k006_mimir.o -o $@

bench-k006: $(BUILD)/bench_k006_glibc $(BUILD)/bench_k006_mimir
	./tests/perf/run_bench_k006.sh /tmp/k006_raw.txt 7
	python3 tests/perf/summarize_k006.py /tmp/k006_raw.txt

test-k006: $(BUILD)/bench_k006_glibc $(BUILD)/bench_k006_mimir
	./tests/perf/run_bench_k006.sh /tmp/k006_smoke.txt 1
	python3 tests/perf/summarize_k006.py /tmp/k006_smoke.txt

.PHONY: bench-k006 test-k006

# K-007: Phase 11 batching experiments - one harness
# (tests/perf/bench_k007.c) built twice: glibc leg
# (-DLEG_GLIBC, stdlib malloc/free) and MimirAlloc leg
# (banked LIBOBJS, shipped -O2 flavour per K-004; harness
# compiled -O2, bench_k002 precedent). Standalone via
# `make bench-k007`; NOT part of `make test`.
$(BUILD)/bench_k007_glibc.o: tests/perf/bench_k007.c | $(BUILD)
	$(CC) $(CFLAGS) -O2 -DLEG_GLIBC -c tests/perf/bench_k007.c -o $@
$(BUILD)/bench_k007_glibc: $(BUILD)/bench_k007_glibc.o
	$(CC) $(CFLAGS) -O2 $(BUILD)/bench_k007_glibc.o -o $@
$(BUILD)/bench_k007_mimir.o: tests/perf/bench_k007.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/perf/bench_k007.c -o $@
$(BUILD)/bench_k007_mimir: $(LIBOBJS) $(BUILD)/bench_k007_mimir.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_k007_mimir.o -o $@
bench-k007: $(BUILD)/bench_k007_glibc $(BUILD)/bench_k007_mimir
	./tests/perf/run_bench_k007.sh /tmp/k007_raw.txt 7
	python3 tests/perf/summarize_k007.py /tmp/k007_raw.txt
.PHONY: bench-k007

# K-008: tail-latency + RSS/PSS + energy probe
# (docs/PHASE_11_TAIL_RSS_ENERGY_K008.md). One source, two
# binaries (bench_k002 pattern): -DLEG_GLIBC and the shipped
# -O2 mimir leg. Standalone via `make bench-k008` (5 fresh-
# process reps + PMIC energy phase); `make test-k008` is the
# 1-rep smoke form. Neither is part of `make test`.
$(BUILD)/bench_k008_glibc.o: tests/perf/probe_k008.c | $(BUILD)
	$(CC) $(CFLAGS) -O2 -DLEG_GLIBC -c tests/perf/probe_k008.c -o $@

$(BUILD)/bench_k008_glibc: $(BUILD)/bench_k008_glibc.o
	$(CC) $(CFLAGS) -O2 $(BUILD)/bench_k008_glibc.o -o $@

$(BUILD)/bench_k008_mimir.o: tests/perf/probe_k008.c include/mimir_alloc.h | $(BUILD)
	$(CC) $(CFLAGS) -O2 -c tests/perf/probe_k008.c -o $@

$(BUILD)/bench_k008_mimir: $(LIBOBJS) $(BUILD)/bench_k008_mimir.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/bench_k008_mimir.o -o $@

bench-k008: $(BUILD)/bench_k008_glibc $(BUILD)/bench_k008_mimir
	./tests/perf/run_bench_k008.sh /tmp/k008_raw.txt 5
	python3 tests/perf/summarize_k008.py /tmp/k008_raw.txt

test-k008: $(BUILD)/bench_k008_glibc $(BUILD)/bench_k008_mimir
	./tests/perf/run_bench_k008.sh /tmp/k008_smoke.txt 1
	python3 tests/perf/summarize_k008.py /tmp/k008_smoke.txt

.PHONY: bench-k008 test-k008

# M-004: Phase 13 fuzzing harness (docs/PHASE_13_FUZZING_M004.md;
# roadmap S10). Standalone via `make test-fuzz-m004`: seeded,
# bounded sequences against an ownership model, linking the
# LIBOBJS directly so the test-visible stats (J-002) and trim
# (I-003) routes are reachable. NOT part of `make test`.
$(BUILD)/fuzz_m004.o: tests/fuzz/fuzz_m004.c include/mimir_alloc.h tests/core/mimir_stats_j002.h tests/core/mimir_trim_i003.h | $(BUILD)
	$(CC) $(CFLAGS) -c tests/fuzz/fuzz_m004.c -o $@

$(BUILD)/fuzz_m004: $(LIBOBJS) $(BUILD)/fuzz_m004.o
	$(CC) $(CFLAGS) $(LIBOBJS) $(BUILD)/fuzz_m004.o -o $@

test-fuzz-m004: $(BUILD)/fuzz_m004
	./tests/fuzz/run_fuzz_m004.sh ./$(BUILD)/fuzz_m004

.PHONY: test-fuzz-m004

# M-007 (Phase 13): versioning, SONAME and install - see
# docs/PHASE_13_VERSIONING_AND_RELEASE_M007.md (decisions
# D1-D5) and docs/REPRODUCIBLE_BUILD.md. Purely additive:
# the banked lib/static-lib/test targets above are untouched;
# the versioned library is built from the same PIC objects
# alongside build/libmimiralloc.so. ABI_EPOCH is the D2 ABI
# version (the SONAME integer); MIMIR_VERSION comes from the
# VERSION file at the repository root (D1).
MIMIR_VERSION := $(shell cat VERSION)
ABI_EPOCH     := 0
VLIB          = $(BUILD)/libmimiralloc.so.$(MIMIR_VERSION)
VSONAME       = libmimiralloc.so.$(ABI_EPOCH)
PREFIX        ?= /usr/local
LIBDIR        = $(PREFIX)/lib
INCLUDEDIR    = $(PREFIX)/include

$(VLIB): $(PICOBJS) $(MAPFILE) VERSION | $(BUILD)
	$(CC) -flto -shared -Wl,--version-script=$(MAPFILE) -Wl,-soname,$(VSONAME) -o $@ $(PICOBJS) -pthread

$(BUILD)/$(VSONAME): $(VLIB)
	ln -sf libmimiralloc.so.$(MIMIR_VERSION) $@

lib-versioned: $(VLIB) $(BUILD)/$(VSONAME)

print-version:
	@echo $(MIMIR_VERSION)

install: $(VLIB) include/mimir_alloc.h
	install -d $(DESTDIR)$(LIBDIR) $(DESTDIR)$(INCLUDEDIR)
	install -m 0755 $(VLIB) $(DESTDIR)$(LIBDIR)/libmimiralloc.so.$(MIMIR_VERSION)
	ln -sf libmimiralloc.so.$(MIMIR_VERSION) $(DESTDIR)$(LIBDIR)/$(VSONAME)
	ln -sf $(VSONAME) $(DESTDIR)$(LIBDIR)/libmimiralloc.so
	install -m 0644 include/mimir_alloc.h $(DESTDIR)$(INCLUDEDIR)/mimir_alloc.h
ifeq ($(DESTDIR),)
	ldconfig $(LIBDIR) 2>/dev/null || true
endif

.PHONY: lib-versioned print-version install
