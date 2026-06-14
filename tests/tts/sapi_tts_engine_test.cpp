// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for SapiTtsEngine's COM construction/synthesis error
///        handling, driven through the #68 test seam with mock COM — no
///        installed SAPI voice required, so these run anywhere (incl. the
///        coverage job) and fault-inject the failure paths.
#if defined(_WIN32)

#  include <array>
#  include <bit>
#  include <cstddef>
#  include <cstdint>
#  include <cstring>
#  include <cwchar>
#  include <new>
#  include <span>
#  include <stdexcept>
#  include <string_view>
#  include <vector>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/audio/audio_format.hpp>
#  include <vox/tts/errors.hpp>
#  include <vox/tts/sapi_test_seam.hpp>
#  include <vox/tts/sapi_tts_engine.hpp>
#  include <vox/tts/voice_selection.hpp>

#  include "sapi_com_mocks.hpp"

// --- Out-of-memory injection -------------------------------------------------
// WRL's MakeAllocator allocates with `operator new(size, nothrow)` only (see
// <wrl/implements.h>: "override one operator only ... to enable different memory
// allocation model"). So a single-shot, thread-local failure of *that* operator
// makes the next Make<PcmSinkStream>() return a null ComPtr — exercising
// makeOutputStream's allocation-failure branch — while every other allocation
// (and its matching delete) behaves exactly as normal.
namespace {
bool& failNextNothrowNewFlag() {
  static thread_local bool flag = false; // function-local static, not a mutable global
  return flag;
}

void failNextNothrowNew() {
  failNextNothrowNewFlag() = true;
}
} // namespace

