// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The MVP screen-reader orchestrator: focus -> utterance -> TTS -> audio.
///
/// `Reader` wires the pipeline of §6.1 over the module *interfaces*
/// (`IProvider` / `ITtsEngine` / `IAudioSink`), so the whole flow is testable
/// with fakes; the Windows `main()` constructs the real implementations and
/// hands them in. On a focus change it builds a German utterance and speaks it;
/// a navigation key barges in.
///
/// Threading (§8): focus events (provider thread) and commands (hook thread)
/// only stash state and wake the synthesis worker thread, which runs the
/// blocking announce + synthesize + write — so the event threads never block on
/// synthesis.
#ifndef VOX_APP_READER_HPP
#define VOX_APP_READER_HPP

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <vox/audio/iaudio_sink.hpp>
#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/provider/iprovider.hpp>
#include <vox/tts/itts_engine.hpp>

namespace vox::app {

class Reader;

namespace detail {
/// Keeps the provider's focus callback safe if it fires after the Reader is
/// gone. Two layers since #60: UiaProvider::stop() guarantees no callback
/// *begins* after it returns (the sink is detached before unregistration,
/// whatever UIA answers), and this guard drops the one invocation possibly
/// still in flight across that stop — plus anything a future, misbehaving
/// IProvider might deliver. The callback holds a shared_ptr to this guard and
/// only touches `reader` while it is non-null under the lock; Reader::stop()
/// detaches it before teardown.
struct ReaderFocusGuard {
  std::mutex mutex;
  Reader* reader{nullptr};
};
} // namespace detail

/// Orchestrates the focus -> speech pipeline and barge-in for the MVP reader.
class Reader : public vox::input::ICommandHandler {
public:
  /// @brief Builds a reader over the given seams. @p provider, @p tts and
  ///        @p audio must outlive the reader; @p output is taken by value.
  Reader(vox::provider::IProvider& provider, vox::tts::ITtsEngine& tts,
         vox::audio::IAudioSink& audio, vox::output::OutputManager output);
  ~Reader() override;

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;
  Reader(Reader&&) = delete;
  Reader& operator=(Reader&&) = delete;

  /// @brief Starts audio, the synthesis worker, and focus subscription, then
  ///        announces the already-focused element.
  /// @throws whatever `IAudioSink::start()` or the provider throw.
  void start();

  /// @brief Stops focus subscription, the worker, and audio, releasing all.
  ///        Idempotent. Must not be called from within the hook callback.
  void stop();

  /// @brief Blocks until a Quit command is received or stop() is called.
  void waitForExit();

  /// @brief Handles a reader command (invoked on the keyboard-hook thread).
  void onCommand(vox::input::Command command) override;

private:
  void onFocusChanged(const vox::model::AccessibleNode& node);
  void bargeIn();
  void workerLoop();

  vox::provider::IProvider& provider_;
  vox::tts::ITtsEngine& tts_;
  vox::audio::IAudioSink& audio_;
  vox::output::OutputManager output_;

  std::jthread worker_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<vox::model::AccessibleNode> pending_; ///< Latest node to announce.
  bool running_{false};                               ///< Worker-loop condition.
  bool started_{false};                               ///< Guards start()/stop().
  std::atomic<bool> speechEnabled_{true};

  std::mutex exitMutex_;
  std::condition_variable exitCv_;
  bool exitRequested_{false};

  /// Outlives a late provider callback. Created with the Reader (an in-class
  /// initializer, so it exists once and is reused across stop()/start() cycles).
  std::shared_ptr<detail::ReaderFocusGuard> guard_{std::make_shared<detail::ReaderFocusGuard>()};
};

} // namespace vox::app

#endif // VOX_APP_READER_HPP
