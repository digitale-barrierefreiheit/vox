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

/// The modifier keys held when a key event occurred. Compared by value, so a
/// binding matches only when exactly the same modifiers are down.
struct KeyModifiers {
  bool shift{false};
  bool control{false};
  bool alt{false};
  bool win{false};

  [[nodiscard]] friend constexpr bool operator==(const KeyModifiers&,
                                                 const KeyModifiers&) noexcept = default;
};

/// One keyboard event, OS-independent. `virtualKey` uses the Windows virtual-key
/// numbering (VK_*) — the only source the MVP hook has — but is just an integer
/// to the pure mapper. Bindings match `modifiers` exactly, so an unrelated held
/// modifier (e.g. Win) simply makes a key fall through to the focused app.
struct KeyEvent {
  std::uint32_t virtualKey{0}; ///< Virtual-key code (VK_*).
  KeyModifiers modifiers{};    ///< Modifiers held at the time.
  bool pressed{false};         ///< True on key-down, false on key-up.
  bool injected{false};        ///< True if synthesized (e.g. SendInput).
};

} // namespace vox::input

#endif // VOX_INPUT_KEY_EVENT_HPP
