<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors -->

# Releasing Vox

Vox is released from `main`; day-to-day work happens on `dev`. Releases are
automated by [`.github/workflows/release.yml`](.github/workflows/release.yml) and
versioned with [Semantic Versioning](https://semver.org). The version lives in
`CMakeLists.txt` (`project(VERSION …)`) — the single source of truth that
`vcpkg.json`, the executable's `VERSIONINFO`, and the package name all follow.

The repo sits at **0.0.0** until the first release — a pre-release sentinel the
publish stage refuses to release. Each release **bumps** the version (in
`CMakeLists.txt` + `vcpkg.json`); afterwards `dev` and `main` stay at that released
number (back-merge keeps them in sync) until the next release bumps it again. There
is **no reset** back to 0.0.0. Git tags are `v<version>` (e.g. `v0.0.1`); the version
*string* everywhere else is unprefixed.

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

`vox.exe` is **Authenticode-signed** via Azure Artifact Signing (formerly Trusted
Signing) and **RFC 3161 timestamped**, so Windows SmartScreen and antivirus no
longer warn on download or first run. The publish job signs the binary before it is
zipped and verifies it (`signtool verify /pa`) before creating the release; signing
is **mandatory** — a misconfigured signer fails the release rather than shipping an
unsigned build.

## One-time setup / requirements

- **`BACKMERGE_SSH_KEY`** repo secret — the private half of a write **deploy key**
  registered as a **bypass actor** (`DeployKey`) on `dev`'s ruleset, so the workflow
  can push the version bump to protected `dev` (the same key `back-merge.yml` uses).
- Repo setting **Allow GitHub Actions to create and approve pull requests**
  (Settings → Actions → General), so Stage 1 can open the release PR.
- `main` branch protection stays on; the release PR goes through it like any other.

### Code signing (Azure Artifact Signing)

The publish job signs `vox.exe` via **Azure Artifact Signing** using **GitHub OIDC**
— a short-lived token federated to a Microsoft Entra app registration, so **no client
secret is stored**. Configure these in **Settings → Secrets and variables → Actions**:

| Kind | Name | Value |
|------|------|-------|
| Secret | `AZURE_CLIENT_ID` | the Entra app registration's **Application (client) ID** |
| Secret | `AZURE_TENANT_ID` | the **Directory (tenant) ID** |
| Secret | `AZURE_SUBSCRIPTION_ID` | the **subscription ID** holding the signing account |
| Variable | `AZURE_SIGNING_ENDPOINT` | region endpoint, e.g. `https://weu.codesigning.azure.net/` |
| Variable | `AZURE_SIGNING_ACCOUNT` | the **signing account** name |
| Variable | `AZURE_SIGNING_PROFILE` | the **certificate profile** name |

(The three IDs aren't cryptographically secret, but live as secrets by convention.)

Azure side, once per setup:

1. **Register the provider:** `az provider register --namespace Microsoft.CodeSigning`.
2. **Account + certificate profile:** create an Artifact Signing account (in the region
   matching `AZURE_SIGNING_ENDPOINT`), complete identity validation, create a
   `PublicTrust` certificate profile.
3. **App registration + federated credential** (Entra → App → Certificates & secrets →
   Federated credentials):
   - **Issuer** `https://token.actions.githubusercontent.com`
   - **Audience** `api://AzureADTokenExchange`
   - **Subject** `repo:digitale-barrierefreiheit/vox:ref:refs/heads/main` — must match the
     publish trigger (push to `main`) **exactly**; a mismatch fails the token exchange.
4. **Role:** grant the app's service principal **Artifact Signing Certificate Profile
   Signer** scoped to the certificate profile (or the account).

Signing is **mandatory**: if any of the six values is absent the publish job fails
fast (it never ships an unsigned release).

## Not yet automated

- A 32-bit artifact and the neural-TTS third-party notices arrive with the features
  that introduce them.
