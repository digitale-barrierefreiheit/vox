// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief `vox::input::HookError` — a keyboard-hook OS call failed.
#ifndef VOX_INPUT_ERRORS_HPP
#define VOX_INPUT_ERRORS_HPP

#include <vox/core/os_error.hpp>

namespace vox::input {

/// @brief Raised when installing or running the low-level keyboard hook fails.
///        Carries the originating Win32 `GetLastError()` code (see
///        `vox::OsError`). Catchable specifically, or as `vox::OsError`.
class HookError : public vox::OsError {
public:
  using vox::OsError::OsError;
};

} // namespace vox::input

#endif // VOX_INPUT_ERRORS_HPP
