// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows low-level keyboard hook driving announce-focus + barge-in.
///
/// `KeyboardHook` installs a `WH_KEYBOARD_LL` hook on its own dedicated
/// message-pump thread (architecture §5.1 / §6.1). The callback translates each
/// key to a `KeyEvent`, routes it through `CommandMap`, and — for a bound key —
/// calls the `ICommandHandler`, swallowing reader-control keys while letting
/// navigation keys pass through so the app still moves focus. The callback is
/// allocation-free and returns promptly (§8.6.4 / R15); the handler owns any slow
/// work. Win32 stays behind a pImpl. Windows-only — the routing logic it relies
/// on is in the portable `CommandMap`, which the sanitizer/clang-tidy build sees.
#ifndef VOX_INPUT_KEYBOARD_HOOK_HPP
#define VOX_INPUT_KEYBOARD_HOOK_HPP

// Declared only on Windows (see src/input/CMakeLists.txt) so any accidental
// non-Windows use is a clear "undeclared identifier", not a link error.
#if defined(_WIN32)

#  include <memory>

#  include <vox/input/command_map.hpp>

namespace vox::input {

class ICommandHandler;

/// Low-level keyboard hook that turns keystrokes into reader commands.
///
/// @note `start()`/`stop()` are not thread-safe with respect to each other;
///       drive the lifecycle from a single thread. `stop()` is idempotent and
///       runs in the destructor.
class KeyboardHook {
public:
  /// @brief Builds a hook that delivers commands to @p handler using @p map.
  ///        Installs nothing until `start()`. @p handler must outlive the hook.
  explicit KeyboardHook(ICommandHandler& handler, CommandMap map = {});
  ~KeyboardHook();

  KeyboardHook(const KeyboardHook&) = delete;
  KeyboardHook& operator=(const KeyboardHook&) = delete;
  KeyboardHook(KeyboardHook&&) = delete;
  KeyboardHook& operator=(KeyboardHook&&) = delete;

  /// @brief Installs the hook on a dedicated message-pump thread.
  /// @throws std::runtime_error if a hook is already active or installation fails.
  void start();

  /// @brief Removes the hook and joins its thread. Idempotent.
  void stop();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vox::input

#endif // defined(_WIN32)

#endif // VOX_INPUT_KEYBOARD_HOOK_HPP
