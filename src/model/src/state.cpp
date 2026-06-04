// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::model::toString(const StateSet&).
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <vox/model/state.hpp>

namespace vox::model {

std::string toString(const StateSet& states) {
  // Ascending bit order, so the rendering of a given set is stable.
  static constexpr std::array<std::pair<State, std::string_view>, 9> StateNames{{
      {State::Focusable, "Focusable"},
      {State::Focused, "Focused"},
      {State::Disabled, "Disabled"},
      {State::Checked, "Checked"},
      {State::Mixed, "Mixed"},
      {State::Expandable, "Expandable"},
      {State::Expanded, "Expanded"},
      {State::Selected, "Selected"},
      {State::ReadOnly, "ReadOnly"},
  }};

  std::string out;
  for (const auto& [state, name] : StateNames) {
    if (states.test(state)) {
      if (!out.empty()) {
        out += '|';
      }
      out += name;
    }
  }
  if (out.empty()) {
    out = "None";
  }
  return out;
}

} // namespace vox::model
