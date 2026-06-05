// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows WASAPI shared-mode implementation of IAudioSink (ADR-10).
///
/// Plays the TTS engine's PCM through the default render device at low latency.
/// A producer feeds PCM via `write()` (converted to the device mix format and
/// pushed to a lock-free `PcmRing`); a dedicated event-driven render thread
/// copies ring -> device buffer with no allocation or locking. `flush()` gives
/// instant barge-in. COM/WASAPI types are hidden behind a pImpl. Windows-only —
/// the portable pieces it relies on (PcmRing, PcmConverter) are what the
/// sanitizer/clang-tidy build sees.
#ifndef VOX_AUDIO_WASAPI_AUDIO_SINK_HPP
#define VOX_AUDIO_WASAPI_AUDIO_SINK_HPP

// WasapiAudioSink is implemented only on Windows (see src/audio/CMakeLists.txt).
// Declaring it only there turns any accidental non-Windows use into a clear
// "undeclared identifier" at compile time rather than a confusing link error.
#if defined(_WIN32)

#  include <cstddef>
#  include <memory>
#  include <span>

#  include <vox/audio/audio_format.hpp>
#  include <vox/audio/iaudio_sink.hpp>

namespace vox::audio {

/// WASAPI shared-mode audio sink. Source PCM is resampled to the device mix
/// format and rendered with instant barge-in.
class WasapiAudioSink : public IAudioSink {
public:
  /// @brief Creates a sink that will play @p sourceFormat PCM (16-bit mono).
  ///        The device is acquired in `start()`, not here.
  explicit WasapiAudioSink(AudioFormat sourceFormat);
  ~WasapiAudioSink() override;

  WasapiAudioSink(const WasapiAudioSink&) = delete;
  WasapiAudioSink& operator=(const WasapiAudioSink&) = delete;
  WasapiAudioSink(WasapiAudioSink&&) = delete;
  WasapiAudioSink& operator=(WasapiAudioSink&&) = delete;

  /// @brief Acquires the default render device and starts the render thread.
  /// @throws std::runtime_error if no render device exists or WASAPI init fails.
  void start() override;

  /// @brief Converts @p pcm to the device format and queues it for playback.
  void write(std::span<const std::byte> pcm) override;

  /// @brief Drops queued + in-flight audio within one buffer period (barge-in).
  void flush() override;

  /// @brief Stops the render thread and releases the device.
  void stop() override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vox::audio

#endif // defined(_WIN32)

#endif // VOX_AUDIO_WASAPI_AUDIO_SINK_HPP
