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
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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
#include <vox/tts/itts_engine.hpp>

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

  void drain() override {
    {
      const std::scoped_lock lock(mutex_);
      drainCount_.fetch_add(1, std::memory_order_relaxed);
    }
    cv_.notify_all(); // wake waitForDrain()
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

  /// @brief Blocks until the worker has drained at least once (the end-of-stream
  ///        tail flush ran), or @p timeout elapses.
  [[nodiscard]] bool waitForDrain(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return cv_.wait_for(lock, timeout,
                        [this] { return drainCount_.load(std::memory_order_relaxed) > 0; });
  }

  [[nodiscard]] int flushCount() const noexcept {
    return flushCount_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] int drainCount() const noexcept {
    return drainCount_.load(std::memory_order_relaxed);
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::size_t bytesWritten_{0};
  std::size_t observed_{0}; ///< Bytes already returned by a prior waitForWrite().
  std::atomic<int> flushCount_{0};
  std::atomic<int> drainCount_{0};
};

/// A dedicated exception for a failing synthesizer (S112: not a generic one).
class SynthError : public std::runtime_error {
public:
  SynthError() : std::runtime_error("synthesize boom") {}
};

/// A TTS engine whose synthesize() throws, to prove the Reader's worker thread
/// swallows a synthesis failure and keeps running. It signals the attempt so the
/// test can wait deterministically.
class FailingTts : public FakeTtsEngine {
public:
  void synthesize(std::string_view /*utf8Text*/,
                  const vox::tts::ITtsEngine::PcmSink& /*sink*/) override {
    {
      const std::scoped_lock lock(mutex_);
      attempted_ = true;
    }
    cv_.notify_all();
    throw SynthError{};
  }

  [[nodiscard]] bool waitForAttempt(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return cv_.wait_for(lock, timeout, [this] { return attempted_; });
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool attempted_{false};
};

/// A TTS engine that parks inside synthesize() until cancelled, so a test can land
/// stop() while the worker is mid-utterance and verify it then skips the
/// end-of-stream drain.
class BlockingTts : public FakeTtsEngine {
public:
  void synthesize(std::string_view /*utf8Text*/,
                  const vox::tts::ITtsEngine::PcmSink& /*sink*/) override {
    std::unique_lock lock(mutex_);
    cancelled_ = false; // a fresh utterance ignores a cancel from before it began
    blocked_ = true;
    blockedCv_.notify_all();
    cancelledCv_.wait(lock, [this] { return cancelled_; });
    blocked_ = false; // clear so a reused BlockingTts blocks afresh on the next utterance
  }

  void cancel() override {
    {
      const std::scoped_lock lock(mutex_);
      cancelled_ = true;
    }
    cancelledCv_.notify_all();
  }

  [[nodiscard]] bool waitUntilBlocked(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return blockedCv_.wait_for(lock, timeout, [this] { return blocked_; });
  }

private:
  std::mutex mutex_;
  std::condition_variable blockedCv_;
  std::condition_variable cancelledCv_;
  bool blocked_{false};
  bool cancelled_{false};
};

/// A dedicated exception (S112: not a generic one) for a provider that fails
/// during the Reader's bring-up.
class ProviderBringUpError : public std::runtime_error {
public:
  ProviderBringUpError() : std::runtime_error("provider bring-up boom") {}
};

/// A provider whose focusedElement() throws, to drive the Reader's start()
/// rollback. focusedElement() is the *last* bring-up step (announceInitialFocus),
/// so by the time it throws the audio sink, worker thread, and focus subscription
/// are all up — exercising the fullest teardown the catch must perform before it
/// rethrows. (Throwing here, not from start(), also keeps the override free of the
/// by-value FocusChangedCallback the IProvider signature would force, S1238.)
class ThrowingProvider : public FakeProvider {
public:
  std::optional<AccessibleNode> focusedElement() const override {
    throw ProviderBringUpError{};
  }
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

TEST(Reader, StartRollsBackAndRethrowsWhenBringUpThrows) {
  ThrowingProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  // Bring-up reaches announceInitialFocus(), where focusedElement() throws: the
  // catch must tear down the audio sink, worker thread, and subscription, then
  // rethrow.
  EXPECT_THROW(reader.start(), ProviderBringUpError);

  // The rollback ran stop(), so started_ was reset — a retry actually tries again
  // (and throws again) rather than early-returning, and the worker is joined each
  // time. The test completing (no hang) confirms the teardown is clean.
  EXPECT_THROW(reader.start(), ProviderBringUpError);
  EXPECT_EQ(tts.synthesizeCount(), 0); // nothing was ever spoken
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

TEST(Reader, DrainsTheSinkAfterAnUtterance) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  provider.simulateFocusChange(button("OK"));
  ASSERT_TRUE(audio.waitForDrain(WaitTimeout)); // synthesis finished and the tail was drained
  reader.stop();

  // The worker flushes the resampler's group-delay tail once the utterance ends.
  EXPECT_EQ(audio.drainCount(), 1);
}

TEST(Reader, SkipsTheEndOfStreamDrainWhenStoppingMidUtterance) {
  FakeProvider provider;
  BlockingTts tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  provider.simulateFocusChange(button("OK"));
  ASSERT_TRUE(tts.waitUntilBlocked(WaitTimeout)); // the worker is parked inside synthesize()
  reader.stop(); // clears running_ and cancels synthesis -> the worker skips the drain

  EXPECT_EQ(audio.drainCount(), 0);
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

TEST(Reader, StartIsIdempotent) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  reader.start(); // already started: a no-op, must not start a second worker
  reader.stop();
}

TEST(Reader, QuitCommandReleasesWaitForExit) {
  FakeProvider provider;
  FakeTtsEngine tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  reader.onCommand(Command::Quit); // sets the exit flag
  reader.waitForExit();            // returns because Quit already fired
  reader.stop();
}

TEST(Reader, WorkerSurvivesASynthesisFailure) {
  FakeProvider provider;
  FailingTts tts;
  SyncAudioSink audio;
  Reader reader(provider, tts, audio, germanOutput());

  reader.start();
  provider.simulateFocusChange(button("OK"));
  EXPECT_TRUE(
      tts.waitForAttempt(WaitTimeout)); // the worker reached synthesize and caught its throw
  reader.stop();                        // a swallowed failure leaves a joinable, healthy worker
}

} // namespace
