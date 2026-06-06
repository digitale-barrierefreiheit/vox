// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::output::OutputManager.
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

/// Appends the German state words for @p node, in announcement order.
void appendStates(std::vector<std::string>& parts, const AccessibleNode& node,
                  const Lexicon& lexicon) {
  const vox::model::StateSet& states = node.states;

  // Toggle state is only meaningful for, and only announced on, checkable roles.
  if (node.role == Role::Checkbox || node.role == Role::RadioButton) {
    StateConcept toggle = StateConcept::Unchecked;
    if (states.test(State::Mixed)) {
      toggle = StateConcept::Mixed;
    } else if (states.test(State::Checked)) {
      toggle = StateConcept::Checked;
    }
    appendWord(parts, lexicon.state(toggle));
  }
  if (states.test(State::Expandable)) {
    appendWord(parts, lexicon.state(states.test(State::Expanded) ? StateConcept::Expanded
                                                                 : StateConcept::Collapsed));
  }
  if (states.test(State::Selected)) {
    appendWord(parts, lexicon.state(StateConcept::Selected));
  }
  if (states.test(State::ReadOnly)) {
    appendWord(parts, lexicon.state(StateConcept::ReadOnly));
  }
  if (states.test(State::Disabled)) {
    appendWord(parts, lexicon.state(StateConcept::Disabled));
  }
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
