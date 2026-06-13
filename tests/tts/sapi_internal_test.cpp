// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for SapiTtsEngine's pure helpers (detail/sapi_internal.hpp):
///        the UTF-8/UTF-16 converters, the "Language" LCID parser, and the token
///        attribute reader. These run with mock COM and no installed voice, so
///        every branch is covered in the ordinary coverage job (#68 / #72).
#if defined(_WIN32)

#  include <string>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/tts/detail/sapi_internal.hpp>

#  include "sapi_com_mocks.hpp"

namespace {

using vox::tts::detail::firstLcid;
using vox::tts::detail::readAttribute;
using vox::tts::detail::toUtf8;
using vox::tts::detail::toWide;
using vox::tts::testing::coTaskString;
using vox::tts::testing::MockSpDataKey;
using vox::tts::testing::MockSpObjectToken;

using ::testing::_;
using ::testing::NiceMock;

// ---- toWide -----------------------------------------------------------------

TEST(SapiToWide, EmptyInputYieldsEmpty) {
  EXPECT_TRUE(toWide("").empty());
}

TEST(SapiToWide, ConvertsAsciiAndMultibyte) {
  EXPECT_EQ(toWide("OK"), L"OK");
  EXPECT_EQ(toWide("Gr\xC3\xBC\xC3\x9F"), L"Grüß"); // "Grüß" in UTF-8
}

TEST(SapiToWide, RejectsInvalidUtf8) {
  EXPECT_TRUE(toWide("\xFF\xFE").empty()); // never-valid UTF-8 lead bytes
  EXPECT_TRUE(toWide("\x80").empty());     // lone continuation byte
}

// ---- toUtf8 -----------------------------------------------------------------

TEST(SapiToUtf8, NullOrEmptyYieldsEmpty) {
  EXPECT_TRUE(toUtf8(nullptr).empty());
  EXPECT_TRUE(toUtf8(L"").empty());
}

TEST(SapiToUtf8, ConvertsAsciiAndMultibyte) {
  EXPECT_EQ(toUtf8(L"OK"), "OK");
  EXPECT_EQ(toUtf8(L"Grüß"), "Gr\xC3\xBC\xC3\x9F");
}

TEST(SapiToUtf8, RejectsInvalidUtf16) {
  const wchar_t loneHighSurrogate[] = {static_cast<wchar_t>(0xD800), L'\0'};
  EXPECT_TRUE(toUtf8(loneHighSurrogate).empty());
}

// ---- firstLcid ----------------------------------------------------------------

TEST(SapiFirstLcid, ParsesASingleHexLcid) {
  EXPECT_EQ(firstLcid(L"407"), 0x407UL); // de-DE
  EXPECT_EQ(firstLcid(L"c07"), 0xC07UL); // de-CH
  EXPECT_EQ(firstLcid(L""), 0UL);
}

TEST(SapiFirstLcid, TakesTheFirstEntryOfAList) {
  // The first entry is the voice's principal language (#88).
  EXPECT_EQ(firstLcid(L"409;407"), 0x409UL);
  EXPECT_EQ(firstLcid(L"407;409"), 0x407UL);
}

TEST(SapiFirstLcid, ToleratesEmptyTokensAndGarbage) {
  EXPECT_EQ(firstLcid(L";407;"), 0x407UL);   // empty tokens around a value
  EXPECT_EQ(firstLcid(L";;"), 0UL);          // only empty tokens
  EXPECT_EQ(firstLcid(L"zzz"), 0UL);         // wcstoul -> 0: not a LCID
  EXPECT_EQ(firstLcid(L"zzz;409"), 0x409UL); // garbage skipped, next one wins
}

// ---- readAttribute ----------------------------------------------------------

class SapiReadAttribute : public ::testing::Test {
protected:
  NiceMock<MockSpObjectToken> token_;
  NiceMock<MockSpDataKey> attributes_;

  // Routes token.OpenKey("Attributes") to the attributes_ key (or a failure).
  void openKeyYields(ISpDataKey* key, HRESULT result) {
    ON_CALL(token_, OpenKey(_, _)).WillByDefault([key, result](LPCWSTR, ISpDataKey** out) {
      *out = key;
      return result;
    });
  }

  // Routes attributes_.GetStringValue to @p value (which may be null).
  void valueYields(LPWSTR value, HRESULT result) {
    ON_CALL(attributes_, GetStringValue(_, _)).WillByDefault([value, result](LPCWSTR, LPWSTR* out) {
      *out = value;
      return result;
    });
  }
};

TEST_F(SapiReadAttribute, ReturnsTheStringValueOnSuccess) {
  openKeyYields(&attributes_, S_OK);
  valueYields(coTaskString(L"Microsoft David"), S_OK);
  EXPECT_EQ(readAttribute(&token_, L"Name"), L"Microsoft David");
}

TEST_F(SapiReadAttribute, EmptyWhenOpenKeyFails) {
  openKeyYields(nullptr, E_FAIL);
  EXPECT_TRUE(readAttribute(&token_, L"Name").empty());
}

TEST_F(SapiReadAttribute, EmptyWhenOpenKeyYieldsNull) {
  openKeyYields(nullptr, S_OK); // success but no key
  EXPECT_TRUE(readAttribute(&token_, L"Name").empty());
}

TEST_F(SapiReadAttribute, EmptyWhenGetStringValueFails) {
  openKeyYields(&attributes_, S_OK);
  valueYields(nullptr, E_FAIL);
  EXPECT_TRUE(readAttribute(&token_, L"Language").empty());
}

TEST_F(SapiReadAttribute, EmptyWhenGetStringValueYieldsNull) {
  openKeyYields(&attributes_, S_OK);
  valueYields(nullptr, S_OK); // success but null string
  EXPECT_TRUE(readAttribute(&token_, L"Language").empty());
}

} // namespace

#endif // defined(_WIN32)
