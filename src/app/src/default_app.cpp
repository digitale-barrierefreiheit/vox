// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The production composition root: constructs the real Windows stack.
#if defined(_WIN32)

#  include <Windows.h>

#  include <filesystem>
#  include <iostream>
#  include <memory>
#  include <string>
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

/// Loads the announcement lexicon per the #61 resolution rule (VOX_LEXICON,
/// then lexicon\<VOX_LANGUAGE>.lex next to the executable, then the embedded
/// German default), reporting every fallback on stderr.
vox::german::Lexicon loadConfiguredLexicon() {
  const std::wstring requestedTagWide = readEnvironment(L"VOX_LANGUAGE");
  // The tag must be ASCII to be valid; non-ASCII survives the narrowing as
  // non-tag characters, so the loader rejects and reports it.
  std::string requestedTag;
  for (const wchar_t letter : requestedTagWide) {
    requestedTag += (letter < 128) ? static_cast<char>(letter) : '?';
  }
  LoadedLexicon loaded = loadLexicon({.explicitFile = readEnvironment(L"VOX_LEXICON"),
                                      .lexiconDir = lexiconDirectory(),
                                      .requestedTag = std::move(requestedTag)});
  for (const std::string& line : loaded.diagnostics) {
    std::cerr << "vox: " << line << '\n';
  }
  return std::move(loaded.lexicon);
}

} // namespace

AppDependencies makeDefaultDependencies() {
  // The engine is built first: the audio sink is configured at its PCM format.
  auto tts = std::make_unique<vox::tts::SapiTtsEngine>(); // prefer German, fall back
  auto audio = std::make_unique<vox::audio::WasapiAudioSink>(tts->format());
  auto provider = std::make_unique<vox::provider::UiaProvider>();
  vox::output::OutputManager output(loadConfiguredLexicon());

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
