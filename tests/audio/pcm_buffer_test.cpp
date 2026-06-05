// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::audio::PcmBuffer (accumulation + frame counting).
#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_buffer.hpp>

namespace {

using vox::audio::AudioFormat;
using vox::audio::PcmBuffer;

TEST(PcmBuffer, DefaultsToEmpty) {
  const PcmBuffer buffer;
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.byteCount(), 0U);
  EXPECT_EQ(buffer.frameCount(), 0U);
}

TEST(PcmBuffer, AppendCollectsChunksInOrder) {
  PcmBuffer buffer;
  const std::array<std::byte, 2> first{std::byte{0x01}, std::byte{0x02}};
  const std::array<std::byte, 2> second{std::byte{0x03}, std::byte{0x04}};

  buffer.append(first);
  buffer.append(second);

  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.byteCount(), 4U);
  ASSERT_EQ(buffer.samples.size(), 4U);
  EXPECT_EQ(buffer.samples.at(0), std::byte{0x01});
  EXPECT_EQ(buffer.samples.at(3), std::byte{0x04});
}

TEST(PcmBuffer, FrameCountDividesByFrameSize) {
  PcmBuffer buffer;
  buffer.format = AudioFormat{22050, 16, 1}; // 2 bytes/frame
  const std::array<std::byte, 6> pcm{};
  buffer.append(pcm);
  EXPECT_EQ(buffer.frameCount(), 3U);
}

TEST(PcmBuffer, FrameCountIsZeroForDegenerateFormat) {
  PcmBuffer buffer;
  buffer.format = AudioFormat{22050, 0, 0}; // no frame size
  const std::array<std::byte, 4> pcm{};
  buffer.append(pcm);
  EXPECT_EQ(buffer.frameCount(), 0U);
}

} // namespace
