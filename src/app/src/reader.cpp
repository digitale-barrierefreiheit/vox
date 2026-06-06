// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>

#include <vox/app/reader.hpp>
#include <vox/audio/iaudio_sink.hpp>
#include <vox/input/command.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/output/utterance.hpp>
#include <vox/provider/iprovider.hpp>
#include <vox/tts/itts_engine.hpp>

namespace vox::app {

Reader::Reader(vox::provider::IProvider& provider, vox::tts::ITtsEngine& tts,
               vox::audio::IAudioSink& audio, vox::output::OutputManager output)
    : provider_(provider), tts_(tts), audio_(audio), output_(std::move(output)),
      guard_(std::make_shared<detail::ReaderFocusGuard>()) {}

Reader::~Reader() {
  try {
    stop();
    // NOLINTNEXTLINE(bugprone-empty-catch) — dtor firewall: must never throw.
  } catch (...) {
  }
}

void Reader::start() {
  if (started_) {
    return;
  }
  {
    const std::lock_guard<std::mutex> lock(exitMutex_);
    exitRequested_ = false; // fresh run: a prior Quit must not leak into waitForExit()
  }
  audio_.start(); // may throw (no device): nothing spawned yet, so nothing to undo
  started_ = true;
  try {
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      running_ = true;
    }
    worker_ = std::thread([this] { workerLoop(); });
    // Route the focus callback through a shared guard so it is safe even if the
    // provider invokes it after this Reader is destroyed (stop() detaches it).
    {
      // Re-attach the single, lifetime-long guard (created in the constructor)
      // under its lock. Reusing it across restarts means a handler the provider
      // could not unregister still points back at this live Reader.
      const std::lock_guard<std::mutex> lock(guard_->mutex);
      guard_->reader = this;
    }
    provider_.start([guard = guard_](const vox::model::AccessibleNode& node) {
      const std::lock_guard<std::mutex> lock(guard->mutex);
      if (guard->reader != nullptr) {
        guard->reader->onFocusChanged(node);
      }
    });
    // Announce whatever already has focus, so launching over a dialog speaks now.
    if (const std::optional<vox::model::AccessibleNode> focused = provider_.focusedElement()) {
      onFocusChanged(*focused);
    }
  } catch (...) {
    stop(); // release the worker/provider/audio we partially brought up
    throw;
  }
}

void Reader::stop() {
  if (!started_) {
    return;
  }
  started_ = false;
  if (guard_) {
    // Detach first: after this, no provider callback will touch this Reader.
    const std::lock_guard<std::mutex> lock(guard_->mutex);
    guard_->reader = nullptr;
  }
  provider_.stop(); // ask the provider to unregister (it may not, hence the guard)
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    pending_.reset();
  }
  tts_.cancel(); // unblock a synthesize() in progress
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
  audio_.stop();
}

void Reader::waitForExit() {
  std::unique_lock<std::mutex> lock(exitMutex_);
  exitCv_.wait(lock, [this] { return exitRequested_; });
}

void Reader::onCommand(vox::input::Command command) {
  switch (command) {
  case vox::input::Command::NavigateNext:
  case vox::input::Command::NavigatePrevious:
  case vox::input::Command::NavigateUp:
  case vox::input::Command::NavigateDown:
    bargeIn(); // the focus-changed event will bring the new announcement
    break;
  case vox::input::Command::ToggleSpeech: {
    const bool wasEnabled = speechEnabled_.load(std::memory_order_acquire);
    speechEnabled_.store(!wasEnabled, std::memory_order_release);
    if (wasEnabled) {
      bargeIn(); // muting: silence what is playing
    }
    break;
  }
  case vox::input::Command::Quit: {
    const std::lock_guard<std::mutex> lock(exitMutex_);
    exitRequested_ = true;
    exitCv_.notify_all();
    break;
  }
  case vox::input::Command::None:
    break;
  }
}

void Reader::onFocusChanged(const vox::model::AccessibleNode& node) {
  if (!speechEnabled_.load(std::memory_order_acquire)) {
    return;
  }
  bargeIn(); // interrupt the previous announcement
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    pending_ = node; // latest focus wins (coalesces rapid navigation)
  }
  cv_.notify_one();
}

void Reader::bargeIn() {
  audio_.flush();
  tts_.cancel();
}

void Reader::workerLoop() {
  while (true) {
    vox::model::AccessibleNode node;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return pending_.has_value() || !running_; });
      if (!running_) {
        return;
      }
      if (!pending_.has_value()) {
        continue; // spurious wake-up
      }
      node = std::move(*pending_);
      pending_.reset();
    }
    {
      // stop() may have set running_ false after we dequeued but before this
      // blocking synthesis begins; cancel() alone can be lost if it lands before
      // synthesize() starts (engines reset their cancel flag there).
      const std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) {
        return;
      }
    }
    if (!speechEnabled_.load(std::memory_order_acquire)) {
      continue; // muted after this node was queued; drop it
    }
    try {
      const vox::output::Utterance utterance = output_.announce(node);
      tts_.synthesize(utterance.text,
                      [this](std::span<const std::byte> pcm) { audio_.write(pcm); });
      // A failed announcement (e.g. a SAPI error) must not escape the worker
      // thread; drop this utterance and keep going.
      // NOLINTNEXTLINE(bugprone-empty-catch)
    } catch (...) {
    }
  }
}

} // namespace vox::app
