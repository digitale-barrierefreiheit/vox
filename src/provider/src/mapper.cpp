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
using vox::model::StateSet;
using enum vox::model::State;

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

// Checked/Mixed from the modern Toggle pattern, else the legacy MSAA state bits
// (standard Win32 controls expose state only through the legacy bridge). legacyState
// is 0 when unread, so the bit tests are safely no-ops then.
void applyToggle(StateSet& states, const UiaElementData& data) {
  if (data.hasToggle) {
    if (data.toggleState == UiaToggleStateOn) {
      states.set(Checked);
    } else if (data.toggleState == UiaToggleStateIndeterminate) {
      states.set(Mixed);
    }
    return;
  }
  if ((data.legacyState & UiaLegacyStateMixed) != 0U) {
    states.set(Mixed);
  } else if ((data.legacyState & UiaLegacyStateChecked) != 0U) {
    states.set(Checked);
  }
}

void applyExpandCollapse(StateSet& states, const UiaElementData& data) {
  if (!data.hasExpandCollapse || data.expandCollapseState == UiaExpandCollapseStateLeafNode) {
    return;
  }
  states.set(Expandable);
  if (data.expandCollapseState == UiaExpandCollapseStateExpanded ||
      data.expandCollapseState == UiaExpandCollapseStatePartiallyExpanded) {
    states.set(Expanded);
  }
}

// Selected from the modern SelectionItem pattern when present (so it can authoritatively
// say "not selected"), else the legacy MSAA state bit.
void applySelected(StateSet& states, const UiaElementData& data) {
  const bool selected =
      data.hasSelectionItem ? data.isSelected : (data.legacyState & UiaLegacyStateSelected) != 0U;
  if (selected) {
    states.set(Selected);
  }
}

// ReadOnly from the ValuePattern's IsReadOnly when it was read, else the legacy state bit.
void applyReadOnly(StateSet& states, const UiaElementData& data) {
  const bool readOnly =
      data.hasReadOnly ? data.isReadOnly : (data.legacyState & UiaLegacyStateReadOnly) != 0U;
  if (readOnly) {
    states.set(ReadOnly);
  }
}

// Value from the ValuePattern, else the legacy value — but only for value-bearing roles,
// so a button's empty legacy value never becomes a spurious empty AccessibleNode value.
void applyValue(vox::model::AccessibleNode& node, const UiaElementData& data) {
  if (data.hasValue) {
    node.value = data.value; // present even when empty; absent stays nullopt
    return;
  }
  const bool valueBearing = node.role == Role::Edit || node.role == Role::Combobox;
  if (data.hasLegacyValue && valueBearing) {
    node.value = data.legacyValue;
  }
}

} // namespace

vox::model::AccessibleNode mapElement(const UiaElementData& data) {
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
  applyToggle(node.states, data);
  applyExpandCollapse(node.states, data);
  applySelected(node.states, data);
  applyReadOnly(node.states, data);
  applyValue(node, data);

  return node;
}

} // namespace vox::provider
