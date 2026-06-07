// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only test seam over SapiTtsEngine's COM object creation.
///
/// `SapiTtsEngine` builds two COM objects with `CoCreateInstance`: the
/// `ISpVoice` (the synthesizer) and the `ISpObjectTokenCategory` (the voice
/// catalogue it enumerates). These seams let a test substitute those root
/// creations with factories that return *mock* objects (or a failure
/// `HRESULT`), so the engine's construction, voice-selection, synthesis, and
/// error paths are unit-tested with no installed SAPI voice (ADR-12, issue #68).
/// Production code never sets a factory and keeps using `CoCreateInstance`.
#ifndef VOX_TTS_SAPI_TEST_SEAM_HPP
#define VOX_TTS_SAPI_TEST_SEAM_HPP

#if defined(_WIN32)

#  include <functional>

// Forward-declared so this header needs no Windows SDK / SAPI headers.
struct ISpVoice;
struct ISpObjectTokenCategory;

namespace vox::tts::testing {

/// @brief Creates the SAPI voice. Mirrors `CoCreateInstance`: returns an
///        `HRESULT` (as `long`) and, on success, sets `*out` to an AddRef'd
///        interface. Return a failure code to exercise the error path.
using VoiceFactory = std::function<long(ISpVoice** out)>;

/// @brief Creates the voice-token category enumerated by the engine. Same
///        contract as @ref VoiceFactory.
using TokenCategoryFactory = std::function<long(ISpObjectTokenCategory** out)>;

/// @brief Installs @p factory for the next `SapiTtsEngine` construction; an empty
///        factory restores the real `CoCreateInstance`. Test-only, not thread-safe.
void setVoiceFactory(VoiceFactory factory);

/// @copydoc setVoiceFactory
void setTokenCategoryFactory(TokenCategoryFactory factory);

} // namespace vox::tts::testing

#endif // defined(_WIN32)

#endif // VOX_TTS_SAPI_TEST_SEAM_HPP
