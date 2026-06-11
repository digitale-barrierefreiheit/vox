// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The shared announce scenario for the TTFA benchmarks (#41).
///
/// Both benchmarks announce the same short, uncached utterance — a focused
/// "Speichern" button — through the real OutputManager with the embedded
/// German lexicon, so their numbers describe the same user-visible moment.
#ifndef VOX_BENCHMARKS_ANNOUNCE_FIXTURE_HPP
#define VOX_BENCHMARKS_ANNOUNCE_FIXTURE_HPP

#include <optional>

#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/output/output_manager.hpp>

namespace vox::bench {

/// The production announcement composer over the embedded German lexicon.
inline vox::output::OutputManager makeOutput() {
  return vox::output::OutputManager(
      vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData));
}

/// The focused element both benchmarks announce: a short, uncached utterance
/// ("Schaltfläche, Speichern").
inline vox::model::AccessibleNode savedButton() {
  return {
      .role = vox::model::Role::Button, .name = "Speichern", .states = {}, .value = std::nullopt};
}

} // namespace vox::bench

#endif // VOX_BENCHMARKS_ANNOUNCE_FIXTURE_HPP
