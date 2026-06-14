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

/// Trimmed `key = value` views over the source text, keyed by trimmed key.
using EntryMap = std::unordered_map<std::string_view, std::string_view>;

/// Strips surrounding ASCII whitespace (handles trailing `\r` on CRLF lines).
std::string_view trim(std::string_view text) {
  const std::size_t first = text.find_first_not_of(Whitespace);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(Whitespace);
  return text.substr(first, last - first + 1);
}

/// Records one trimmed source @p line into @p entries: skips blank lines,
/// comments, lines without `=`, and lines with an empty key; later keys win.
void recordLine(EntryMap& entries, std::string_view line) {
  if (line.empty() || line.front() == '#') {
    return;
  }
  const std::size_t equals = line.find('=');
  if (equals == std::string_view::npos) {
    return;
  }
  const std::string_view key = trim(line.substr(0, equals));
  if (key.empty()) {
    return;
  }
  entries[key] = trim(line.substr(equals + 1)); // later keys override earlier
}

/// Splits @p text into newline-delimited lines and records each (views over
/// `text`; the winners are copied into the Lexicon, which then owns them).
EntryMap collectEntries(std::string_view text) {
  EntryMap entries;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t newline = text.find('\n', pos);
    const std::size_t end = (newline == std::string_view::npos) ? text.size() : newline;
    recordLine(entries, trim(text.substr(pos, end - pos)));
    pos = end + 1;
  }
  return entries;
}

/// Copies the value of `<prefix><suffix>` from @p entries into @p target,
/// leaving it untouched when the key is absent. Used for the `role.*` and
/// `state.*` tables, which are indexed in lockstep with their suffix arrays.
template<std::size_t N>
void fillWords(std::array<std::string, N>& target, std::string_view prefix,
               const std::array<std::string_view, N>& suffixes, const EntryMap& entries) {
  for (std::size_t i = 0; i < suffixes.size(); ++i) {
    const std::string key = std::string(prefix) + std::string(suffixes.at(i));
    if (const auto found = entries.find(key); found != entries.end()) {
      target.at(i) = std::string(found->second);
    }
  }
}

} // namespace

Lexicon Lexicon::parse(std::string_view text) {
  const EntryMap entries = collectEntries(text);

  Lexicon lexicon;
  if (const auto found = entries.find("language"); found != entries.end()) {
    lexicon.language_ = std::string(found->second);
  }
  fillWords(lexicon.roleWords_, "role.", RoleSuffix, entries);
  fillWords(lexicon.stateWords_, "state.", StateSuffix, entries);
  return lexicon;
}

std::string_view Lexicon::language() const {
  return language_;
}

std::string_view Lexicon::role(vox::model::Role role) const {
  if (role == vox::model::Role::Unknown) {
    return {}; // contract: an unknown role is never spoken, whatever the table holds
  }
  const auto index = static_cast<std::size_t>(std::to_underlying(role));
  if (index >= roleWords_.size()) {
    return {}; // LCOV_EXCL_LINE — defensive out-of-range guard (see lexicon_test.cpp)
  }
  return roleWords_.at(index);
}

std::string_view Lexicon::state(StateConcept stateConcept) const {
  const auto index = static_cast<std::size_t>(std::to_underlying(stateConcept));
  if (index >= stateWords_.size()) {
    return {}; // LCOV_EXCL_LINE — defensive out-of-range guard (see lexicon_test.cpp)
  }
  return stateWords_.at(index);
}

namespace {

/// The role index whose key is optional: an unknown role is announced as
/// nothing, so a missing `role.unknown` is never a gap.
constexpr std::size_t UnknownRoleIndex =
    static_cast<std::size_t>(std::to_underlying(vox::model::Role::Unknown));

/// Appends `role.*` keys whose word is empty (skipping the optional unknown
/// role) to @p missing, preserving table order.
void collectMissingRoles(const std::array<std::string, 10>& roleWords,
                         std::vector<std::string>& missing) {
  for (std::size_t i = 0; i < roleWords.size(); ++i) {
    if (i != UnknownRoleIndex && roleWords.at(i).empty()) {
      missing.push_back("role." + std::string(RoleSuffix.at(i)));
    }
  }
}

/// Appends `state.*` keys whose word is empty to @p missing, preserving order.
void collectMissingStates(const std::array<std::string, StateConceptCount>& stateWords,
                          std::vector<std::string>& missing) {
  for (std::size_t i = 0; i < stateWords.size(); ++i) {
    if (stateWords.at(i).empty()) {
      missing.push_back("state." + std::string(StateSuffix.at(i)));
    }
  }
}

} // namespace

std::vector<std::string> Lexicon::missingRequiredKeys() const {
  std::vector<std::string> missing;
  collectMissingRoles(roleWords_, missing);
  collectMissingStates(stateWords_, missing);
  return missing;
}

} // namespace vox::german
