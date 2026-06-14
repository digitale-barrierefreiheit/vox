// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The application object: owns the injected dependencies and runs the
///        reader + keyboard-hook lifecycle.
///
/// `App` is the composition-independent run-loop. It takes its OS-facing
/// collaborators (provider, TTS engine, audio sink) behind their interfaces plus
/// a factory for the input hook, builds the `Reader`, and orchestrates
/// start -> wait-for-exit -> stop. Because the dependencies are injected, the
/// whole run-loop — including its fatal-error handling — is unit-tested with
/// fakes; the Windows `main()` supplies the real implementations through
/// `makeDefaultDependencies()` (default_app.hpp). This is the "thin entry point,
/// testable app object" split (architecture §8.6.2).
#ifndef VOX_APP_APP_HPP
#define VOX_APP_APP_HPP

#include <exception>
#include <functional>
#include <memory>
#include <utility>

#include <vox/app/reader.hpp>
#include <vox/audio/iaudio_sink.hpp>
#include <vox/input/command_handler.hpp>
#include <vox/input/iinput_hook.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/provider/iprovider.hpp>
#include <vox/tts/itts_engine.hpp>

namespace vox::app {

/// Builds the input hook bound to @p handler. Injected so production uses the
/// real `KeyboardHook` while tests use a fake.
using HookFactory =
    std::function<std::unique_ptr<vox::input::IInputHook>(vox::input::ICommandHandler&)>;

/// The collaborators the App owns and orchestrates. The provider, TTS engine,
/// and audio sink are held behind their interfaces; the hook is built from the
/// Reader via @ref makeHook once the Reader exists.
struct AppDependencies {
  std::unique_ptr<vox::provider::IProvider> provider;
  std::unique_ptr<vox::tts::ITtsEngine> tts;
  std::unique_ptr<vox::audio::IAudioSink> audio;
  vox::output::OutputManager output;
  HookFactory makeHook;
};

/// Wires the Reader + keyboard hook from injected dependencies and runs the
/// reader lifecycle until exit.
class App {
public:
  /// @brief Takes ownership of @p deps and builds the Reader and input hook.
  ///        @p deps.provider / .tts / .audio and .makeHook must be non-null.
  explicit App(AppDependencies deps);

  App(const App&) = delete;
  App& operator=(const App&) = delete;
  App(App&&) = delete;
  App& operator=(App&&) = delete;
  ~App() = default;

  /// @brief Starts the reader and hook, blocks until a Quit command, then tears
  ///        them down in order.
  /// @return 0 on a clean exit; 1 if a fatal exception escaped startup (logged
  ///         to stderr). Never throws — it is the process's top-level boundary.
  [[nodiscard]] int run() noexcept;

private:
  /// Stops the hook then the reader (idempotent; never throws). Used by both the
  /// normal exit and the failure path of run().
  void teardown() noexcept;

  AppDependencies deps_;
  Reader reader_;
  std::unique_ptr<vox::input::IInputHook> hook_;
};

namespace detail {
/// Logs a top-level fatal error to stderr. Kept in the .cpp so this header needs
/// no <iostream>; used by runApp's construction firewall.
void reportFatalError(const char* what) noexcept;
} // namespace detail

/// @brief Builds the App from @p makeDependencies and runs it, mapping any
///        *construction* failure (the factory or the App constructor throwing) to
///        exit code 1 logged to stderr. App::run() already firewalls everything
///        after construction; this covers the one window it cannot, so the real
///        `main()` is a trivial wrapper over this testable function.
/// @tparam MakeDependencies a callable returning an AppDependencies.
/// @return App::run()'s result, or 1 if construction threw. Never throws.
template<typename MakeDependencies>
[[nodiscard]] int runApp(MakeDependencies&& makeDependencies) noexcept {
  try {
    return App{std::forward<MakeDependencies>(makeDependencies)()}.run();
  } catch (const std::exception& error) {
    detail::reportFatalError(error.what());
    return 1;
    // The process boundary: a non-std construction failure maps to exit 1. Tests
    // cannot throw a non-std type (clang-tidy hicpp-exception-baseclass), so this
    // arm is unreachable from the factory / App constructor seams.
  } catch (...) {                                  // LCOV_EXCL_LINE
    detail::reportFatalError("unknown exception"); // LCOV_EXCL_LINE
    return 1;                                      // LCOV_EXCL_LINE
  } // LCOV_EXCL_LINE
}

} // namespace vox::app

#endif // VOX_APP_APP_HPP
