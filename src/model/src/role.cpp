// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::model::toString(Role).
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

#include <vox/model/role.hpp>

namespace vox::model {

namespace {

// Diagnostic name per Role, indexed by the enum's underlying value. Role values
// are contiguous from Unknown == 0, so the cast indexes directly. Keep this in
// the same order as the enum; an out-of-range value falls back to "Unknown".
constexpr std::array<std::string_view, 10> RoleNames{
    "Unknown",  "Button",   "Checkbox", "RadioButton", "Edit",
    "Combobox", "ListItem", "MenuItem", "Link",        "StaticText",
};

} // namespace

std::string_view toString(Role role) noexcept {
  const auto index = static_cast<std::size_t>(std::to_underlying(role));
  if (index >= RoleNames.size()) {
    return "Unknown"; // LCOV_EXCL_LINE — defensive out-of-range guard (see role_test.cpp)
  }
  return RoleNames.at(index);
}

} // namespace vox::model
