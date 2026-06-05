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
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_converter.hpp>

namespace vox::audio {

namespace {

constexpr float Int16Scale = 32768.0F; ///< Divisor mapping int16 -> [-1, 1).
constexpr float Int16Peak = 32767.0F;  ///< Max magnitude when writing int16.

} // namespace

PcmConverter::PcmConverter(AudioFormat source, std::uint32_t targetRate,
                           std::uint16_t targetChannels, SampleFormat targetFormat)
    : step_(static_cast<double>(source.sampleRate) / static_cast<double>(targetRate)),
      targetRate_(targetRate), targetChannels_(targetChannels), targetFormat_(targetFormat) {}

void PcmConverter::reset() noexcept {
  fractionalPos_ = 0.0;
  previous_ = 0.0F;
}

void PcmConverter::emitFrame(float sample, std::vector<std::byte>& out) const {
  if (targetFormat_ == SampleFormat::Float32) {
    std::array<std::byte, sizeof(float)> bytes{};
    std::memcpy(bytes.data(), &sample, sizeof(float));
    for (std::uint16_t channel = 0; channel < targetChannels_; ++channel) {
      out.insert(out.end(), bytes.begin(), bytes.end());
    }
    return;
  }
  const float clamped = std::clamp(sample, -1.0F, 1.0F);
  const auto value = static_cast<std::int16_t>(std::lround(clamped * Int16Peak));
  std::array<std::byte, sizeof(std::int16_t)> bytes{};
  std::memcpy(bytes.data(), &value, sizeof(std::int16_t));
  for (std::uint16_t channel = 0; channel < targetChannels_; ++channel) {
    out.insert(out.end(), bytes.begin(), bytes.end());
  }
}

void PcmConverter::convert(std::span<const std::byte> sourcePcm, std::vector<std::byte>& out) {
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