// Replaces only the nothrow operator new for this test binary. When not armed it
// forwards to the default operator new, so the object is allocated — and later
// freed by the default operator delete — exactly as usual.
void* operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept {
  if (failNextNothrowNewFlag()) {
    failNextNothrowNewFlag() = false;
    return nullptr;
  }
  try {
    return ::operator new(size);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}

// -----------------------------------------------------------------------------

namespace {

using vox::tts::EngineError;
using vox::tts::SapiTtsEngine;
using vox::tts::VoiceSelectionRequest;
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

/// A dedicated exception a misbehaving PcmSink might raise (S112: not a generic
/// one), to prove the output stream firewalls it into E_FAIL at the COM boundary.
class SinkError : public std::runtime_error {
public:
  SinkError() : std::runtime_error("sink boom") {}
};

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

  EXPECT_THROW(SapiTtsEngine{}, EngineError);
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

  /// Runs one synthesize() and invokes @p probe with the engine's live output
  /// stream (its PcmSinkStream) from inside Speak, where the stream is valid.
  /// Lets the suite test the stream's IStream/ISpStreamFormat surface through the
  /// engine — no need to expose PcmSinkStream. @p sink is the caller's PcmSink.
  template<typename Probe>
  void withOutputStream(Probe probe, const vox::tts::ITtsEngine::PcmSink& sink) {
    // EXPECT_CALL (not ON_CALL) so gmock verifies Speak — and therefore the probe
    // — actually ran; a synthesize() that short-circuited would fail the test.
    EXPECT_CALL(voice_, Speak(_, _, _)).WillOnce([this, probe](LPCWSTR, DWORD, ULONG*) {
      if (capturedOutput_ == nullptr) {
        return E_FAIL;
      }
      ISpStreamFormat* stream = nullptr;
      if (FAILED(capturedOutput_->QueryInterface(IID_PPV_ARGS(&stream))) || stream == nullptr) {
        return E_FAIL;
      }
      probe(stream);
      stream->Release();
      return S_OK;
    });
    SapiTtsEngine engine{};
    engine.synthesize("hallo", sink);
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
  const SapiTtsEngine engine{};
  EXPECT_EQ(engine.selectedVoice().id, "VOX-TEST-VOICE-DE");
  EXPECT_EQ(engine.selectedVoice().language, "de");
  EXPECT_EQ(engine.selectedVoice().choice, vox::tts::VoiceChoice::RequestedLanguage);
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
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

TEST_F(SapiEngineTest, ThrowsWhenTokenEnumerationFails) {
  // Both discovery passes (classic + OneCore, #52) fail to enumerate.
  EXPECT_CALL(category_, EnumTokens(_, _, _)).WillRepeatedly(Return(ErrorFail));
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

TEST_F(SapiEngineTest, ThrowsWhenActivatingTheVoiceFails) {
  EXPECT_CALL(voice_, SetVoice(_)).WillOnce(Return(ErrorFail));
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

TEST_F(SapiEngineTest, ThrowsWhenGermanIsRequiredButTheVoiceIsEnglish) {
  makeVoiceEnglish();
  VoiceSelectionRequest requireGerman; // language defaults to "de"
  requireGerman.required = true;
  EXPECT_THROW(SapiTtsEngine{requireGerman}, EngineError);
}

TEST_F(SapiEngineTest, FallsBackToTheEnglishVoiceWhenNoGermanOneExists) {
  makeVoiceEnglish();
  const SapiTtsEngine engine{};
  EXPECT_EQ(engine.selectedVoice().language, "en");
  EXPECT_EQ(engine.selectedVoice().choice, vox::tts::VoiceChoice::Fallback);
}

TEST_F(SapiEngineTest, SynthesizeEmptyTextIsANoOp) {
  SapiTtsEngine engine{};
  EXPECT_CALL(voice_, Speak(_, _, _)).Times(0);
  engine.synthesize("", [](std::span<const std::byte>) { /* never called: empty input */ });
}

TEST_F(SapiEngineTest, SynthesizeThrowsOnInvalidUtf8) {
  SapiTtsEngine engine{};
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

  SapiTtsEngine engine{};
  std::vector<std::byte> received;
  engine.synthesize("hallo", [&received](std::span<const std::byte> chunk) {
    received.insert(received.end(), chunk.begin(), chunk.end());
  });
  EXPECT_EQ(received.size(), pcm.size());
}

TEST_F(SapiEngineTest, SynthesizeThrowsWhenSpeakFails) {
  EXPECT_CALL(voice_, Speak(_, _, _)).WillOnce(Return(ErrorFail));
  SapiTtsEngine engine{};
  EXPECT_THROW(engine.synthesize("hallo", [](std::span<const std::byte>) {}), EngineError);
}

TEST_F(SapiEngineTest, SynthesizeThrowsWhenTheOutputStreamCannotBeAllocated) {
  SapiTtsEngine engine{};
  EXPECT_CALL(voice_, Speak(_, _, _)).Times(0); // the throw precedes Speak
  // The next nothrow allocation is Make<PcmSinkStream>'s; force it to fail so the
  // engine sees a null output stream and throws its allocation-failure EngineError.
  failNextNothrowNew();
  EXPECT_THROW(engine.synthesize("hallo", [](std::span<const std::byte>) {}), EngineError);
}

TEST_F(SapiEngineTest, SynthesizeThrowsWhenSettingOutputFails) {
  SapiTtsEngine engine{};
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

  SapiTtsEngine engine{};
  EXPECT_NO_THROW(
      engine.synthesize("hallo", [&engine](std::span<const std::byte>) { engine.cancel(); }));
}

TEST_F(SapiEngineTest, SetRateForwardsClampedValueToSapi) {
  SapiTtsEngine engine{};
  EXPECT_CALL(voice_, SetRate(5)).WillOnce(Return(S_OK));
  engine.setRate(5);
}

TEST_F(SapiEngineTest, SetRateClampsOutOfRangeValues) {
  SapiTtsEngine engine{};
  EXPECT_CALL(voice_, SetRate(10)).WillOnce(Return(S_OK)); // clamped from 99
  engine.setRate(99);
}

TEST_F(SapiEngineTest, FormatReportsTheFixedOutputShape) {
  const SapiTtsEngine engine{};
  const vox::audio::AudioFormat format = engine.format();
  EXPECT_EQ(format.sampleRate, 22050U);
  EXPECT_EQ(format.bitsPerSample, 16U);
  EXPECT_EQ(format.channels, 1U);
}

TEST_F(SapiEngineTest, EnumerationStopsWhenSettingTheCategoryIdFails) {
  // Neither category id can be set (classic + OneCore, #52): no voices
  // enumerate, so selection finds nothing and construction fails.
  EXPECT_CALL(category_, SetId(_, _)).WillRepeatedly(Return(ErrorFail));
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

TEST_F(SapiEngineTest, EnumeratesTheClassicAndThenTheOneCoreCategory) {
  // Discovery must look at both catalogues (#52), classic first — the merge
  // gives classic precedence, so the order is part of the contract.
  const ::testing::InSequence ordered;
  EXPECT_CALL(
      category_,
      SetId(::testing::StrEq(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices"), _))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(
      category_,
      SetId(::testing::StrEq(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices"),
            _))
      .WillOnce(Return(S_OK));
  const SapiTtsEngine engine{};
  EXPECT_EQ(engine.selectedVoice().id, "VOX-TEST-VOICE-DE");
}

TEST_F(SapiEngineTest, DiscoversAVoiceOnlyVisibleInTheOneCoreCategory) {
  // The Settings-installed-voice case (#52): the classic catalogue is empty,
  // only the OneCore pass serves the (German) token — it must still be found
  // and selected, with no registry bridge.
  std::wstring currentCategory;
  ON_CALL(category_, SetId(_, _)).WillByDefault([&currentCategory](LPCWSTR id, BOOL) {
    currentCategory = (id != nullptr) ? id : L"";
    return S_OK;
  });
  ON_CALL(enumTokens_, Next(_, _, _))
      .WillByDefault([this, &currentCategory](ULONG, ISpObjectToken** pelt, ULONG* fetched) {
        if (const bool oneCorePass = currentCategory.contains(L"Speech_OneCore");
            !oneCorePass || tokenServed_) {
          if (fetched != nullptr) {
            *fetched = 0;
          }
          return S_FALSE; // the classic pass sees nothing
        }
        tokenServed_ = true;
        *pelt = &token_;
        if (fetched != nullptr) {
          *fetched = 1;
        }
        return S_OK;
      });

  VoiceSelectionRequest requireGerman; // language defaults to "de"
  requireGerman.required = true;
  const SapiTtsEngine engine{requireGerman};
  EXPECT_EQ(engine.selectedVoice().id, "VOX-TEST-VOICE-DE");
  EXPECT_EQ(engine.selectedVoice().language, "de");
}

TEST_F(SapiEngineTest, SkipsATokenWhoseIdCannotBeRead) {
  EXPECT_CALL(token_, GetId(_)).WillRepeatedly([](LPWSTR* out) {
    if (out != nullptr) {
      *out = nullptr;
    }
    return ErrorFail; // the only token is skipped -> no usable voice
  });
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

TEST_F(SapiEngineTest, SkipsATokenWithAnEmptyId) {
  EXPECT_CALL(token_, GetId(_)).WillRepeatedly([](LPWSTR* out) {
    *out = coTaskString(L""); // empty id -> descriptor dropped
    return S_OK;
  });
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

TEST_F(SapiEngineTest, OutputStreamRejectsANullWriteBuffer) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        ULONG written = 1;
        EXPECT_EQ(stream->Write(nullptr, 4, &written), E_POINTER);
        EXPECT_EQ(written, 0U);
      },
      [](std::span<const std::byte>) { ADD_FAILURE() << "sink must not run on a rejected write"; });
}

TEST_F(SapiEngineTest, OutputStreamWriteOfZeroBytesIsANoOp) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        const std::byte byte{1};
        ULONG written = 9;
        EXPECT_EQ(stream->Write(&byte, 0, &written), S_OK);
        EXPECT_EQ(written, 0U);
      },
      [](std::span<const std::byte>) {
        ADD_FAILURE() << "sink must not run on a zero-byte write";
      });
}

TEST_F(SapiEngineTest, OutputStreamReportsItsWaveFormat) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        GUID formatId{};
        WAVEFORMATEX* waveFormat = nullptr;
        EXPECT_EQ(stream->GetFormat(&formatId, &waveFormat), S_OK);
        EXPECT_EQ(formatId, SPDFID_WaveFormatEx);
        ASSERT_NE(waveFormat, nullptr);
        EXPECT_EQ(waveFormat->nSamplesPerSec, 22050U); // the fixed 22050/16/1 output
        EXPECT_EQ(waveFormat->wBitsPerSample, 16U);
        EXPECT_EQ(waveFormat->nChannels, 1U);
        ::CoTaskMemFree(waveFormat);
      },
      [](std::span<const std::byte>) { ADD_FAILURE() << "sink must not run on GetFormat"; });
}

TEST_F(SapiEngineTest, OutputStreamWriteFirewallsASinkException) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        const std::byte byte{1};
        ULONG written = 1;
        // The sink throws; PcmSinkStream must turn that into E_FAIL rather than
        // let it cross the COM ABI boundary, and report zero bytes written.
        EXPECT_EQ(stream->Write(&byte, 1, &written), E_FAIL);
        EXPECT_EQ(written, 0U);
      },
      [](std::span<const std::byte>) { throw SinkError{}; });
}

