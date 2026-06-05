// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

#include <cstddef>
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
    : provider_(provider), tts_(tts), audio_(audio), output_(std::move(output)) {}

Reader::~Reader() {
  try {
    stop();
  } catch (...) {
    // Destructors are noexcept; never let a teardown failure terminate.
  }
}

void Reader::start() {
  if (started_) {
    return;
  }
  audio_.start(); // may throw (no device): nothing spawned yet, so nothing to undo
  started_ = true;
  try {
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      running_ = true;
    }
    worker_ = std::thread([this] { workerLoop(); });
    provider_.start([this](const vox::model::AccessibleNode& node) { onFocusChanged(node); });
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
  provider_.stop(); // no further focus callbacks
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
      node = std::move(*pending_);
      pending_.reset();
    }
    try {
      const vox::output::Utterance utterance = output_.announce(node);
      tts_.synthesize(utterance.text,
                      [this](std::span<const std::byte> pcm) { audio_.write(pcm); });
    } catch (...) {
      // A failed announcement (e.g. SAPI error) must not crash the reader or
      // escape the worker thread; drop this utterance and keep going.
    }
  }
}

} // namespace vox::app
