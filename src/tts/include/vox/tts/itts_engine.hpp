// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The TTS engine seam: stream PCM for text, cancellable, with rate.
///
/// `ITtsEngine` abstracts the synthesizer (SAPI5 today, ADR-05; a neural Tier-1
/// voice later) so Core and tests depend on the interface, not the engine.
/// Synthesis is streaming: PCM chunks are delivered to a sink as they are
/// produced, which gives low time-to-first-audio and makes barge-in (cancel)
/// effective at chunk granularity. `FakeTtsEngine` (vox::testing) fakes it.
#ifndef VOX_TTS_ITTS_ENGINE_HPP
#define VOX_TTS_ITTS_ENGINE_HPP

#include <cstddef>
#include <functional>
#include <span>
#include <string_view>

#include <vox/audio/audio_format.hpp>

namespace vox::tts {

/// Interface to a text-to-speech engine producing linear PCM.
class ITtsEngine {
public:
  /// Receives one chunk of PCM (in the engine's `format()`) as it is synthesized.
  /// Invoked zero or more times during a `synthesize()` call, on that call's
  /// thread. A sink may call `cancel()` to stop the stream after the next chunk.
  using PcmSink = std::function<void(std::span<const std::byte>)>;

  ITtsEngine() = default;
  ITtsEngine(const ITtsEngine&) = delete;
  ITtsEngine& operator=(const ITtsEngine&) = delete;
  ITtsEngine(ITtsEngine&&) = delete;
  ITtsEngine& operator=(ITtsEngine&&) = delete;
  virtual ~ITtsEngine() = default;

  /// @brief The PCM format every chunk from this engine is delivered in.
  [[nodiscard]] virtual vox::audio::AudioFormat format() const = 0;

  /// @brief Synthesizes @p utf8Text (German), streaming PCM chunks to @p sink.
  ///
  /// Blocks until synthesis finishes or is cancelled; @p sink runs on the
  /// calling thread. @p utf8Text is plain text, not markup.
  virtual void synthesize(std::string_view utf8Text, const PcmSink& sink) = 0;

  /// @brief Requests that the in-flight synthesis stop promptly (barge-in).
  ///
  /// Safe to call from another thread, or from within a `PcmSink`. Takes effect
  /// at the next chunk boundary; a no-op when nothing is being synthesized.
  virtual void cancel() = 0;

  /// @brief Sets the speaking rate, normalized to [-10, +10] (0 = default).
  ///
  /// Out-of-range values are clamped. The mapping to the backend is the engine's
  /// concern; the normalized scale is what callers use.
  virtual void setRate(int rate) = 0;
};

} // namespace vox::tts

#endif // VOX_TTS_ITTS_ENGINE_HPP
