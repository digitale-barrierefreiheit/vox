// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::audio::detail::pushFramesToRing — the producer's
///        back-pressure push, covered thread-free with a real PcmRing and
///        injected hooks whose `wait` advances state like the real consumer.
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include <vox/audio/detail/ring_push.hpp>
#include <vox/audio/pcm_ring.hpp>

namespace {

using vox::audio::PcmRing;
using vox::audio::detail::backOffOneTick;
using vox::audio::detail::pushFramesToRing;

std::vector<std::byte> bytes(std::size_t count) {
  return std::vector<std::byte>(count, std::byte{0x5A});
}

TEST(RingPush, QueuesEverythingWhenTheRingHasRoomAndNeverWaits) {
  PcmRing ring(64);
  const std::vector<std::byte> src = bytes(16);
  int waits = 0;
  const auto left = pushFramesToRing(
      ring, src, [] { return false; }, [] { return false; }, [&waits] { ++waits; });

  EXPECT_TRUE(left.empty()); // fully queued
  EXPECT_EQ(waits, 0);       // room available -> no back-off
  EXPECT_EQ(ring.readableBytes(), 16U);
}

TEST(RingPush, AbandonsImmediatelyWhenStoppedOrBargedIn) {
  PcmRing ring(64);
  const std::vector<std::byte> src = bytes(16);
  int waits = 0;
  const auto left =
      pushFramesToRing(ring, src, [] { return true; }, [] { return false; }, [&waits] { ++waits; });

  EXPECT_EQ(left.size(), src.size()); // nothing queued; the rest is dropped
  EXPECT_EQ(waits, 0);
  EXPECT_EQ(ring.readableBytes(), 0U);
}

TEST(RingPush, WaitsWhileAFlushIsPendingThenProceeds) {
  PcmRing ring(64);
  const std::vector<std::byte> src = bytes(16);
  bool flushPending = true;
  int waits = 0;
  const auto left = pushFramesToRing(
      ring, src, [] { return false; }, [&flushPending] { return flushPending; },
      [&waits, &flushPending] {
        ++waits;
        flushPending = false; // the render thread "services" the pending flush
      });

  EXPECT_TRUE(left.empty());
  EXPECT_EQ(waits, 1); // waited once for the flush, then queued everything
  EXPECT_EQ(ring.readableBytes(), 16U);
}

TEST(RingPush, BacksOffWhenTheRingIsFullThenProceedsAsTheConsumerDrains) {
  PcmRing ring(16);
  const std::vector<std::byte> src = bytes(48); // more than the ring holds at once
  std::array<std::byte, 16> sink{};
  int waits = 0;
  const auto left = pushFramesToRing(
      ring, src, [] { return false; }, [] { return false; },
      [&waits, &ring, &sink] {
        ++waits;
        (void)ring.read(sink); // the consumer makes room on each back-off
      });

  EXPECT_TRUE(left.empty()); // all 48 bytes eventually queued
  EXPECT_GE(waits, 1);       // backed off at least once on the full ring
}

TEST(RingPush, BackOffOneTickReturns) {
  backOffOneTick(); // the production wait: yields one tick, then returns
  SUCCEED();
}

} // namespace
