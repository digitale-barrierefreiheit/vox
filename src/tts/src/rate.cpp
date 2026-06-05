// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::tts::clampRate.
#include <algorithm>

#include <vox/tts/rate.hpp>

namespace vox::tts {

int clampRate(int rate) noexcept {
  return std::clamp(rate, MinRate, MaxRate);
}

} // namespace vox::tts
