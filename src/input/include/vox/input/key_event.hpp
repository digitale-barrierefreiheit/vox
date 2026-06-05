// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief OS-independent keyboard event + modifier flags for the input seam.
///
/// `KeyEvent` is the small value type the Windows keyboard hook (#38) hands to
/// the pure `CommandMap`: a virtual-key code plus the modifier state at the time
/// of the press. Keeping it OS-free lets the command mapping be unit-tested
/// everywhere, with no Win32 (architecture §5.1).
#ifndef VOX_INPUT_KEY_EVENT_HPP
#define VOX_INPUT_KEY_EVENT_HPP

#include <cstdint>

namespace vox::input {

/// Bitmask of the modifier keys held when a key event occurred.
enum class KeyModifiers : std::uint8_t {
  None = 0U,
  Shift = 1U << 0U,
  Control = 1U << 1U,
  Alt = 1U << 2U,
  Win = 1U << 3U,
};

[[nodiscard]] constexpr KeyModifiers operator|(KeyModifiers lhs, KeyModifiers rhs) noexcept {
  return static_cast<KeyModifiers>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr KeyModifiers operator&(KeyModifiers lhs, KeyModifiers rhs) noexcept {
  return static_cast<KeyModifiers>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

constexpr KeyModifiers& operator|=(KeyModifiers& lhs, KeyModifiers rhs) noexcept {
  lhs = lhs | rhs;
  return lhs;
}

/// @brief True if @p set contains every bit of @p query (with @p query != None).
[[nodiscard]] constexpr bool contains(KeyModifiers set, KeyModifiers query) noexcept {
  return (set & query) == query;
}

/// One keyboard event, OS-independent. `virtualKey` uses the Windows virtual-key
/// numbering (VK_*) — the only source the MVP hook has — but is just an integer
/// to the pure mapper. Bindings match `modifiers` exactly, so an unrelated held
/// modifier (e.g. Win) simply makes a key fall through to the focused app.
struct KeyEvent {
  std::uint32_t virtualKey{0};                ///< Virtual-key code (VK_*).
  KeyModifiers modifiers{KeyModifiers::None}; ///< Modifiers held at the time.
  bool pressed{false};                        ///< True on key-down, false on key-up.
};

} // namespace vox::input

#endif // VOX_INPUT_KEY_EVENT_HPP
