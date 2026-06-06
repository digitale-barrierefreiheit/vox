// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief `vox::audio::DeviceError` — a WASAPI / audio-device call failed.
#ifndef VOX_AUDIO_ERRORS_HPP
#define VOX_AUDIO_ERRORS_HPP

#include <vox/core/os_error.hpp>

namespace vox::audio {

/// @brief Raised when a WASAPI/audio-device operation fails. Carries the
///        originating `HRESULT` (see `vox::OsError`); a caller may catch it to
///        react to audio failures specifically, or `vox::OsError` for any.
class DeviceError : public vox::OsError {
public:
  using vox::OsError::OsError;
};

} // namespace vox::audio

#endif // VOX_AUDIO_ERRORS_HPP
