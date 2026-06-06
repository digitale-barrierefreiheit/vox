// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::audio::toString(AudioFormat).
#include <cstdint>
#include <format>
#include <string>

#include <vox/audio/audio_format.hpp>

namespace vox::audio {

namespace {

std::string channelLayout(std::uint16_t channels) {
  switch (channels) {
  case 1U:
    return "mono";
  case 2U:
    return "stereo";
  default:
    return std::format("{}ch", channels);
  }
}

} // namespace

std::string toString(const AudioFormat& format) {
  return std::format("{} Hz, {}-bit, {}", format.sampleRate, format.bitsPerSample,
                     channelLayout(format.channels));
}

} // namespace vox::audio