/// A no-op probe sink: a metadata stream call (Read/SetSize/Stat-null/...) that
/// forwards no PCM must leave the caller's sink idle.
const vox::tts::ITtsEngine::PcmSink kIdleSink = [](std::span<const std::byte>) {
  ADD_FAILURE() << "sink must not run for a metadata stream call";
};

/// A sink that silently accepts PCM, for probes that first Write real bytes (to
/// advance the stream position) before exercising Seek/Stat.
const vox::tts::ITtsEngine::PcmSink kAcceptingSink = [](std::span<const std::byte>) {
  // Intentionally empty: these probes only need the stream position to advance.
};

/// Reads a ULARGE_INTEGER's 64-bit value without touching `QuadPart` directly:
/// the production Seek writes that union member, but Sonar cannot track the write
/// across the COM call (cpp:S6232), so bit_cast the whole union to read its value.
std::uint64_t quadValue(const ULARGE_INTEGER& value) {
  return std::bit_cast<std::uint64_t>(value);
}

TEST_F(SapiEngineTest, OutputStreamReadIsNotImplemented) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        std::array<std::byte, 4> buffer{};
        ULONG read = 7;
        // PcmSinkStream is write-only: ISequentialStream::Read is rejected.
        EXPECT_EQ(stream->Read(buffer.data(), static_cast<ULONG>(buffer.size()), &read), E_NOTIMPL);
      },
      kIdleSink);
}

