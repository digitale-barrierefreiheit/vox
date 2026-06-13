// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::audio::PcmConverter.
///
/// Streaming windowed-sinc polyphase resampler. A Kaiser-windowed sinc prototype
/// low-pass is precomputed at construction and decomposed into `Phases` sub-phase
/// rows of `Taps` taps each; every output sample convolves the `Taps` most recent
/// source samples (held in a ring) with the row nearest its fractional phase,
/// linearly interpolated between the two adjacent rows for arbitrary ratios. The
/// cutoff is the lower of the source/target Nyquist, so one kernel anti-images on
/// upsampling and anti-aliases on downsampling. At equal rates the resampler is
/// bypassed entirely — an exact, zero-delay passthrough. Source samples are read
/// via memcpy (not a reinterpreted pointer) so the input need not be 2-byte
/// aligned — UBSan-clean.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_converter.hpp>

namespace vox::audio {

namespace {

constexpr float Int16Scale = 32768.0F; ///< Scale between int16 and [-1, 1].
constexpr long Int16Min = -32768;      ///< Lowest int16 value.
constexpr long Int16Max = 32767;       ///< Highest int16 value.

// --- Kernel shape -----------------------------------------------------------
// Taps drives stopband sharpness; Phases drives the phase-quantization floor
// (interpolating between adjacent phases lifts the effective resolution far above
// Phases alone). KaiserBeta ~9 yields a ~ -90 dB stopband. Taps is a power of two
// so the history-ring index is a cheap mask. Tuned for a single speech stream,
// where the cost is negligible (see benchmarks/resampler_benchmark.cpp).
constexpr std::size_t Taps = 32;
constexpr std::size_t HalfTaps = Taps / 2;
constexpr std::size_t Phases = 256;
constexpr double KaiserBeta = 9.0;

/// Normalized sinc sin(pi·x)/(pi·x), with the removable singularity at 0.
double normalizedSinc(double x) {
  if (std::abs(x) < 1e-9) {
    return 1.0;
  }
  const double pix = std::numbers::pi * x;
  return std::sin(pix) / pix;
}

/// Modified Bessel function of the first kind, order 0 — for the Kaiser window.
double besselI0(double x) {
  double sum = 1.0;
  double term = 1.0;
  for (int k = 1; k < 64; ++k) {
    const auto kd = static_cast<double>(k);
    term *= (x * x) / (4.0 * kd * kd);
    sum += term;
    if (term < sum * 1e-12) {
      break;
    }
  }
  return sum;
}

/// Kaiser window over u ∈ [-1, 1] at the fixed KaiserBeta. The endpoints u = ±1
/// evaluate to I0(0)/I0(KaiserBeta) (small but non-zero), not zero — so the guard
/// row (phase == Phases) stays continuous with phase 0 at the wrap boundary. The
/// radicand is clamped so |u| ≥ 1 stays finite (no sqrt of a negative), though the
/// kernel only ever samples it on [-1, 1].
double kaiser(double u) {
  const double radicand = 1.0 - (u * u);
  const double arg = radicand > 0.0 ? std::sqrt(radicand) : 0.0;
  return besselI0(KaiserBeta * arg) / besselI0(KaiserBeta);
}

} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — rate/channels/format are distinct roles
PcmConverter::PcmConverter(AudioFormat source, std::uint32_t targetRate,
                           std::uint16_t targetChannels, SampleFormat targetFormat)
    : history_(Taps, 0.0F), targetRate_(targetRate), targetChannels_(targetChannels),
      targetFormat_(targetFormat) {
  if (source.bitsPerSample != 16U || source.channels != 1U || source.sampleRate == 0U) {
    throw std::invalid_argument("PcmConverter: source must be 16-bit mono PCM at a non-zero rate");
  }
  if (targetRate == 0U || targetChannels == 0U) {
    throw std::invalid_argument("PcmConverter: target rate and channel count must be non-zero");
  }
  step_ = static_cast<double>(source.sampleRate) / static_cast<double>(targetRate);
  bypass_ = source.sampleRate == targetRate;
  if (!bypass_) {
    buildKernel();
  }
}

void PcmConverter::buildKernel() {
  // Cutoff at the lower Nyquist (cycles per source sample): 0.5 when upsampling,
  // scaled down by the ratio when downsampling so we anti-alias rather than image.
  const double ratio = 1.0 / step_; // target / source
  const double cutoff = 0.5 * std::min(1.0, ratio);

  kernel_.resize((Phases + 1) * Taps);
  for (std::size_t phase = 0; phase <= Phases; ++phase) {
    const double frac = static_cast<double>(phase) / static_cast<double>(Phases);
    double rowSum = 0.0;
    for (std::size_t tap = 0; tap < Taps; ++tap) {
      // Distance from the output point to this tap's source sample, in source-
      // sample units: tap 0 is the oldest (HalfTaps-1 ahead), tap Taps-1 newest.
      const double x = frac + (static_cast<double>(HalfTaps) - 1.0 - static_cast<double>(tap));
      const double h = normalizedSinc(2.0 * cutoff * x) * kaiser(x / static_cast<double>(HalfTaps));
      kernel_[(phase * Taps) + tap] = static_cast<float>(h);
      rowSum += h;
    }
    // Normalize each phase row to unity DC gain (robust to window/cutoff choice).
    const auto norm = rowSum != 0.0 ? static_cast<float>(1.0 / rowSum) : 1.0F;
    for (std::size_t tap = 0; tap < Taps; ++tap) {
      kernel_[(phase * Taps) + tap] *= norm;
    }
  }
}

void PcmConverter::pushSample(float sample) noexcept {
  history_[writePos_] = sample;
  writePos_ = (writePos_ + 1U) % Taps;
  ++inputCount_;
}

float PcmConverter::recent(std::size_t back) const noexcept {
  // The newest sample sits at writePos_ - 1; step further back with wraparound.
  return history_[(writePos_ + Taps - 1U - back) % Taps];
}

void PcmConverter::emitResampled(std::vector<std::byte>& out) const {
  const double positionFrac = nextOutput_ - std::floor(nextOutput_);
  const double phasePos = positionFrac * static_cast<double>(Phases);
  const auto phase = static_cast<std::size_t>(phasePos); // 0 .. Phases-1
  const auto phaseFrac = static_cast<float>(phasePos - static_cast<double>(phase));

  const float* low = kernel_.data() + (phase * Taps);
  const float* high = kernel_.data() + ((phase + 1U) * Taps);
  float acc = 0.0F;
  for (std::size_t tap = 0; tap < Taps; ++tap) {
    const float weight = low[tap] + ((high[tap] - low[tap]) * phaseFrac);
    acc += recent(Taps - 1U - tap) * weight;
  }
  emitFrame(acc, out);
}

void PcmConverter::emitFrame(float sample, std::vector<std::byte>& out) const {
  // Encode one channel's bytes, then grow the buffer once and replicate across
  // channels — avoids per-channel vector::insert bookkeeping on this path.
  std::array<std::byte, sizeof(float)> encoded{};
  std::size_t channelBytes = 0;
  if (targetFormat_ == SampleFormat::Float32) {
    std::memcpy(encoded.data(), &sample, sizeof(float));
    channelBytes = sizeof(float);
  } else {
    // Scale by 32768 (symmetric with the input) and clamp into the int16 range,
    // so same-rate/same-format conversion round-trips exactly.
    const long scaled = std::lround(sample * Int16Scale);
    const auto value = static_cast<std::int16_t>(std::clamp(scaled, Int16Min, Int16Max));
    std::memcpy(encoded.data(), &value, sizeof(std::int16_t));
    channelBytes = sizeof(std::int16_t);
  }
  const std::size_t offset = out.size();
  out.resize(offset + (channelBytes * targetChannels_));
  for (std::uint16_t channel = 0; channel < targetChannels_; ++channel) {
    std::memcpy(out.data() + offset + (static_cast<std::size_t>(channel) * channelBytes),
                encoded.data(), channelBytes);
  }
}

void PcmConverter::convert(std::span<const std::byte> sourcePcm, std::vector<std::byte>& out) {
  if (sourcePcm.size() % sizeof(std::int16_t) != 0U) {
    // A 16-bit stream must arrive in whole samples; a stray byte signals an
    // upstream framing bug, so reject it rather than silently truncate.
    throw std::invalid_argument("PcmConverter: source size must be a multiple of 2 bytes");
  }
  const std::size_t sampleCount = sourcePcm.size() / sizeof(std::int16_t);
  for (std::size_t i = 0; i < sampleCount; ++i) {
    std::int16_t raw = 0;
    std::memcpy(&raw, sourcePcm.data() + (i * sizeof(std::int16_t)), sizeof(raw));
    const float sample = static_cast<float>(raw) / Int16Scale;
    if (bypass_) {
      emitFrame(sample, out); // exact, zero-delay passthrough at equal rates
      continue;
    }
    pushSample(sample);
    // Emit every output whose centered window now ends at or before the newest
    // source sample — i.e. its latest needed tap, floor(t) + HalfTaps, has
    // arrived. Rearranged to avoid an unsigned underflow when few samples are in.
    while (static_cast<std::uint64_t>(std::floor(nextOutput_)) + HalfTaps + 1U <= inputCount_) {
      emitResampled(out);
      nextOutput_ += step_;
    }
  }
}

void PcmConverter::reset() noexcept {
  std::fill(history_.begin(), history_.end(), 0.0F);
  writePos_ = 0;
  inputCount_ = 0;
  nextOutput_ = 0.0;
}

} // namespace vox::audio
