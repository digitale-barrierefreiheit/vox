// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for SapiTtsEngine's COM construction/synthesis error
///        handling, driven through the #68 test seam with mock COM — no
///        installed SAPI voice required, so these run anywhere (incl. the
///        coverage job) and fault-inject the failure paths.
#if defined(_WIN32)

#  include <array>
#  include <cstddef>
#  include <cstring>
#  include <cwchar>
#  include <span>
#  include <string_view>
#  include <vector>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/tts/errors.hpp>
#  include <vox/tts/sapi_test_seam.hpp>
#  include <vox/tts/sapi_tts_engine.hpp>
#  include <vox/tts/voice_selection.hpp>

#  include "sapi_com_mocks.hpp"

namespace {

using vox::tts::EngineError;
using vox::tts::SapiTtsEngine;
using vox::tts::VoiceSelectionPolicy;
using vox::tts::testing::coTaskString;
using vox::tts::testing::MockEnumSpObjectTokens;
using vox::tts::testing::MockSpDataKey;
using vox::tts::testing::MockSpObjectToken;
using vox::tts::testing::MockSpObjectTokenCategory;
using vox::tts::testing::MockSpVoice;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr long ErrorFail = static_cast<long>(0x80004005U); // E_FAIL

/// Restores the real COM factories on scope exit, so a test never leaks a seam
/// into the next one.
class SeamGuard {
public:
  SeamGuard() = default;

  ~SeamGuard() {
    vox::tts::testing::setVoiceFactory({});
    vox::tts::testing::setTokenCategoryFactory({});
  }

  SeamGuard(const SeamGuard&) = delete;
  SeamGuard& operator=(const SeamGuard&) = delete;
  SeamGuard(SeamGuard&&) = delete;
  SeamGuard& operator=(SeamGuard&&) = delete;
};

/// The very first link: the engine cannot even create the SAPI voice.
TEST(SapiTtsEngineErrors, ThrowsEngineErrorWhenVoiceCreationFails) {
  [[maybe_unused]] const SeamGuard guard;
  vox::tts::testing::setVoiceFactory([](struct ISpVoice** out) {
    *out = nullptr;
    return ErrorFail;
  });

  EXPECT_THROW(SapiTtsEngine{VoiceSelectionPolicy::PreferGerman}, EngineError);
}

/// Fixture wiring the full mock SAPI chain: the seams hand back `voice_` and
/// `category_`; the category enumerates one German voice token (`enumTokens_` ->
/// `token_`) whose "Attributes" key is `dataKey_`. The happy defaults let the
/// engine construct successfully; each test then breaks one link or drives a
/// post-construction call.
class SapiEngineTest : public ::testing::Test {
protected:
  static constexpr wchar_t kTokenId[] = L"VOX-TEST-VOICE-DE";

  void SetUp() override {
    vox::tts::testing::setVoiceFactory([this](ISpVoice** out) {
      *out = &voice_;
      return 0L; // S_OK
    });
    vox::tts::testing::setTokenCategoryFactory([this](ISpObjectTokenCategory** out) {
      *out = &category_;
      return 0L;
    });
    installHappyChain();
  }

  void TearDown() override {
    vox::tts::testing::setVoiceFactory({});
    vox::tts::testing::setTokenCategoryFactory({});
  }

