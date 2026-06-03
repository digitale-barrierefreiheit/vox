# Security Policy

Vox is accessibility-critical software: users operate their machine through it,
and it injects an in-process helper into other applications. Security and
stability are first-class concerns (architecture §8.3.6, ADR-03).

## Reporting a vulnerability

Please report security issues **privately**, not via public issues or pull
requests:

- Use GitHub's **"Report a vulnerability"** (Security → Advisories) on this
  repository, or
- email the maintainer at the address in the repository profile.

Include reproduction steps, affected component (e.g. helper injection, SHM
channel, TTS pipeline), and impact. We aim to acknowledge within a few days and
will coordinate a fix and disclosure timeline with you.

## Scope of particular interest

- The named shared-memory channel and its ACL/nonce setup (§8.3.6).
- Helper injection and lifecycle (`DllMain` discipline, R10).
- Parsing of untrusted accessibility data and the record codec (must be robust
  against malformed input even though we produce it, §8.6.3).

## Supported versions

The project is pre-release (0.x); only the `main` branch is supported until a
tagged release line exists.
