// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::tts::clampRate (the normalized rate scale).
#include <gtest/gtest.h>

#include <vox/tts/rate.hpp>

namespace {

using vox::tts::clampRate;
using vox::tts::MaxRate;
using vox::tts::MinRate;

TEST(ClampRate, PassesThroughInRangeValues) {
  EXPECT_EQ(clampRate(0), 0);
  EXPECT_EQ(clampRate(5), 5);
  EXPECT_EQ(clampRate(-5), -5);
}

TEST(ClampRate, ClampsToTheBoundaries) {
  EXPECT_EQ(clampRate(MinRate), MinRate);
  EXPECT_EQ(clampRate(MaxRate), MaxRate);
}

TEST(ClampRate, ClampsOutOfRangeValues) {
  EXPECT_EQ(clampRate(100), MaxRate);
  EXPECT_EQ(clampRate(-100), MinRate);
}

} // namespace
