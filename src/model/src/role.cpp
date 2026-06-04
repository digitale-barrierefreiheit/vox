// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::model::toString(Role).
#include <string_view>

#include <vox/model/role.hpp>

namespace vox::model {

std::string_view toString(Role role) noexcept {
  switch (role) {
  case Role::Unknown:
    return "Unknown";
  case Role::Button:
    return "Button";
  case Role::Checkbox:
    return "Checkbox";
  case Role::RadioButton:
    return "RadioButton";
  case Role::Edit:
    return "Edit";
  case Role::Combobox:
    return "Combobox";
  case Role::ListItem:
    return "ListItem";
  case Role::MenuItem:
    return "MenuItem";
  case Role::Link:
    return "Link";
  case Role::StaticText:
    return "StaticText";
  }
  return "Unknown";
}

} // namespace vox::model