  /// Default behaviours for the whole chain — overridden per-test to inject one
  /// failure (or a different voice language).
  void installHappyChain() {
    ON_CALL(category_, SetId(_, _)).WillByDefault(Return(S_OK));
    ON_CALL(category_, GetDefaultTokenId(_)).WillByDefault([](LPWSTR* out) {
      *out = coTaskString(kTokenId);
      return S_OK;
    });
    ON_CALL(category_, EnumTokens(_, _, _))
        .WillByDefault([this](LPCWSTR, LPCWSTR, IEnumSpObjectTokens** out) {
          *out = &enumTokens_;
          return S_OK;
        });
    ON_CALL(enumTokens_, Next(_, _, _))
        .WillByDefault([this](ULONG, ISpObjectToken** pelt, ULONG* fetched) {
          if (tokenServed_) {
            if (fetched != nullptr) {
              *fetched = 0;
            }
            return S_FALSE; // end of enumeration
          }
          tokenServed_ = true;
          *pelt = &token_;
          if (fetched != nullptr) {
            *fetched = 1;
          }
          return S_OK;
        });
    ON_CALL(token_, GetId(_)).WillByDefault([](LPWSTR* out) {
      *out = coTaskString(kTokenId);
      return S_OK;
    });
    ON_CALL(token_, OpenKey(_, _)).WillByDefault([this](LPCWSTR, ISpDataKey** out) {
      *out = &dataKey_;
      return S_OK;
    });
    ON_CALL(dataKey_, GetStringValue(_, _)).WillByDefault([](LPCWSTR name, LPWSTR* out) {
      // "407" is a German LCID; the Name value is arbitrary.
      if (name != nullptr && std::wcscmp(name, L"Language") == 0) {
        *out = coTaskString(L"407");
      } else {
        *out = coTaskString(L"Hedda");
      }
      return S_OK;
    });
    ON_CALL(voice_, SetVoice(_)).WillByDefault(Return(S_OK));
    // Capture the output stream the engine installs, so a test's Speak() can
    // forward PCM through it (exercising the engine's PcmSinkStream::Write).
    ON_CALL(voice_, SetOutput(_, _)).WillByDefault([this](IUnknown* output, BOOL) {
      capturedOutput_ = output;
      return S_OK;
    });
    ON_CALL(voice_, Speak(_, _, _)).WillByDefault(Return(S_OK));
    ON_CALL(voice_, SetRate(_)).WillByDefault(Return(S_OK));
  }

  /// Pushes @p pcm into the engine's captured output stream (the engine's
  /// PcmSinkStream), as real SAPI would while speaking — driving the chunk
  /// through to the caller's PcmSink. Returns the stream's `Write` HRESULT.
  HRESULT emitTo(std::span<const std::byte> pcm) {
    if (capturedOutput_ == nullptr) {
      return E_FAIL;
    }
    ISequentialStream* stream = nullptr;
    if (FAILED(capturedOutput_->QueryInterface(IID_PPV_ARGS(&stream))) || stream == nullptr) {
      return E_FAIL;
    }
    ULONG written = 0;
    const HRESULT hr = stream->Write(pcm.data(), static_cast<ULONG>(pcm.size()), &written);
    stream->Release();
    return hr;
  }

  /// Reconfigures the voice's reported language to en-US (a non-German LCID).
  void makeVoiceEnglish() {
    ON_CALL(dataKey_, GetStringValue(_, _)).WillByDefault([](LPCWSTR name, LPWSTR* out) {
      if (name != nullptr && std::wcscmp(name, L"Language") == 0) {
        *out = coTaskString(L"409"); // en-US
      } else {
        *out = coTaskString(L"David");
      }
      return S_OK;
    });
  }

