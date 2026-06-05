// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::audio::toString(AudioFormat).
#include <cstdint>
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
    return std::to_string(channels) + "ch";
  }
}

} // namespace

std::string toString(const AudioFormat& format) {
  return std::to_string(format.sampleRate) + " Hz, " + std::to_string(format.bitsPerSample) +
         "-bit, " + channelLayout(format.channels);
}

} // namespace vox::audio
