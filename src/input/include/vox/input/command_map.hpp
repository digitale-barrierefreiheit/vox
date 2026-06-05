// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Key-event -> reader-command mapping, and the pure routing helper.
///
/// This is the OS-independent seam where key bindings live (architecture §5.1):
/// the Windows hook feeds it translated `KeyEvent`s, tests feed it directly. The
/// bindings are data (a small table), so they stay easy to test now and to make
/// configurable later. The MVP bindings are fixed.
#ifndef VOX_INPUT_COMMAND_MAP_HPP
#define VOX_INPUT_COMMAND_MAP_HPP

#include <array>
#include <cstdint>

#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>
#include <vox/input/key_event.hpp>

namespace vox::input {

/// Maps key events to reader commands via a table of bindings.
class CommandMap {
public:
  /// @brief Builds the map with the fixed MVP key bindings.
  CommandMap();

  /// @brief Resolves @p event to a Command, or `Command::None` if it is not a
  ///        bound reader key. Only key-down events bind; key-up is always None.
  ///        A binding matches only when the held modifiers match it exactly.
  [[nodiscard]] Command map(const KeyEvent& event) const;

private:
  /// One key binding: an exact (virtual-key, modifiers) pair and its command.
  struct Binding {
    std::uint32_t virtualKey;
    KeyModifiers modifiers;
    Command command;
  };

  std::array<Binding, 8> bindings_;
};

/// @brief Resolves @p event through @p map and, for a bound key, calls
///        @p handler with the command. Returns true if the key should be
///        consumed (hidden from the focused app).
///
/// The pure heart of the keyboard hook — unit-tested without Win32. The hook's
/// callback just translates the OS event into a `KeyEvent` and calls this.
[[nodiscard]] bool routeKeyEvent(const KeyEvent& event, const CommandMap& map,
                                 ICommandHandler& handler);

} // namespace vox::input

#endif // VOX_INPUT_COMMAND_MAP_HPP
