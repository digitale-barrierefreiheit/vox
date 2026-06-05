// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows SAPI5 implementation of ITtsEngine (ADR-05 MVP bootstrap).
///
/// Synthesizes German text with a built-in SAPI5 voice, streaming raw PCM to the
/// sink via a custom SAPI output stream. COM types are hidden behind a pImpl so
/// this header stays clean. Windows-only — built solely on the Windows toolchain;
/// the portable pieces it relies on (voice selection, rate, the audio POD) are
/// what the sanitizer/clang-tidy build sees.
#ifndef VOX_TTS_SAPI_TTS_ENGINE_HPP
#define VOX_TTS_SAPI_TTS_ENGINE_HPP

// SapiTtsEngine is implemented only on Windows (see src/tts/CMakeLists.txt).
// Declaring it only there turns any accidental non-Windows use into a clear
// "undeclared identifier" at compile time rather than a confusing link error.
#if defined(_WIN32)

#  include <memory>
#  include <span>
#  include <string_view>

#  include <vox/audio/audio_format.hpp>
#  include <vox/tts/itts_engine.hpp>
#  include <vox/tts/voice_selection.hpp>

namespace vox::tts {

/// SAPI5-backed TTS engine. Streams 22.05 kHz/16-bit/mono PCM.
class SapiTtsEngine : public ITtsEngine {
public:
  /// @brief Creates an engine, selecting a voice per @p policy (default: prefer
  ///        German, fall back to the system voice so it still speaks on an
  ///        English-only machine).
  /// @throws std::runtime_error if COM/SAPI cannot be initialized or no usable
  ///         voice is available under @p policy.
  explicit SapiTtsEngine(VoiceSelectionPolicy policy = VoiceSelectionPolicy::PreferGerman);
  ~SapiTtsEngine() override;

  SapiTtsEngine(const SapiTtsEngine&) = delete;
  SapiTtsEngine& operator=(const SapiTtsEngine&) = delete;
  SapiTtsEngine(SapiTtsEngine&&) = delete;
  SapiTtsEngine& operator=(SapiTtsEngine&&) = delete;

  /// @brief The fixed PCM format SAPI output is converted to (22050/16/mono).
  [[nodiscard]] vox::audio::AudioFormat format() const override;

  /// @brief Synthesizes @p utf8Text, streaming PCM chunks to @p sink. Empty
  ///        input is a no-op.
  /// @throws std::runtime_error if @p utf8Text is non-empty but not valid UTF-8,
  ///         or if SAPI synthesis fails.
  void synthesize(std::string_view utf8Text, const PcmSink& sink) override;

  /// @brief Aborts the in-flight synthesis at the next chunk boundary.
  void cancel() override;

  /// @brief Sets the SAPI speaking rate (normalized [-10, +10], clamped).
  void setRate(int rate) override;

  /// @brief The voice chosen at construction (for diagnostics/tests).
  [[nodiscard]] const SelectedVoice& selectedVoice() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vox::tts

#endif // defined(_WIN32)

#endif // VOX_TTS_SAPI_TTS_ENGINE_HPP
