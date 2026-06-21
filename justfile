# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Vox — developer task runner (https://github.com/casey/just).
# Run `just` (or `just --list`) to see every task. Build/test go through
# CMakePresets.json so Visual Studio, VS Code, CLion, CI and this file all agree.
#
# Recipes run in PowerShell on Windows and sh elsewhere — no Git Bash needed. On
# Windows run `just` from a "Developer PowerShell for VS" so cl/cmake/ninja are on
# PATH. clang-tidy has no usable native Windows path — see `tidy` and README.

set windows-shell := ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command"]

# Repo root: forward-slashed (portable) and native (OpenCppCoverage needs Windows '\').
root        := replace(justfile_directory(), '\', '/')
root_native := justfile_directory()

# Per-platform default presets (override by passing a preset to a recipe; CI does).
configure_preset := if os() == "windows" { "x64-msvc" } else if os() == "macos" { "macos-clang" } else { "linux-clang" }
build_preset     := if os() == "windows" { "x64-msvc-debug" } else if os() == "macos" { "macos-clang-debug" } else { "linux-clang-debug" }
test_preset      := build_preset

# Tools (override via env). OpenCppCoverage on Windows; the clang-tidy driver.
# Linux/WSL uses run-clang-tidy-18 — exactly what CI runs: LLVM 18 (a newer toolchain
# diverges) and the per-file parallel wrapper (plain clang-tidy over many files at once
# accumulates per-TU header diagnostics into a noisy flood).
occ        := env_var_or_default("VOX_OCC", "OpenCppCoverage")
clang_tidy := env_var_or_default("CLANG_TIDY", if os() == "linux" { "run-clang-tidy-18" } else { "clang-tidy" })

# (default) Show every available task.
[private]
default:
    @just --list --unsorted

# 🚀 Run all CI gates in parallel: format-check ∥ (build+coverage) ∥ tidy.
[windows]
check:
    & "{{justfile_directory()}}\tools\win-check.ps1" -RepoNative "{{root_native}}"

# 🚀 Run all CI gates in parallel: format-check ∥ (build+test) ∥ tidy.
# (coverage is Windows-only, so Linux/macOS run build+test as the middle gate.)
[unix]
check:
    just format-check & p1=$!; just test & p2=$!; just tidy & p3=$!; rc=0; wait $p1 || rc=1; wait $p2 || rc=1; wait $p3 || rc=1; if [ $rc -eq 0 ]; then echo 'check: all gates passed'; else echo 'check: FAILED'; exit 1; fi

# 🎨 Reformat every C++ source in place (clang-format).
format:
    clang-format -i $(git ls-files '*.cpp' '*.hpp')

# 🔍 Verify formatting; fail if anything is unformatted.
format-check:
    clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')

# ⚙️  Configure a build (restores vcpkg deps, writes compile_commands.json).
configure preset=configure_preset:
    cmake --preset {{preset}}

# 🔨 Build [preset] (self-configures the matching configure preset first).
build preset=build_preset:
    cmake --preset {{replace(replace(preset, '-release', ''), '-debug', '')}}
    cmake --build --preset {{preset}}

# ✅ Run the tests [preset] (integration tests self-skip without hardware).
# Emits JUnit (test-results.xml) for CI's run-summary / PR-comment reporter (#65).
test preset=test_preset: (build preset)
    ctest --preset {{preset}} --output-junit "{{root}}/test-results.xml"

# Pins clang-18 + clang-tidy-18 to match CI; VCPKG_ROOT falls back to
# ~/.local/share/vcpkg for non-interactive WSL invocations (expanded by Linux sh, so
# no Windows-boundary quoting). A base ref limits tidy to merge-base-changed sources.
# Uses a dedicated build/linux-clang-tidy dir (clang-18) so it never collides with a
# `just build`/`just test` of build/linux-clang (e.g. run in parallel by `check`).
# 🧹 clang-tidy — the CI gate (Linux/Clang). Pass a base ref (e.g. origin/dev) for changed-only.
[linux]
tidy base='':
    VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}" cmake --preset linux-clang -B build/linux-clang-tidy -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DVCPKG_MANIFEST_FEATURES=benchmarks -DVOX_BUILD_BENCHMARKS=ON
    if [ -n '{{base}}' ]; then mb=$(git merge-base HEAD '{{base}}') || exit 1; files=$(git diff --name-only "$mb" -- 'src/*.cpp' 'tests/*.cpp' 'benchmarks/*.cpp'); else files=$(git ls-files 'src/*.cpp' 'tests/*.cpp' 'benchmarks/*.cpp'); fi; if [ -n "$files" ]; then {{clang_tidy}} -p build/linux-clang-tidy -warnings-as-errors='*' $files; else echo 'tidy: no C++ sources to check'; fi

