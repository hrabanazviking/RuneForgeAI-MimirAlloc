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
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc
BUILD   = build

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
	$(CC) -shared -Wl,--version-script=$(MAPFILE) -o $@ $(PICOBJS) -pthread

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
