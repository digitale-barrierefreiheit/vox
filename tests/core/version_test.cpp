// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Smoke tests for vox::core::version().
///
/// These exist to prove the whole toolchain — configure, compile, link, test
/// discovery — is wired correctly end to end. They are intentionally trivial;
/// real cores arrive with their own TDD suites.
#include <gtest/gtest.h>

#include <vox/core/version.hpp>

namespace {

TEST(CoreVersion, ReportsConfiguredComponents) {
  const vox::core::VersionInfo info = vox::core::version();
  EXPECT_EQ(info.major, VOX_VERSION_MAJOR);
  EXPECT_EQ(info.minor, VOX_VERSION_MINOR);
  EXPECT_EQ(info.patch, VOX_VERSION_PATCH);
}

TEST(CoreVersion, TextMatchesNumericComponents) {
  const vox::core::VersionInfo info = vox::core::version();
  EXPECT_EQ(info.text, VOX_VERSION_STRING);
  EXPECT_FALSE(info.text.empty());
}

} // namespace
