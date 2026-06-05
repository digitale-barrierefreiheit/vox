// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::audio::PcmConverter.
///
/// Streaming linear-interpolation resampler. For each source sample we emit
/// every output sample whose fractional position falls between the previous and
/// the current source sample, carrying the phase across chunks. Source samples
/// are read via memcpy (not a reinterpreted pointer) so the input need not be
/// 2-byte aligned — UBSan-clean.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_converter.hpp>

namespace vox::audio {

namespace {

constexpr float Int16Scale = 32768.0F; ///< Scale between int16 and [-1, 1].
constexpr long Int16Min = -32768;      ///< Lowest int16 value.
constexpr long Int16Max = 32767;       ///< Highest int16 value.

} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — rate/channels/format are distinct roles
PcmConverter::PcmConverter(AudioFormat source, std::uint32_t targetRate,
                           std::uint16_t targetChannels, SampleFormat targetFormat)
    : targetRate_(targetRate), targetChannels_(targetChannels), targetFormat_(targetFormat) {
  if (source.bitsPerSample != 16U || source.channels != 1U || source.sampleRate == 0U) {
    throw std::invalid_argument("PcmConverter: source must be 16-bit mono PCM at a non-zero rate");
  }
  if (targetRate == 0U || targetChannels == 0U) {
    throw std::invalid_argument("PcmConverter: target rate and channel count must be non-zero");
  }
  step_ = static_cast<double>(source.sampleRate) / static_cast<double>(targetRate);
}

void PcmConverter::reset() noexcept {
  fractionalPos_ = 0.0;
  previous_ = 0.0F;
}

void PcmConverter::emitFrame(float sample, std::vector<std::byte>& out) const {
  // Encode one channel's bytes, then grow the buffer once and replicate across
  // channels — avoids per-channel vector::insert bookkeeping on this path.
  std::array<std::byte, sizeof(float)> encoded{};
  std::size_t channelBytes = 0;
  if (targetFormat_ == SampleFormat::Float32) {
    std::memcpy(encoded.data(), &sample, sizeof(float));
    channelBytes = sizeof(float);
  } else {
    // Scale by 32768 (symmetric with the input) and clamp into the int16 range,
    // so same-rate/same-format conversion round-trips exactly.
    const long scaled = std::lround(sample * Int16Scale);
    const auto value = static_cast<std::int16_t>(std::clamp(scaled, Int16Min, Int16Max));
    std::memcpy(encoded.data(), &value, sizeof(std::int16_t));
    channelBytes = sizeof(std::int16_t);
  }
  const std::size_t offset = out.size();
  out.resize(offset + (channelBytes * targetChannels_));
  for (std::uint16_t channel = 0; channel < targetChannels_; ++channel) {
    std::memcpy(out.data() + offset + (static_cast<std::size_t>(channel) * channelBytes),
                encoded.data(), channelBytes);
  }
}

void PcmConverter::convert(std::span<const std::byte> sourcePcm, std::vector<std::byte>& out) {
  if (sourcePcm.size() % sizeof(std::int16_t) != 0U) {
    // A 16-bit stream must arrive in whole samples; a stray byte signals an
    // upstream framing bug, so reject it rather than silently truncate.
    throw std::invalid_argument("PcmConverter: source size must be a multiple of 2 bytes");
  }
  const std::size_t sampleCount = sourcePcm.size() / sizeof(std::int16_t);
  for (std::size_t i = 0; i < sampleCount; ++i) {
    std::int16_t raw = 0;
    std::memcpy(&raw, sourcePcm.data() + (i * sizeof(std::int16_t)), sizeof(raw));
    const float current = static_cast<float>(raw) / Int16Scale;
    while (fractionalPos_ < 1.0) {
      const auto fraction = static_cast<float>(fractionalPos_);
      emitFrame(previous_ + ((current - previous_) * fraction), out);
      fractionalPos_ += step_;
    }
    fractionalPos_ -= 1.0;
    previous_ = current;
  }
}

} // namespace vox::audio
