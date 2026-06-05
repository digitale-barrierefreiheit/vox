// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The audio-output seam: play PCM, flush instantly for barge-in.
///
/// `IAudioSink` abstracts the output device (WASAPI today) so Core and tests
/// depend on the interface, not COM. The producer (the TTS/output path) feeds
/// PCM with `write()`; a keypress triggers `flush()` to drop queued and in-flight
/// audio for instant barge-in (architecture §6.1 / ADR-10). The Windows
/// `WasapiAudioSink` implements it; `FakeAudioSink` fakes it for tests.
#ifndef VOX_AUDIO_IAUDIO_SINK_HPP
#define VOX_AUDIO_IAUDIO_SINK_HPP

#include <cstddef>
#include <span>

namespace vox::audio {

/// Interface to a low-latency PCM output sink.
class IAudioSink {
public:
  IAudioSink() = default;
  IAudioSink(const IAudioSink&) = delete;
  IAudioSink& operator=(const IAudioSink&) = delete;
  IAudioSink(IAudioSink&&) = delete;
  IAudioSink& operator=(IAudioSink&&) = delete;
  virtual ~IAudioSink() = default;

  /// @brief Begins rendering (acquires the device, starts the render thread).
  virtual void start() = 0;

  /// @brief Enqueues PCM (in the sink's source format) for playback. Called on
  ///        the producer thread; may block briefly when the buffer is full.
  virtual void write(std::span<const std::byte> pcm) = 0;

  /// @brief Drops all queued and in-flight audio within one buffer period.
  ///        Safe to call from another thread (barge-in on a keypress).
  virtual void flush() = 0;

  /// @brief Stops rendering and releases the device.
  virtual void stop() = 0;
};

} // namespace vox::audio

#endif // VOX_AUDIO_IAUDIO_SINK_HPP
