// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::model::AccessibleNode.
#include <optional>

#include <gtest/gtest.h>

#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/model/state.hpp>

namespace {

using vox::model::AccessibleNode;
using vox::model::Role;
using vox::model::State;
using vox::model::StateSet;

TEST(AccessibleNode, DefaultIsUnknownEmptyAndValueless) {
  const AccessibleNode node;
  EXPECT_EQ(node.role, Role::Unknown);
  EXPECT_TRUE(node.name.empty());
  EXPECT_TRUE(node.states.none());
  EXPECT_FALSE(node.value.has_value());
}

TEST(AccessibleNode, AggregateInitialization) {
  const AccessibleNode node{
      .role = Role::Button,
      .name = "Speichern",
      .states = StateSet{State::Focusable}.set(State::Focused),
      .value = std::nullopt,
  };
  EXPECT_EQ(node.role, Role::Button);
  EXPECT_EQ(node.name, "Speichern");
  EXPECT_TRUE(node.states.test(State::Focused));
  EXPECT_FALSE(node.value.has_value());
}

// The optional value distinguishes "no value concept" from "empty value".
TEST(AccessibleNode, AbsentValueDiffersFromEmptyValue) {
  AccessibleNode button; // a button has no value concept
  button.role = Role::Button;
  EXPECT_FALSE(button.value.has_value());

  AccessibleNode emptyEdit; // an empty edit field has an (empty) value
  emptyEdit.role = Role::Edit;
  emptyEdit.value = "";
  ASSERT_TRUE(emptyEdit.value.has_value());
  EXPECT_TRUE(emptyEdit.value->empty());

  EXPECT_NE(button.value, emptyEdit.value);
}

TEST(AccessibleNode, NameIsUtf8) {
  // UTF-8 multibyte content survives round-trip and length is in bytes.
  AccessibleNode node;
  node.name = "Größe \x{C3}\x{9C}"; // "Größe Ü"
  EXPECT_EQ(node.name, "Größe \x{C3}\x{9C}");
}

TEST(AccessibleNode, Equality) {
  const AccessibleNode a{
      .role = Role::Checkbox,
      .name = "Newsletter",
      .states = StateSet{State::Checked},
      .value = std::nullopt,
  };
  AccessibleNode b = a;
  EXPECT_EQ(a, b);

  b.name = "Other";
  EXPECT_NE(a, b);

  b = a;
  b.states.set(State::Focused);
  EXPECT_NE(a, b);

  b = a;
  b.value = "x";
  EXPECT_NE(a, b);

  b = a;
  b.role = Role::RadioButton;
  EXPECT_NE(a, b);
}

} // namespace
