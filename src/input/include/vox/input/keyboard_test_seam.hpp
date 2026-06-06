// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only test seams over KeyboardHook (issue #68).
///
/// Two seams make the hook unit-testable with no real WH_KEYBOARD_LL install:
///  - `detail::processKey` is the per-key decision the hook callback makes
///    (auto-repeat / consume / key-up swallowing), factored out of the Win32
///    `hookProc` so it is tested directly with a fake handler.
///  - `testing::setInstallHookOverride` replaces the `SetWindowsHookEx` call so a
///    test can simulate a successful or failed install and exercise the
///    start/stop lifecycle without touching the desktop.
///
/// Production code sets no override and installs the real low-level hook.
#ifndef VOX_INPUT_KEYBOARD_TEST_SEAM_HPP
#define VOX_INPUT_KEYBOARD_TEST_SEAM_HPP

#if defined(_WIN32)

#  include <array>
#  include <cstddef>
#  include <functional>

#  include <vox/input/command_handler.hpp>
#  include <vox/input/command_map.hpp>
#  include <vox/input/key_event.hpp>

namespace vox::input {

/// What the keyboard hook should do with a key it just saw.
enum class HookAction {
  PassThrough, ///< Let the key reach the foreground app (CallNextHookEx).
  Consume,     ///< Hide the key from the app (it drove a reader command).
};

namespace detail {

/// @brief The keyboard hook's per-key decision, factored out of the Win32
///        callback so it is testable with no real hook.
///
/// On key-down it routes @p event through @p map / @p handler; a consumed key is
/// remembered in @p consumed (indexed by @p vk) so its auto-repeat and key-up are
/// also swallowed. On key-up it consumes iff the matching key-down was consumed.
/// @param pressed  True for key-down/sys-key-down, false for key-up.
/// @param vk       The virtual-key code, masked to [0, 255].
HookAction processKey(bool pressed, std::size_t vk, const KeyEvent& event,
                      std::array<bool, 256>& consumed, const CommandMap& map,
                      ICommandHandler& handler);

} // namespace detail

namespace testing {

/// @brief Replaces the low-level hook install. Returns a non-null opaque handle
///        to simulate success, or nullptr to simulate `SetWindowsHookEx` failing.
///        While an override is installed, the matching `UnhookWindowsHookEx` is
///        skipped (the handle is not real).
using InstallHookOverride = std::function<void*()>;

/// @brief Installs @p override for subsequent `KeyboardHook::start()` calls; an
///        empty override restores the real install. Test-only, not thread-safe.
void setInstallHookOverride(InstallHookOverride override);

} // namespace testing

} // namespace vox::input

#endif // defined(_WIN32)

#endif // VOX_INPUT_KEYBOARD_TEST_SEAM_HPP
