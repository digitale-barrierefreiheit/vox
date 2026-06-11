// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief An IAudioSink that timestamps the first PCM write of each cycle.
///
/// Benchmark harness only (#41). The Reader's worker thread calls write();
/// the benchmark thread arms the sink, triggers a focus event, and blocks on
/// awaitFirstWrite() — the timestamp is taken *inside* write() on the worker
/// thread, so the waiter's own wakeup latency never pollutes the measurement.
#ifndef VOX_BENCHMARKS_TIMING_SINK_HPP
#define VOX_BENCHMARKS_TIMING_SINK_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <span>

#include <vox/audio/iaudio_sink.hpp>

namespace vox::bench {

/// Records when the first write() of the current cycle arrived and how many
/// bytes have been written since arm(), with blocking waits for both.
class TimingSink final : public vox::audio::IAudioSink {
public:
  void start() override {}

  void stop() override {}

  void flush() override {} // barge-in: nothing is queued here to drop

  void write(std::span<const std::byte> pcm) override {
    if (!firstWriteSeen_.load(std::memory_order_relaxed)) {
      firstWriteAt_ = std::chrono::steady_clock::now(); // published by the release below
      firstWriteSeen_.store(true, std::memory_order_release);
      firstWriteSeen_.notify_one();
    }
    bytesWritten_.fetch_add(pcm.size(), std::memory_order_release);
    bytesWritten_.notify_one();
  }

  /// Re-arms for the next cycle. Call only while the producer is idle (i.e.
  /// after awaitBytes() saw the previous utterance complete).
  void arm() {
    bytesWritten_.store(0, std::memory_order_relaxed);
    firstWriteSeen_.store(false, std::memory_order_relaxed);
  }

  /// Blocks until the first write() since arm() has happened.
  void awaitFirstWrite() {
    firstWriteSeen_.wait(false, std::memory_order_acquire);
  }

  /// When that first write() arrived. Valid after awaitFirstWrite() returned.
  [[nodiscard]] std::chrono::steady_clock::time_point firstWriteAt() const {
    return firstWriteAt_;
  }

  /// Blocks until at least @p expected bytes arrived since arm() — used to
  /// drain a whole utterance so cycles never barge into each other.
  void awaitBytes(std::size_t expected) {
    auto seen = bytesWritten_.load(std::memory_order_acquire);
    while (seen < expected) {
      bytesWritten_.wait(seen, std::memory_order_acquire);
      seen = bytesWritten_.load(std::memory_order_acquire);
    }
  }

private:
  std::atomic<bool> firstWriteSeen_{false};
  std::atomic<std::size_t> bytesWritten_{0};
  std::chrono::steady_clock::time_point firstWriteAt_;
};

} // namespace vox::bench

#endif // VOX_BENCHMARKS_TIMING_SINK_HPP
