// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The input-hook lifecycle seam: start/stop the global key listener.
///
/// `IInputHook` abstracts the OS keyboard hook (the Windows `KeyboardHook`
/// today) so the application orchestrator (`vox::app::App`) drives it without a
/// dependency on Win32 — letting the run-loop be unit-tested with a fake hook.
/// Portable on purpose (no `#if _WIN32`): the interface is platform-neutral;
/// only its concrete implementation is Windows-specific.
#ifndef VOX_INPUT_IINPUT_HOOK_HPP
#define VOX_INPUT_IINPUT_HOOK_HPP

namespace vox::input {

/// Lifecycle of the global keyboard hook the app installs and removes.
class IInputHook {
public:
  IInputHook() = default;
  virtual ~IInputHook() = default;

  IInputHook(const IInputHook&) = delete;
  IInputHook& operator=(const IInputHook&) = delete;
  IInputHook(IInputHook&&) = delete;
  IInputHook& operator=(IInputHook&&) = delete;

  /// @brief Begins delivering keystrokes to the bound command handler.
  virtual void start() = 0;

  /// @brief Stops delivering keystrokes. Idempotent.
  virtual void stop() = 0;
};

} // namespace vox::input

#endif // VOX_INPUT_IINPUT_HOOK_HPP
