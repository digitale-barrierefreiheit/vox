// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::tts::selectVoice — the require/prefer German policies.
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <vox/tts/voice_selection.hpp>

namespace {

using vox::tts::SelectedVoice;
using vox::tts::selectVoice;
using vox::tts::VoiceDescriptor;
using vox::tts::VoiceSelectionPolicy;

VoiceDescriptor voice(std::string id, bool isGerman, bool isDefault) {
  VoiceDescriptor descriptor;
  descriptor.id = std::move(id);
  descriptor.isGerman = isGerman;
  descriptor.isDefault = isDefault;
  return descriptor;
}

// Comparing the whole optional (rather than dereferencing after has_value())
// keeps the analyzer happy about optional access and reads as one assertion.

TEST(SelectVoice, PicksTheGermanVoiceWhenPresent) {
  const std::vector<VoiceDescriptor> available{
      voice("en-US", false, true),
      voice("de-DE", true, false),
  };
  EXPECT_EQ(selectVoice(available, VoiceSelectionPolicy::PreferGerman),
            std::optional(SelectedVoice{"de-DE", true}));
}

TEST(SelectVoice, RequireGermanPicksGermanOverTheDefault) {
  const std::vector<VoiceDescriptor> available{
      voice("en-US", false, true),
      voice("de-DE", true, false),
  };
  // Even under RequireGerman the German voice is chosen regardless of default.
  EXPECT_EQ(selectVoice(available, VoiceSelectionPolicy::RequireGerman),
            std::optional(SelectedVoice{"de-DE", true}));
}

TEST(SelectVoice, PicksTheFirstGermanVoiceAmongSeveral) {
  const std::vector<VoiceDescriptor> available{
      voice("de-AT", true, false),
      voice("de-DE", true, true),
  };
  EXPECT_EQ(selectVoice(available, VoiceSelectionPolicy::PreferGerman),
            std::optional(SelectedVoice{"de-AT", true}));
}

TEST(SelectVoice, RequireGermanYieldsNothingWithoutAGermanVoice) {
  const std::vector<VoiceDescriptor> available{
      voice("en-US", false, true),
      voice("fr-FR", false, false),
  };
  EXPECT_EQ(selectVoice(available, VoiceSelectionPolicy::RequireGerman), std::nullopt);
}

TEST(SelectVoice, PreferGermanFallsBackToTheDefaultVoice) {
  const std::vector<VoiceDescriptor> available{
      voice("fr-FR", false, false),
      voice("en-US", false, true),
  };
  EXPECT_EQ(selectVoice(available, VoiceSelectionPolicy::PreferGerman),
            std::optional(SelectedVoice{"en-US", false})); // the default
}

TEST(SelectVoice, PreferGermanFallsBackToTheFirstVoiceWhenNoneIsDefault) {
  const std::vector<VoiceDescriptor> available{
      voice("fr-FR", false, false),
      voice("en-US", false, false),
  };
  EXPECT_EQ(selectVoice(available, VoiceSelectionPolicy::PreferGerman),
            std::optional(SelectedVoice{"fr-FR", false}));
}

TEST(SelectVoice, EmptySetYieldsNothingUnderEitherPolicy) {
  const std::vector<VoiceDescriptor> none;
  EXPECT_EQ(selectVoice(none, VoiceSelectionPolicy::PreferGerman), std::nullopt);
  EXPECT_EQ(selectVoice(none, VoiceSelectionPolicy::RequireGerman), std::nullopt);
}

} // namespace
