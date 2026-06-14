// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::german::normalizeName.
#include <gtest/gtest.h>

#include <vox/german/normalize.hpp>

namespace {

using vox::german::normalizeName;

TEST(NormalizeName, TrimsAndCollapsesWhitespace) {
  EXPECT_EQ(normalizeName("  OK \t Button "), "OK Button");
}

TEST(NormalizeName, EmptyAndAllWhitespaceBecomeEmpty) {
  EXPECT_EQ(normalizeName(""), "");
  EXPECT_EQ(normalizeName("   \t\r\n "), "");
}

TEST(NormalizeName, AlreadyNormalIsUnchanged) {
  EXPECT_EQ(normalizeName("Datei öffnen"), "Datei öffnen");
}

TEST(NormalizeName, CollapsesNewlinesAndTabs) {
  EXPECT_EQ(normalizeName("a\n\n\tb"), "a b");
}

// UTF-8 multibyte content (umlauts) must survive byte-wise whitespace handling.
TEST(NormalizeName, PreservesUtf8Multibyte) {
  EXPECT_EQ(normalizeName("  Größe  \t Über "), "Größe Über");
}

} // namespace