TEST_F(SapiEngineTest, OutputStreamSeekTracksTheAppendOnlyPosition) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        // First write three bytes so the position is non-zero, then probe Seek.
        const std::array<std::byte, 3> pcm{std::byte{1}, std::byte{2}, std::byte{3}};
        ULONG written = 0;
        ASSERT_EQ(stream->Write(pcm.data(), static_cast<ULONG>(pcm.size()), &written), S_OK);

        ULARGE_INTEGER where{};
        LARGE_INTEGER move{};

        // SET: absolute offset from the start.
        move.QuadPart = 1;
        EXPECT_EQ(stream->Seek(move, STREAM_SEEK_SET, &where), S_OK);
        EXPECT_EQ(quadValue(where), 1U);

        // CUR: relative to the current position (now 1) — advance by 2.
        move.QuadPart = 2;
        EXPECT_EQ(stream->Seek(move, STREAM_SEEK_CUR, &where), S_OK);
        EXPECT_EQ(quadValue(where), 3U);

        // END: append-only stream reports no real end, so END uses the current
        // position as its base — a zero move leaves it unchanged.
        move.QuadPart = 0;
        EXPECT_EQ(stream->Seek(move, STREAM_SEEK_END, &where), S_OK);
        EXPECT_EQ(quadValue(where), 3U);

        // A null out-param is allowed (the seek still succeeds).
        move.QuadPart = 0;
        EXPECT_EQ(stream->Seek(move, STREAM_SEEK_SET, nullptr), S_OK);
      },
      kAcceptingSink);
}

TEST_F(SapiEngineTest, OutputStreamSeekRejectsAnUnknownOriginAndNegativeTarget) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        ULARGE_INTEGER where{};
        LARGE_INTEGER move{};

        // An origin that is neither SET/CUR/END is rejected outright.
        move.QuadPart = 0;
        EXPECT_EQ(stream->Seek(move, 99U, &where), STG_E_INVALIDFUNCTION);

        // A target before the start of the stream is rejected (negative offset
        // from position 0 via SET).
        move.QuadPart = -1;
        EXPECT_EQ(stream->Seek(move, STREAM_SEEK_SET, &where), STG_E_INVALIDFUNCTION);
      },
      kIdleSink);
}

