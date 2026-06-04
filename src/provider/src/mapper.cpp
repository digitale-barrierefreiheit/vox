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
  switch (controlTypeId) {
  case UiaButtonControlTypeId:
    return Role::Button;
  case UiaCheckBoxControlTypeId:
    return Role::Checkbox;
  case UiaRadioButtonControlTypeId:
    return Role::RadioButton;
  case UiaEditControlTypeId:
    return Role::Edit;
  case UiaComboBoxControlTypeId:
    return Role::Combobox;
  case UiaListItemControlTypeId:
    return Role::ListItem;
  case UiaMenuItemControlTypeId:
    return Role::MenuItem;
  case UiaHyperlinkControlTypeId:
    return Role::Link;
  case UiaTextControlTypeId:
    return Role::StaticText;
  default:
    return Role::Unknown;
  }
}

} // namespace

vox::model::AccessibleNode mapElement(const UiaElementData& data) {
  vox::model::AccessibleNode node;
  node.role = mapRole(data.controlTypeId);
  node.name = data.name;

  if (!data.isEnabled) {
    node.states.set(State::Disabled);
  }
  if (data.hasKeyboardFocus) {
    node.states.set(State::Focused);
  }
  if (data.isKeyboardFocusable) {
    node.states.set(State::Focusable);
  }
  if (data.hasToggle) {
    if (data.toggleState == UiaToggleStateOn) {
      node.states.set(State::Checked);
    } else if (data.toggleState == UiaToggleStateIndeterminate) {
      node.states.set(State::Mixed);
    }
  }
  if (data.hasExpandCollapse && data.expandCollapseState != UiaExpandCollapseStateLeafNode) {
    node.states.set(State::Expandable);
    if (data.expandCollapseState == UiaExpandCollapseStateExpanded ||
        data.expandCollapseState == UiaExpandCollapseStatePartiallyExpanded) {
      node.states.set(State::Expanded);
    }
  }
  if (data.hasSelectionItem && data.isSelected) {
    node.states.set(State::Selected);
  }
  if (data.hasValuePattern && data.isReadOnly) {
    node.states.set(State::ReadOnly); // read-only-ness is independent of the text
  }
  if (data.hasValue) {
    node.value = data.value; // present even when empty; absent stays nullopt
  }

  return node;
}

} // namespace vox::provider
