// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for the App run-loop, driven entirely through injected fakes:
///        a clean run to a Quit command, and a fatal startup exception mapped to
///        a non-zero exit code. No UIA / SAPI / WASAPI / real keyboard hook.
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include <vox/app/app.hpp>
#include <vox/audio/audio_format.hpp>
#include <vox/audio/iaudio_sink.hpp>
#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>
#include <vox/input/iinput_hook.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/testing/fake_audio_sink.hpp>
#include <vox/testing/fake_provider.hpp>
#include <vox/testing/fake_tts_engine.hpp>

namespace {

using vox::app::App;
using vox::app::AppDependencies;
using vox::audio::AudioFormat;
using vox::input::Command;
using vox::input::ICommandHandler;
using vox::input::IInputHook;
using vox::output::OutputManager;
using vox::testing::FakeAudioSink;
using vox::testing::FakeProvider;
using vox::testing::FakeTtsEngine;

constexpr AudioFormat TestFormat{22050, 16, 1};

OutputManager germanOutput() {
  return OutputManager(vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData));
}

/// Fake hook standing in for the real KeyboardHook: on start() it immediately
/// issues a Quit, so the App's waitForExit() returns and the run-loop completes.
class QuitOnStartHook : public IInputHook {
public:
  explicit QuitOnStartHook(ICommandHandler& handler) : handler_(handler) {}

  void start() override {
    ++startCount_;
    handler_.onCommand(Command::Quit);
  }

  void stop() override {
    ++stopCount_;
  }

  [[nodiscard]] int startCount() const noexcept {
    return startCount_;
  }

  [[nodiscard]] int stopCount() const noexcept {
    return stopCount_;
  }

private:
  ICommandHandler& handler_;
  int startCount_{0};
  int stopCount_{0};
};

/// An audio sink whose start() throws, to drive the App's fatal-error path.
class ThrowingAudioSink : public vox::audio::IAudioSink {
public:
  void start() override {
    throw std::runtime_error("no audio device");
  }

  void write(std::span<const std::byte>) override {}

  void flush() override {}

  void stop() override {}
};

TEST(AppTest, RunsUntilQuitAndReturnsZero) {
  QuitOnStartHook* hook = nullptr;
  AppDependencies deps{
      .provider = std::make_unique<FakeProvider>(),
      .tts = std::make_unique<FakeTtsEngine>(TestFormat),
      .audio = std::make_unique<FakeAudioSink>(),
      .output = germanOutput(),
      .makeHook = [&hook](ICommandHandler& handler) -> std::unique_ptr<IInputHook> {
        auto owned = std::make_unique<QuitOnStartHook>(handler);
        hook = owned.get();
        return owned;
      },
  };

  App app(std::move(deps));
  EXPECT_EQ(app.run(), 0);

  ASSERT_NE(hook, nullptr);
  EXPECT_EQ(hook->startCount(), 1);
  EXPECT_GE(hook->stopCount(), 1); // stopped by run() (and/or App teardown)
}

TEST(AppTest, ReturnsNonZeroWhenStartupThrows) {
  AppDependencies deps{
      .provider = std::make_unique<FakeProvider>(),
      .tts = std::make_unique<FakeTtsEngine>(TestFormat),
      .audio = std::make_unique<ThrowingAudioSink>(), // reader.start() will throw
      .output = germanOutput(),
      .makeHook = [](ICommandHandler& handler) -> std::unique_ptr<IInputHook> {
        return std::make_unique<QuitOnStartHook>(handler);
      },
  };

  App app(std::move(deps));
  EXPECT_EQ(app.run(), 1); // the startup exception is caught and mapped to exit 1
}

} // namespace
