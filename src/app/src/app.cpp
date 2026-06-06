// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::app::App — the reader + hook run-loop.
#include <exception>
#include <iostream>
#include <utility>

#include <vox/app/app.hpp>

namespace vox::app {

App::App(AppDependencies deps)
    : deps_(std::move(deps)),
      reader_(*deps_.provider, *deps_.tts, *deps_.audio, std::move(deps_.output)),
      hook_(deps_.makeHook(reader_)) {}

int App::run() noexcept {
  try {
    reader_.start();
    hook_->start();
    reader_.waitForExit(); // until Ctrl+Shift+Q
    // Tear down in dependency order: the hook first (off its own thread), then
    // the reader. The App destructor also stops both, so an exception below
    // still leaves nothing running.
    hook_->stop();
    reader_.stop();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "vox: fatal error: " << error.what() << '\n';
    return 1;
  }
}

} // namespace vox::app
