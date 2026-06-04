// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The inspectable speech artifact produced by the Output Manager.
///
/// `Utterance` is the would-be-spoken text exposed *before* TTS (ADR-12, §8.6.1)
/// so behaviour is asserted on text, not audio. The `Priority`/`Source` metadata
/// is carried for the future priority queue and interruption logic (a later
/// milestone); v0 fills in sensible defaults.
#ifndef VOX_OUTPUT_UTTERANCE_HPP
#define VOX_OUTPUT_UTTERANCE_HPP

#include <cstdint>
#include <string>

namespace vox::output {

/// Relative urgency of an utterance in the (future) output queue.
enum class Priority : std::uint8_t {
  Low,    ///< Background chatter, interruptible by anything.
  Normal, ///< Default: ordinary focus/navigation announcements.
  High,   ///< Urgent (e.g. alerts) — jumps the queue.
};

/// What caused an utterance, so the queue can apply source-specific policy.
enum class Source : std::uint8_t {
  Unknown,     ///< Unspecified origin.
  FocusChange, ///< The focused control changed (the MVP path).
};

/// The would-be-spoken text plus minimal routing metadata.
struct Utterance {
  std::string text;                    ///< Rendered announcement (UTF-8).
  Priority priority{Priority::Normal}; ///< Queue urgency.
  Source source{Source::FocusChange};  ///< What produced it.

  /// Value equality across every field — for golden-text test assertions.
  [[nodiscard]] friend bool operator==(const Utterance&, const Utterance&) = default;
};

} // namespace vox::output

#endif // VOX_OUTPUT_UTTERANCE_HPP
