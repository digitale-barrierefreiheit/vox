// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::core::version().
#include <vox/core/version.hpp>

namespace vox::core {

VersionInfo version() noexcept {
  return VersionInfo{
      .major = VersionMajor,
      .minor = VersionMinor,
      .patch = VersionPatch,
      .text = VersionString,
  };
}

} // namespace vox::core
