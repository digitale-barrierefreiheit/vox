<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors -->

# Releasing Vox

Vox is released from `main`; day-to-day work happens on `dev`. Releases are
automated by [`.github/workflows/release.yml`](.github/workflows/release.yml) and
versioned with [Semantic Versioning](https://semver.org). The version lives in
`CMakeLists.txt` (`project(VERSION …)`) — the single source of truth that
`vcpkg.json`, the executable's `VERSIONINFO`, and the package name all follow.

Between releases the repo version is **0.0.0**, a development sentinel the release
workflow refuses to publish; each release bumps it to the real number. Git tags are
`v<version>` (e.g. `v0.0.1`); the version *string* everywhere else is unprefixed.

## Cut a release

1. **Write the notes.** Accumulate user-facing changes under the `## [Unreleased]`
   heading in [`CHANGELOG.md`](CHANGELOG.md) (Keep a Changelog: `Added` / `Changed`
   / `Fixed` / `Removed`). The release publishes exactly this section.
2. **Run the *Release* workflow** (Actions → *Release* → *Run workflow*, from
   `dev`) and choose the bump — **patch / minor / major**. Stage 1 then:
   - bumps `CMakeLists.txt` + `vcpkg.json`,
   - promotes `[Unreleased]` to `## [<version>] - <date>` and leaves a fresh empty
     `[Unreleased]`,
   - commits `release: v<version>` to `dev` (via the deploy key, below),
   - opens a **`dev → main` pull request** titled *Release v&lt;version&gt;* whose
     body is the changelog section.
3. **Merge the release PR** once the normal gate is green (checks, reviews, no
   unresolved threads). Use a **merge commit** — never squash `dev` into `main`.
   The merge triggers Stage 2.
4. **Stage 2 publishes automatically** on the push to `main`: it builds the Release
   x64 ZIP, smoke-checks it, and creates the GitHub release (tag `v<version>`,
   marked **pre-release** for `0.x`) with the ZIP + `.sha256` attached and the
   changelog as the notes. [`back-merge.yml`](.github/workflows/back-merge.yml) then
   syncs `main → dev`.

## What ships

`vox-<version>-windows-x64.zip` contains `vox.exe`, the `lexicon/` data it loads at
runtime, and `LICENSE` / `NOTICE` / `THIRD-PARTY-NOTICES.md` / `CHANGELOG.md`, plus
a `.sha256` checksum. **x64 only** for now (a 32-bit helper DLL ships later, when
injection exists).

## One-time setup / requirements

- **`BACKMERGE_SSH_KEY`** repo secret — the private half of a write **deploy key**
  registered as a **bypass actor** (`DeployKey`) on `dev`'s ruleset, so the workflow
  can push the version bump to protected `dev` (the same key `back-merge.yml` uses).
- Repo setting **Allow GitHub Actions to create and approve pull requests**
  (Settings → Actions → General), so Stage 1 can open the release PR.
- `main` branch protection stays on; the release PR goes through it like any other.

## Not yet automated

- **Code signing (#104).** Releases are currently **unsigned** — SmartScreen / AV
  may warn. Signing via Azure Trusted Signing is wired in once verification
  completes.
- A 32-bit artifact and the neural-TTS third-party notices arrive with the features
  that introduce them.
