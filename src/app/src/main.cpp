// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The Vox MVP reader executable: constructs the real Windows
///        implementations, wires them into the portable Reader, and runs.

#if defined(_WIN32)

#  include <exception>
#  include <iostream>
#  include <utility>

#  include <vox/app/reader.hpp>
#  include <vox/audio/wasapi_audio_sink.hpp>
#  include <vox/german/de_lex_data.hpp>
#  include <vox/german/lexicon.hpp>
#  include <vox/input/keyboard_hook.hpp>
#  include <vox/output/output_manager.hpp>
#  include <vox/provider/uia_provider.hpp>
#  include <vox/tts/sapi_tts_engine.hpp>

int main() {
  try {
    vox::provider::UiaProvider provider;
    vox::tts::SapiTtsEngine tts; // prefer German, fall back to the system voice
    vox::audio::WasapiAudioSink audio(tts.format());
    vox::output::OutputManager output(
        vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData));

    vox::app::Reader reader(provider, tts, audio, std::move(output));
    vox::input::KeyboardHook hook(reader);

    reader.start();
    hook.start();
    reader.waitForExit(); // until Ctrl+Shift+Q

    // Tear down off the hook callback, in dependency order.
    hook.stop();
    reader.stop();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "vox: fatal error: " << error.what() << '\n';
    return 1;
  }
}

#endif // defined(_WIN32)
