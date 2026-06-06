// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief `vox::OsError` — base exception for failed OS/platform calls.
///
/// The OS-glue layer (WASAPI, SAPI, the keyboard hook) calls Win32/COM APIs that
/// fail with an `HRESULT` or a `GetLastError()` code. Throwing a bare
/// `std::runtime_error` discards that code and lets callers discriminate only on
/// the message. `OsError` carries the native code, and each module derives a
/// specific type (`vox::audio::DeviceError`, `vox::tts::EngineError`,
/// `vox::input::HookError`) so a handler can catch by subsystem. The type and its
/// message formatting are pure, so the taxonomy is unit-tested off Windows
/// (ADR-12); only the *capture* of a code at a call site is platform code.
#ifndef VOX_CORE_OS_ERROR_HPP
#define VOX_CORE_OS_ERROR_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace vox {

/// @brief Builds an OS-error message: @p context, plus the native @p code as
///        `0x`-prefixed 8-digit hex when it is non-zero (0 means "no code").
/// @note Pure and platform-independent.
[[nodiscard]] std::string formatOsError(std::uint32_t code, std::string_view context);

/// @brief Base exception for a failed OS/platform call.
///
/// Carries the originating native error code (an `HRESULT` or a Win32 `DWORD`),
/// or 0 when the failure is a precondition/validation one with no OS code.
class OsError : public std::runtime_error {
public:
  /// @brief Failure with a native @p code (an `HRESULT` or `GetLastError()`).
  OsError(std::uint32_t code, std::string_view context)
      : std::runtime_error(formatOsError(code, context)), code_(code) {}

  /// @brief Failure with no meaningful native code (precondition/validation).
  explicit OsError(std::string_view context) : OsError(0U, context) {}

  /// @brief The originating native error code, or 0 if none was available.
  [[nodiscard]] std::uint32_t code() const noexcept {
    return code_;
  }

private:
  std::uint32_t code_;
};

} // namespace vox

#endif // VOX_CORE_OS_ERROR_HPP
