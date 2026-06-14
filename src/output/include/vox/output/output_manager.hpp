// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Output Manager v0: turns an AccessibleNode into an Utterance.
///
/// This is the testability seam of the whole MVP (ADR-12, §8.6.1): it composes
/// the provider-independent node (#32) and the German lexicon (#34) into the
/// would-be-spoken text, with no TTS or audio. Composition order is
/// role, name, states, value (see announce()). Pure and OS-independent.
#ifndef VOX_OUTPUT_OUTPUT_MANAGER_HPP
#define VOX_OUTPUT_OUTPUT_MANAGER_HPP

#include <vox/german/lexicon.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/output/utterance.hpp>

namespace vox::output {

/// Composes announcements from accessibility nodes using a German lexicon.
///
/// v0 has no priority queue or interruption — it is a pure function object over
/// the lexicon it is constructed with.
class OutputManager {
public:
  /// Constructs a manager that speaks using @p lexicon (taken by value).
  explicit OutputManager(vox::german::Lexicon lexicon);

  /// @brief Renders @p node as an Utterance.
  ///
  /// Order: German role word, accessible name, state words, then value —
  /// joined by ", ". Empty parts are skipped; name and value are passed through
  /// `vox::german::normalizeName`. States: the toggle (checked/indeterminate/
  /// unchecked, for checkbox & radio), expanded/collapsed, selected, read-only,
  /// disabled. Value: "leer" when empty, otherwise the content; absent when the
  /// node has no value.
  [[nodiscard]] Utterance announce(const vox::model::AccessibleNode& node) const;

private:
  vox::german::Lexicon lexicon_;
};

} // namespace vox::output

#endif // VOX_OUTPUT_OUTPUT_MANAGER_HPP
