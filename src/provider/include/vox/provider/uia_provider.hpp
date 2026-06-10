// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows UI Automation implementation of IProvider (ADR-03/ADR-04).
///
/// Reads the focused element out-of-process via one batched `CacheRequest` and
/// delivers UIA focus-change events. COM types are hidden behind a pImpl so this
/// header stays clean. Windows-only — built solely on the Windows toolchain (the
/// portable mapper it delegates to is what the sanitizer/clang-tidy build sees).
///
/// @note Focus-change callbacks are invoked on a UIA worker thread; a caller
///       that touches non-thread-safe state must marshal to its own thread.
#ifndef VOX_PROVIDER_UIA_PROVIDER_HPP
#define VOX_PROVIDER_UIA_PROVIDER_HPP

// UiaProvider is implemented only on Windows (see src/provider/CMakeLists.txt).
// Declaring it only there turns any accidental non-Windows use into a clear
// "undeclared identifier" at compile time rather than a confusing link error.
#if defined(_WIN32)

#  include <memory>
#  include <optional>
#  include <string_view>

#  include <vox/model/accessible_node.hpp>
#  include <vox/provider/iprovider.hpp>

// Win32 window handle (HWND): forward-declared so this header needs no <windows.h>.
struct HWND__;

namespace vox::provider {

/// Out-of-process UI Automation provider for the focused element.
class UiaProvider : public IProvider {
public:
  /// Sets up the UI Automation client + cache request. Attempts to initialize
  /// COM as MTA but tolerates an already-initialized apartment (e.g. STA), so it
  /// works regardless of the caller's COM state.
  UiaProvider();
  ~UiaProvider() override;

  UiaProvider(const UiaProvider&) = delete;
  UiaProvider& operator=(const UiaProvider&) = delete;
  UiaProvider(UiaProvider&&) = delete;
  UiaProvider& operator=(UiaProvider&&) = delete;

  /// @brief Reads the focused element, or `std::nullopt` if none/unreadable.
  [[nodiscard]] std::optional<vox::model::AccessibleNode> focusedElement() const override;

  /// @brief Subscribes @p onFocusChanged to UIA focus-change events.
  void start(FocusChangedCallback onFocusChanged) override;

  /// @brief Unsubscribes from focus-change events.
  void stop() override;

  /// @brief Reads the first element named @p name in the subtree of window @p windowHandle
  ///        (an `HWND`, forward-declared above so this header needs no Windows headers), or
  ///        `std::nullopt` if not found/unreadable. The focus path can only reach focusable
  ///        controls; this lets the #40 integration test read non-focusable roles (static
  ///        text, menu items) by name.
  [[nodiscard]] std::optional<vox::model::AccessibleNode> nodeByName(HWND__* windowHandle,
                                                                     std::string_view name) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vox::provider

#endif // defined(_WIN32)

#endif // VOX_PROVIDER_UIA_PROVIDER_HPP
