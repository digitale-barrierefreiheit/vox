// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::german::Lexicon.
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vox/german/lexicon.hpp>
#include <vox/model/role.hpp>

namespace vox::german {

namespace {

/// Key suffix per `vox::model::Role`, indexed by the enum's value.
constexpr std::array<std::string_view, 10> RoleSuffix{
    "unknown",  "button",   "checkbox", "radiobutton", "edit",
    "combobox", "listitem", "menuitem", "link",        "statictext"};

/// Key suffix per `StateConcept`, indexed by the enum's value.
constexpr std::array<std::string_view, StateConceptCount> StateSuffix{
    "checked",  "unchecked", "mixed",    "expanded",  "collapsed",
    "selected", "disabled",  "readonly", "emptyvalue"};

constexpr std::string_view Whitespace = " \t\r\n\f\v";

/// Strips surrounding ASCII whitespace (handles trailing `\r` on CRLF lines).
std::string_view trim(std::string_view text) {
  const std::size_t first = text.find_first_not_of(Whitespace);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(Whitespace);
  return text.substr(first, last - first + 1);
}

} // namespace

Lexicon Lexicon::parse(std::string_view text) {
  // Scan into views over `text`; copy the winners into the Lexicon below so it
  // owns its strings independently of the input's lifetime.
  std::unordered_map<std::string_view, std::string_view> entries;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t newline = text.find('\n', pos);
    const std::size_t end = (newline == std::string_view::npos) ? text.size() : newline;
    const std::string_view line = trim(text.substr(pos, end - pos));
    pos = end + 1;

    if (line.empty() || line.front() == '#') {
      continue;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string_view::npos) {
      continue;
    }
    const std::string_view key = trim(line.substr(0, equals));
    if (key.empty()) {
      continue;
    }
    entries[key] = trim(line.substr(equals + 1)); // later keys override earlier
  }

  Lexicon lexicon;
  for (std::size_t i = 0; i < RoleSuffix.size(); ++i) {
    const std::string key = "role." + std::string(RoleSuffix.at(i));
    if (const auto found = entries.find(key); found != entries.end()) {
      lexicon.roleWords_.at(i) = std::string(found->second);
    }
  }
  for (std::size_t i = 0; i < StateSuffix.size(); ++i) {
    const std::string key = "state." + std::string(StateSuffix.at(i));
    if (const auto found = entries.find(key); found != entries.end()) {
      lexicon.stateWords_.at(i) = std::string(found->second);
    }
  }
  return lexicon;
}

std::string_view Lexicon::role(vox::model::Role role) const {
  if (role == vox::model::Role::Unknown) {
    return {}; // contract: an unknown role is never spoken, whatever the table holds
  }
  const auto index = static_cast<std::size_t>(std::to_underlying(role));
  if (index >= roleWords_.size()) {
    return {};
  }
  return roleWords_.at(index);
}

std::string_view Lexicon::state(StateConcept stateConcept) const {
  const auto index = static_cast<std::size_t>(std::to_underlying(stateConcept));
  if (index >= stateWords_.size()) {
    return {};
  }
  return stateWords_.at(index);
}

std::vector<std::string> Lexicon::missingRequiredKeys() const {
  std::vector<std::string> missing;
  for (std::size_t i = 0; i < roleWords_.size(); ++i) {
    if (i == static_cast<std::size_t>(std::to_underlying(vox::model::Role::Unknown))) {
      continue; // an unknown role is announced as nothing — not required
    }
    if (roleWords_.at(i).empty()) {
      missing.push_back("role." + std::string(RoleSuffix.at(i)));
    }
  }
  for (std::size_t i = 0; i < stateWords_.size(); ++i) {
    if (stateWords_.at(i).empty()) {
      missing.push_back("state." + std::string(StateSuffix.at(i)));
    }
  }
  return missing;
}

} // namespace vox::german
