// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The production composition root: constructs the real Windows stack.
#if defined(_WIN32)

#  include <cstddef>
#  include <filesystem>
#  include <iostream>
#  include <memory>
#  include <string>
#  include <string_view>
#  include <utility>

#  include <vox/app/default_app.hpp>
#  include <vox/app/lexicon_loader.hpp>
#  include <vox/audio/wasapi_audio_sink.hpp>
#  include <vox/input/command_handler.hpp>
#  include <vox/input/iinput_hook.hpp>
#  include <vox/input/keyboard_hook.hpp>
#  include <vox/output/output_manager.hpp>
#  include <vox/provider/uia_provider.hpp>
#  include <vox/tts/sapi_tts_engine.hpp>

// After the vox headers (repo convention): windows.h drags in a global
// enumerator `Unknown` (winioctl.h, _MEDIA_TYPE) that would otherwise read as
// shadowed by Role::Unknown / Source::Unknown above (Sonar S1117).
#  include <Windows.h>

namespace vox::app {

namespace {

/// The value of environment variable @p name, or empty when unset (or set to
/// the empty string — both mean "not configured").
std::wstring readEnvironment(const wchar_t* name) {
  // The buffer starts deliberately small: any real value takes the grow-once
  // path, so it stays exercised; the loop reads every length faithfully.
  std::wstring value(8, L'\0');
  DWORD written = ::GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
  while (written >= value.size()) {
    value.resize(written); // too small: `written` is the required size incl. the NUL
    written = ::GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
  }
  value.resize(written); // on success `written` excludes the NUL; 0 when unset
  return value;
}

/// The directory holding the running executable, read completely whatever its
/// length. On genuine failure (length 0) this is the empty path, which makes
/// the loader skip the directory lookup entirely (never CWD-relative) and fall
/// back to the embedded default with a diagnostic.
std::filesystem::path executableDirectory() {
  // Starts deliberately small: the grow path runs on every start, so it stays
  // exercised; GetModuleFileNameW reports truncation by filling the buffer.
  std::wstring path(8, L'\0');
  DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  while (length == path.size()) {
    path.resize(path.size() * 2);
    length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  }
  path.resize(length); // 0 on failure: the empty path
  return std::filesystem::path(path).parent_path();
}

/// The per-language lexicon directory next to the executable — or empty when
/// the executable directory is unknown, so the loader skips the lookup instead
/// of being handed the CWD-relative path "lexicon".
std::filesystem::path lexiconDirectory() {
  const std::filesystem::path exeDir = executableDirectory();
  return exeDir.empty() ? exeDir : exeDir / L"lexicon";
}

/// @p wide rendered as UTF-8 (voice names may carry non-ASCII characters). The
/// explicit length (not -1) means the buffer need not be NUL-terminated, so a
/// string_view is a faithful input.
std::string utf8FromWide(std::wstring_view wide) {
  if (wide.empty()) {
    return {};
  }
  const auto toUtf8 = [wide](char* out, int capacity) {
    return ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out,
                                 capacity, nullptr, nullptr);
  };
  const int bytes = toUtf8(nullptr, 0);
  std::string out(static_cast<std::size_t>(bytes > 0 ? bytes : 0), '\0');
  const int written = toUtf8(out.data(), static_cast<int>(out.size()));
  out.resize(
      static_cast<std::size_t>(written > 0 ? written : 0)); // shrink to written; empty on fail
  return out;
}

/// The language the user asked for (#88): a validated VOX_LANGUAGE, or the
/// ADR-07 default "de" — reporting an unusable tag on stderr. The result
/// drives both the voice selection and the lexicon resolution.
std::string requestedLanguage() {
  // The tag must be ASCII to be valid; non-ASCII survives the narrowing as
  // non-tag characters, so the validation below rejects it.
  std::string tag;
  for (const wchar_t letter : readEnvironment(L"VOX_LANGUAGE")) {
    tag += (letter < 128) ? static_cast<char>(letter) : '?';
  }
  if (tag.empty()) {
    return std::string(DefaultLanguageTag);
  }
  if (!isLanguageTag(tag)) {
    std::cerr << "vox: requested language \"" << tag
              << R"(" (VOX_LANGUAGE) is not a language tag (ASCII letters, digits, "-"); using ")"
              << DefaultLanguageTag << "\"\n";
    return std::string(DefaultLanguageTag);
  }
  return tag;
}

/// Reports how the voice request worked out (#88): fallbacks and divergences
/// go to stderr here — the engine itself does no I/O.
void reportVoiceOutcome(const vox::tts::VoiceSelectionRequest& request,
                        const vox::tts::SelectedVoice& voice) {
  using enum vox::tts::VoiceChoice;
  if (!request.explicitVoice.empty() && voice.choice != ExplicitName) {
    std::cerr << "vox: voice \"" << request.explicitVoice
              << "\" (VOX_VOICE) is not installed; using \"" << voice.name << "\"\n";
  } else if (voice.choice == Fallback) {
    std::cerr << "vox: no \"" << request.language << "\" voice is installed; using \"" << voice.name
              << "\"\n";
    // Only warn about a real divergence: a known voice language that differs
    // from the request (compared case-insensitively by primary subtag).
  } else if (voice.choice == ExplicitName && !voice.language.empty() &&
             !vox::tts::sameLanguage(voice.language, request.language)) {
    std::cerr << "vox: voice \"" << voice.name << "\" (VOX_VOICE) speaks \"" << voice.language
              << "\" while the requested language (VOX_LANGUAGE) is \"" << request.language
              << "\"; the explicit voice wins\n";
  }
}

/// Loads the announcement lexicon for @p languageTag per the #61/#88 rules
/// (VOX_LEXICON authoritative, then lexicon\<tag>.lex next to the executable,
/// then the embedded German default), reporting every fallback on stderr.
vox::german::Lexicon loadConfiguredLexicon(const std::string& languageTag) {
  LoadedLexicon loaded = loadLexicon({.explicitFile = readEnvironment(L"VOX_LEXICON"),
                                      .lexiconDir = lexiconDirectory(),
                                      .requestedTag = languageTag});
  for (const std::string& line : loaded.diagnostics) {
    std::cerr << "vox: " << line << '\n';
  }
  return std::move(loaded.lexicon);
}

} // namespace

AppDependencies makeDefaultDependencies() {
  // One requested language drives both the voice and the lexicon (#88);
  // VOX_VOICE / VOX_LEXICON override their part with higher precedence.
  const std::string languageTag = requestedLanguage();
  vox::tts::VoiceSelectionRequest voiceRequest;
  voiceRequest.language = languageTag;
  voiceRequest.explicitVoice = utf8FromWide(readEnvironment(L"VOX_VOICE"));

  // The engine is built first: the audio sink is configured at its PCM format.
  auto tts = std::make_unique<vox::tts::SapiTtsEngine>(voiceRequest);
  reportVoiceOutcome(voiceRequest, tts->selectedVoice());
  auto audio = std::make_unique<vox::audio::WasapiAudioSink>(tts->format());
  auto provider = std::make_unique<vox::provider::UiaProvider>();
  vox::output::OutputManager output(loadConfiguredLexicon(languageTag));

  return AppDependencies{
      .provider = std::move(provider),
      .tts = std::move(tts),
      .audio = std::move(audio),
      .output = std::move(output),
      .makeHook =
          [](vox::input::ICommandHandler& handler) -> std::unique_ptr<vox::input::IInputHook> {
        return std::make_unique<vox::input::KeyboardHook>(handler);
      },
  };
}

} // namespace vox::app

#endif // defined(_WIN32)
