// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Integration tests for vox::audio::WasapiAudioSink against a real device.
///
/// OS glue, not the pure-core suite: these drive the live WASAPI render path.
/// GitHub runners have no audio endpoint, so the `windows-audio` CI job installs
/// a virtual device and sets `VOX_REQUIRE_AUDIO_DEVICE=1` — under that flag a
/// missing device fails the build; elsewhere (a dev box, other runners) the
/// tests skip when no device exists. Audible correctness is a manual dev-box
/// check; here we assert the start -> write -> flush -> stop path does not error.
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/wasapi_audio_sink.hpp>

namespace {

using vox::audio::AudioFormat;
using vox::audio::WasapiAudioSink;

/// A short 16-bit mono sine tone (audible on a real device for manual checks).
std::vector<std::byte> sineTone(double frequencyHz, double seconds, std::uint32_t sampleRate) {
  const auto sampleCount = static_cast<std::size_t>(seconds * sampleRate);
  std::vector<std::byte> pcm(sampleCount * sizeof(std::int16_t));
  for (std::size_t i = 0; i < sampleCount; ++i) {
    const double time = static_cast<double>(i) / sampleRate;
    const double radians = 2.0 * std::numbers::pi * frequencyHz * time;
    const auto sample = static_cast<std::int16_t>(std::sin(radians) * 8000.0);
    std::memcpy(pcm.data() + (i * sizeof(std::int16_t)), &sample, sizeof(sample));
  }
  return pcm;
}

class WasapiAudioSinkTest : public ::testing::Test {
protected:
  /// True on the CI audio job: a missing device must fail, not silently skip.
  static bool audioRequired() {
    char* value = nullptr;
    if (std::size_t size = 0;
        ::_dupenv_s(&value, &size, "VOX_REQUIRE_AUDIO_DEVICE") != 0 || value == nullptr) {
      return false;
    }
    const bool required = std::string_view(value) == "1";
    std::free(value);
    return required;
  }

  void SetUp() override {
    sink_ = std::make_unique<WasapiAudioSink>(AudioFormat{22050, 16, 1});
    try {
      sink_->start();
    } catch (const std::runtime_error& error) {
      sink_.reset();
      if (audioRequired()) {
        FAIL() << "VOX_REQUIRE_AUDIO_DEVICE is set but start() failed: " << error.what();
      }
      GTEST_SKIP() << "No usable audio render device (start() failed: " << error.what() << ").";
    }
  }

  void TearDown() override {
    if (sink_) {
      sink_->stop();
    }
  }

  std::unique_ptr<WasapiAudioSink> sink_;
};

TEST_F(WasapiAudioSinkTest, PlaysAToneWithoutError) {
  const std::vector<std::byte> tone = sineTone(440.0, 0.15, 22050);
  EXPECT_NO_THROW({
    sink_->write(tone);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });
}

TEST_F(WasapiAudioSinkTest, FlushAfterWriteIsBargeIn) {
  const std::vector<std::byte> longTone = sineTone(440.0, 0.5, 22050);
  const std::vector<std::byte> higherTone = sineTone(880.0, 0.1, 22050);
  EXPECT_NO_THROW({
    sink_->write(longTone);   // queue a longer tone
    sink_->flush();           // barge-in
    sink_->write(higherTone); // a fresh, higher tone plays
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });
}

TEST_F(WasapiAudioSinkTest, StopThenRestart) {
  const std::vector<std::byte> first = sineTone(440.0, 0.05, 22050);
  const std::vector<std::byte> second = sineTone(660.0, 0.05, 22050);
  sink_->write(first);
  sink_->stop();
  EXPECT_NO_THROW(sink_->start()); // re-acquire the device
  sink_->write(second);
}

} // namespace
