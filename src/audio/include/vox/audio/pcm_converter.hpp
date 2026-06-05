// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Streaming PCM conversion from the TTS format to a device mix format.
///
/// WASAPI shared mode plays in the device's mix format (commonly 48 kHz float32
/// stereo), while the TTS engine emits 16-bit mono at its own rate. `PcmConverter`
/// bridges the two: sample-rate (linear interpolation), channel up-mix, and
/// sample-format. It is a pure, OS-independent core (unit-tested, sanitizer-
/// covered); the WASAPI layer only drives it. It is stateful — the resampler
/// phase carries across `convert()` calls — so use one per stream.
#ifndef VOX_AUDIO_PCM_CONVERTER_HPP
#define VOX_AUDIO_PCM_CONVERTER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <vox/audio/audio_format.hpp>

namespace vox::audio {

/// How individual samples are encoded in a PCM stream.
enum class SampleFormat : std::uint8_t {
  Int16,   ///< 16-bit signed integer, little-endian.
  Float32, ///< 32-bit IEEE float in [-1, 1].
};

/// Converts 16-bit mono PCM to a target device format, chunk by chunk.
class PcmConverter {
public:
  /// @param source         input format (must be 16-bit, mono).
  /// @param targetRate     output sample rate, in Hz.
  /// @param targetChannels output channel count (a mono source is duplicated).
  /// @param targetFormat   output sample encoding.
  PcmConverter(AudioFormat source, std::uint32_t targetRate, std::uint16_t targetChannels,
               SampleFormat targetFormat);

  /// @brief Converts @p sourcePcm (16-bit mono) and appends target frames to @p out.
  void convert(std::span<const std::byte> sourcePcm, std::vector<std::byte>& out);

  /// @brief Clears streaming state so the next convert() starts fresh.
  void reset() noexcept;

  [[nodiscard]] std::uint32_t targetSampleRate() const noexcept {
    return targetRate_;
  }

  [[nodiscard]] std::uint16_t targetChannels() const noexcept {
    return targetChannels_;
  }

  [[nodiscard]] SampleFormat targetFormat() const noexcept {
    return targetFormat_;
  }

private:
  void emitFrame(float sample, std::vector<std::byte>& out) const;

  double step_{0.0};          ///< Source samples advanced per output sample.
  double fractionalPos_{0.0}; ///< Position between previous_ and the current sample.
  float previous_{0.0F};      ///< Last source sample (for cross-chunk interpolation).
  std::uint32_t targetRate_;
  std::uint16_t targetChannels_;
  SampleFormat targetFormat_;
};

} // namespace vox::audio

#endif // VOX_AUDIO_PCM_CONVERTER_HPP
