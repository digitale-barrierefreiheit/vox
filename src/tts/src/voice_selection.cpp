// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::tts::selectVoice.
#include <optional>
#include <span>

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
