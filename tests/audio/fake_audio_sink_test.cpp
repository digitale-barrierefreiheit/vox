// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::testing::FakeAudioSink (recording + flush semantics).
#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include <vox/testing/fake_audio_sink.hpp>

namespace {

using vox::testing::FakeAudioSink;

constexpr std::array<std::byte, 4> kChunk{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};

TEST(FakeAudioSink, StartAndStopTrackState) {
  FakeAudioSink sink;
  EXPECT_FALSE(sink.started());
  sink.start();
  EXPECT_TRUE(sink.started());
  sink.stop();
  EXPECT_FALSE(sink.started());
}

TEST(FakeAudioSink, WriteBuffersAndCounts) {
  FakeAudioSink sink;
  sink.start();
  sink.write(kChunk);
  sink.write(kChunk);

  EXPECT_EQ(sink.writeCount(), 2);
  EXPECT_EQ(sink.bytesWritten(), 8U);
  EXPECT_EQ(sink.buffered().size(), 8U);
}

TEST(FakeAudioSink, FlushDropsBufferedButKeepsCumulativeCount) {
  FakeAudioSink sink;
  sink.start();
  sink.write(kChunk);
  sink.flush();

  EXPECT_EQ(sink.flushCount(), 1);
  EXPECT_TRUE(sink.buffered().empty());
  EXPECT_EQ(sink.bytesWritten(), 4U); // cumulative survives flush

  sink.write(kChunk);
  EXPECT_EQ(sink.buffered().size(), 4U);
  EXPECT_EQ(sink.bytesWritten(), 8U);
}

} // namespace
