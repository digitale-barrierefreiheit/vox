// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The Vox MVP reader executable: the thin entry point.
///
/// Behaviour lives in the testable `vox::app::App` (the run-loop) and
/// `makeDefaultDependencies()` (the composition root). `main` only builds the
/// App and runs it, mapping a fatal startup failure (e.g. no SAPI voice) to a
/// non-zero exit — the one piece App::run() cannot cover, since it catches only
/// what happens after construction.

#if defined(_WIN32)

#  include <exception>
#  include <iostream>

#  include <vox/app/app.hpp>
#  include <vox/app/default_app.hpp>

int main() {
  try {
    return vox::app::App{vox::app::makeDefaultDependencies()}.run();
  } catch (const std::exception& error) {
    std::cerr << "vox: fatal error: " << error.what() << '\n';
    return 1;
  } catch (...) {
    // Top-level boundary for construction failures (before App::run()'s own
    // firewall); a non-std exception must map to exit 1, not terminate.
    std::cerr << "vox: fatal error: unknown exception\n";
    return 1;
  }
}

#endif // defined(_WIN32)