TEST_F(SapiEngineTest, OutputStreamUnsupportedIStreamMethodsReturnNotImplemented) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        const ULARGE_INTEGER zero{};
        EXPECT_EQ(stream->SetSize(zero), E_NOTIMPL);
        EXPECT_EQ(stream->CopyTo(nullptr, zero, nullptr, nullptr), E_NOTIMPL);
        EXPECT_EQ(stream->LockRegion(zero, zero, 0), E_NOTIMPL);
        EXPECT_EQ(stream->UnlockRegion(zero, zero, 0), E_NOTIMPL);
        IStream* clone = nullptr;
        EXPECT_EQ(stream->Clone(&clone), E_NOTIMPL);
        EXPECT_EQ(clone, nullptr);
      },
      kIdleSink);
}

TEST_F(SapiEngineTest, OutputStreamCommitAndRevertSucceed) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        // The stream forwards as it writes; there is nothing to flush or roll
        // back, so both transactional no-ops report success.
        EXPECT_EQ(stream->Commit(STGC_DEFAULT), S_OK);
        EXPECT_EQ(stream->Revert(), S_OK);
      },
      kIdleSink);
}

TEST_F(SapiEngineTest, OutputStreamStatReportsTheWrittenSize) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        const std::array<std::byte, 5> pcm{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
                                           std::byte{5}};
        ULONG written = 0;
        ASSERT_EQ(stream->Write(pcm.data(), static_cast<ULONG>(pcm.size()), &written), S_OK);

        STATSTG stat{};
        stat.type = 0xBADU;            // overwritten by Stat
        stat.cbSize.QuadPart = 0xBADU; // overwritten by Stat
        EXPECT_EQ(stream->Stat(&stat, STATFLAG_NONAME), S_OK);
        EXPECT_EQ(stat.type, static_cast<DWORD>(STGTY_STREAM));
        EXPECT_EQ(stat.cbSize.QuadPart, pcm.size()); // bytes forwarded so far
        EXPECT_EQ(stat.pwcsName, nullptr);           // STATFLAG_NONAME: no name
      },
      kAcceptingSink);
}

TEST_F(SapiEngineTest, OutputStreamStatRejectsANullTarget) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        EXPECT_EQ(stream->Stat(nullptr, STATFLAG_NONAME), STG_E_INVALIDPOINTER);
      },
      kIdleSink);
}

TEST_F(SapiEngineTest, OutputStreamGetFormatToleratesNullOutParams) {
  withOutputStream(
      [](ISpStreamFormat* stream) {
        // Both out-params optional: a caller may probe only one (or neither).
        EXPECT_EQ(stream->GetFormat(nullptr, nullptr), S_OK);
      },
      kIdleSink);
}

/// The classic and OneCore voice categories both fail to instantiate, so no
/// voices enumerate and selection finds nothing — driving openVoiceCategory's
/// create-failure early return (the category factory returns a fault HRESULT).
TEST_F(SapiEngineTest, ConstructionFailsWhenTheVoiceCategoryCannotBeCreated) {
  vox::tts::testing::setTokenCategoryFactory([](ISpObjectTokenCategory** out) {
    *out = nullptr;
    return ErrorFail; // createTokenCategory fails -> openVoiceCategory returns null
  });
  EXPECT_THROW(SapiTtsEngine{}, EngineError);
}

/// Balances a successful CoInitializeEx with CoUninitialize on scope exit, even
/// if the body throws.
class ComScope {
public:
  ComScope() = default;

  ~ComScope() {
    ::CoUninitialize();
  }

  ComScope(const ComScope&) = delete;
  ComScope& operator=(const ComScope&) = delete;
  ComScope(ComScope&&) = delete;
  ComScope& operator=(ComScope&&) = delete;
};

TEST_F(SapiEngineTest, ToleratesAComApartmentAlreadyInitializedInAnotherMode) {
  // Pre-initialize COM as STA on this thread; the engine's ComApartment then
  // requests MTA and gets RPC_E_CHANGED_MODE, which it must tolerate (not own).
  // CoInitializeEx may return S_OK or S_FALSE (already initialized on this
  // thread); both own a reference that ComScope balances even if the engine
  // constructor throws.
  ASSERT_TRUE(SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
  const ComScope balance;
  const SapiTtsEngine engine{};
  EXPECT_EQ(engine.selectedVoice().id, "VOX-TEST-VOICE-DE");
}

} // namespace

#endif // defined(_WIN32)
