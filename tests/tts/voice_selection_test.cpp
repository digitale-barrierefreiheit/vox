// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::tts voice selection: the request-driven choice (#88) —
///        requested language, explicit VOX_VOICE name, fallback provenance —
///        plus the LANGID→tag mapping and the OneCore merge (#52).
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <vox/tts/voice_selection.hpp>

namespace {

using vox::tts::languageTagFromLangId;
using vox::tts::mergeVoices;
using vox::tts::primarySubtag;
using vox::tts::SelectedVoice;
using vox::tts::selectVoice;
using vox::tts::VoiceChoice;
using vox::tts::VoiceDescriptor;
using vox::tts::VoiceSelectionRequest;

VoiceDescriptor voice(std::string id, std::string language, bool isDefault = false) {
  VoiceDescriptor descriptor;
  descriptor.id = std::move(id);
  descriptor.name = "Voice " + descriptor.id;
  descriptor.language = std::move(language);
  descriptor.isDefault = isDefault;
  return descriptor;
}

SelectedVoice chosen(const VoiceDescriptor& descriptor, VoiceChoice choice) {
  return SelectedVoice{descriptor.id, descriptor.name, descriptor.language, choice};
}

VoiceSelectionRequest prefer(std::string language) {
  VoiceSelectionRequest request;
  request.language = std::move(language);
  return request;
}

VoiceSelectionRequest require(std::string language) {
  VoiceSelectionRequest request;
  request.language = std::move(language);
  request.required = true;
  return request;
}

VoiceSelectionRequest byName(std::string name) {
  VoiceSelectionRequest request; // language stays the "de" default
  request.explicitVoice = std::move(name);
  return request;
}

// Comparing the whole optional (rather than dereferencing after has_value())
// keeps the analyzer happy about optional access and reads as one assertion.

TEST(SelectVoice, PicksTheVoiceMatchingTheRequestedLanguage) {
  const std::vector<VoiceDescriptor> available{
      voice("en-1", "en", true),
      voice("de-1", "de"),
  };
  EXPECT_EQ(selectVoice(available, prefer("de")),
            std::optional(chosen(available[1], VoiceChoice::RequestedLanguage)));
}

TEST(SelectVoice, MatchesByPrimarySubtagCaseInsensitively) {
  const std::vector<VoiceDescriptor> available{voice("de-1", "de")};
  // "de-AT" and "DE" both mean the primary language de.
  EXPECT_EQ(selectVoice(available, prefer("de-AT")),
            std::optional(chosen(available[0], VoiceChoice::RequestedLanguage)));
  EXPECT_EQ(selectVoice(available, prefer("DE")),
            std::optional(chosen(available[0], VoiceChoice::RequestedLanguage)));
}

TEST(SelectVoice, PicksTheFirstMatchAmongSeveral) {
  const std::vector<VoiceDescriptor> available{
      voice("de-1", "de"),
      voice("de-2", "de", true),
  };
  EXPECT_EQ(selectVoice(available, require("de")),
            std::optional(chosen(available[0], VoiceChoice::RequestedLanguage)));
}

TEST(SelectVoice, RequiredYieldsNothingWithoutAMatch) {
  const std::vector<VoiceDescriptor> available{
      voice("en-1", "en", true),
      voice("fr-1", "fr"),
  };
  EXPECT_EQ(selectVoice(available, require("de")), std::nullopt);
}

TEST(SelectVoice, AVoiceWithoutAKnownLanguageNeverMatches) {
  const std::vector<VoiceDescriptor> available{voice("odd-1", "", true)};
  EXPECT_EQ(selectVoice(available, prefer("de")),
            std::optional(chosen(available[0], VoiceChoice::Fallback)));
}

TEST(SelectVoice, FallsBackToTheDefaultVoice) {
  const std::vector<VoiceDescriptor> available{
      voice("fr-1", "fr"),
      voice("en-1", "en", true),
  };
  EXPECT_EQ(selectVoice(available, prefer("de")),
            std::optional(chosen(available[1], VoiceChoice::Fallback)));
}

TEST(SelectVoice, FallsBackToTheFirstVoiceWhenNoneIsDefault) {
  const std::vector<VoiceDescriptor> available{
      voice("fr-1", "fr"),
      voice("en-1", "en"),
  };
  EXPECT_EQ(selectVoice(available, prefer("de")),
            std::optional(chosen(available[0], VoiceChoice::Fallback)));
}

TEST(SelectVoice, EmptySetYieldsNothingUnderAnyRequest) {
  const std::vector<VoiceDescriptor> none;
  EXPECT_EQ(selectVoice(none, prefer("de")), std::nullopt);
  EXPECT_EQ(selectVoice(none, require("de")), std::nullopt);
  EXPECT_EQ(selectVoice(none, byName("Voice x")), std::nullopt);
}

TEST(SelectVoice, AnExplicitVoiceNameWinsOverTheLanguagePreference) {
  const std::vector<VoiceDescriptor> available{
      voice("de-1", "de"),
      voice("en-1", "en"),
  };
  // The request still asks for "de" (the default), but VOX_VOICE wins.
  EXPECT_EQ(selectVoice(available, byName("Voice en-1")),
            std::optional(chosen(available[1], VoiceChoice::ExplicitName)));
}

TEST(SelectVoice, TheExplicitNameIsCaseInsensitive) {
  const std::vector<VoiceDescriptor> available{voice("de-1", "de")};
  EXPECT_EQ(selectVoice(available, byName("VOICE DE-1")),
            std::optional(chosen(available[0], VoiceChoice::ExplicitName)));
}

TEST(SelectVoice, AMissingExplicitVoiceSkipsTheLanguagePreference) {
  // A broken override must not silently re-enable what it replaced (mirrors
  // VOX_LEXICON): the fallback applies even though a "de" voice exists.
  const std::vector<VoiceDescriptor> available{
      voice("de-1", "de"),
      voice("en-1", "en", true),
  };
  EXPECT_EQ(selectVoice(available, byName("Voice nope")),
            std::optional(chosen(available[1], VoiceChoice::Fallback)));
}

TEST(SelectVoice, AMissingExplicitVoiceUnderRequiredYieldsNothing) {
  const std::vector<VoiceDescriptor> available{voice("de-1", "de")};
  VoiceSelectionRequest request = require("de");
  request.explicitVoice = "Voice nope";
  EXPECT_EQ(selectVoice(available, request), std::nullopt);
}

TEST(PrimarySubtag, CutsAtTheFirstHyphen) {
  EXPECT_EQ(primarySubtag("de-AT"), "de");
  EXPECT_EQ(primarySubtag("en"), "en");
  EXPECT_EQ(primarySubtag(""), "");
}

TEST(LanguageTagFromLangId, MapsPrimaryLanguagesAcrossRegions) {
  EXPECT_EQ(languageTagFromLangId(0x407UL), "de"); // de-DE
  EXPECT_EQ(languageTagFromLangId(0xC07UL), "de"); // de-AT — same primary
  EXPECT_EQ(languageTagFromLangId(0x409UL), "en"); // en-US
  EXPECT_EQ(languageTagFromLangId(0x40CUL), "fr"); // fr-FR
}

TEST(LanguageTagFromLangId, UnmappedLanguagesYieldAnEmptyTag) {
  EXPECT_EQ(languageTagFromLangId(0UL), "");
  EXPECT_EQ(languageTagFromLangId(0x3FUL), ""); // a primary id outside the table
}

// The tests below cover mergeVoices, which folds the OneCore discovery pass
// (secondary) into the classic one (primary) with classic precedence (#52).

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
