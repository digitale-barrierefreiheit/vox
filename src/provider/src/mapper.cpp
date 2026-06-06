// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::provider::mapElement.
#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/model/state.hpp>
#include <vox/provider/mapper.hpp>
#include <vox/provider/uia_element_data.hpp>
#include <vox/provider/uia_ids.hpp>

namespace vox::provider {

namespace {

using vox::model::Role;
using vox::model::State;

Role mapRole(int controlTypeId) {
  using enum Role;
  switch (controlTypeId) {
  case UiaButtonControlTypeId:
    return Button;
  case UiaCheckBoxControlTypeId:
    return Checkbox;
  case UiaRadioButtonControlTypeId:
    return RadioButton;
  case UiaEditControlTypeId:
    return Edit;
  case UiaComboBoxControlTypeId:
    return Combobox;
  case UiaListItemControlTypeId:
    return ListItem;
  case UiaMenuItemControlTypeId:
    return MenuItem;
  case UiaHyperlinkControlTypeId:
    return Link;
  case UiaTextControlTypeId:
    return StaticText;
  default:
    return Unknown;
  }
}

} // namespace

vox::model::AccessibleNode mapElement(const UiaElementData& data) {
  using enum State;
  vox::model::AccessibleNode node;
  node.role = mapRole(data.controlTypeId);
  node.name = data.name;

  if (!data.isEnabled) {
    node.states.set(Disabled);
  }
  if (data.hasKeyboardFocus) {
    node.states.set(Focused);
  }
  if (data.isKeyboardFocusable) {
    node.states.set(Focusable);
  }
  if (data.hasToggle) {
    if (data.toggleState == UiaToggleStateOn) {
      node.states.set(Checked);
    } else if (data.toggleState == UiaToggleStateIndeterminate) {
      node.states.set(Mixed);
    }
  }
  if (data.hasExpandCollapse && data.expandCollapseState != UiaExpandCollapseStateLeafNode) {
    node.states.set(Expandable);
    if (data.expandCollapseState == UiaExpandCollapseStateExpanded ||
        data.expandCollapseState == UiaExpandCollapseStatePartiallyExpanded) {
      node.states.set(Expanded);
    }
  }
  if (data.hasSelectionItem && data.isSelected) {
    node.states.set(Selected);
  }
  if (data.hasValuePattern && data.isReadOnly) {
    node.states.set(ReadOnly); // read-only-ness is independent of the text
  }
  if (data.hasValue) {
    node.value = data.value; // present even when empty; absent stays nullopt
  }

  return node;
}

} // namespace vox::provider
