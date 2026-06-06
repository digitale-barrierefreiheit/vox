// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief `vox::tts::EngineError` — a SAPI / TTS-engine call failed.
#ifndef VOX_TTS_ERRORS_HPP
#define VOX_TTS_ERRORS_HPP

#include <vox/core/os_error.hpp>

namespace vox::tts {

/// @brief Raised when a SAPI/TTS-engine operation fails (COM init, voice
///        selection, synthesis). Carries the originating `HRESULT` (see
///        `vox::OsError`). Catchable specifically, or as `vox::OsError`.
class EngineError : public vox::OsError {
public:
  using vox::OsError::OsError;
};

} // namespace vox::tts

#endif // VOX_TTS_ERRORS_HPP
