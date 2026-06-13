// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Streaming PCM conversion from the TTS format to a device mix format.
///
/// WASAPI shared mode plays in the device's mix format (commonly 48 kHz float32
/// stereo), while the TTS engine emits 16-bit mono at its own rate. `PcmConverter`
/// bridges the two: sample-rate (windowed-sinc polyphase resampling), channel
/// up-mix, and sample-format. It is a pure, OS-independent core (unit-tested,
/// sanitizer-covered); the WASAPI layer only drives it. It is stateful — the
/// resampler phase and its filter history carry across `convert()` calls — so use
/// one per stream. The kernel and history are allocated once at construction, so
/// `convert()` performs no internal heap allocation on the steady-state path
/// (ADR-10): only the caller-owned @p out buffer grows.
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
  ///
  /// The linear-phase FIR delays output by its group delay, so the last ~taps/2
  /// source samples of a stream stay buffered until the next call. Call drain() at
  /// end of stream to flush them.
  void convert(std::span<const std::byte> sourcePcm, std::vector<std::byte>& out);

  /// @brief Flushes the resampler's group-delay tail — the trailing ~taps/2 source
  ///        samples the FIR still held back — appending the final frames to @p out,
  ///        then resets streaming state. Call once the stream has ended; a no-op
  ///        at equal rates (the exact passthrough buffers nothing).
  void drain(std::vector<std::byte>& out);

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
  /// Builds the polyphase windowed-sinc kernel for the source→target ratio.
  void buildKernel();
  /// Pushes one source sample into the history ring (newest-overwrites-oldest).
  void pushSample(float sample) noexcept;
  /// Reads the source sample @p back positions before the newest (0 == newest).
  [[nodiscard]] float recent(std::size_t back) const noexcept;
  /// Resamples one output sample at the current phase and appends its frame.
  void emitResampled(std::vector<std::byte>& out) const;
  /// Encodes @p sample into @p out, duplicated across the target channels.
  void emitFrame(float sample, std::vector<std::byte>& out) const;

  std::vector<float> kernel_;   ///< (phases+1) × taps polyphase table; empty when bypassing.
  std::vector<float> history_;  ///< Ring of the last `taps` source samples.
  std::size_t writePos_{0};     ///< Ring slot the next pushed sample overwrites.
  std::uint64_t inputCount_{0}; ///< Source samples consumed since the last reset().
  double step_{0.0};            ///< Source samples advanced per output sample.
  double nextOutput_{0.0};      ///< Position of the next output, in source-sample units.
  bool bypass_{false};          ///< Source rate == target rate: passthrough, no resampling.
  std::uint32_t targetRate_;
  std::uint16_t targetChannels_;
  SampleFormat targetFormat_;
};

} // namespace vox::audio

#endif // VOX_AUDIO_PCM_CONVERTER_HPP
