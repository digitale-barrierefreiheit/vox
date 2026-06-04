// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::provider::mapElement (UIA snapshot -> AccessibleNode).
#include <gtest/gtest.h>

#include <vox/model/role.hpp>
#include <vox/model/state.hpp>
#include <vox/provider/mapper.hpp>
#include <vox/provider/uia_element_data.hpp>
#include <vox/provider/uia_ids.hpp>

namespace {

namespace vp = vox::provider;
using vox::model::Role;
using vox::model::State;
using vp::mapElement;
using vp::UiaElementData;

UiaElementData withControlType(int controlTypeId) {
  UiaElementData data;
  data.controlTypeId = controlTypeId;
  return data;
}

TEST(Mapper, MapsKnownControlTypesToRoles) {
  EXPECT_EQ(mapElement(withControlType(vp::UiaButtonControlTypeId)).role, Role::Button);
  EXPECT_EQ(mapElement(withControlType(vp::UiaCheckBoxControlTypeId)).role, Role::Checkbox);
  EXPECT_EQ(mapElement(withControlType(vp::UiaRadioButtonControlTypeId)).role, Role::RadioButton);
  EXPECT_EQ(mapElement(withControlType(vp::UiaEditControlTypeId)).role, Role::Edit);
  EXPECT_EQ(mapElement(withControlType(vp::UiaComboBoxControlTypeId)).role, Role::Combobox);
  EXPECT_EQ(mapElement(withControlType(vp::UiaListItemControlTypeId)).role, Role::ListItem);
  EXPECT_EQ(mapElement(withControlType(vp::UiaMenuItemControlTypeId)).role, Role::MenuItem);
  EXPECT_EQ(mapElement(withControlType(vp::UiaHyperlinkControlTypeId)).role, Role::Link);
  EXPECT_EQ(mapElement(withControlType(vp::UiaTextControlTypeId)).role, Role::StaticText);
}

TEST(Mapper, UnknownControlTypeMapsToUnknown) {
  EXPECT_EQ(mapElement(withControlType(99999)).role, Role::Unknown);
  EXPECT_EQ(mapElement(withControlType(0)).role, Role::Unknown);
}

TEST(Mapper, NameIsCopiedThrough) {
  UiaElementData data;
  data.name = "Speichern";
  EXPECT_EQ(mapElement(data).name, "Speichern");
}

TEST(Mapper, DisabledFromIsEnabledFalse) {
  UiaElementData data;
  data.isEnabled = false;
  EXPECT_TRUE(mapElement(data).states.test(State::Disabled));

  data.isEnabled = true;
  EXPECT_FALSE(mapElement(data).states.test(State::Disabled));
}

TEST(Mapper, FocusFlags) {
  UiaElementData data;
  data.hasKeyboardFocus = true;
  data.isKeyboardFocusable = true;
  const auto node = mapElement(data);
  EXPECT_TRUE(node.states.test(State::Focused));
  EXPECT_TRUE(node.states.test(State::Focusable));
}

TEST(Mapper, ToggleStateOnOffIndeterminate) {
  UiaElementData data;
  data.hasToggle = true;

  data.toggleState = vp::UiaToggleStateOff;
  EXPECT_FALSE(mapElement(data).states.test(State::Checked));
  EXPECT_FALSE(mapElement(data).states.test(State::Mixed));

  data.toggleState = vp::UiaToggleStateOn;
  EXPECT_TRUE(mapElement(data).states.test(State::Checked));
  EXPECT_FALSE(mapElement(data).states.test(State::Mixed));

  data.toggleState = vp::UiaToggleStateIndeterminate;
  EXPECT_FALSE(mapElement(data).states.test(State::Checked));
  EXPECT_TRUE(mapElement(data).states.test(State::Mixed));
}

TEST(Mapper, ToggleIgnoredWithoutPattern) {
  UiaElementData data;
  data.hasToggle = false;
  data.toggleState = vp::UiaToggleStateOn; // ignored without the pattern
  EXPECT_FALSE(mapElement(data).states.test(State::Checked));
}

TEST(Mapper, ExpandCollapseStates) {
  UiaElementData data;
  data.hasExpandCollapse = true;

  data.expandCollapseState = vp::UiaExpandCollapseStateCollapsed;
  EXPECT_TRUE(mapElement(data).states.test(State::Expandable));
  EXPECT_FALSE(mapElement(data).states.test(State::Expanded));

  data.expandCollapseState = vp::UiaExpandCollapseStateExpanded;
  EXPECT_TRUE(mapElement(data).states.test(State::Expandable));
  EXPECT_TRUE(mapElement(data).states.test(State::Expanded));

  data.expandCollapseState = vp::UiaExpandCollapseStatePartiallyExpanded;
  EXPECT_TRUE(mapElement(data).states.test(State::Expandable));
  EXPECT_TRUE(mapElement(data).states.test(State::Expanded));

  data.expandCollapseState = vp::UiaExpandCollapseStateLeafNode;
  EXPECT_FALSE(mapElement(data).states.test(State::Expandable));
  EXPECT_FALSE(mapElement(data).states.test(State::Expanded));
}

TEST(Mapper, SelectedFromSelectionItem) {
  UiaElementData data;
  data.hasSelectionItem = true;
  data.isSelected = true;
  EXPECT_TRUE(mapElement(data).states.test(State::Selected));

  data.isSelected = false;
  EXPECT_FALSE(mapElement(data).states.test(State::Selected));
}

TEST(Mapper, ReadOnlyFromValuePattern) {
  UiaElementData data;
  data.hasValuePattern = true;
  data.isReadOnly = true;
  EXPECT_TRUE(mapElement(data).states.test(State::ReadOnly));

  data.isReadOnly = false;
  EXPECT_FALSE(mapElement(data).states.test(State::ReadOnly));
}

// Read-only-ness comes from the pattern, so it is reported even if the value
// text itself was unreadable (then the value is absent, not a spurious empty).
TEST(Mapper, ReadOnlyReportedEvenWhenValueTextAbsent) {
  UiaElementData data;
  data.hasValuePattern = true;
  data.isReadOnly = true;
  data.hasValue = false;
  const auto node = mapElement(data);
  EXPECT_TRUE(node.states.test(State::ReadOnly));
  EXPECT_FALSE(node.value.has_value());
}

TEST(Mapper, ValuePresentEmptyVersusAbsent) {
  const UiaElementData absent; // no value text
  EXPECT_FALSE(mapElement(absent).value.has_value());

  UiaElementData empty;
  empty.hasValue = true;
  empty.value = "";
  EXPECT_EQ(mapElement(empty).value, ""); // present and empty

  UiaElementData filled;
  filled.hasValue = true;
  filled.value = "hallo";
  EXPECT_EQ(mapElement(filled).value, "hallo");
}

} // namespace
