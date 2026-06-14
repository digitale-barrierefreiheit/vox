// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief An IAudioSink that timestamps the first PCM write of each cycle.
///
/// Benchmark harness only (#41). The Reader's worker thread calls write();
/// the benchmark thread arms the sink, triggers a focus event, and blocks on
/// awaitFirstWrite() — the timestamp is taken *inside* write() on the worker
/// thread (before any locking), so neither the waiter's wakeup latency nor
/// lock contention pollutes the measurement. Both waits are timeout-bound: a
/// pipeline that stops producing PCM fails the benchmark fast with a clear
/// error instead of hanging the CI job into its timeout.
#ifndef VOX_BENCHMARKS_TIMING_SINK_HPP
#define VOX_BENCHMARKS_TIMING_SINK_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <span>

#include <vox/audio/iaudio_sink.hpp>

namespace vox::bench {

/// Records when the first write() of the current cycle arrived and how many
/// bytes have been written since arm(), with timeout-bound waits for both.
class TimingSink final : public vox::audio::IAudioSink {
public:
  void start() override {}

  void stop() override {}

  void drain() override {} // end of stream: nothing is buffered here to flush

  void flush() override {} // barge-in: nothing is queued here to drop

  void write(std::span<const std::byte> pcm) override {
    const auto now = std::chrono::steady_clock::now(); // timestamp before the lock
    {
      const std::scoped_lock lock(mutex_);
      if (!firstWriteSeen_) {
        firstWriteAt_ = now;
        firstWriteSeen_ = true;
      }
      bytesWritten_ += pcm.size();
    }
    written_.notify_one();
  }

  /// Re-arms for the next cycle. Call only while the producer is idle (i.e.
  /// after awaitBytes() saw the previous utterance complete).
  void arm() {
    const std::scoped_lock lock(mutex_);
    firstWriteSeen_ = false;
    bytesWritten_ = 0;
  }

  /// Waits for the first write() since arm(). False on @p timeout — the
  /// pipeline produced no PCM; fail the benchmark instead of hanging.
  [[nodiscard]] bool awaitFirstWrite(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return written_.wait_for(lock, timeout, [this] { return firstWriteSeen_; });
  }

  /// When that first write() arrived. Valid after awaitFirstWrite() succeeded.
  [[nodiscard]] std::chrono::steady_clock::time_point firstWriteAt() const {
    const std::scoped_lock lock(mutex_);
    return firstWriteAt_;
  }

  /// Waits until at least @p expected bytes arrived since arm() — drains a
  /// whole utterance so cycles never barge into each other. False on timeout.
  [[nodiscard]] bool awaitBytes(std::size_t expected, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return written_.wait_for(lock, timeout, [this, expected] { return bytesWritten_ >= expected; });
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable written_;
  bool firstWriteSeen_{false};
  std::size_t bytesWritten_{0};
  std::chrono::steady_clock::time_point firstWriteAt_;
};

} // namespace vox::bench

#endif // VOX_BENCHMARKS_TIMING_SINK_HPP
