# Reproducible-build instructions - MimirAlloc 0.1.0 (slice M-007)

This document is deliverable 4 of slice M-007. It defines the exact
procedure that produced the release artifacts whose SHA-256 checksums
are committed at release/SHA256SUMS. Re-running this procedure from
the committed tree is the durability mechanism: no binary, archive or
tarball is committed to the repository. The decisions behind these
steps are D1-D5 in docs/PHASE_13_VERSIONING_AND_RELEASE_M007.md; the
verification evidence for the runs performed in this slice is in
tests/abi/recorded_run_m007.txt.

## Scope of the claim (D5)

Byte-identical means: the same sha256 for an artifact produced by two
full clean builds of the same tree state on this Pi, following this
document exactly. Inside the claim: the two built binary artifacts
(libmimiralloc.so.0.1.0, libmimiralloc-0.1.0.a) and the source
tarball (mimiralloc-0.1.0.tar.gz) - the tarball is inside only
because its generation is normalised (step 4); any two-build
difference withdraws its claim in the record. The public header
(mimir_alloc.h) is a verbatim copy, listed in SHA256SUMS but not a
build claim. No cross-machine claim is made: the claim is this Pi,
this compiler, this tree state.

## Pinned inputs

- Device: Raspberry Pi Mimir (purpleviking1), aarch64, Debian 12.
- Compiler: cc (Debian 12.2.0-14+deb12u1) 12.2.0, as recorded on
  the Pi in the recorded run.
- Flags: exactly the banked Makefile flags; no flag is added or
  overridden by this procedure.
- Version: VERSION at the repository root (0.1.0).
- SOURCE_DATE_EPOCH = 1794084216 (the timestamp of the base commit
  6b312e3197cdb2e491ab0ffa24a4ba8ab95a1b17).
- Locale/timezone neutrality for the tarball step: LC_ALL=C, TZ=UTC.

## Procedure

1. Clean build (from the worktree at the slice tree state):

    make clean
    make lib-versioned static-lib

   The unversioned lib target is not used for release artifacts.

2. Verify the build before cutting:

    make test
       # expect: CHECKS: 4590 run, 0 failed
    nm -D --defined-only build/libmimiralloc.so.0.1.0
       # expect: exactly 4 symbols (mimir_malloc, mimir_free,
       # mimir_calloc, mimir_realloc)
    readelf -d build/libmimiralloc.so.0.1.0
       # expect SONAME: libmimiralloc.so.0

3. Stage (staging directory OUTSIDE the committed tree; the runs in
   this slice used /tmp/m007-release-0.1.0 and
   /tmp/m007-release2-0.1.0):

    cp build/libmimiralloc.so.0.1.0 STAGING/libmimiralloc.so.0.1.0
    cp build/libmimiralloc.a STAGING/libmimiralloc-0.1.0.a
    cp include/mimir_alloc.h STAGING/mimir_alloc.h

4. Source tarball (normalised). The file list is the tracked tree at
   worktree state plus the new source files of this slice (VERSION,
   docs/PHASE_13_VERSIONING_AND_RELEASE_M007.md and this file).
   Files generated FROM the artifacts (release/SHA256SUMS and
   tests/abi/recorded_run_m007.txt) are excluded: they contain the
   hashes of the artifacts, so including them would be circular.
   Build products, .git and hidden_files/ are excluded.

    git ls-files | grep -v -e ^release/SHA256SUMS$ \
      -e ^tests/abi/recorded_run_m007.txt$ > /tmp/m007-filelist.txt
    printf '%s\n' VERSION \
      docs/PHASE_13_VERSIONING_AND_RELEASE_M007.md \
      docs/REPRODUCIBLE_BUILD.md >> /tmp/m007-filelist.txt
    LC_ALL=C sort -u -o /tmp/m007-filelist.txt /tmp/m007-filelist.txt
    LC_ALL=C TZ=UTC tar --sort=name --owner=0 --group=0 \
      --numeric-owner --mtime=@1794084216 -cf - \
      -T /tmp/m007-filelist.txt | gzip -n > STAGING/mimiralloc-0.1.0.tar.gz

5. Checksums, inside the staging directory:

    sha256sum libmimiralloc.so.0.1.0 libmimiralloc-0.1.0.a \
      mimir_alloc.h mimiralloc-0.1.0.tar.gz > SHA256SUMS

   The committed release/SHA256SUMS is this exact file
   (byte-identical, proven by hash comparison in the recorded run).
   Consumer verification: copy the four artifacts and SHA256SUMS
   into one directory and run: sha256sum -c SHA256SUMS

6. Verify from clean extraction: copy the artifacts plus SHA256SUMS
   to a fresh directory, run sha256sum -c SHA256SUMS (every line
   must report OK), then build the repository consumer proof
   (tests/abi/test_e004.c) against the extracted versioned library:
   link by path against libmimiralloc.so.0.1.0, create the SONAME
   symlink name libmimiralloc.so.0 in the extraction directory, and
   run the consumer with LD_LIBRARY_PATH set to that directory.
   Exit 0 is required. The artifacts as shipped, not the build
   tree, are what get verified.

7. Reproduce: repeat steps 1, 3, 4 and 5 from a second full clean
   build into a second staging directory, then compare sha256
   artifact by artifact against the first build. Any differing
   artifact is named with both hashes in the record, the cause is
   investigated, and either these instructions are corrected or
   the claim for that artifact is withdrawn - never averaged over
   and never retried silently until two builds happen to match.

## Environment notes

- Builds run on the Pi with loads kept light (Samtal village lives
  on this machine); no network access is needed at any step.
- The build directory is the banked default (build/). A second
  build means make clean followed by the same targets, so both
  builds share the same relative build path.
- Staging directories under /tmp are ephemeral custody, not
  durable storage. Durability is re-cutting from the committed
  tree by this document and comparing against release/SHA256SUMS.
