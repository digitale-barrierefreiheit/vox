// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Pure, policy-driven choice of a TTS voice from the available set.
///
/// Voice selection is the one piece of the SAPI backend with real branching, so
/// it lives here as a pure function over plain descriptors — unit-tested without
/// any installed voice or COM. The Windows backend enumerates real SAPI voices
/// into `VoiceDescriptor`s and applies this; ADR-07 wants German, but the MVP
/// must still speak on an English-only machine, hence the two policies.
#ifndef VOX_TTS_VOICE_SELECTION_HPP
#define VOX_TTS_VOICE_SELECTION_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace vox::tts {

/// A backend-neutral description of one installed voice.
struct VoiceDescriptor {
  std::string id;        ///< Opaque backend identifier (e.g. a SAPI token id).
  std::string name;      ///< Human-readable voice name (diagnostics only).
  bool isGerman{false};  ///< True if the voice's primary language is German.
  bool isDefault{false}; ///< True if it is the system default voice.

  /// Two descriptors are equal iff every field matches.
  [[nodiscard]] friend bool operator==(const VoiceDescriptor&, const VoiceDescriptor&) = default;
};

/// How hard to insist on a German voice.
enum class VoiceSelectionPolicy : std::uint8_t {
  RequireGerman, ///< Only a German voice is acceptable; otherwise none is chosen.
  PreferGerman,  ///< Prefer German, but fall back to another voice if needed.
};

/// The outcome of selecting a voice.
struct SelectedVoice {
  std::string id;       ///< The chosen voice's @ref VoiceDescriptor::id.
  bool isGerman{false}; ///< True if the chosen voice is German (false = fallback).

  /// Two selections are equal iff both fields match.
  [[nodiscard]] friend bool operator==(const SelectedVoice&, const SelectedVoice&) = default;
};

/// @brief Chooses a voice from @p available according to @p policy.
///
/// Prefers the first German voice. Under `PreferGerman`, if none is German it
/// falls back to the system-default voice, or the first available one. Under
/// `RequireGerman`, a non-German set yields no selection.
///
/// @return The chosen voice, or `std::nullopt` when none is acceptable (an empty
///         set, or `RequireGerman` with no German voice).
[[nodiscard]] std::optional<SelectedVoice> selectVoice(std::span<const VoiceDescriptor> available,
                                                       VoiceSelectionPolicy policy);

} // namespace vox::tts

#endif // VOX_TTS_VOICE_SELECTION_HPP
