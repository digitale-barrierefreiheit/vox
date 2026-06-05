// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A lock-free single-producer/single-consumer byte ring for PCM hand-off.
///
/// One thread writes (the producer — the TTS/output path) and one thread reads
/// (the consumer — the WASAPI render thread). No locks and no allocation after
/// construction (ADR-10 / architecture §8.1): the render path must never block
/// or churn the heap. The two cursors live on separate cache lines to avoid
/// false sharing (§8.3). `clear()` is a consumer-side barge-in primitive.
#ifndef VOX_AUDIO_PCM_RING_HPP
#define VOX_AUDIO_PCM_RING_HPP

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace vox::audio {

// The cursor alignment below deliberately pads the object onto separate cache
// lines; MSVC's C4324 ("structure padded due to alignment specifier") just
// reports that intent, so it is silenced here (and only here) under /W4 /WX.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4324)
#endif

/// A fixed-capacity, lock-free SPSC ring of raw PCM bytes.
// The cursors are deliberately on separate cache lines (false-sharing
// avoidance, §8.3); the resulting padding is intentional, so the padding
// analyzer is silenced for this class.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class PcmRing {
public:
  /// Constructs a ring that can hold @p capacityBytes bytes. Allocates once.
  explicit PcmRing(std::size_t capacityBytes);

  PcmRing(const PcmRing&) = delete;
  PcmRing& operator=(const PcmRing&) = delete;
  PcmRing(PcmRing&&) = delete;
  PcmRing& operator=(PcmRing&&) = delete;
  ~PcmRing() = default;

  /// @brief Producer: copies up to @p data.size() bytes in; returns the count
  ///        actually written (fewer than requested when the ring is near full).
  std::size_t write(std::span<const std::byte> data) noexcept;

  /// @brief Consumer: copies up to @p out.size() bytes out; returns the count
  ///        actually read (fewer than requested when the ring is near empty).
  std::size_t read(std::span<std::byte> out) noexcept;

  /// @brief Consumer-side: discards all currently readable bytes (barge-in).
  ///        Must only be called from the consumer thread.
  void clear() noexcept;

  /// @brief Producer's view of free space, in bytes.
  [[nodiscard]] std::size_t writableBytes() const noexcept;

  /// @brief Consumer's view of available data, in bytes.
  [[nodiscard]] std::size_t readableBytes() const noexcept;

  /// @brief Total capacity, in bytes.
  [[nodiscard]] std::size_t capacity() const noexcept {
    return buffer_.size();
  }

private:
  /// Cache line on the target; cursors are aligned to it to avoid false sharing.
  static constexpr std::size_t CacheLineSize = 64;

  std::vector<std::byte> buffer_;
  alignas(CacheLineSize) std::atomic<std::size_t> writeCursor_{0};
  alignas(CacheLineSize) std::atomic<std::size_t> readCursor_{0};
};

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

} // namespace vox::audio

#endif // VOX_AUDIO_PCM_RING_HPP
