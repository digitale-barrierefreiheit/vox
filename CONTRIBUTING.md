# Contributing to Vox

Thank you for helping build a faster, more natural screen reader. This project
is a collaborative learning journey — questions and design discussion are
welcome alongside code.

Please read [`AGENTS.md`](AGENTS.md) and
[`doc/architecture/architecture.md`](doc/architecture/architecture.md) before
starting; they define the goals, constraints, and decisions all changes must
respect.

## Prerequisites

- **Windows (shipping build):** Visual Studio 2022/2026 (Enterprise or
  Professional) with the *Desktop development with C++* and *C++ CMake tools*
  workloads, or the Build Tools. This provides MSVC, clang-cl, CMake, and Ninja.
- **vcpkg:** set the `VCPKG_ROOT` environment variable to your vcpkg checkout.
  Dependencies are restored automatically from [`vcpkg.json`](vcpkg.json).
- **Cross-platform / sanitizer work:** use the
  [`.devcontainer`](.devcontainer/) (Clang, sanitizers, lint tools).

## Build, test, lint

Everything is driven by [`CMakePresets.json`](CMakePresets.json), so Visual
Studio, VSCode, and the command line behave identically.

```sh
cmake --preset x64-msvc            # configure (restores deps via vcpkg)
cmake --build --preset x64-msvc-debug
ctest --preset x64-msvc-debug      # run the test suite

python tools/run-clang-format.py --check   # formatting gate
```

> The formatting gate is pinned to **clang-format 18** (matching CI and the
> devcontainer). Use that version locally to avoid spurious re-formatting from
> version skew.

- **Visual Studio:** *File → Open → Folder…* on the repo root; pick a preset
  from the configuration dropdown.
- **VSCode:** open the folder, install the recommended extensions, select a
  preset in the CMake Tools status bar.

## Definition of done (review checklist)

Per architecture §8.6.7, every change is reviewed against:

- [ ] **Tests added/updated** — new behaviour is covered; pure cores are TDD'd.
- [ ] **No hot-path allocation** in the audio callback or in-context hook.
- [ ] **Thread-safety / memory ordering** considered for concurrent code.
- [ ] **Intention-revealing names**; abbreviations only for domain acronyms.
- [ ] **Graceful degradation** — a fault must never crash the host app.
- [ ] **Docs updated** — architecture/ADRs/requirements kept in sync; doc
      comments on public *and* private members.
- [ ] **SPDX header** present on every new source file (see below).
- [ ] **No copyleft linked** into first-party binaries (GPL/LGPL — ADR-16).
- [ ] **CI green** — format, clang-tidy, MSVC + 32/64-bit builds, sanitizers.

## License & CLA

Vox is **Apache-2.0** (see [`LICENSE`](LICENSE)). By contributing you agree to
the [Contributor License Agreement](CLA.md); a CI bot will ask first-time
contributors to sign on their pull request. Every source file must start with:

```cpp
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
```

Use `#` comments for CMake/Python/YAML. Do **not** introduce GPL/LGPL code into
any first-party binary.

## Branching & workflow (gitflow)

We plan and track everything as **GitHub issues** — there is no `doc/todo`.

- Branch off `dev` as `<type>/<issue-number>-<slug>` (e.g. `fix/8-issue-lifecycle`).
- Open a pull request **into `dev`**. Releases go out via a `dev` → `main` PR.
- Status labels are automated: creating the branch marks the issue
  **In Progress**; merging into `dev` makes it **Resolved** (and deletes the
  branch); merging into `main` makes it **Released** and closes it.
- Tick an issue's acceptance-criteria checkboxes as they are actually met.

## Commit style

Write clear, imperative commit subjects (e.g. "Add SPSC ring index math").
Reference issues where relevant. Keep changes small and focused — the
architecture explicitly favours small steps over large ones.

**Merge strategy.** Squash at the feature level only:

- **feature → `dev`:** squash-merge (one clean commit per issue), with a
  Conventional-Commits message: lowercase after the `type:`, imperative, and no
  `Co-Authored-By` trailer.
- **`dev` → `main`:** **merge commit** — never squash or rewrite `dev` (it diverges
  the branches and breaks feature branches based on `dev`); **tag releases on `main`**.
