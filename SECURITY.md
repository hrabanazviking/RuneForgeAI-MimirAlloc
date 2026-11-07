# Security Policy

MimirAlloc is a pre-release research allocator. This document states
how to report a security problem, what is in scope, and what response
a reporter can honestly expect. It invents nothing: no version number,
no response-time promise, no bounty, and no contact address that the
repository itself has not published.

## Supported versions

Security reports are accepted against the current state of the
repository: the current `main` branch and the current release
artifacts and their published checksums, as the release process
defines them. There is no long-term-support branch and no promise
that older states will be patched. Versioning and artifact naming
are being settled in the Phase 13 release work; until that work is
banked, refer to a report by commit hash, not by a version number.

## Reporting a vulnerability

Report privately first. Do not open a public issue for a suspected
vulnerability.

- Use GitHub's private vulnerability reporting for this repository
  (GitHub Security Advisories: the repository's "Report a
  vulnerability" route, if the repository owner has enabled it).
  The repository is `hrabanazviking/RuneForgeAI-MimirAlloc` on
  GitHub, as shown by the repository's own git remote.
- If that route is not enabled or not available to you, open a
  minimal public issue that says only that you have a security
  report and asks for a private channel. Do not put reproducer
  details, exploit notes, or heap layouts in that public issue.

This repository publishes no security contact email address. A
sweep of the repository tree (README, LICENSE, docs, .github) at
the base of this document found no email address published for
security reports, so none is printed here. Do not use addresses
found in other projects or other contexts for this project.

Please include, as far as you can:

- the commit hash (or release artifact and its checksum) tested;
- the environment: architecture, OS, libc, page size, device class;
- a minimal reproducer or a precise description of the failing call
  sequence;
- the impact you observed or believe possible, stated separately
  from the observation.

## Scope

In scope: the allocator library code in this repository, its
public header and ABI surface, its build (Makefile and version
script), and artifacts built from this repository.

Out of scope: third-party toolchains (compiler, libc, the Mojo /
pixi test environment and its proprietary components), other
software that happens to share a development machine (household
services are not part of this project), reports that depend on an
attacker already having arbitrary code execution or the ability to
pass invalid pointers (freeing or reallocating a foreign, interior,
or already-freed pointer, and use after free, are undefined
behaviour with no defined error by documented contract), and
social-engineering reports.

## Response expectations

This is a one-maintainer research project. Reports are handled on a
best-effort basis: acknowledgement and assessment as time allows,
with no service-level commitment of any kind. There is no bug
bounty and this document must not be read as implying one. Fixes,
when made, land in the repository with the project's usual recorded
evidence; there is no separate security-release channel.

## Coordinated disclosure

Please give the maintainer a reasonable opportunity to assess and
fix a confirmed problem before publishing details. The project will
credit reporters in the fix record unless a reporter asks not to be
named. Disagreements about severity are settled by the evidence in
the report and the project's banked records, in plain language.

## Security model - what this allocator does and does not do

MimirAlloc is an explicitly adopted allocator library: a program
must call its `mimir_*` functions (or link them deliberately) to use
it. It is not a sandbox, not a global interposition layer by
default, and not a defence against a hostile host process.

Its hardening flavour and diagnostics are detection aids, not
guarantees. Debugging features can detect some overruns, stale
metadata, repeated frees, or corrupted canaries; they cannot safely
recover every corrupted heap, and allocator checks are not ECC
(Roadmap Section 9.2). A diagnostic report shows that something was
detected; silence does not prove a heap is sound. Known limits of
the detection and reporting surface - including failure-reporting
gaps recorded as INC-M002-01 and INC-M002-02 - are listed publicly
in `docs/LIMITATIONS_AND_KNOWN_DEFECTS.md`.

