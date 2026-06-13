// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A scriptable in-memory IAudioSink for tests (no WASAPI, no COM).
///
/// Test-support only — never part of a shipped library. It records what the
/// pipeline plays so tests (#39, …) can assert "this utterance produced this
/// PCM" and "barge-in flushed the queue" without a sound device. Intended for
/// single-threaded test use; the real WasapiAudioSink owns the cross-thread
/// synchronization the interface describes.
#ifndef VOX_TESTING_FAKE_AUDIO_SINK_HPP
#define VOX_TESTING_FAKE_AUDIO_SINK_HPP

#include <cstddef>
#include <span>
#include <vector>

#include <vox/audio/iaudio_sink.hpp>

namespace vox::testing {

/// An IAudioSink that buffers written PCM and counts calls for inspection.
class FakeAudioSink : public vox::audio::IAudioSink {
public:
  void start() override {
    started_ = true;
  }

  void write(std::span<const std::byte> pcm) override {
    if (!started_) {
      return; // the real sink ignores writes before start()
    }
    buffered_.insert(buffered_.end(), pcm.begin(), pcm.end());
    bytesWritten_ += pcm.size();
    ++writeCount_;
  }

  void drain() override {
    if (!started_) {
      return; // before-start no-op, matching write() and the real sink
    }
    ++drainCount_; // end of stream: the fake buffers raw PCM, so nothing to flush
  }

  void flush() override {
    if (!started_) {
      return; // before-start no-op, matching write()
    }
    ++flushCount_;
    buffered_.clear(); // barge-in drops what is queued
  }

  void stop() override {
    started_ = false;
    buffered_.clear(); // the real sink drops queued audio on stop()
  }

  /// @brief Whether the sink is currently started.
  [[nodiscard]] bool started() const noexcept {
    return started_;
  }

  /// @brief PCM still queued (since the last flush).
  [[nodiscard]] const std::vector<std::byte>& buffered() const noexcept {
    return buffered_;
  }

  /// @brief Cumulative bytes ever written (not reset by flush).
  [[nodiscard]] std::size_t bytesWritten() const noexcept {
    return bytesWritten_;
  }

  /// @brief Number of `write()` calls.
  [[nodiscard]] int writeCount() const noexcept {
    return writeCount_;
  }

  /// @brief Number of `flush()` calls.
  [[nodiscard]] int flushCount() const noexcept {
    return flushCount_;
  }

  /// @brief Number of `drain()` calls (one per completed utterance).
  [[nodiscard]] int drainCount() const noexcept {
    return drainCount_;
  }

private:
  std::vector<std::byte> buffered_;
  std::size_t bytesWritten_{0};
  int writeCount_{0};
  int flushCount_{0};
  int drainCount_{0};
  bool started_{false};
};

} // namespace vox::testing

#endif // VOX_TESTING_FAKE_AUDIO_SINK_HPP
