// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::output::OutputManager.
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vox/german/lexicon.hpp>
#include <vox/german/normalize.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/model/state.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/output/utterance.hpp>

namespace vox::output {

namespace {

using vox::german::Lexicon;
using vox::german::StateConcept;
using vox::model::AccessibleNode;
using vox::model::Role;
using vox::model::State;

/// Appends @p word to @p parts unless it is empty.
void appendWord(std::vector<std::string>& parts, std::string_view word) {
  if (!word.empty()) {
    parts.emplace_back(word);
  }
}

/// Whether @p role announces a toggle (checked/unchecked/mixed) at all.
bool isCheckableRole(Role role) {
  return role == Role::Checkbox || role == Role::RadioButton;
}

/// The toggle concept a checkable control is in (mixed wins over checked).
StateConcept toggleConcept(const vox::model::StateSet& states) {
  if (states.test(State::Mixed)) {
    return StateConcept::Mixed;
  }
  if (states.test(State::Checked)) {
    return StateConcept::Checked;
  }
  return StateConcept::Unchecked;
}

/// Appends the toggle word for a checkable role; nothing for other roles.
void appendToggle(std::vector<std::string>& parts, const AccessibleNode& node,
                  const Lexicon& lexicon) {
  if (isCheckableRole(node.role)) {
    appendWord(parts, lexicon.state(toggleConcept(node.states)));
  }
}

/// Appends expanded/collapsed for an expandable control; nothing otherwise.
void appendExpansion(std::vector<std::string>& parts, const vox::model::StateSet& states,
                     const Lexicon& lexicon) {
  if (states.test(State::Expandable)) {
    appendWord(parts, lexicon.state(states.test(State::Expanded) ? StateConcept::Expanded
                                                                 : StateConcept::Collapsed));
  }
}

/// A control state announced verbatim when its flag is present, in table order.
struct FlagWord {
  State flag;
  StateConcept stateConcept;
};

/// Plain "flag present → word" states, in announcement order (selected, then
/// read-only, then disabled). Toggle and expansion are conditional and handled
/// separately above.
constexpr std::array<FlagWord, 3> FlagWords{{
    {.flag = State::Selected, .stateConcept = StateConcept::Selected},
    {.flag = State::ReadOnly, .stateConcept = StateConcept::ReadOnly},
    {.flag = State::Disabled, .stateConcept = StateConcept::Disabled},
}};

/// Appends the @ref FlagWords whose flags are present, preserving table order.
void appendFlagWords(std::vector<std::string>& parts, const vox::model::StateSet& states,
                     const Lexicon& lexicon) {
  for (const FlagWord& entry : FlagWords) {
    if (states.test(entry.flag)) {
      appendWord(parts, lexicon.state(entry.stateConcept));
    }
  }
}

/// Appends the German state words for @p node, in announcement order.
void appendStates(std::vector<std::string>& parts, const AccessibleNode& node,
                  const Lexicon& lexicon) {
  // Toggle state is only meaningful for, and only announced on, checkable roles.
  appendToggle(parts, node, lexicon);
  appendExpansion(parts, node.states, lexicon);
  appendFlagWords(parts, node.states, lexicon);
}

/// Joins @p parts with ", ".
std::string join(const std::vector<std::string>& parts) {
  std::string out;
  for (const std::string& part : parts) {
    if (!out.empty()) {
      out += ", ";
    }
    out += part;
  }
  return out;
}

} // namespace

OutputManager::OutputManager(vox::german::Lexicon lexicon) : lexicon_(std::move(lexicon)) {}

Utterance OutputManager::announce(const vox::model::AccessibleNode& node) const {
  std::vector<std::string> parts;

  appendWord(parts, lexicon_.role(node.role));

  if (std::string name = vox::german::normalizeName(node.name); !name.empty()) {
    parts.push_back(std::move(name));
  }

  appendStates(parts, node, lexicon_);

  if (node.value.has_value()) {
    std::string value = vox::german::normalizeName(*node.value);
    if (value.empty()) {
      appendWord(parts, lexicon_.state(StateConcept::EmptyValue)); // "leer"
    } else {
      parts.push_back(std::move(value));
    }
  }

  return Utterance{.text = join(parts)};
}

} // namespace vox::output
