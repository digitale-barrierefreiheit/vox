// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::german::numberToWords.
#include <gtest/gtest.h>

#include <vox/german/numbers.hpp>

namespace {

using vox::german::numberToWords;

TEST(NumberToWords, UnitsTeensAndTens) {
  EXPECT_EQ(numberToWords(0), "null");
  EXPECT_EQ(numberToWords(1), "eins");
  EXPECT_EQ(numberToWords(7), "sieben");
  EXPECT_EQ(numberToWords(10), "zehn");
  EXPECT_EQ(numberToWords(16), "sechzehn"); // contracted, not "sechszehn"
  EXPECT_EQ(numberToWords(17), "siebzehn"); // contracted, not "siebenzehn"
  EXPECT_EQ(numberToWords(20), "zwanzig");
  EXPECT_EQ(numberToWords(21), "einundzwanzig");
  EXPECT_EQ(numberToWords(30), "dreißig"); // ß
  EXPECT_EQ(numberToWords(76), "sechsundsiebzig");
  EXPECT_EQ(numberToWords(99), "neunundneunzig");
}

TEST(NumberToWords, HundredsAndThousands) {
  EXPECT_EQ(numberToWords(100), "einhundert");
  EXPECT_EQ(numberToWords(101), "einhunderteins"); // trailing "eins", not "ein"
  EXPECT_EQ(numberToWords(121), "einhunderteinundzwanzig");
  EXPECT_EQ(numberToWords(200), "zweihundert");
  EXPECT_EQ(numberToWords(999), "neunhundertneunundneunzig");
  EXPECT_EQ(numberToWords(1000), "eintausend");
  EXPECT_EQ(numberToWords(1001), "eintausendeins");
  EXPECT_EQ(numberToWords(1234), "eintausendzweihundertvierunddreißig");
  EXPECT_EQ(numberToWords(9999), "neuntausendneunhundertneunundneunzig");
}

TEST(NumberToWords, OutOfRangeFallsBackToDigits) {
  EXPECT_EQ(numberToWords(10000), "10000");
  EXPECT_EQ(numberToWords(-5), "-5");
}

} // namespace
