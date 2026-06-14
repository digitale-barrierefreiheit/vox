// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The production composition root: the real Windows dependencies for App.
#ifndef VOX_APP_DEFAULT_APP_HPP
#define VOX_APP_DEFAULT_APP_HPP

// The default dependencies construct the Windows-only UIA / SAPI / WASAPI /
// keyboard-hook implementations, so this is declared only on Windows.
#if defined(_WIN32)

#  include <vox/app/app.hpp>

namespace vox::app {

/// @brief Builds the production @ref AppDependencies: the real UIA provider,
///        the SAPI engine, the WASAPI sink at the engine's format, the output
///        speaking the configured lexicon, and a factory that makes the Windows
///        `KeyboardHook`. One requested language (`VOX_LANGUAGE`, default `de`)
///        drives both the voice and the lexicon (#88); `VOX_VOICE` /
///        `VOX_LEXICON` override their part with higher precedence, fallbacks
///        (system voice; embedded German table, #61) stay unchanged, and every
///        fallback or divergence is reported here on stderr.
/// @throws vox::tts::EngineError if no usable SAPI voice is available.
[[nodiscard]] AppDependencies makeDefaultDependencies();

} // namespace vox::app

#endif // defined(_WIN32)

#endif // VOX_APP_DEFAULT_APP_HPP
