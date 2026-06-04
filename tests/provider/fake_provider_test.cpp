// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::testing::FakeProvider (query + focus-event dispatch).
#include <optional>
#include <string_view>

#include <gtest/gtest.h>

#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/testing/fake_provider.hpp>

namespace {

using vox::model::AccessibleNode;
using vox::model::Role;
using vox::testing::FakeProvider;

AccessibleNode named(Role role, std::string_view name) {
  AccessibleNode node;
  node.role = role;
  node.name = name;
  return node;
}

TEST(FakeProvider, FocusedElementDefaultsToNone) {
  const FakeProvider provider;
  EXPECT_FALSE(provider.focusedElement().has_value());
}

TEST(FakeProvider, ReturnsTheSetFocusedElement) {
  FakeProvider provider;
  provider.setFocusedElement(named(Role::Button, "OK"));
  EXPECT_EQ(provider.focusedElement(), named(Role::Button, "OK"));
}

TEST(FakeProvider, StartDeliversFocusChangeEvents) {
  FakeProvider provider;
  std::optional<AccessibleNode> received;
  provider.start([&received](const AccessibleNode& node) { received = node; });

  provider.simulateFocusChange(named(Role::Checkbox, "Newsletter"));

  EXPECT_EQ(received, named(Role::Checkbox, "Newsletter"));
  // The query reflects the change too.
  EXPECT_EQ(provider.focusedElement(), named(Role::Checkbox, "Newsletter"));
}

TEST(FakeProvider, StopHaltsEventDelivery) {
  FakeProvider provider;
  int callbackCount = 0;
  provider.start([&callbackCount](const AccessibleNode&) { ++callbackCount; });

  provider.simulateFocusChange(named(Role::Button, "A"));
  provider.stop();
  provider.simulateFocusChange(named(Role::Button, "B"));

  EXPECT_EQ(callbackCount, 1);
}

TEST(FakeProvider, SimulateBeforeStartUpdatesQueryButFiresNoCallback) {
  FakeProvider provider;
  provider.simulateFocusChange(named(Role::Edit, "Suche")); // never started
  EXPECT_EQ(provider.focusedElement(), named(Role::Edit, "Suche"));
}

} // namespace
