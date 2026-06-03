// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::core::version().
#include <vox/core/version.hpp>

namespace vox::core {

VersionInfo version() noexcept {
  return VersionInfo{
      .major = VOX_VERSION_MAJOR,
      .minor = VOX_VERSION_MINOR,
      .patch = VOX_VERSION_PATCH,
      .text = VOX_VERSION_STRING,
  };
}

} // namespace vox::core
