// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Back-pressure push of PCM frames into a PcmRing, with injectable hooks.
///
/// The producer (WasapiAudioSink::write / drain) pushes whole frames into the
/// lock-free ring, retrying when the ring is momentarily full and abandoning when
/// the stream is stopped or barged-in. The stop/flush polling and the back-off are
/// injected as callables, so every branch is unit-tested with a real ring and no
/// threads: the test's `wait` advances the polled state (drains the ring, clears
/// the pending flush) exactly as the real consumer would, the same humble-object
/// seam idea as detail::renderDeviceBuffer. The hooks are template callables
/// (inlinable, no std::function on the producer path; distinct closure types, so
/// no bugprone-easily-swappable-parameters).
#ifndef VOX_AUDIO_DETAIL_RING_PUSH_HPP
#define VOX_AUDIO_DETAIL_RING_PUSH_HPP

#include <chrono>
#include <cstddef>
#include <span>
#include <thread>

#include <vox/audio/pcm_ring.hpp>

namespace vox::audio::detail {

/// @brief The production back-off: yield one tick so the render thread drains the
///        ring / services a pending flush before the producer retries. A named
///        function (not an inline lambda) so its one line is covered by a direct
///        unit test rather than depending on a full ring during an integration run.
inline void backOffOneTick() {
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

/// @brief Pushes @p frames into @p ring with back-pressure; returns the bytes left
///        unqueued (empty == fully queued, non-empty == abandoned mid-push).
/// @param isAbandoned    polled each iteration; true => stopped/barged-in, give up.
/// @param isFlushPending polled each iteration; true => a flush awaits the render
///        thread, so wait rather than enqueue audio it is about to drop.
/// @param wait           backs off one tick (production: backOffOneTick; tests: a
///        callable that advances the polled state so the loop makes progress).
template<typename IsAbandoned, typename IsFlushPending, typename Wait>
std::span<const std::byte> pushFramesToRing(PcmRing& ring, std::span<const std::byte> frames,
                                            IsAbandoned isAbandoned, IsFlushPending isFlushPending,
                                            Wait wait) {
  std::span<const std::byte> remaining = frames;
  while (!remaining.empty()) {
    if (isAbandoned()) {
      return remaining; // stopped or barged-in: drop the now-stale audio
    }
    if (isFlushPending()) {
      wait(); // a flush is being serviced; let the render thread clear the ring
      continue;
    }
    // `remaining` (whole frames) and the ring's free space (frame-aligned) are both
    // frame multiples, so `written` is too — a frame is never split mid-write.
    const std::size_t written = ring.write(remaining);
    remaining = remaining.subspan(written);
    if (written == 0U) {
      wait(); // the ring is momentarily full: back off, then retry
    }
  }
  return remaining;
}

} // namespace vox::audio::detail

#endif // VOX_AUDIO_DETAIL_RING_PUSH_HPP