# 🧹 clang-tidy on macOS (warns: differs from CI). Pass a base ref for changed-only.
[macos]
tidy base='':
    @echo 'warning: macOS clang-tidy differs from the CI gate (Linux/Clang).'
    VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.local/share/vcpkg}" cmake --preset macos-clang -B build/macos-clang-tidy -DVCPKG_MANIFEST_FEATURES=benchmarks -DVOX_BUILD_BENCHMARKS=ON
    if [ -n '{{base}}' ]; then mb=$(git merge-base HEAD '{{base}}') || exit 1; files=$(git diff --name-only "$mb" -- 'src/*.cpp' 'tests/*.cpp' 'benchmarks/*.cpp'); else files=$(git ls-files 'src/*.cpp' 'tests/*.cpp' 'benchmarks/*.cpp'); fi; if [ -n "$files" ]; then {{clang_tidy}} -p build/macos-clang-tidy -warnings-as-errors='*' $files; else echo 'tidy: no C++ sources to check'; fi

# Picks an Ubuntu-24.04 WSL distro (matches CI), else native clang-cl. See tools/win-tidy.ps1.
# 🧹 clang-tidy from Windows (WSL Ubuntu-24.04, else native). Pass a base ref for changed-only.
[windows]
tidy base='':
    & "{{justfile_directory()}}\tools\win-tidy.ps1" -RepoNative "{{root_native}}" -Base "{{base}}"

# 🔎 Run clang-tidy only on C++ sources changed vs a base branch (default origin/dev).
tidy-changed base='origin/dev': (tidy base)

# 📊 Run the suite under OpenCppCoverage and write coverage.xml (Cobertura), as the SonarQube job does.
[windows]
coverage preset=test_preset: (build preset)
    & "{{occ}}" --sources "{{root_native}}\src" --modules "{{root_native}}\build" --cover_children --excluded_line_regex ".*LCOV_EXCL_LINE.*" --export_type "cobertura:{{root_native}}\coverage.xml" -- ctest --preset {{preset}}

# 📊 Coverage is measured on Windows (OpenCppCoverage) and in CI; not yet wired for this OS.
[unix]
coverage:
    @echo 'Coverage runs on Windows (OpenCppCoverage) and in CI; Linux/macOS coverage is not yet wired.'

# 📈 Build (Release) + run the TTFA microbenchmarks (#41); writes benchmark-results.json.
# Absolute latency budgets fail the run; VOX_BENCH_SAPI=1 adds the real-engine measurement.
[windows]
bench:
    cmake --preset x64-msvc-bench
    cmake --build --preset x64-msvc-bench-release
    & "{{root_native}}\build\x64-msvc-bench\benchmarks\Release\vox_benchmarks.exe" --benchmark_out="{{root_native}}\benchmark-results.json" --benchmark_out_format=json

# 📈 Build (Release) + run the TTFA microbenchmarks (#41); writes benchmark-results.json.
# (The real-engine SAPI measurement is Windows-only; this runs the pipeline gate.)
[linux]
bench:
    cmake --preset linux-clang-bench
    cmake --build --preset linux-clang-bench-release
    "{{root}}/build/linux-clang-bench/benchmarks/Release/vox_benchmarks" --benchmark_out="{{root}}/benchmark-results.json" --benchmark_out_format=json

# 📈 Benchmarks run on Windows and Linux; there is no macOS bench preset yet.
[macos]
bench:
    @echo 'Benchmarks run on Windows (x64-msvc-bench) and Linux (linux-clang-bench); no macOS preset yet.'

# 📊 Preview the cost-ledger snapshot to stdout. CI refreshes the live copy on the cost-data branch. --month YYYY-MM targets a month.
[windows]
cost *args:
    python tools/cost_collector.py --print {{args}}

# 📊 Preview the cost-ledger snapshot to stdout. CI refreshes the live copy on the cost-data branch. --month YYYY-MM targets a month.
[unix]
cost *args:
    python3 tools/cost_collector.py --print {{args}}

# 🪝 Install the opt-in git hooks (pre-push: reports Claude-token month-to-date cost). Verify with `sh tools/hooks/pre-push --dry-run`.
[windows]
install-hooks:
    Copy-Item "{{root_native}}\tools\hooks\pre-push" "{{root_native}}\.git\hooks\pre-push" -Force; Write-Host "Installed .git/hooks/pre-push (Claude-cost reporter). Verify (no push): sh tools/hooks/pre-push --dry-run"

# 🪝 Install the opt-in git hooks (pre-push: reports Claude-token month-to-date cost). Verify with `sh tools/hooks/pre-push --dry-run`.
[unix]
install-hooks:
    cp "{{root}}/tools/hooks/pre-push" "{{root}}/.git/hooks/pre-push" && chmod +x "{{root}}/.git/hooks/pre-push" && echo "Installed .git/hooks/pre-push (Claude-cost reporter). Verify (no push): sh tools/hooks/pre-push --dry-run"

# 🧽 Delete the build/ directory.
clean:
    cmake -E rm -rf "{{root}}/build"
