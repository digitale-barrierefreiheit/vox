// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The action seam: receives resolved commands from the keyboard hook.
///
/// Keeping this interface here lets `vox::input` stay free of any audio/provider
/// dependency — the hook only knows how to produce `Command`s and hand them to
/// an `ICommandHandler`. The app (#39) wires a concrete handler that performs
/// barge-in (`IAudioSink::flush`) and the focus announcement. A
/// `FakeCommandHandler` backs the tests.
#ifndef VOX_INPUT_COMMAND_HANDLER_HPP
#define VOX_INPUT_COMMAND_HANDLER_HPP

#include <vox/input/command.hpp>

namespace vox::input {

/// Sink for resolved reader commands.
///
/// @note `onCommand` is invoked from the keyboard hook's low-level callback, a
///       latency-critical, timeout-bound path (architecture §8.6.4 / R15).
///       Implementations MUST return promptly and not block: do fast effects
///       (barge-in) inline and dispatch slow work (focus read + synthesis) to
///       another thread.
class ICommandHandler {
public:
  ICommandHandler() = default;
  ICommandHandler(const ICommandHandler&) = delete;
  ICommandHandler& operator=(const ICommandHandler&) = delete;
  ICommandHandler(ICommandHandler&&) = delete;
  ICommandHandler& operator=(ICommandHandler&&) = delete;
  virtual ~ICommandHandler() = default;

  /// @brief Called when a bound key resolves to @p command. Must return promptly.
  /// @warning Invoked from the keyboard hook's callback, which keeps using the
  ///          hook after onCommand returns. Do not destroy the KeyboardHook from
  ///          within onCommand (it would free state the callback still touches).
  ///          To exit, request shutdown and tear the hook down on another thread.
  virtual void onCommand(Command command) = 0;
};

} // namespace vox::input

#endif // VOX_INPUT_COMMAND_HANDLER_HPP
