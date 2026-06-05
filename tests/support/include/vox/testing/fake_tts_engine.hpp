// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A scriptable in-memory ITtsEngine for tests (no SAPI, no COM).
///
/// Test-support only — never part of a shipped library. It produces
/// deterministic synthetic PCM (one fixed-size chunk per input byte) so pipeline
/// tests (#39, …) can drive the speech path without a real synthesizer, observe
/// streaming, and verify that cancellation stops the stream promptly.
#ifndef VOX_TESTING_FAKE_TTS_ENGINE_HPP
#define VOX_TESTING_FAKE_TTS_ENGINE_HPP

#include <atomic>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/tts/itts_engine.hpp>
#include <vox/tts/rate.hpp>

namespace vox::testing {

/// An ITtsEngine whose output and observed calls are inspected by tests.
class FakeTtsEngine : public vox::tts::ITtsEngine {
public:
  FakeTtsEngine() = default;

  /// Constructs a fake reporting @p format and emitting @p bytesPerByte bytes of
  /// silence per input byte.
  explicit FakeTtsEngine(vox::audio::AudioFormat format, std::size_t bytesPerByte = 2)
      : format_(format), bytesPerByte_(bytesPerByte) {}

  [[nodiscard]] vox::audio::AudioFormat format() const override {
    return format_;
  }

  /// Streams one chunk of silence per input byte, stopping early if cancelled.
  void synthesize(std::string_view utf8Text, const PcmSink& sink) override {
    ++synthesizeCount_;
    lastText_ = std::string(utf8Text);
    cancelled_.store(false);
    chunksEmitted_ = 0;
    bytesEmitted_ = 0;

    const std::vector<std::byte> chunk(bytesPerByte_, std::byte{0});
    for (std::size_t i = 0; i < utf8Text.size(); ++i) {
      if (cancelled_.load()) {
        break;
      }
      if (sink) {
        sink(std::span<const std::byte>(chunk.data(), chunk.size()));
      }
      ++chunksEmitted_;
      bytesEmitted_ += chunk.size();
    }
  }

  void cancel() override {
    cancelled_.store(true);
    cancelCount_.fetch_add(1, std::memory_order_relaxed);
  }

  void setRate(int rate) override {
    rate_ = vox::tts::clampRate(rate); // mirror the interface's clamping contract
  }

  /// @brief The text passed to the most recent `synthesize()` call.
  [[nodiscard]] const std::string& lastText() const noexcept {
    return lastText_;
  }

  /// @brief The most recently set rate (0 until `setRate` is called).
  [[nodiscard]] int rate() const noexcept {
    return rate_;
  }

  /// @brief How many times `synthesize()` has been called.
  [[nodiscard]] int synthesizeCount() const noexcept {
    return synthesizeCount_;
  }

  /// @brief How many times `cancel()` has been called.
  [[nodiscard]] int cancelCount() const noexcept {
    return cancelCount_.load(std::memory_order_relaxed);
  }

  /// @brief Chunks emitted by the most recent `synthesize()` call.
  [[nodiscard]] std::size_t chunksEmitted() const noexcept {
    return chunksEmitted_;
  }

  /// @brief Total PCM bytes emitted by the most recent `synthesize()` call.
  [[nodiscard]] std::size_t bytesEmitted() const noexcept {
    return bytesEmitted_;
  }

private:
  vox::audio::AudioFormat format_;
  std::size_t bytesPerByte_{2};
  std::string lastText_;
  int rate_{0};
  int synthesizeCount_{0};
  std::atomic<int> cancelCount_{0}; // cancel() is thread-safe per the interface
  std::size_t chunksEmitted_{0};
  std::size_t bytesEmitted_{0};
  std::atomic<bool> cancelled_{false};
};

} // namespace vox::testing

#endif // VOX_TESTING_FAKE_TTS_ENGINE_HPP
