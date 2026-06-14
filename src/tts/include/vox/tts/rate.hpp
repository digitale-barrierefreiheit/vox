// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The normalized speaking-rate scale shared by TTS engines.
#ifndef VOX_TTS_RATE_HPP
#define VOX_TTS_RATE_HPP

namespace vox::tts {

/// Slowest normalized rate (`setRate` clamps to this).
inline constexpr int MinRate = -10;

/// Fastest normalized rate (`setRate` clamps to this).
inline constexpr int MaxRate = 10;

/// @brief Clamps @p rate into the normalized [MinRate, MaxRate] range.
///
/// The normalized scale matches SAPI's own -10..+10 rate, so the SAPI backend
/// needs only this clamp; a neural engine maps the same scale to its own range.
[[nodiscard]] int clampRate(int rate) noexcept;

} // namespace vox::tts

#endif // VOX_TTS_RATE_HPP
