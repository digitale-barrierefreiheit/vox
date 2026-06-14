// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::testing::FakeTtsEngine (streaming + cancel + recording).
#include <cstddef>
#include <span>

#include <gtest/gtest.h>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_buffer.hpp>
#include <vox/testing/fake_tts_engine.hpp>

namespace {

using vox::audio::AudioFormat;
using vox::audio::PcmBuffer;
using vox::testing::FakeTtsEngine;

TEST(FakeTtsEngine, ReportsItsConfiguredFormat) {
  const FakeTtsEngine engine{AudioFormat{16000, 16, 1}};
  EXPECT_EQ(engine.format(), (AudioFormat{16000, 16, 1}));
}

TEST(FakeTtsEngine, StreamsOneChunkPerByteIntoTheSink) {
  FakeTtsEngine engine{AudioFormat{22050, 16, 1}, 2};
  PcmBuffer collected;
  collected.format = engine.format();

  engine.synthesize("abc",
                    [&collected](std::span<const std::byte> chunk) { append(collected, chunk); });

  EXPECT_EQ(engine.chunksEmitted(), 3U);
  EXPECT_EQ(engine.bytesEmitted(), 6U);
  EXPECT_EQ(byteCount(collected), 6U);
  EXPECT_EQ(frameCount(collected), 3U);
}

TEST(FakeTtsEngine, RecordsTextRateAndCallCounts) {
  FakeTtsEngine engine;
  engine.setRate(4);
  engine.synthesize("Hallo", [](std::span<const std::byte>) { /* discard PCM */ });
  engine.synthesize("Welt", [](std::span<const std::byte>) { /* discard PCM */ });

  EXPECT_EQ(engine.lastText(), "Welt");
  EXPECT_EQ(engine.rate(), 4);
  EXPECT_EQ(engine.synthesizeCount(), 2);
}

TEST(FakeTtsEngine, CancelFromWithinTheSinkStopsAfterTheNextChunk) {
  FakeTtsEngine engine;
  int chunks = 0;
  engine.synthesize("abcdef", [&chunks, &engine](std::span<const std::byte>) {
    ++chunks;
    engine.cancel(); // request barge-in mid-stream
  });

  // The chunk being delivered completes; the loop then stops at the boundary.
  EXPECT_EQ(chunks, 1);
  EXPECT_EQ(engine.chunksEmitted(), 1U);
  EXPECT_EQ(engine.cancelCount(), 1);
}

} // namespace
