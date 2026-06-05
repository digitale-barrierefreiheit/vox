// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The reader commands the keyboard hook produces from key events.
#ifndef VOX_INPUT_COMMAND_HPP
#define VOX_INPUT_COMMAND_HPP

#include <cstdint>

namespace vox::input {

/// A resolved reader command, produced by `CommandMap` from a `KeyEvent`.
/// `None` means "not a reader key — pass it through to the focused app".
enum class Command : std::uint8_t {
  None,             ///< Not a bound key; the app should receive it.
  NavigateNext,     ///< Tab / Right arrow — move to the next element.
  NavigatePrevious, ///< Shift+Tab / Left arrow — move to the previous element.
  NavigateUp,       ///< Up arrow.
  NavigateDown,     ///< Down arrow.
  Quit,             ///< Exit the reader.
  ToggleSpeech,     ///< Mute / unmute announcements.
};

/// @brief Whether @p command should be consumed (hidden from the focused app).
///
/// Navigation keys pass through so the app still moves focus (the announcement
/// follows the resulting focus change); reader-control keys (Quit, ToggleSpeech)
/// are swallowed so they do not leak into the app.
[[nodiscard]] constexpr bool consumesKey(Command command) noexcept {
  return command == Command::Quit || command == Command::ToggleSpeech;
}

} // namespace vox::input

#endif // VOX_INPUT_COMMAND_HPP
