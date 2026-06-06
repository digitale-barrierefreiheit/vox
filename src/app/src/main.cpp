// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The Vox MVP reader executable: the thin entry point.
///
/// All behaviour lives in the testable `vox::app::App` (the run-loop) and
/// `makeDefaultDependencies()` (the composition root). `main` only wires them,
/// so it carries no logic of its own.

#if defined(_WIN32)

#  include <vox/app/app.hpp>
#  include <vox/app/default_app.hpp>

int main() {
  return vox::app::App{vox::app::makeDefaultDependencies()}.run();
}

#endif // defined(_WIN32)
