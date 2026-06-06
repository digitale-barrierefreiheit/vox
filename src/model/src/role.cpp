// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::model::toString(Role).
#include <string_view>

#include <vox/model/role.hpp>

namespace vox::model {

std::string_view toString(Role role) noexcept {
  using enum Role;
  switch (role) {
  case Unknown:
    return "Unknown";
  case Button:
    return "Button";
  case Checkbox:
    return "Checkbox";
  case RadioButton:
    return "RadioButton";
  case Edit:
    return "Edit";
  case Combobox:
    return "Combobox";
  case ListItem:
    return "ListItem";
  case MenuItem:
    return "MenuItem";
  case Link:
    return "Link";
  case StaticText:
    return "StaticText";
  }
  return "Unknown";
}

} // namespace vox::model
