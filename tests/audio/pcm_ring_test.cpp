// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::audio::PcmRing — SPSC correctness, wraparound, and a
///        concurrent producer/consumer run (data-race coverage under TSan).
#include <array>
#include <cstddef>
#include <span>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <vox/audio/pcm_ring.hpp>

namespace {

using vox::audio::PcmRing;

std::byte byteOf(std::size_t value) {
  return std::byte{static_cast<unsigned char>(value & 0xFFU)};
}

std::vector<std::byte> sequence(std::size_t count, std::size_t start = 0) {
  std::vector<std::byte> data(count);
  for (std::size_t i = 0; i < count; ++i) {
    data.at(i) = byteOf(start + i);
  }
  return data;
}

TEST(PcmRing, StartsEmpty) {
  const PcmRing ring{16};
  EXPECT_EQ(ring.capacity(), 16U);
  EXPECT_EQ(ring.readableBytes(), 0U);
  EXPECT_EQ(ring.writableBytes(), 16U);
}

TEST(PcmRing, WriteThenReadRoundTrips) {
  PcmRing ring{16};
  const std::vector<std::byte> in = sequence(8);

  EXPECT_EQ(ring.write(in), 8U);
  EXPECT_EQ(ring.readableBytes(), 8U);
  EXPECT_EQ(ring.writableBytes(), 8U);

  std::vector<std::byte> out(8);
  EXPECT_EQ(ring.read(out), 8U);
  EXPECT_EQ(out, in);
  EXPECT_EQ(ring.readableBytes(), 0U);
}

TEST(PcmRing, WriteIsTruncatedWhenFull) {
  PcmRing ring{4};
  const std::vector<std::byte> in = sequence(8);
  EXPECT_EQ(ring.write(in), 4U); // only capacity fits
  EXPECT_EQ(ring.write(in), 0U); // now full
}

TEST(PcmRing, ReadIsTruncatedWhenEmpty) {
  PcmRing ring{8};
  EXPECT_EQ(ring.write(sequence(3)), 3U);
  std::vector<std::byte> out(8);
  EXPECT_EQ(ring.read(out), 3U); // only what is available
}

TEST(PcmRing, WrapsAroundTheCapacityBoundary) {
  PcmRing ring{8};
  // Advance the cursors near the end, then write a block that straddles it.
  EXPECT_EQ(ring.write(sequence(6)), 6U);
  std::vector<std::byte> drain(6);
  EXPECT_EQ(ring.read(drain), 6U);

  const std::vector<std::byte> in = sequence(8, 100); // wraps: 2 at end, 6 at start
  EXPECT_EQ(ring.write(in), 8U);
  std::vector<std::byte> out(8);
  EXPECT_EQ(ring.read(out), 8U);
  EXPECT_EQ(out, in);
}

TEST(PcmRing, ClearDiscardsReadableData) {
  PcmRing ring{16};
  EXPECT_EQ(ring.write(sequence(10)), 10U);
  ring.clear();
  EXPECT_EQ(ring.readableBytes(), 0U);
  EXPECT_EQ(ring.writableBytes(), 16U);

  // The ring is reusable after a clear.
  EXPECT_EQ(ring.write(sequence(5, 50)), 5U);
  std::vector<std::byte> out(5);
  EXPECT_EQ(ring.read(out), 5U);
  EXPECT_EQ(out, sequence(5, 50));
}

// Concurrent run: a producer streams a long known sequence through a small ring
// while a consumer drains it. Asserts every byte arrives exactly once and in
// order — and, under TSan, that the cursor handshake has no data race.
TEST(PcmRing, ConcurrentProducerConsumerPreservesOrder) {
  constexpr std::size_t Capacity = 64;
  constexpr std::size_t Total = 1U << 16U;
  PcmRing ring{Capacity};

  std::vector<std::byte> received;
  received.reserve(Total);

  std::thread consumer([&ring, &received] {
    std::array<std::byte, 32> buf{};
    while (received.size() < Total) {
      const std::size_t n = ring.read(buf);
      received.insert(received.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
    }
  });

  const std::vector<std::byte> all = sequence(Total);
  std::size_t offset = 0;
  while (offset < Total) {
    offset += ring.write(std::span<const std::byte>(all).subspan(offset));
  }
  consumer.join();

  ASSERT_EQ(received.size(), Total);
  EXPECT_TRUE(received == all);
}

} // namespace
