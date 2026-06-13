// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::tts voice selection (#52 merge, #88 request).
#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vox/tts/voice_selection.hpp>

namespace vox::tts {

namespace {

char toLowerAscii(char letter) {
  return (letter >= 'A' && letter <= 'Z') ? static_cast<char>(letter - 'A' + 'a') : letter;
}

bool equalsIgnoreCaseAscii(std::string_view left, std::string_view right) {
  return std::ranges::equal(left, right,
                            [](char a, char b) { return toLowerAscii(a) == toLowerAscii(b); });
}

/// True if @p voice speaks the requested @p language (primary subtags match).
bool speaksLanguage(const VoiceDescriptor& voice, std::string_view language) {
  return !voice.language.empty() && sameLanguage(voice.language, language);
}

const VoiceDescriptor* findByName(std::span<const VoiceDescriptor> available,
                                  std::string_view name) {
  for (const VoiceDescriptor& voice : available) {
    if (equalsIgnoreCaseAscii(voice.name, name)) {
      return &voice;
    }
  }
  return nullptr;
}

const VoiceDescriptor* findByLanguage(std::span<const VoiceDescriptor> available,
                                      std::string_view language) {
  for (const VoiceDescriptor& voice : available) {
    if (speaksLanguage(voice, language)) {
      return &voice;
    }
  }
  return nullptr;
}

const VoiceDescriptor* findFallback(std::span<const VoiceDescriptor> available) {
  for (const VoiceDescriptor& voice : available) {
    if (voice.isDefault) {
      return &voice;
    }
  }
  return available.empty() ? nullptr : &available.front();
}

SelectedVoice selected(const VoiceDescriptor& voice, VoiceChoice choice) {
  return SelectedVoice{voice.id, voice.name, voice.language, choice};
}

/// The voice the request *prefers*, before any fallback: the explicit VOX_VOICE
/// name when set, otherwise the requested language. An explicit name that is
/// set but missing yields nullopt without trying the language — a broken
/// override must not re-enable what it replaced — so the caller goes straight
/// to the fallback either way nullopt is returned.
std::optional<SelectedVoice> preferredVoice(std::span<const VoiceDescriptor> available,
                                            const VoiceSelectionRequest& request) {
  if (!request.explicitVoice.empty()) {
    const VoiceDescriptor* byName = findByName(available, request.explicitVoice);
    return byName == nullptr ? std::nullopt
                             : std::optional(selected(*byName, VoiceChoice::ExplicitName));
  }
  const VoiceDescriptor* byLanguage = findByLanguage(available, request.language);
  return byLanguage == nullptr
             ? std::nullopt
             : std::optional(selected(*byLanguage, VoiceChoice::RequestedLanguage));
}

} // namespace

std::string_view primarySubtag(std::string_view tag) {
  return tag.substr(0, tag.find('-'));
}

bool sameLanguage(std::string_view left, std::string_view right) {
  return equalsIgnoreCaseAscii(primarySubtag(left), primarySubtag(right));
}

std::string_view languageTagFromLangId(unsigned long langId) {
  // Windows primary-language identifier (LANGID & 0x3FF) → BCP-47 primary
  // subtag, for the languages SAPI voices realistically ship in. Unmapped
  // languages yield an empty tag (the voice then never matches a request).
  constexpr std::array<std::pair<unsigned long, std::string_view>, 23> Map{{
      {0x01UL, "ar"}, {0x04UL, "zh"}, {0x05UL, "cs"}, {0x06UL, "da"}, {0x07UL, "de"},
      {0x08UL, "el"}, {0x09UL, "en"}, {0x0AUL, "es"}, {0x0BUL, "fi"}, {0x0CUL, "fr"},
      {0x0EUL, "hu"}, {0x10UL, "it"}, {0x11UL, "ja"}, {0x12UL, "ko"}, {0x13UL, "nl"},
      {0x14UL, "no"}, {0x15UL, "pl"}, {0x16UL, "pt"}, {0x18UL, "ro"}, {0x19UL, "ru"},
      {0x1DUL, "sv"}, {0x1FUL, "tr"}, {0x22UL, "uk"},
  }};
  const unsigned long primary = langId & 0x3FFUL;
  for (const auto& [id, tag] : Map) {
    if (id == primary) {
      return tag;
    }
  }
  return {};
}

std::vector<VoiceDescriptor> mergeVoices(std::vector<VoiceDescriptor> primary,
                                         std::span<const VoiceDescriptor> secondary) {
  const bool primaryHasDefault =
      std::ranges::any_of(primary, [](const VoiceDescriptor& voice) { return voice.isDefault; });
  for (const VoiceDescriptor& candidate : secondary) {
    // Only a non-empty name identifies a duplicate; unnamed voices are never
    // collapsed into each other. Checked against the result so far, so a name
    // repeated within @p secondary itself is also added only once.
    if (const bool duplicate = !candidate.name.empty() &&
                               std::ranges::any_of(primary,
                                                   [&candidate](const VoiceDescriptor& kept) {
                                                     return kept.name == candidate.name;
                                                   });
        duplicate) {
      continue;
    }
    VoiceDescriptor added = candidate;
    if (primaryHasDefault) {
      added.isDefault = false; // the classic default stays the system default
    }
    primary.push_back(std::move(added));
  }
  return primary;
}

std::optional<SelectedVoice> selectVoice(std::span<const VoiceDescriptor> available,
                                         const VoiceSelectionRequest& request) {
  if (std::optional<SelectedVoice> preferred = preferredVoice(available, request)) {
    return preferred;
  }
  if (request.required) {
    return std::nullopt; // the gate: a missing match must fail, not degrade
  }
  const VoiceDescriptor* fallback = findFallback(available);
  return fallback == nullptr ? std::nullopt
                             : std::optional(selected(*fallback, VoiceChoice::Fallback));
}

} // namespace vox::tts
