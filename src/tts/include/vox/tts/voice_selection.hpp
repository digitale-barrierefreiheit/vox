// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Pure, request-driven choice of a TTS voice from the available set.
///
/// Voice selection is the one piece of the SAPI backend with real branching, so
/// it lives here as a pure function over plain descriptors — unit-tested without
/// any installed voice or COM. The Windows backend enumerates real SAPI voices
/// into `VoiceDescriptor`s and applies this. Since #88 the selection is driven
/// by a requested language (`VOX_LANGUAGE`, default German per ADR-07) shared
/// with the lexicon, plus an optional explicit voice (`VOX_VOICE`) that wins
/// over the language preference. The MVP must still speak on a machine without
/// a matching voice, hence the unchanged fallback chain; the caller reads the
/// outcome's provenance (@ref VoiceChoice) to report fallbacks — this module
/// does no I/O.
#ifndef VOX_TTS_VOICE_SELECTION_HPP
#define VOX_TTS_VOICE_SELECTION_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vox::tts {

/// A backend-neutral description of one installed voice.
struct VoiceDescriptor {
  std::string id;        ///< Opaque backend identifier (e.g. a SAPI token id).
  std::string name;      ///< Human-readable voice name (diagnostics, VOX_VOICE).
  std::string language;  ///< BCP-47 primary subtag ("de", "en", …); empty = unknown.
  bool isDefault{false}; ///< True if it is the system default voice.

  /// Two descriptors are equal iff every field matches.
  [[nodiscard]] friend bool operator==(const VoiceDescriptor&, const VoiceDescriptor&) = default;
};

/// What the caller asks of @ref selectVoice (#88).
struct VoiceSelectionRequest {
  std::string language{"de"}; ///< Requested language tag (`VOX_LANGUAGE`; ADR-07 default).
  std::string explicitVoice;  ///< Voice name to use verbatim (`VOX_VOICE`), or empty.
  bool required{false};       ///< True: no voice in the requested language means no selection.
};

/// How the selected voice was chosen — the caller's signal for fallback warnings.
enum class VoiceChoice : std::uint8_t {
  ExplicitName,      ///< The requested `VOX_VOICE` name matched.
  RequestedLanguage, ///< A voice in the requested language.
  Fallback,          ///< The system default voice (or the first available one).
};

/// The outcome of selecting a voice.
struct SelectedVoice {
  std::string id;                            ///< The chosen voice's @ref VoiceDescriptor::id.
  std::string name;                          ///< The chosen voice's name (for diagnostics).
  std::string language;                      ///< The chosen voice's primary subtag (may be empty).
  VoiceChoice choice{VoiceChoice::Fallback}; ///< Provenance.

  /// Two selections are equal iff every field matches.
  [[nodiscard]] friend bool operator==(const SelectedVoice&, const SelectedVoice&) = default;
};

/// @brief The primary subtag of a BCP-47 @p tag ("de-AT" → "de"); the whole
///        @p tag when it has no subtags.
[[nodiscard]] std::string_view primarySubtag(std::string_view tag);

/// @brief The BCP-47 primary subtag for a Windows LANGID / LCID @p langId
///        (e.g. 0x407 → "de", 0x409 → "en"), or empty when unmapped. Only the
///        primary-language bits are considered, so every regional variant of a
///        mapped language resolves to the same subtag.
[[nodiscard]] std::string_view languageTagFromLangId(unsigned long langId);

/// @brief Merges two discovery passes into one selectable voice list (#52).
///
/// @p primary (the classic SAPI5 catalogue) wins: a @p secondary (OneCore)
/// voice whose non-empty name already appears in the result is dropped. The
/// observable duplicate is the same voice registered in both hives (e.g. by a
/// OneCore→SAPI registry bridge); token *ids* differ across hives even then,
/// so the name is the identity. Distinct variants ("Microsoft Hedda Desktop"
/// vs "Microsoft Hedda") have distinct names and are both kept. When
/// @p primary contains a default voice, default flags on surviving
/// @p secondary entries are cleared — "the system default" stays the classic
/// one the user actually set.
[[nodiscard]] std::vector<VoiceDescriptor> mergeVoices(std::vector<VoiceDescriptor> primary,
                                                       std::span<const VoiceDescriptor> secondary);

/// @brief Chooses a voice from @p available according to @p request (#88).
///
/// Precedence: a non-empty `request.explicitVoice` is authoritative — the
/// first voice with that name (ASCII case-insensitive) is chosen; when no such
/// voice exists, the language preference is *skipped* (a broken override must
/// not silently re-enable what it replaced) and the fallback chain applies
/// directly. Otherwise the first voice matching `request.language` (compared
/// by primary subtag, case-insensitive) is chosen. Without a match,
/// `request.required` yields no selection (the CI gate); otherwise the
/// system-default voice, or the first available one, is the fallback.
///
/// @return The chosen voice with its provenance, or `std::nullopt` when none
///         is acceptable (an empty set, or `required` without a match).
[[nodiscard]] std::optional<SelectedVoice> selectVoice(std::span<const VoiceDescriptor> available,
                                                       const VoiceSelectionRequest& request);

} // namespace vox::tts

#endif // VOX_TTS_VOICE_SELECTION_HPP
