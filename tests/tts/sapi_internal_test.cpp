// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for SapiTtsEngine's internals (detail/sapi_internal.hpp):
///        the UTF-8/UTF-16 converters, the "Language" LCID parser, the token
///        attribute reader, and the PcmSinkStream SAPI writes PCM into. These run
///        with mock COM and no installed voice, so every branch is covered in the
///        ordinary coverage job (#68 / #72).
#if defined(_WIN32)

#  include <array>
#  include <atomic>
#  include <cstddef>
#  include <span>
#  include <stdexcept>
#  include <string>
#  include <vector>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/tts/detail/sapi_internal.hpp>
#  include <vox/tts/itts_engine.hpp>

#  include "sapi_com_mocks.hpp"

namespace {

using vox::tts::detail::languageIsGerman;
using vox::tts::detail::makeOutputWaveFormat;
using vox::tts::detail::PcmSinkStream;
using vox::tts::detail::readAttribute;
using vox::tts::detail::toUtf8;
using vox::tts::detail::toWide;
using vox::tts::testing::coTaskString;
using vox::tts::testing::MockSpDataKey;
using vox::tts::testing::MockSpObjectToken;

using ::testing::_;
using ::testing::NiceMock;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

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

// ---- languageIsGerman -------------------------------------------------------

TEST(SapiLanguageIsGerman, MatchesGermanPrimaryLanguage) {
  EXPECT_TRUE(languageIsGerman(L"407")); // de-DE
  EXPECT_TRUE(languageIsGerman(L"c07")); // de-CH (still LANG_GERMAN primary)
}

TEST(SapiLanguageIsGerman, RejectsNonGerman) {
  EXPECT_FALSE(languageIsGerman(L"409")); // en-US
  EXPECT_FALSE(languageIsGerman(L""));
}

TEST(SapiLanguageIsGerman, ScansSemicolonSeparatedList) {
  EXPECT_TRUE(languageIsGerman(L"409;407"));  // German not first
  EXPECT_TRUE(languageIsGerman(L"407;409"));  // German first
  EXPECT_FALSE(languageIsGerman(L"409;809")); // none German
}

TEST(SapiLanguageIsGerman, ToleratesEmptyTokensAndGarbage) {
  EXPECT_TRUE(languageIsGerman(L";407;")); // empty tokens around a match
  EXPECT_FALSE(languageIsGerman(L";;"));   // only empty tokens
  EXPECT_FALSE(languageIsGerman(L"zzz"));  // wcstoul -> 0, not German
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

  void valueYields(LPWSTR (*make)(), HRESULT result) {
    ON_CALL(attributes_, GetStringValue(_, _)).WillByDefault([make, result](LPCWSTR, LPWSTR* out) {
      *out = make != nullptr ? make() : nullptr;
      return result;
    });
  }
};

TEST_F(SapiReadAttribute, ReturnsTheStringValueOnSuccess) {
  openKeyYields(&attributes_, S_OK);
  valueYields([]() -> LPWSTR { return coTaskString(L"Microsoft David"); }, S_OK);
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

// ---- PcmSinkStream ----------------------------------------------------------

class SapiPcmSinkStream : public ::testing::Test {
protected:
  std::vector<std::byte> received_;
  std::atomic<bool> cancelled_{false};
  WAVEFORMATEX format_{makeOutputWaveFormat()};
  // The stream holds the sink and cancel flag by reference (it outlives neither),
  // so they must be fixture members that outlast the ComPtr.
  vox::tts::ITtsEngine::PcmSink sink_ = [this](std::span<const std::byte> block) {
    received_.insert(received_.end(), block.begin(), block.end());
  };

  ComPtr<PcmSinkStream> makeStream() {
    return Make<PcmSinkStream>(sink_, cancelled_, format_);
  }

  static ULONG write(const ComPtr<PcmSinkStream>& stream, const std::vector<std::byte>& bytes) {
    ULONG written = 0;
    EXPECT_EQ(stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &written), S_OK);
    return written;
  }
};

TEST_F(SapiPcmSinkStream, WriteForwardsBytesToTheSinkAndReportsCount) {
  const auto stream = makeStream();
  const std::vector<std::byte> block{std::byte{1}, std::byte{2}, std::byte{3}};
  EXPECT_EQ(write(stream, block), 3U);
  EXPECT_EQ(received_, block);
}

TEST_F(SapiPcmSinkStream, WriteWithNullWrittenPointerStillForwards) {
  const auto stream = makeStream();
  const std::array<std::byte, 2> block{std::byte{7}, std::byte{8}};
  EXPECT_EQ(stream->Write(block.data(), 2, nullptr), S_OK);
  EXPECT_EQ(received_.size(), 2U);
}

TEST_F(SapiPcmSinkStream, WriteAbortsWhenCancelled) {
  cancelled_.store(true, std::memory_order_relaxed);
  const auto stream = makeStream();
  const std::byte byte{1};
  ULONG written = 1;
  EXPECT_EQ(stream->Write(&byte, 1, &written), E_ABORT);
  EXPECT_EQ(written, 0U);
  EXPECT_TRUE(received_.empty());
}

TEST_F(SapiPcmSinkStream, WriteRejectsNullBufferWithNonZeroLength) {
  const auto stream = makeStream();
  ULONG written = 1;
  EXPECT_EQ(stream->Write(nullptr, 4, &written), E_POINTER);
  EXPECT_EQ(written, 0U);
}

TEST_F(SapiPcmSinkStream, WriteOfZeroBytesIsANoOp) {
  const auto stream = makeStream();
  const std::byte byte{1};
  ULONG written = 99;
  EXPECT_EQ(stream->Write(&byte, 0, &written), S_OK);
  EXPECT_EQ(written, 0U);
  EXPECT_TRUE(received_.empty());
}

TEST_F(SapiPcmSinkStream, WriteSwallowsASinkExceptionAsEFail) {
  // Declared before the stream so it outlives the ComPtr that references it.
  const vox::tts::ITtsEngine::PcmSink throwing = [](std::span<const std::byte>) {
    throw std::runtime_error("sink boom");
  };
  const auto stream = Make<PcmSinkStream>(throwing, cancelled_, format_);
  const std::byte byte{1};
  ULONG written = 0;
  EXPECT_EQ(stream->Write(&byte, 1, &written), E_FAIL);
}

TEST_F(SapiPcmSinkStream, ReadIsNotImplemented) {
  const auto stream = makeStream();
  ULONG read = 0;
  std::array<std::byte, 1> buffer{};
  EXPECT_EQ(stream->Read(buffer.data(), 1, &read), E_NOTIMPL);
}

TEST_F(SapiPcmSinkStream, SeekTracksThePositionAcrossOrigins) {
  const auto stream = makeStream();
  ULARGE_INTEGER pos{};

  LARGE_INTEGER ten{};
  ten.QuadPart = 10;
  EXPECT_EQ(stream->Seek(ten, STREAM_SEEK_SET, &pos), S_OK);
  EXPECT_EQ(pos.QuadPart, 10U);

  LARGE_INTEGER three{};
  three.QuadPart = 3;
  EXPECT_EQ(stream->Seek(three, STREAM_SEEK_CUR, &pos), S_OK);
  EXPECT_EQ(pos.QuadPart, 13U);

  LARGE_INTEGER zero{};
  EXPECT_EQ(stream->Seek(zero, STREAM_SEEK_END, &pos), S_OK); // append-only: end == current
  EXPECT_EQ(pos.QuadPart, 13U);
}

TEST_F(SapiPcmSinkStream, SeekRejectsUnknownOriginAndNegativeTarget) {
  const auto stream = makeStream();
  LARGE_INTEGER zero{};
  EXPECT_EQ(stream->Seek(zero, 99, nullptr), STG_E_INVALIDFUNCTION);

  LARGE_INTEGER negative{};
  negative.QuadPart = -1;
  EXPECT_EQ(stream->Seek(negative, STREAM_SEEK_SET, nullptr), STG_E_INVALIDFUNCTION);
}

TEST_F(SapiPcmSinkStream, UnsupportedIStreamOperationsReportSensibleCodes) {
  const auto stream = makeStream();
  ULARGE_INTEGER size{};
  EXPECT_EQ(stream->SetSize(size), E_NOTIMPL);
  EXPECT_EQ(stream->CopyTo(nullptr, size, nullptr, nullptr), E_NOTIMPL);
  EXPECT_EQ(stream->Commit(0), S_OK);
  EXPECT_EQ(stream->Revert(), S_OK);
  EXPECT_EQ(stream->LockRegion(size, size, 0), E_NOTIMPL);
  EXPECT_EQ(stream->UnlockRegion(size, size, 0), E_NOTIMPL);
  IStream* clone = nullptr;
  EXPECT_EQ(stream->Clone(&clone), E_NOTIMPL);
}

TEST_F(SapiPcmSinkStream, StatReportsTheStreamTypeAndSize) {
  const auto stream = makeStream();
  const std::vector<std::byte> block{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  write(stream, block);

  STATSTG stat{};
  EXPECT_EQ(stream->Stat(&stat, STATFLAG_NONAME), S_OK);
  EXPECT_EQ(stat.type, static_cast<DWORD>(STGTY_STREAM));
  EXPECT_EQ(stat.cbSize.QuadPart, 4U);

  EXPECT_EQ(stream->Stat(nullptr, 0), STG_E_INVALIDPOINTER);
}

TEST_F(SapiPcmSinkStream, GetFormatHandsBackTheWaveFormat) {
  const auto stream = makeStream();
  GUID formatId{};
  WAVEFORMATEX* waveFormat = nullptr;
  EXPECT_EQ(stream->GetFormat(&formatId, &waveFormat), S_OK);
  EXPECT_EQ(formatId, SPDFID_WaveFormatEx);
  ASSERT_NE(waveFormat, nullptr);
  EXPECT_EQ(waveFormat->nSamplesPerSec, format_.nSamplesPerSec);
  EXPECT_EQ(waveFormat->wBitsPerSample, format_.wBitsPerSample);
  ::CoTaskMemFree(waveFormat);

  EXPECT_EQ(stream->GetFormat(nullptr, nullptr), S_OK); // both out-params optional
}

} // namespace

#endif // defined(_WIN32)
