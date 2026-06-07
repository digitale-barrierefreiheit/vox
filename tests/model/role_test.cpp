// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::model::Role.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include <vox/model/role.hpp>

namespace {

using vox::model::Role;
using vox::model::toString;

// Every MVP role, so the toString switch is fully covered and the set is
// pinned against accidental reordering/removal.
constexpr std::array<std::pair<Role, std::string_view>, 10> AllRoles{{
    {Role::Unknown, "Unknown"},
    {Role::Button, "Button"},
    {Role::Checkbox, "Checkbox"},
    {Role::RadioButton, "RadioButton"},
    {Role::Edit, "Edit"},
    {Role::Combobox, "Combobox"},
    {Role::ListItem, "ListItem"},
    {Role::MenuItem, "MenuItem"},
    {Role::Link, "Link"},
    {Role::StaticText, "StaticText"},
}};

TEST(Role, ToStringCoversEveryValue) {
  for (const auto& [role, name] : AllRoles) {
    EXPECT_EQ(toString(role), name);
  }
}

TEST(Role, NamesAreDistinct) {
  for (std::size_t i = 0; i < AllRoles.size(); ++i) {
    for (std::size_t j = i + 1; j < AllRoles.size(); ++j) {
      EXPECT_NE(AllRoles.at(i).second, AllRoles.at(j).second);
    }
  }
}

TEST(Role, UnknownIsTheDefaultSentinel) {
  EXPECT_EQ(static_cast<std::uint8_t>(Role::Unknown), 0U);
}

} // namespace
