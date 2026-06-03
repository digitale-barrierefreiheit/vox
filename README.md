# Vox — Open-Source Accessibility Tool

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

## License

Vox is licensed under the **Apache License 2.0** — see [`LICENSE`](LICENSE) and
[`NOTICE`](NOTICE). Contributions are accepted under the
[Contributor License Agreement](CLA.md) (CI-enforced), which keeps copyright
consolidated so future versions can be relicensed if needed.

GPL/LGPL code is never linked into a first-party binary. The GPLv3 espeak-ng G2P,
where used, runs as a separate optional process (ADR-15/ADR-16).
