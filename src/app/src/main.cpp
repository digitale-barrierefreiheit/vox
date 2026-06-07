// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The Vox MVP reader executable: the thin entry point.
///
/// All behaviour lives in testable code: `vox::app::runApp` (build + run + map a
/// fatal startup failure to exit 1) and `makeDefaultDependencies()` (the
/// composition root). `main` only hands the real dependency factory to runApp.

#if defined(_WIN32)

#  include <vox/app/app.hpp>
#  include <vox/app/default_app.hpp>

int main() {
  return vox::app::runApp(vox::app::makeDefaultDependencies);
}

#endif // defined(_WIN32)
