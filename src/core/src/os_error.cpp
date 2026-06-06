// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::formatOsError.
#include <vox/core/os_error.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace vox {

std::string formatOsError(std::uint32_t code, std::string_view context) {
  if (code == 0U) {
    return std::string(context);
  }
  // HRESULT/DWORD codes read naturally as 8-digit hex (e.g. 0x80004005).
  return std::format("{} (0x{:08X})", context, code);
}

} // namespace vox
