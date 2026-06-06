// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief End-to-end tests for the MVP Reader, asserting on captured utterance
///        text (not audio): focus -> German announcement, and barge-in.
///
/// Pure: it wires FakeProvider + the real OutputManager + FakeTtsEngine + a
/// thread-synchronizing audio sink, so the whole §6.1 pipeline runs with no UIA,
/// SAPI, or WASAPI. The Reader synthesizes on a worker thread, so the sink lets
/// the test wait for the audio write before asserting.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <span>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include <vox/app/reader.hpp>
#include <vox/audio/iaudio_sink.hpp>
#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/input/command.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/testing/fake_provider.hpp>
#include <vox/testing/fake_tts_engine.hpp>

namespace {

using vox::app::Reader;
using vox::input::Command;
using vox::model::AccessibleNode;
using vox::model::Role;
using vox::output::OutputManager;
using vox::testing::FakeProvider;
using vox::testing::FakeTtsEngine;

constexpr auto WaitTimeout = std::chrono::seconds(2);

/// An IAudioSink that records writes/flushes and lets the test thread block
/// until the Reader's worker thread produces audio.
class SyncAudioSink : public vox::audio::IAudioSink {
public:
  void start() override { /* the sink is always ready; nothing to set up */ }

  void stop() override { /* nothing to tear down */ }

  void write(std::span<const std::byte> pcm) override {
    const std::scoped_lock lock(mutex_);
    bytesWritten_ += pcm.size();
    cv_.notify_all();
  }

  void flush() override {
    flushCount_.fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Blocks until PCM is written beyond what a previous waitForWrite()
  ///        already observed, or @p timeout elapses. Tracking the observed count
  ///        as state (not a per-call baseline) means a write that lands before
  ///        the call is not missed, and repeated waits still require new audio.
  [[nodiscard]] bool waitForWrite(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    const bool produced = cv_.wait_for(lock, timeout, [this] { return bytesWritten_ > observed_; });
    if (produced) {
      observed_ = bytesWritten_;
    }
    return produced;
  }

  [[nodiscard]] int flushCount() const noexcept {
    return flushCount_.load(std::memory_order_relaxed);
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::size_t bytesWritten_{0};
  std::size_t observed_{0}; ///< Bytes already returned by a prior waitForWrite().
  std::atomic<int> flushCount_{0};
};

OutputManager germanOutput() {
  return OutputManager(vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData));
}

AccessibleNode button(std::string name) {
  AccessibleNode node;
  node.role = Role::Button;
  node.name = std::move(name);
  return node;
}

TEST(Reader, SpeaksInitialFocusInGerman) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  provider.setFocusedElement(button("OK")); // already focused when the app starts
  reader.start();

  ASSERT_TRUE(audio.waitForWrite(WaitTimeout));
  reader.stop(); // joins the worker, so the reads below are race-free

  EXPECT_EQ(tts.synthesizeCount(), 1);
  EXPECT_EQ(tts.lastText(), "Schaltfläche, OK");
}

TEST(Reader, SpeaksFocusChange) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start(); // nothing focused yet
  provider.simulateFocusChange(button("Abbrechen"));

  ASSERT_TRUE(audio.waitForWrite(WaitTimeout));
  reader.stop();

  EXPECT_EQ(tts.lastText(), "Schaltfläche, Abbrechen");
}

TEST(Reader, NavigationKeyBargesIn) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  provider.simulateFocusChange(button("OK"));
  ASSERT_TRUE(audio.waitForWrite(WaitTimeout));

  const int cancelsBefore = tts.cancelCount();
  const int flushesBefore = audio.flushCount();
  reader.onCommand(Command::NavigateNext); // barge-in only

  EXPECT_GT(tts.cancelCount(), cancelsBefore);  // synthesis cancelled
  EXPECT_GT(audio.flushCount(), flushesBefore); // audio flushed
  reader.stop();
}

TEST(Reader, ToggleSpeechMutesThenUnmutes) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  reader.onCommand(Command::ToggleSpeech);    // mute
  provider.simulateFocusChange(button("OK")); // dropped while muted
  reader.onCommand(Command::ToggleSpeech);    // unmute
  provider.simulateFocusChange(button("Abbrechen"));

  ASSERT_TRUE(audio.waitForWrite(WaitTimeout));
  reader.stop();

  EXPECT_EQ(tts.synthesizeCount(), 1); // only the unmuted announcement spoke
  EXPECT_EQ(tts.lastText(), "Schaltfläche, Abbrechen");
}

} // namespace
