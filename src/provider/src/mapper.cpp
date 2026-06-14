// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::provider::mapElement.
#include <array>
#include <utility>

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

// UIA CONTROLTYPEID -> vox Role. The IDs are non-contiguous, so a flat lookup
// table (scanned linearly) is clearer than a switch and keeps mapRole trivial.
// Any control type not listed maps to Role::Unknown.
constexpr std::array<std::pair<int, Role>, 9> ControlTypeRoles{{
    {UiaButtonControlTypeId, Role::Button},
    {UiaCheckBoxControlTypeId, Role::Checkbox},
    {UiaRadioButtonControlTypeId, Role::RadioButton},
    {UiaEditControlTypeId, Role::Edit},
    {UiaComboBoxControlTypeId, Role::Combobox},
    {UiaListItemControlTypeId, Role::ListItem},
    {UiaMenuItemControlTypeId, Role::MenuItem},
    {UiaHyperlinkControlTypeId, Role::Link},
    {UiaTextControlTypeId, Role::StaticText},
}};

Role mapRole(int controlTypeId) {
  for (const auto& [id, role] : ControlTypeRoles) {
    if (id == controlTypeId) {
      return role;
    }
  }
  return Role::Unknown;
}

// --- Legacy MSAA state-bit predicates ----------------------------------------
// Standard Win32 controls reach UIA through the MSAA bridge and expose state via these
// bits rather than the modern patterns. legacyState is 0 when unread, so each is a safe
// no-op then. Naming the tests keeps the apply* helpers free of raw bit arithmetic.
bool legacyMixed(const UiaElementData& data) {
  return (data.legacyState & UiaLegacyStateMixed) != 0U;
}

bool legacyChecked(const UiaElementData& data) {
  return (data.legacyState & UiaLegacyStateChecked) != 0U;
}

bool legacyExpanded(const UiaElementData& data) {
  return (data.legacyState & UiaLegacyStateExpanded) != 0U;
}

bool legacyCollapsed(const UiaElementData& data) {
  return (data.legacyState & UiaLegacyStateCollapsed) != 0U;
}

bool legacySelected(const UiaElementData& data) {
  return (data.legacyState & UiaLegacyStateSelected) != 0U;
}

bool legacyReadOnly(const UiaElementData& data) {
  return (data.legacyState & UiaLegacyStateReadOnly) != 0U;
}

// --- Toggle (Checked/Mixed) --------------------------------------------------
void applyModernToggle(StateSet& states, const UiaElementData& data) {
  if (data.toggleState == UiaToggleStateOn) {
    states.set(Checked);
  } else if (data.toggleState == UiaToggleStateIndeterminate) {
    states.set(Mixed);
  }
}

void applyLegacyToggle(StateSet& states, const UiaElementData& data) {
  if (legacyMixed(data)) {
    states.set(Mixed);
  } else if (legacyChecked(data)) {
    states.set(Checked);
  }
}

// Checked/Mixed from the modern Toggle pattern, else the legacy MSAA state bits
// (standard Win32 controls expose state only through the legacy bridge).
void applyToggle(StateSet& states, const UiaElementData& data) {
  if (data.hasToggle) {
    applyModernToggle(states, data);
  } else {
    applyLegacyToggle(states, data);
  }
}

// --- ExpandCollapse ----------------------------------------------------------
void applyModernExpandCollapse(StateSet& states, const UiaElementData& data) {
  if (data.expandCollapseState == UiaExpandCollapseStateLeafNode) {
    return;
  }
  states.set(Expandable);
  if (data.expandCollapseState == UiaExpandCollapseStateExpanded ||
      data.expandCollapseState == UiaExpandCollapseStatePartiallyExpanded) {
    states.set(Expanded);
  }
}

// Legacy fallback: a standard Win32 combobox surfaces expand/collapse through the legacy
// state bits, not the modern pattern.
void applyLegacyExpandCollapse(StateSet& states, const UiaElementData& data) {
  if (legacyExpanded(data)) {
    states.set(Expandable);
    states.set(Expanded);
  } else if (legacyCollapsed(data)) {
    states.set(Expandable);
  }
}

void applyExpandCollapse(StateSet& states, const UiaElementData& data) {
  if (data.hasExpandCollapse) {
    applyModernExpandCollapse(states, data);
  } else {
    applyLegacyExpandCollapse(states, data);
  }
}

// --- Selected ----------------------------------------------------------------
// Selected from the modern SelectionItem pattern when present (so it can authoritatively
// say "not selected"), else the legacy MSAA state bit.
void applySelected(StateSet& states, const UiaElementData& data) {
  const bool selected = data.hasSelectionItem ? data.isSelected : legacySelected(data);
  if (selected) {
    states.set(Selected);
  }
}

// --- ReadOnly / Value --------------------------------------------------------
// "Read-only" and a value are concepts only for value-bearing roles (an editable field or a
// combo box); other roles (e.g. a static text, trivially read-only) must not report them.
bool isValueBearing(Role role) {
  return role == Role::Edit || role == Role::Combobox;
}

// ReadOnly from the ValuePattern's IsReadOnly when it was read, else the legacy state bit — but
// only for value-bearing roles, so a static text does not announce a vacuous "read-only".
void applyReadOnly(vox::model::AccessibleNode& node, const UiaElementData& data) {
  if (!isValueBearing(node.role)) {
    return;
  }
  const bool readOnly = data.hasReadOnly ? data.isReadOnly : legacyReadOnly(data);
  if (readOnly) {
    node.states.set(ReadOnly);
  }
}

// Value from the ValuePattern, else the legacy value — but only for value-bearing roles,
// so a button's empty legacy value never becomes a spurious empty AccessibleNode value.
void applyValue(vox::model::AccessibleNode& node, const UiaElementData& data) {
  if (data.hasValue) {
    node.value = data.value; // present even when empty; absent stays nullopt
    return;
  }
  if (data.hasLegacyValue && isValueBearing(node.role)) {
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
  applyReadOnly(node, data);
  applyValue(node, data);

  return node;
}

} // namespace vox::provider
