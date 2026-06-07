// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The production composition root: constructs the real Windows stack.
#if defined(_WIN32)

#  include <memory>
#  include <utility>

#  include <vox/app/default_app.hpp>
#  include <vox/audio/wasapi_audio_sink.hpp>
#  include <vox/german/de_lex_data.hpp>
#  include <vox/german/lexicon.hpp>
#  include <vox/input/command_handler.hpp>
#  include <vox/input/iinput_hook.hpp>
#  include <vox/input/keyboard_hook.hpp>
#  include <vox/output/output_manager.hpp>
#  include <vox/provider/uia_provider.hpp>
#  include <vox/tts/sapi_tts_engine.hpp>

namespace vox::app {

AppDependencies makeDefaultDependencies() {
  // The engine is built first: the audio sink is configured at its PCM format.
  auto tts = std::make_unique<vox::tts::SapiTtsEngine>(); // prefer German, fall back
  auto audio = std::make_unique<vox::audio::WasapiAudioSink>(tts->format());
  auto provider = std::make_unique<vox::provider::UiaProvider>();
  vox::output::OutputManager output(
      vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData));

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
