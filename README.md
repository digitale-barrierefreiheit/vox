# Vox — Open-Source Accessibility Tool

[![CI](https://github.com/digitale-barrierefreiheit/vox/actions/workflows/ci.yml/badge.svg)](https://github.com/digitale-barrierefreiheit/vox/actions/workflows/ci.yml)
[![SonarQube](https://github.com/digitale-barrierefreiheit/vox/actions/workflows/sonar.yml/badge.svg)](https://github.com/digitale-barrierefreiheit/vox/actions/workflows/sonar.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=vox)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=coverage)](https://sonarcloud.io/summary/new_code?id=vox)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=reliability_rating)](https://sonarcloud.io/summary/new_code?id=vox)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=security_rating)](https://sonarcloud.io/summary/new_code?id=vox)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=vox)
[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=bugs)](https://sonarcloud.io/summary/new_code?id=vox)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=vox&metric=code_smells)](https://sonarcloud.io/summary/new_code?id=vox)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](CMakePresets.json)

Vox is a high-efficiency, low-latency **screen reader**, targeting Windows 11+
first (Linux and macOS later). Its priorities are responsiveness, low system
overhead, and natural **German** speech — areas where existing readers fall
short.

> **Status:** design & scaffolding phase. The build, test, and CI infrastructure
> is in place; the screen-reader functionality is not yet implemented.

## Documentation

- **Goals & contributor guidelines:** [`AGENTS.md`](AGENTS.md)
- **Architecture (arc42):** [`doc/architecture/architecture.md`](doc/architecture/architecture.md)
- **Requirements:** [`doc/requirements/`](doc/requirements/)
- **Planning / open topics:** [GitHub issues](https://github.com/digitale-barrierefreiheit/vox/issues)
- **Contributing:** [`CONTRIBUTING.md`](CONTRIBUTING.md)

## Toolchain

- **Language:** C++ (`/std:c++latest` → C++26, falling back to C++23).
- **Build:** CMake (≥ 3.25) + Ninja, driven entirely by
  [`CMakePresets.json`](CMakePresets.json) so Visual Studio, VSCode, and the CLI
  agree.
- **Compilers:** MSVC and clang-cl for the shipping Windows binary; Clang for the
  CI sanitizer builds (ADR-14).
- **Dependencies:** [vcpkg](https://vcpkg.io) manifest ([`vcpkg.json`](vcpkg.json));
  set `VCPKG_ROOT` to your vcpkg checkout.
- **Tests:** GoogleTest + GMock via CTest.

## Build & test

```sh
# Configure (restores dependencies via vcpkg) and build the Debug config.
cmake --preset x64-msvc
cmake --build --preset x64-msvc-debug

# Run the test suite.
ctest --preset x64-msvc-debug
```

On Windows, run these from a **Developer PowerShell for VS** (so `cl`/`ninja`
are on PATH), or simply open the folder in Visual Studio / VSCode and pick a
preset. Available presets: `x64-msvc`, `x86-msvc`, `x64-clang-cl`, `linux-clang`
(+ `-asan` / `-tsan`).

Generate the developer reference (requires Doxygen):

```sh
cmake --preset x64-msvc -DVOX_BUILD_DOCS=ON
cmake --build --preset x64-msvc-debug --target docs
```

## Developer tasks (`just`)

Common workflows are wrapped in a [`justfile`](justfile) so they are one short
command on every platform. [`just`](https://github.com/casey/just) is a small,
cross-platform command runner — install it once:

| OS | Install |
|----|---------|
| **Windows** | `winget install Casey.Just` (or `scoop install just`, `choco install just`) |
| **macOS** | `brew install just` |
| **Linux** | `apt install just` (Debian/Ubuntu) or `cargo install just` — see the [install docs](https://github.com/casey/just#installation) |

Run `just` (or `just --list`) to see every task. `build`, `test`, `coverage` and
`configure` take an **optional preset** argument; omitted, they default per platform
(Windows → `x64-msvc` / `x64-msvc-debug`, Linux → `linux-clang` / `linux-clang-debug`,
macOS → `macos-clang`). CI passes explicit presets, e.g. `just build x64-msvc-release`.

| Task | What it does |
|------|--------------|
| `just check` | Run all CI gates **in parallel**: format-check ∥ (build + coverage) ∥ tidy |
| `just format` | Reformat all C++ sources in place (clang-format) |
| `just format-check` | Verify formatting; fail if anything is unformatted |
| `just configure [preset]` | Configure the build (vcpkg restore + `compile_commands.json`) |
| `just build [preset]` | Build (Debug by default; self-configures the matching preset) |
| `just test [preset]` | Build, then run the tests |
| `just tidy [base]` | Run clang-tidy (see the platform notes below); a base ref limits it to changed sources |
| `just tidy-changed [base]` | clang-tidy only on C++ sources changed vs a base branch (default `origin/dev`) |
| `just coverage [preset]` | Build, run the suite under coverage, write `coverage.xml` (Cobertura) |
| `just clean` | Delete the `build/` directory |

The recipes drive the CMake presets, and **the CI workflows call these same tasks**,
so local and CI stay in lock-step. Recipes run in **PowerShell on Windows** and **sh
elsewhere** — no Git Bash needed. On Windows, run `just` from a **Developer PowerShell
for VS** so `cl`/`cmake`/`ninja` are on PATH.

**clang-tidy (`just tidy`)** is the Linux/Clang gate and is authoritative on CI:
- **Linux:** runs natively.
- **macOS:** runs with the installed clang-tidy, warning that results may differ from CI.
- **Windows:** `just tidy` scans your installed WSL distros for one that is **Ubuntu 24.04
  with the toolchain** (matching CI — a newer clang/libstdc++ can't parse the C++26 standard
  library) and runs the Linux recipe there; otherwise it falls back to **native clang-cl,
  which cannot parse the bleeding-edge MSVC STL and will fail**. So you can keep a newer
  default distro and add an `Ubuntu-24.04` one just for tidy.

> **Strongly recommended for Windows developers:** install WSL (`wsl --install Ubuntu-24.04`) and, in
> the distro, set up the Linux build toolchain `just tidy` needs:
> `sudo apt install just build-essential cmake ninja-build clang-18 clang-tidy-18 clang-tools-18` **plus a
> [vcpkg](https://vcpkg.io) checkout with `VCPKG_ROOT` exported** (the `linux-clang`
> preset restores gtest through it). Without the full toolchain, `just tidy` — and the
> `tidy` part of `just check` — fails (either in WSL configuration or on the native
> clang-cl fallback); push and let CI run tidy, or run the other tasks individually.

One-time setup — run **inside the Ubuntu-24.04 distro** (`wsl -d Ubuntu-24.04`), not a
newer default distro, so the toolchain matches CI:

```sh
sudo apt update
# LLVM 18 to match CI: clang-18 (compiler), clang-tidy-18, clang-tools-18 (run-clang-tidy-18).
sudo apt install -y just build-essential cmake ninja-build clang-18 clang-tidy-18 clang-tools-18 \
                    git curl zip unzip tar pkg-config
mkdir -p $HOME/.local/share
git clone https://github.com/microsoft/vcpkg "$HOME/.local/share/vcpkg"
"$HOME/.local/share/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
echo 'export VCPKG_ROOT="$HOME/.local/share/vcpkg"' >> "$HOME/.profile"
source "$HOME/.profile"
```

You don't need to pin vcpkg to a specific commit: the package versions are fixed by
`builtin-baseline` in [`vcpkg.json`](vcpkg.json), which a full clone resolves from its
history. If you install vcpkg at the recommended `~/.local/share/vcpkg`, `just tidy`
even works without the export (the recipe falls back to that path). After this, run
`just tidy` from Windows and it delegates into WSL. (The first run restores gtest for
Linux and can take a few minutes.)

**coverage (`just coverage`)** uses [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage)
on Windows (`winget install OpenCppCoverage`); set `VOX_OCC=<path>` if it is not on PATH.

## License

Vox is licensed under the **Apache License 2.0** — see [`LICENSE`](LICENSE) and
[`NOTICE`](NOTICE). Contributions are accepted under the
[Contributor License Agreement](CLA.md) (CI-enforced), which keeps copyright
consolidated so future versions can be relicensed if needed.

GPL/LGPL code is never linked into a first-party binary. The GPLv3 espeak-ng G2P,
where used, runs as a separate optional process (ADR-15/ADR-16).
