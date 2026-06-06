// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for vox::OsError and its pure message formatter. Portable: runs
///        on every platform (no Win32), so the OS-error taxonomy is covered by
///        the sanitizer/clang-tidy build too (ADR-12).
#include <stdexcept>
#include <string>
#include <type_traits>

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

TEST(OsError, DerivesFromRuntimeError) {
  // Compile-time proof of the is-a relationship, so a `catch (std::runtime_error&)`
  // (or std::exception) still catches every OsError-derived type.
  static_assert(std::is_base_of_v<std::runtime_error, vox::OsError>,
                "OsError must be catchable as std::runtime_error");
  const vox::OsError error(1U, "boom");
  const std::runtime_error& asBase = error; // usable through the std base
  EXPECT_EQ(asBase.what(), std::string("boom (0x00000001)"));
}

} // namespace
