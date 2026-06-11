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

using vox::tts::mergeVoices;
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

// --- mergeVoices (#52): classic (primary) + OneCore (secondary) discovery ----

VoiceDescriptor namedVoice(std::string id, std::string name, bool isDefault = false) {
  VoiceDescriptor descriptor;
  descriptor.id = std::move(id);
  descriptor.name = std::move(name);
  descriptor.isDefault = isDefault;
  return descriptor;
}

TEST(MergeVoices, AppendsSecondaryVoicesAfterThePrimaryOnes) {
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({namedVoice("classic-1", "Microsoft Hedda Desktop")},
                  std::vector{namedVoice("onecore-1", "Microsoft Katja")});
  EXPECT_EQ(merged, (std::vector{namedVoice("classic-1", "Microsoft Hedda Desktop"),
                                 namedVoice("onecore-1", "Microsoft Katja")}));
}

TEST(MergeVoices, DropsASecondaryVoiceWhoseNameIsAlreadyKnown) {
  // The registry-bridge case: the same voice visible in both hives under
  // different token ids — the classic entry wins, the OneCore one is dropped.
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({namedVoice("classic-hedda", "Microsoft Hedda")},
                  std::vector{namedVoice("onecore-hedda", "Microsoft Hedda")});
  EXPECT_EQ(merged, (std::vector{namedVoice("classic-hedda", "Microsoft Hedda")}));
}

TEST(MergeVoices, KeepsDistinctVariantsOfTheSameVoiceFamily) {
  // "Desktop" and OneCore variants are different tokens with different names
  // (and engines) — both stay selectable.
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({namedVoice("classic-hedda", "Microsoft Hedda Desktop")},
                  std::vector{namedVoice("onecore-hedda", "Microsoft Hedda")});
  EXPECT_EQ(merged.size(), 2U);
}

TEST(MergeVoices, AddsANameRepeatedWithinSecondaryOnlyOnce) {
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({}, std::vector{namedVoice("onecore-1", "Microsoft Katja"),
                                  namedVoice("onecore-2", "Microsoft Katja")});
  EXPECT_EQ(merged, (std::vector{namedVoice("onecore-1", "Microsoft Katja")}));
}

TEST(MergeVoices, NeverCollapsesUnnamedVoices) {
  // An empty name identifies nothing; two unnamed voices are distinct.
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({namedVoice("classic-1", "")}, std::vector{namedVoice("onecore-1", "")});
  EXPECT_EQ(merged.size(), 2U);
}

TEST(MergeVoices, ClearsTheSecondaryDefaultWhenThePrimaryHasOne) {
  // Each category reports its own default; the classic one is the system
  // default the user actually set, so it must stay the only default.
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({namedVoice("classic-1", "Microsoft Hedda Desktop", true)},
                  std::vector{namedVoice("onecore-1", "Microsoft Katja", true)});
  EXPECT_EQ(merged, (std::vector{namedVoice("classic-1", "Microsoft Hedda Desktop", true),
                                 namedVoice("onecore-1", "Microsoft Katja", false)}));
}

TEST(MergeVoices, KeepsTheSecondaryDefaultWhenThePrimaryHasNone) {
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({namedVoice("classic-1", "Microsoft Hedda Desktop")},
                  std::vector{namedVoice("onecore-1", "Microsoft Katja", true)});
  EXPECT_EQ(merged, (std::vector{namedVoice("classic-1", "Microsoft Hedda Desktop"),
                                 namedVoice("onecore-1", "Microsoft Katja", true)}));
}

TEST(MergeVoices, EmptyPrimaryYieldsTheSecondaryUnchanged) {
  // E.g. the classic hive missing or empty: OneCore alone must work.
  const std::vector<VoiceDescriptor> merged =
      mergeVoices({}, std::vector{namedVoice("onecore-1", "Microsoft Katja", true)});
  EXPECT_EQ(merged, (std::vector{namedVoice("onecore-1", "Microsoft Katja", true)}));
}

} // namespace
