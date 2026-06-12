// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The production composition root: constructs the real Windows stack.
#if defined(_WIN32)

#  include <windows.h>

#  include <array>
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

/// The value of environment variable @p name, or empty when unset.
std::wstring readEnvironment(const wchar_t* name) {
  // The buffer starts deliberately small: any real value takes the grow-once
  // path, so it stays exercised; the loop reads every length faithfully.
  std::wstring value(8, L'\0');
  while (true) {
    const DWORD written =
        ::GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
    if (written == 0) {
      return {}; // unset (or set to the empty string — both mean "not configured")
    }
    if (written < value.size()) {
      value.resize(written);
      return value;
    }
    value.resize(written); // too small: `written` is the required size incl. the NUL
  }
}

/// The directory holding the running executable. On failure (length 0) this is
/// the empty path and on MAX_PATH truncation a cut-off one — either way the
/// lookup degrades safely: the loader validates whatever it finds there and
/// otherwise falls back to the embedded default.
std::filesystem::path executableDirectory() {
  std::array<wchar_t, MAX_PATH> buffer{};
  const DWORD length =
      ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
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
                                      .lexiconDir = executableDirectory() / L"lexicon",
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
