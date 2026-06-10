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
  data.controlTypeId = vp::UiaEditControlTypeId; // ReadOnly applies only to value-bearing roles
  data.hasReadOnly = true;
  data.isReadOnly = true;
  EXPECT_TRUE(mapElement(data).states.test(State::ReadOnly));

  data.isReadOnly = false;
  EXPECT_FALSE(mapElement(data).states.test(State::ReadOnly));
}

// Read-only-ness comes from the pattern, so it is reported even if the value
// text itself was unreadable (then the value is absent, not a spurious empty).
TEST(Mapper, ReadOnlyReportedEvenWhenValueTextAbsent) {
  UiaElementData data;
  data.controlTypeId = vp::UiaEditControlTypeId; // value-bearing, so ReadOnly applies
  data.hasReadOnly = true;
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

// --- LegacyIAccessiblePattern fallback ---------------------------------------
// Standard Win32 controls reach UIA through the MSAA bridge, which exposes state
// via the legacy state bits rather than the modern Toggle/Value/SelectionItem
// patterns. The mapper falls back to those bits when the modern pattern is absent.

TEST(Mapper, CheckedFromLegacyStateWithoutTogglePattern) {
  UiaElementData data;
  data.hasToggle = false; // a standard Win32 checkbox exposes no modern Toggle pattern
  data.legacyState = vp::UiaLegacyStateChecked;
  EXPECT_TRUE(mapElement(data).states.test(State::Checked));
  EXPECT_FALSE(mapElement(data).states.test(State::Mixed));
}

TEST(Mapper, MixedFromLegacyState) {
  UiaElementData data;
  data.legacyState = vp::UiaLegacyStateMixed;
  EXPECT_TRUE(mapElement(data).states.test(State::Mixed));
  EXPECT_FALSE(mapElement(data).states.test(State::Checked));
}

TEST(Mapper, TogglePatternTakesPrecedenceOverLegacyState) {
  UiaElementData data;
  data.hasToggle = true;
  data.toggleState = vp::UiaToggleStateOff;     // modern says off...
  data.legacyState = vp::UiaLegacyStateChecked; // ...legacy says checked; modern wins
  EXPECT_FALSE(mapElement(data).states.test(State::Checked));
}

TEST(Mapper, SelectedFromLegacyStateWithoutSelectionItem) {
  UiaElementData data;
  data.legacyState = vp::UiaLegacyStateSelected;
  EXPECT_TRUE(mapElement(data).states.test(State::Selected));
}

// When the modern SelectionItem pattern is present it is authoritative: a legacy bit
// must not override its "not selected" (the patterns can disagree on bridged controls).
TEST(Mapper, SelectionItemTakesPrecedenceOverLegacyState) {
  UiaElementData data;
  data.hasSelectionItem = true;
  data.isSelected = false;                       // modern says not selected...
  data.legacyState = vp::UiaLegacyStateSelected; // ...legacy says selected; modern wins
  EXPECT_FALSE(mapElement(data).states.test(State::Selected));
}

// Same precedence for ReadOnly: a present ValuePattern that is not read-only wins over a
// stray legacy read-only bit.
TEST(Mapper, ValuePatternReadOnlyTakesPrecedenceOverLegacyState) {
  UiaElementData data;
  data.controlTypeId = vp::UiaEditControlTypeId; // value-bearing, so ReadOnly is considered
  data.hasReadOnly = true;
  data.isReadOnly = false;
  data.legacyState = vp::UiaLegacyStateReadOnly;
  EXPECT_FALSE(mapElement(data).states.test(State::ReadOnly));
}

TEST(Mapper, ReadOnlyFromLegacyStateWithoutValuePattern) {
  UiaElementData data;
  data.controlTypeId = vp::UiaEditControlTypeId; // value-bearing, so the legacy bit applies
  data.hasReadOnly = false;
  data.legacyState = vp::UiaLegacyStateReadOnly;
  EXPECT_TRUE(mapElement(data).states.test(State::ReadOnly));
}

// A non-value-bearing role (e.g. a static text) is trivially read-only via the legacy bridge,
// but "read-only" is a value concept, so the mapper suppresses it there (it would be noise).
TEST(Mapper, ReadOnlySuppressedForNonValueBearingRole) {
  UiaElementData data;
  data.controlTypeId = vp::UiaTextControlTypeId; // StaticText
  data.legacyState = vp::UiaLegacyStateReadOnly;
  EXPECT_FALSE(mapElement(data).states.test(State::ReadOnly));
}

TEST(Mapper, ValueFromLegacyForValueBearingRole) {
  UiaElementData data;
  data.controlTypeId = vp::UiaEditControlTypeId; // Edit -> value-bearing
  data.hasValue = false;                         // no modern Value pattern text
  data.hasLegacyValue = true;
  data.legacyValue = "Hallo";
  EXPECT_EQ(mapElement(data).value, "Hallo");
}

TEST(Mapper, LegacyValueIgnoredForNonValueRole) {
  UiaElementData data;
  data.controlTypeId = vp::UiaButtonControlTypeId; // a button has no value concept
  data.hasLegacyValue = true;
  data.legacyValue = ""; // must not become a spurious empty AccessibleNode value
  EXPECT_FALSE(mapElement(data).value.has_value());
}

TEST(Mapper, ValuePatternTakesPrecedenceOverLegacyValue) {
  UiaElementData data;
  data.controlTypeId = vp::UiaEditControlTypeId;
  data.hasValue = true;
  data.value = "modern";
  data.hasLegacyValue = true;
  data.legacyValue = "legacy";
  EXPECT_EQ(mapElement(data).value, "modern");
}

// A standard Win32 combobox exposes expand/collapse through the legacy state bits, not the
// modern ExpandCollapse pattern, so the mapper falls back to them.
TEST(Mapper, ExpandableFromLegacyCollapsedWithoutPattern) {
  UiaElementData data;
  data.legacyState = vp::UiaLegacyStateCollapsed;
  const auto node = mapElement(data);
  EXPECT_TRUE(node.states.test(State::Expandable));
  EXPECT_FALSE(node.states.test(State::Expanded));
}

TEST(Mapper, ExpandedFromLegacyExpandedWithoutPattern) {
  UiaElementData data;
  data.legacyState = vp::UiaLegacyStateExpanded;
  const auto node = mapElement(data);
  EXPECT_TRUE(node.states.test(State::Expandable));
  EXPECT_TRUE(node.states.test(State::Expanded));
}

TEST(Mapper, ExpandCollapsePatternTakesPrecedenceOverLegacyExpand) {
  UiaElementData data;
  data.hasExpandCollapse = true;
  data.expandCollapseState = vp::UiaExpandCollapseStateLeafNode; // modern: not expandable...
  data.legacyState = vp::UiaLegacyStateCollapsed;                // ...legacy says collapsed
  EXPECT_FALSE(mapElement(data).states.test(State::Expandable));
}

} // namespace
