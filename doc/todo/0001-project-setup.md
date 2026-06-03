# 0001 — Project setup & build scaffolding

**Status:** in progress
**Goal:** a repository that builds, tests, and lints cleanly in Visual Studio,
VSCode, and CI, on the toolchains the architecture mandates (ADR-08/12/13/14).

## Done

- [x] Git hygiene: `.gitignore`, `.gitattributes` (LF normalize), `.editorconfig`
      (2-space indent).
- [x] Code style/analysis: `.clang-format`, `.clang-tidy` (Core Guidelines,
      project naming).
- [x] CMake build: root `CMakeLists.txt`, `cmake/` helper modules (standard,
      warnings, sanitizers, static analysis), `vcpkg.json` manifest,
      `CMakePresets.json` (MSVC x64/x86, clang-cl, Linux Clang + sanitizers).
- [x] Seed library `vox::core` + GoogleTest smoke test wired into CTest.
- [x] IDE integration: `.vscode/` (presets-driven), `.devcontainer/` (Clang +
      sanitizers + lint).
- [x] CI: `.github/workflows/ci.yml` — format, MSVC 32/64-bit build+test,
      clang-tidy, ASan+UBSan and TSan matrices.
- [x] Doxygen developer-reference target (`-DVOX_BUILD_DOCS=ON`).
- [x] Community files: CONTRIBUTING, CODE_OF_CONDUCT, SECURITY, PR/issue
      templates, CODEOWNERS.

## Open / next small steps

- [x] **Choose a LICENSE.** Apache-2.0 + CLA (ADR-15); `LICENSE`, `NOTICE`,
      `CLA.md`, SPDX file headers, and the CI CLA gate are in place.
- [ ] **Finish CLA enforcement setup:** no secret needed — `.github/workflows/cla.yml`
      uses the built-in `GITHUB_TOKEN` (signatures stored in-repo). Just ensure
      repo **Settings → Actions → Workflow permissions = "Read and write"**, and
      that no branch-protection rule blocks the `github-actions[bot]` from pushing
      the `cla-signatures` branch. Have counsel review `CLA.md`. Copyright holder /
      CLA beneficiary is **Digitale Barrierefreiheit e.V.** (Nuremberg, Germany).
- [ ] **Add a CI header-presence check** and backfill SPDX headers across the
      CMake modules and scripts (C++ sources already carry them).
- [ ] **espeak-ng GPL isolation (ADR-16):** implement the G2P inside the separate
      TTS worker process behind a text→phoneme interface; add a microbenchmark
      proving per-utterance G2P + IPC stays well under the TTFA budget; keep a
      permissively-licensed phonemizer as the documented fallback.
- [ ] **Pin dependencies for reproducibility** (§8.6.7): add a vcpkg
      `builtin-baseline` (a real registry commit SHA) and version constraints to
      `vcpkg.json`.
- [ ] Verify the build end to end locally (configure/build/test) on this machine
      via a *Developer PowerShell for VS* or by opening the folder in VS.
- [ ] Add the first portable core with TDD (candidate: the SPSC ring index math,
      architecture §8.3) — this exercises the sanitizer CI for real.
- [ ] Add the cross-bitness wire-layout test (`static_assert` on
      `sizeof`/`offsetof`) once shared wire structs exist (R12).
- [ ] Add a benchmark target behind the `benchmarks` vcpkg feature and a
      CI performance-regression gate (§8.6.4) — deferred until hot paths exist.
- [ ] Capture initial requirements in `doc/requirements` from the quality goals.

## Notes

- Build tools are not on the global PATH; they live inside Visual Studio 2026.
  Use a *Developer PowerShell for VS* for CLI builds, or let VS/VSCode provide
  the environment. CI installs its own toolchain.
- C++ standard is `/std:c++latest` (C++26), falling back to C++23 via
  `-DVOX_CXX_STANDARD=23`.
