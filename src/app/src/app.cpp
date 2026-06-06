// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::app::App — the reader + hook run-loop.
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

#include <vox/app/app.hpp>

namespace vox::app {

namespace {

/// Enforces App's documented non-null preconditions before any of the
/// dependencies are dereferenced in the member initializer list.
AppDependencies validated(AppDependencies deps) {
  if (!deps.provider || !deps.tts || !deps.audio || !deps.makeHook) {
    throw std::invalid_argument("App: provider, tts, audio, and makeHook must all be non-null");
  }
  return deps;
}

} // namespace

App::App(AppDependencies deps)
    : deps_(validated(std::move(deps))),
      reader_(*deps_.provider, *deps_.tts, *deps_.audio, std::move(deps_.output)),
      hook_(deps_.makeHook(reader_)) {
  if (!hook_) {
    throw std::invalid_argument("App: the hook factory returned null");
  }
}

int App::run() noexcept {
  try {
    reader_.start();
    hook_->start();
    reader_.waitForExit(); // until Ctrl+Shift+Q
    teardown();            // hook first (off its own thread), then the reader
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "vox: fatal error: " << error.what() << '\n';
    teardown(); // stop whatever start() brought up before failing
    return 1;
  } catch (...) {
    // This is the process boundary; a non-std exception must not terminate it.
    std::cerr << "vox: fatal error: unknown exception\n";
    teardown();
    return 1;
  }
}

void App::teardown() noexcept {
  // In dependency order. stop() is idempotent on both, so the normal and the
  // failure path can both call this; neither stop() throws.
  if (hook_) {
    hook_->stop();
  }
  reader_.stop();
}

} // namespace vox::app
