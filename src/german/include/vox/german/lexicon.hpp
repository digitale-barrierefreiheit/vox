// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief German announcement vocabulary: role and state words (ADR-07, §5.2).
///
/// The `Lexicon` maps a provider-independent control role (#32) and an
/// announceable state concept onto the word a screen-reader user hears.
/// It is pure and OS-independent: it is built by parsing a `key = value` text
/// table (`Lexicon::parse`), never by touching the filesystem — loading a
/// `.lex` file from disk is the app's job (#61). A table declares the language
/// it stands for (`language = <BCP-47 tag>`), so users and contributors can
/// supply files for languages beyond the canonical German `de.lex` (#34). The
/// composition of a full utterance from a node is the Output Manager's job
/// (#33); this layer only supplies words.
#ifndef VOX_GERMAN_LEXICON_HPP
#define VOX_GERMAN_LEXICON_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <vox/model/role.hpp>

namespace vox::german {

/// An announceable property of a control. Unlike `vox::model::State`, these are
/// the *spoken* concepts: they include cases that are the absence of a flag
/// (`Unchecked`, `Collapsed`) or live outside the state set (`EmptyValue`), so
/// the Output Manager (#33) can ask for exactly the right word.
enum class StateConcept : std::uint8_t {
  Checked,    ///< Toggle is on.
  Unchecked,  ///< Toggle is off.
  Mixed,      ///< Toggle is indeterminate.
  Expanded,   ///< Expandable and open.
  Collapsed,  ///< Expandable and closed.
  Selected,   ///< Selected within its container.
  Disabled,   ///< Present but not operable.
  ReadOnly,   ///< Value shown but not editable.
  EmptyValue, ///< Editable control whose value is empty.
};

/// Number of `StateConcept` values; used to size the lookup table.
inline constexpr std::size_t StateConceptCount = 9;

/// Announcement vocabulary, built from a `key = value` table (German by
/// default; the table's `language` declaration says what it holds, #61).
///
/// Lookups are O(1) and return a view into storage that stays valid for the
/// Lexicon's lifetime. A missing entry yields an empty view (the caller skips
/// it); `role(Role::Unknown)` is *always* empty — the contract is enforced in
/// code, so a stray `role.unknown` entry can never cause it to be spoken.
class Lexicon {
public:
  /// @brief Parses a `key = value` table (UTF-8) into a Lexicon.
  ///
  /// Blank lines and lines beginning with `#` (after leading spaces) are
  /// ignored; a line with no `=` is skipped. Keys and values are trimmed of
  /// surrounding ASCII whitespace. Later keys override earlier ones. Recognized
  /// keys are `language` (the BCP-47 tag the table stands for, e.g. `de`),
  /// `role.<role>` (e.g. `role.button`), and `state.<concept>` (e.g.
  /// `state.checked`); unrecognized keys are ignored. Never reads the
  /// filesystem; its only failure mode is allocation (`std::bad_alloc`).
  [[nodiscard]] static Lexicon parse(std::string_view text);

  /// @brief The language tag the table declared (`language = …`), or empty if
  ///        it declared none. Not normalized; matching is the loader's job.
  [[nodiscard]] std::string_view language() const;

  /// @brief The announcement word for a control @p role (in the table's
  ///        declared language), or empty if none.
  [[nodiscard]] std::string_view role(vox::model::Role role) const;

  /// @brief The announcement word for a state @p stateConcept (in the table's
  ///        declared language), or empty if none.
  [[nodiscard]] std::string_view state(StateConcept stateConcept) const;

  /// @brief Required `role.*` / `state.*` keys that are missing or empty.
  ///
  /// "Required" excludes `role.unknown` (intentionally unspoken). A populated
  /// list means the table is incomplete; used by tests to guard `de.lex`.
  [[nodiscard]] std::vector<std::string> missingRequiredKeys() const;

private:
  std::string language_;                                  ///< Declared BCP-47 tag, or empty.
  std::array<std::string, 10> roleWords_;                 ///< Indexed by vox::model::Role.
  std::array<std::string, StateConceptCount> stateWords_; ///< By StateConcept.
};

} // namespace vox::german

#endif // VOX_GERMAN_LEXICON_HPP
