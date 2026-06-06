// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::OsError and its pure message formatter. Portable: runs
///        on every platform (no Win32), so the OS-error taxonomy is covered by
///        the sanitizer/clang-tidy build too (ADR-12).
#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <vox/core/os_error.hpp>

namespace {

TEST(FormatOsError, AppendsNonZeroCodeAsHex) {
  EXPECT_EQ(vox::formatOsError(0x80004005U, "activate failed"), "activate failed (0x80004005)");
}

TEST(FormatOsError, OmitsZeroCode) {
  EXPECT_EQ(vox::formatOsError(0U, "malformed format"), "malformed format");
}

TEST(FormatOsError, PadsToEightHexDigits) {
  EXPECT_EQ(vox::formatOsError(0x1FU, "x"), "x (0x0000001F)");
}

TEST(OsError, CarriesCodeAndFormattedMessage) {
  const vox::OsError error(0x80070002U, "open failed");
  EXPECT_EQ(error.code(), 0x80070002U);
  EXPECT_STREQ(error.what(), "open failed (0x80070002)");
}

TEST(OsError, ContextOnlyConstructorHasZeroCode) {
  const vox::OsError error("precondition violated");
  EXPECT_EQ(error.code(), 0U);
  EXPECT_STREQ(error.what(), "precondition violated");
}

TEST(OsError, IsCaughtAsRuntimeError) {
  bool caught = false;
  try {
    throw vox::OsError(1U, "boom");
  } catch (const std::runtime_error& error) {
    caught = true;
    EXPECT_EQ(error.what(), std::string("boom (0x00000001)"));
  }
  EXPECT_TRUE(caught) << "OsError should be catchable as std::runtime_error";
}

} // namespace
