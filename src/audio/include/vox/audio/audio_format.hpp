// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief PCM audio format descriptor shared across the speech path.
///
/// `AudioFormat` is the small, OS-independent value type that travels with raw
/// PCM between the TTS subsystem (#35) and the WASAPI audio pipeline (#36). It
/// is deliberately interleaved-PCM only — exactly what SAPI emits and what the
/// audio device consumes; richer formats are out of scope for the MVP.
#ifndef VOX_AUDIO_AUDIO_FORMAT_HPP
#define VOX_AUDIO_AUDIO_FORMAT_HPP

#include <cstdint>
#include <string>

namespace vox::audio {

/// Describes one stream of interleaved, little-endian linear PCM.
///
/// The defaults are the MVP SAPI5 output format (22.05 kHz, 16-bit, mono) — a
/// good latency/quality trade-off for navigation speech and the format the SAPI
/// backend forces so downstream code sees a single, predictable shape.
struct AudioFormat {
  std::uint32_t sampleRate{22050}; ///< Frames per second, in Hz.
  std::uint16_t bitsPerSample{16}; ///< Bits per sample (8, 16, …).
  std::uint16_t channels{1};       ///< Interleaved channel count (1 = mono).

  /// @brief Bytes occupied by one frame (one sample across all channels).
  [[nodiscard]] constexpr std::uint32_t bytesPerFrame() const noexcept {
    return static_cast<std::uint32_t>(channels) * (static_cast<std::uint32_t>(bitsPerSample) / 8U);
  }

  /// @brief Bytes streamed per second at this format.
  [[nodiscard]] constexpr std::uint32_t bytesPerSecond() const noexcept {
    return sampleRate * bytesPerFrame();
  }

  /// Two formats are equal iff every field matches.
  [[nodiscard]] friend constexpr bool operator==(const AudioFormat&,
                                                 const AudioFormat&) noexcept = default;
};

/// @brief Returns a stable diagnostic description, e.g. "22050 Hz, 16-bit, mono".
/// @return A string for logging and test output — not a user-facing announcement.
[[nodiscard]] std::string toString(const AudioFormat& format);

} // namespace vox::audio

#endif // VOX_AUDIO_AUDIO_FORMAT_HPP
