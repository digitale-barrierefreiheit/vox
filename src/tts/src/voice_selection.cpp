// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::tts::selectVoice and vox::tts::mergeVoices.
#include <algorithm>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <vox/tts/voice_selection.hpp>

namespace vox::tts {

namespace {

const VoiceDescriptor* findGerman(std::span<const VoiceDescriptor> available) {
  for (const VoiceDescriptor& voice : available) {
    if (voice.isGerman) {
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

} // namespace

std::vector<VoiceDescriptor> mergeVoices(std::vector<VoiceDescriptor> primary,
                                         std::span<const VoiceDescriptor> secondary) {
  const bool primaryHasDefault =
      std::ranges::any_of(primary, [](const VoiceDescriptor& voice) { return voice.isDefault; });
  for (const VoiceDescriptor& candidate : secondary) {
    // Only a non-empty name identifies a duplicate; unnamed voices are never
    // collapsed into each other. Checked against the result so far, so a name
    // repeated within @p secondary itself is also added only once.
    const bool duplicate = !candidate.name.empty() &&
                           std::ranges::any_of(primary, [&candidate](const VoiceDescriptor& kept) {
                             return kept.name == candidate.name;
                           });
    if (duplicate) {
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
                                         VoiceSelectionPolicy policy) {
  if (const VoiceDescriptor* german = findGerman(available); german != nullptr) {
    return SelectedVoice{german->id, true};
  }
  if (policy == VoiceSelectionPolicy::RequireGerman) {
    return std::nullopt;
  }
  if (const VoiceDescriptor* fallback = findFallback(available); fallback != nullptr) {
    return SelectedVoice{fallback->id, false};
  }
  return std::nullopt;
}

} // namespace vox::tts
