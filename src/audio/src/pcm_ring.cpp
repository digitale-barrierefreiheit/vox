// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::audio::PcmRing.
///
/// Monotonic (free-running) cursors index the buffer modulo its capacity, so
/// "full vs empty" is unambiguous without wasting a slot: bytes in flight =
/// writeCursor - readCursor. The producer publishes data with a release store
/// to writeCursor; the consumer observes it with an acquire load (and vice
/// versa for free space), which is the SPSC release/acquire handshake.
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>

#include <vox/audio/pcm_ring.hpp>

namespace vox::audio {

PcmRing::PcmRing(std::size_t capacityBytes) : buffer_(capacityBytes) {
  if (capacityBytes == 0U) {
    // write()/read() index the buffer modulo its capacity; a zero capacity
    // would be a division by zero, so reject it up front.
    throw std::invalid_argument("PcmRing capacity must be non-zero");
  }
}

std::size_t PcmRing::write(std::span<const std::byte> data) noexcept {
  const std::size_t capacity = buffer_.size();
  const std::size_t writePos = writeCursor_.load(std::memory_order_relaxed);
  const std::size_t inFlight = writePos - readCursor_.load(std::memory_order_acquire);
  const std::size_t count = std::min(data.size(), capacity - inFlight);
  if (count == 0U) {
    return 0U;
  }
  const std::size_t head = writePos % capacity;
  const std::size_t firstChunk = std::min(count, capacity - head);
  std::memcpy(buffer_.data() + head, data.data(), firstChunk);
  if (count > firstChunk) {
    std::memcpy(buffer_.data(), data.data() + firstChunk, count - firstChunk);
  }
  writeCursor_.store(writePos + count, std::memory_order_release);
  return count;
}

std::size_t PcmRing::read(std::span<std::byte> out) noexcept {
  const std::size_t capacity = buffer_.size();
  const std::size_t readPos = readCursor_.load(std::memory_order_relaxed);
  const std::size_t available = writeCursor_.load(std::memory_order_acquire) - readPos;
  const std::size_t count = std::min(out.size(), available);
  if (count == 0U) {
    return 0U;
  }
  const std::size_t head = readPos % capacity;
  const std::size_t firstChunk = std::min(count, capacity - head);
  std::memcpy(out.data(), buffer_.data() + head, firstChunk);
  if (count > firstChunk) {
    std::memcpy(out.data() + firstChunk, buffer_.data(), count - firstChunk);
  }
  readCursor_.store(readPos + count, std::memory_order_release);
  return count;
}

void PcmRing::clear() noexcept {
  // Consumer-side: jump the read cursor up to the published write cursor.
  readCursor_.store(writeCursor_.load(std::memory_order_acquire), std::memory_order_release);
}

std::size_t PcmRing::writableBytes() const noexcept {
  const std::size_t inFlight =
      writeCursor_.load(std::memory_order_relaxed) - readCursor_.load(std::memory_order_acquire);
  return buffer_.size() - inFlight;
}

std::size_t PcmRing::readableBytes() const noexcept {
  return writeCursor_.load(std::memory_order_acquire) - readCursor_.load(std::memory_order_relaxed);
}

} // namespace vox::audio
