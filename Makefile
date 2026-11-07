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