  bool tokenServed_{false};
  IUnknown* capturedOutput_{nullptr};
  NiceMock<MockSpVoice> voice_;
  NiceMock<MockSpObjectTokenCategory> category_;
  NiceMock<MockEnumSpObjectTokens> enumTokens_;
  NiceMock<MockSpObjectToken> token_;
  NiceMock<MockSpDataKey> dataKey_;
};

TEST_F(SapiEngineTest, ConstructsAndSelectsTheGermanVoice) {
  const SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_EQ(engine.selectedVoice().id, "VOX-TEST-VOICE-DE");
  EXPECT_TRUE(engine.selectedVoice().isGerman);
}

TEST_F(SapiEngineTest, ThrowsWhenNoVoiceIsAvailable) {
  EXPECT_CALL(enumTokens_, Next(_, _, _))
      .WillRepeatedly([](ULONG, ISpObjectToken** pelt, ULONG* fetched) {
        if (pelt != nullptr) {
          *pelt = nullptr;
        }
        if (fetched != nullptr) {
          *fetched = 0;
        }
        return S_FALSE;
      });
  EXPECT_THROW(SapiTtsEngine{VoiceSelectionPolicy::PreferGerman}, EngineError);
}

TEST_F(SapiEngineTest, ThrowsWhenTokenEnumerationFails) {
  EXPECT_CALL(category_, EnumTokens(_, _, _)).WillOnce(Return(ErrorFail));
  EXPECT_THROW(SapiTtsEngine{VoiceSelectionPolicy::PreferGerman}, EngineError);
}

TEST_F(SapiEngineTest, ThrowsWhenActivatingTheVoiceFails) {
  EXPECT_CALL(voice_, SetVoice(_)).WillOnce(Return(ErrorFail));
  EXPECT_THROW(SapiTtsEngine{VoiceSelectionPolicy::PreferGerman}, EngineError);
}

TEST_F(SapiEngineTest, ThrowsWhenRequireGermanButVoiceIsEnglish) {
  makeVoiceEnglish();
  EXPECT_THROW(SapiTtsEngine{VoiceSelectionPolicy::RequireGerman}, EngineError);
}

TEST_F(SapiEngineTest, FallsBackToNonGermanUnderPreferGerman) {
  makeVoiceEnglish();
  const SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_FALSE(engine.selectedVoice().isGerman);
}

TEST_F(SapiEngineTest, SynthesizeEmptyTextIsANoOp) {
  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_CALL(voice_, Speak(_, _, _)).Times(0);
  engine.synthesize("", [](std::span<const std::byte>) {});
}

TEST_F(SapiEngineTest, SynthesizeThrowsOnInvalidUtf8) {
  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  // 0xFF is never a valid UTF-8 lead byte, so the conversion fails.
  const char invalid[] = {static_cast<char>(0xFF), static_cast<char>(0xFE)};
  EXPECT_THROW(engine.synthesize(std::string_view(invalid, sizeof invalid),
                                 [](std::span<const std::byte>) {}),
               EngineError);
}

TEST_F(SapiEngineTest, SynthesizeForwardsPcmToTheSink) {
  const std::array<std::byte, 4> pcm{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  ON_CALL(voice_, Speak(_, _, _)).WillByDefault([this, &pcm](LPCWSTR, DWORD, ULONG*) {
    return emitTo(pcm);
  });

  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  std::vector<std::byte> received;
  engine.synthesize("hallo", [&received](std::span<const std::byte> chunk) {
    received.insert(received.end(), chunk.begin(), chunk.end());
  });
  EXPECT_EQ(received.size(), pcm.size());
}

TEST_F(SapiEngineTest, SynthesizeThrowsWhenSpeakFails) {
  EXPECT_CALL(voice_, Speak(_, _, _)).WillOnce(Return(ErrorFail));
  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_THROW(engine.synthesize("hallo", [](std::span<const std::byte>) {}), EngineError);
}

TEST_F(SapiEngineTest, SynthesizeThrowsWhenSettingOutputFails) {
  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_CALL(voice_, SetOutput(_, _)).WillOnce(Return(ErrorFail));
  EXPECT_THROW(engine.synthesize("hallo", [](std::span<const std::byte>) {}), EngineError);
}

TEST_F(SapiEngineTest, CancelDuringSynthesisAbortsWithoutThrowing) {
  const std::array<std::byte, 2> pcm{std::byte{7}, std::byte{8}};
  // Speak emits twice: the first chunk's sink cancels, so the second Write hits
  // PcmSinkStream's E_ABORT path; Speak then reports a failure, which the engine
  // must swallow because cancellation was requested.
  ON_CALL(voice_, Speak(_, _, _)).WillByDefault([this, &pcm](LPCWSTR, DWORD, ULONG*) {
    emitTo(pcm);
    emitTo(pcm);
    return ErrorFail;
  });

  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_NO_THROW(
      engine.synthesize("hallo", [&engine](std::span<const std::byte>) { engine.cancel(); }));
}

TEST_F(SapiEngineTest, SetRateForwardsClampedValueToSapi) {
  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_CALL(voice_, SetRate(5)).WillOnce(Return(S_OK));
  engine.setRate(5);
}

TEST_F(SapiEngineTest, SetRateClampsOutOfRangeValues) {
  SapiTtsEngine engine{VoiceSelectionPolicy::PreferGerman};
  EXPECT_CALL(voice_, SetRate(10)).WillOnce(Return(S_OK)); // clamped from 99
  engine.setRate(99);
}

} // namespace

#endif // defined(_WIN32)
