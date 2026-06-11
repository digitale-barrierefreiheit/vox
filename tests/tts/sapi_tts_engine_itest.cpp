// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Integration tests for vox::tts::SapiTtsEngine against real SAPI5.
///
/// OS glue, not the pure-core suite (architecture §8.6.2): these drive the live
/// SAPI engine, synthesizing to memory (no audio device) so they run unattended
/// on CI. They adapt to the installed voices and synthesize German text with
/// whatever voice exists, so the path is exercised on an English-only runner too.
///
/// The de-DE CI job sets `VOX_REQUIRE_GERMAN_VOICE=1`; under that flag a missing
/// or SAPI-invisible German voice becomes a hard failure rather than a silent
/// English fallback, so the German path is genuinely gated where it is provided.
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_buffer.hpp>
#include <vox/tts/sapi_tts_engine.hpp>
#include <vox/tts/voice_selection.hpp>

namespace {

using vox::audio::AudioFormat;
using vox::audio::PcmBuffer;
using vox::tts::SapiTtsEngine;
using vox::tts::VoiceSelectionPolicy;

PcmBuffer synthesizeToBuffer(SapiTtsEngine& engine, std::string_view text) {
  PcmBuffer buffer;
  buffer.format = engine.format();
  engine.synthesize(text, [&buffer](std::span<const std::byte> chunk) { append(buffer, chunk); });
  return buffer;
}

class SapiTtsEngineTest : public ::testing::Test {
protected:
  /// True when the environment demands a German voice (the de-DE CI job). Then a
  /// missing voice fails the build instead of falling back to English silently.
  static bool germanRequired() {
    char* value = nullptr;
    if (std::size_t size = 0;
        ::_dupenv_s(&value, &size, "VOX_REQUIRE_GERMAN_VOICE") != 0 || value == nullptr) {
      return false;
    }
    const bool required = std::string_view(value) == "1";
    std::free(value);
    return required;
  }

  /// Builds an engine under @p policy, or returns nullptr if SAPI/voices are
  /// unavailable for it.
  static std::unique_ptr<SapiTtsEngine> makeEngine(VoiceSelectionPolicy policy) {
    try {
      return std::make_unique<SapiTtsEngine>(policy);
    } catch (const std::runtime_error&) {
      return nullptr;
    }
  }

  void SetUp() override {
    engine_ = makeEngine(VoiceSelectionPolicy::PreferGerman);
    if (!engine_) {
      if (germanRequired()) {
        FAIL() << "VOX_REQUIRE_GERMAN_VOICE is set but no usable SAPI voice was found.";
      }
      GTEST_SKIP() << "No SAPI voice installed on this machine.";
    }
  }

  std::unique_ptr<SapiTtsEngine> engine_;
};

TEST_F(SapiTtsEngineTest, ExposesAVoiceAndTheForcedFormat) {
  EXPECT_EQ(engine_->format(), (AudioFormat{22050, 16, 1}));
  EXPECT_FALSE(engine_->selectedVoice().id.empty());
}

TEST_F(SapiTtsEngineTest, SynthesizesTextToWholeFramePcm) {
  const PcmBuffer pcm = synthesizeToBuffer(*engine_, "Hallo Welt");
  EXPECT_FALSE(isEmpty(pcm));
  EXPECT_EQ(byteCount(pcm) % bytesPerFrame(engine_->format()), 0U); // whole frames only
  EXPECT_GT(frameCount(pcm), 0U);
}

TEST_F(SapiTtsEngineTest, CancelFromTheSinkPreventsFurtherChunks) {
  constexpr std::string_view LongText =
      "Dies ist ein deutlich laengerer Satz, der genug Audio erzeugt, "
      "damit ein Abbruch nach dem ersten Block wirksam werden kann.";

  int chunks = 0;
  engine_->synthesize(LongText, [&chunks, this](std::span<const std::byte>) {
    ++chunks;
    engine_->cancel(); // barge-in during the first delivered chunk
  });

  // Cancelling during the first chunk must prevent any further chunk, regardless
  // of how SAPI buffers its output (one large Write or several small ones).
  EXPECT_EQ(chunks, 1);
}

TEST_F(SapiTtsEngineTest, SetRateDoesNotThrow) {
  engine_->setRate(5);
  engine_->setRate(-100); // clamped internally
  EXPECT_NO_THROW(synthesizeToBuffer(*engine_, "Test"));
}

// The require-German path against real SAPI: it must select a German voice on
// the de-DE runner (gated by the environment flag), runs where a German voice
// happens to be installed (dev box), and skips on machines without one.
TEST_F(SapiTtsEngineTest, RequireGermanSelectsAGermanVoiceWhereAvailable) {
  const std::unique_ptr<SapiTtsEngine> german = makeEngine(VoiceSelectionPolicy::RequireGerman);
  if (!german) {
    if (germanRequired()) {
      FAIL() << "VOX_REQUIRE_GERMAN_VOICE is set but RequireGerman found no German voice "
                "(language provisioning failed, or OneCore voice discovery (#52) is broken).";
    }
    GTEST_SKIP() << "No German SAPI voice installed on this machine.";
  }
  EXPECT_TRUE(german->selectedVoice().isGerman);
  const PcmBuffer pcm = synthesizeToBuffer(*german, "Guten Tag, dies ist ein Test.");
  EXPECT_FALSE(isEmpty(pcm));
}

} // namespace
