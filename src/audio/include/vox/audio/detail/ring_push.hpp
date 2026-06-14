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

/// @brief The producer's poll/back-off hooks, bundled so pushFramesToRing keeps to
///        a small argument count. Distinct callable member types keep binding them
///        clear of bugprone-easily-swappable-parameters; build one with
///        makePushHooks() so call sites need no CTAD.
/// @tparam IsAbandoned    () -> bool: true => stopped/barged-in, give up.
/// @tparam IsFlushPending () -> bool: true => a flush awaits the render thread.
/// @tparam Wait           () -> void: back off one tick (production: backOffOneTick;
///         tests: a callable that advances the polled state so the loop progresses).
template<typename IsAbandoned, typename IsFlushPending, typename Wait>
struct PushHooks {
  IsAbandoned isAbandoned;
  IsFlushPending isFlushPending;
  Wait wait;
};

/// @brief Builds a PushHooks, deducing the callable types via ordinary function-
///        template deduction (so call sites stay simple and portable).
template<typename IsAbandoned, typename IsFlushPending, typename Wait>
PushHooks<IsAbandoned, IsFlushPending, Wait>
makePushHooks(IsAbandoned isAbandoned, IsFlushPending isFlushPending, Wait wait) {
  return {.isAbandoned = isAbandoned, .isFlushPending = isFlushPending, .wait = wait};
}

/// @brief Pushes @p frames into @p ring with back-pressure; returns the bytes left
///        unqueued (empty == fully queued, non-empty == abandoned mid-push).
template<typename IsAbandoned, typename IsFlushPending, typename Wait>
std::span<const std::byte>
pushFramesToRing(PcmRing& ring, std::span<const std::byte> frames,
                 const PushHooks<IsAbandoned, IsFlushPending, Wait>& hooks) {
  std::span<const std::byte> remaining = frames;
  while (!remaining.empty()) {
    if (hooks.isAbandoned()) {
      return remaining; // stopped or barged-in: drop the now-stale audio
    }
    if (hooks.isFlushPending()) {
      hooks.wait(); // a flush is being serviced; let the render thread clear the ring
      continue;
    }
    // `remaining` (whole frames) and the ring's free space (frame-aligned) are both
    // frame multiples, so `written` is too — a frame is never split mid-write.
    const std::size_t written = ring.write(remaining);
    remaining = remaining.subspan(written);
    if (written == 0U) {
      hooks.wait(); // the ring is momentarily full: back off, then retry
    }
  }
  return remaining;
}

} // namespace vox::audio::detail

#endif // VOX_AUDIO_DETAIL_RING_PUSH_HPP
