// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::audio::AudioFormat (frame math + diagnostics).
#include <gtest/gtest.h>

#include <vox/audio/audio_format.hpp>

namespace {

using vox::audio::AudioFormat;

TEST(AudioFormat, DefaultsToSapiMvpFormat) {
  const AudioFormat format;
  EXPECT_EQ(format.sampleRate, 22050U);
  EXPECT_EQ(format.bitsPerSample, 16U);
  EXPECT_EQ(format.channels, 1U);
}

TEST(AudioFormat, BytesPerFrameFollowsBitsAndChannels) {
  const AudioFormat mono16{22050, 16, 1};
  const AudioFormat stereo16{44100, 16, 2};
  const AudioFormat mono8{8000, 8, 1};
  EXPECT_EQ(bytesPerFrame(mono16), 2U);
  EXPECT_EQ(bytesPerFrame(stereo16), 4U);
  EXPECT_EQ(bytesPerFrame(mono8), 1U);
}

TEST(AudioFormat, BytesPerSecondIsRateTimesFrame) {
  const AudioFormat format{22050, 16, 1};
  EXPECT_EQ(bytesPerSecond(format), 44100U);
}

TEST(AudioFormat, EqualityComparesEveryField) {
  const AudioFormat base{22050, 16, 1};
  EXPECT_EQ(base, (AudioFormat{22050, 16, 1}));
  EXPECT_NE(base, (AudioFormat{44100, 16, 1}));
  EXPECT_NE(base, (AudioFormat{22050, 8, 1}));
  EXPECT_NE(base, (AudioFormat{22050, 16, 2}));
}

TEST(AudioFormat, ToStringDescribesChannelLayout) {
  EXPECT_EQ(toString(AudioFormat{22050, 16, 1}), "22050 Hz, 16-bit, mono");
  EXPECT_EQ(toString(AudioFormat{44100, 16, 2}), "44100 Hz, 16-bit, stereo");
  EXPECT_EQ(toString(AudioFormat{48000, 24, 6}), "48000 Hz, 24-bit, 6ch");
}

} // namespace
