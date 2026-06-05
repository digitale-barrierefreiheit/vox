// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief An owning buffer of PCM samples tagged with its AudioFormat.
///
/// `PcmBuffer` is the accumulating counterpart to the streaming `PcmSink`
/// (vox::tts): a sink can append chunks into one, and tests assert on the
/// collected result. The streaming path itself does not allocate one per
/// utterance — it hands raw chunks straight to the audio pipeline.
#ifndef VOX_AUDIO_PCM_BUFFER_HPP
#define VOX_AUDIO_PCM_BUFFER_HPP

#include <cstddef>
#include <span>
#include <vector>

#include <vox/audio/audio_format.hpp>

namespace vox::audio {

/// Raw little-endian PCM bytes plus the format needed to interpret them.
struct PcmBuffer {
  AudioFormat format;             ///< How to interpret @ref samples.
  std::vector<std::byte> samples; ///< Interleaved PCM, `format`-encoded.

  /// @brief True when no samples are held.
  [[nodiscard]] bool empty() const noexcept {
    return samples.empty();
  }

  /// @brief Size of the PCM payload in bytes.
  [[nodiscard]] std::size_t byteCount() const noexcept {
    return samples.size();
  }

  /// @brief Number of whole frames held, or 0 when the format has no frame size.
  [[nodiscard]] std::size_t frameCount() const noexcept {
    const std::size_t frameBytes = format.bytesPerFrame();
    return frameBytes == 0U ? 0U : samples.size() / frameBytes;
  }

  /// @brief Appends a chunk of PCM bytes (e.g. from a streaming sink).
  void append(std::span<const std::byte> chunk) {
    samples.insert(samples.end(), chunk.begin(), chunk.end());
  }

  /// Two buffers are equal iff their format and samples match.
  [[nodiscard]] friend bool operator==(const PcmBuffer&, const PcmBuffer&) = default;
};

} // namespace vox::audio

#endif // VOX_AUDIO_PCM_BUFFER_HPP
