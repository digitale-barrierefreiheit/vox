// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Resampler microbenchmark (#55, §8.6.4): the per-chunk CPU cost of the
///        windowed-sinc PcmConverter.
///
/// The render path resamples TTS PCM (22.05 kHz/16-bit/mono) to the device mix
/// format (commonly 48 kHz/float32/stereo) on the producer thread. This measures
/// one delivered chunk's convert() so the #41 baseline tracks the median, and a
/// gross regression — a per-sample allocation, an O(n²) inner loop, a lost
/// passthrough — trips the absolute budget. Pure DSP: no provider, engine, or
/// device. The `out` scratch is reserved once and reused, mirroring the sink, so
/// the steady-state path measured here performs no heap allocation.
#include <benchmark/benchmark.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <ratio>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/audio/pcm_converter.hpp>

#include "percentile_report.hpp"

namespace {

using vox::audio::AudioFormat;
using vox::audio::PcmConverter;
using vox::audio::SampleFormat;

/// One delivered TTS chunk: ~0.19 s of 22.05 kHz 16-bit mono.
constexpr std::size_t ChunkSamples = 4096;

/// The SAPI5 wire format the converter consumes.
constexpr AudioFormat EngineFormat{22050, 16, 1};

/// Catastrophe net: converting ~0.19 s of audio must stay well under 8 ms — still
/// ~23x faster than real time, and ~10x over the measured p99 (~0.8 ms) so noisy
/// hosted runners never trip it. Only a gross regression (per-sample alloc, an
/// O(n²) inner loop, a 10x slowdown) blows it; the relative #41 median gate tracks
/// the finer drift this coarse budget deliberately ignores.
constexpr double ConvertBudgetUs = 8'000.0;

std::vector<std::byte> makeChunk() {
  std::vector<std::byte> bytes(ChunkSamples * sizeof(std::int16_t));
  for (std::size_t k = 0; k < ChunkSamples; ++k) {
    const double v =
        12000.0 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(k) / 22050.0);
    const auto sample = static_cast<std::int16_t>(v);
    std::memcpy(bytes.data() + (k * sizeof(std::int16_t)), &sample, sizeof(sample));
  }
  return bytes;
}

/// One cycle: resample a single chunk to 48 kHz float32 stereo, reusing the
/// scratch buffer. The converter is reset each cycle so every iteration measures
/// the same cold-start-to-steady-state work on identical input.
void resampleChunk(benchmark::State& state) {
  const std::vector<std::byte> chunk = makeChunk();
  PcmConverter converter{EngineFormat, 48000, 2, SampleFormat::Float32};
  std::vector<std::byte> out;
  // Pre-grown so steady-state convert() never reallocates: one chunk upsampled
  // 22.05 -> 48 kHz (~2.18x) to stereo float32. Reserve above 2.18x x 2 ch x float.
  out.reserve(ChunkSamples * 3U * 2U * sizeof(float)); // ~96 KiB, reused like the sink's scratch

  std::vector<double> samplesUs;
  samplesUs.reserve(static_cast<std::size_t>(state.max_iterations));

  for ([[maybe_unused]] auto keepRunning : state) {
    converter.reset();
    out.clear();
    const auto start = std::chrono::steady_clock::now();
    converter.convert(chunk, out);
    const std::chrono::duration<double, std::micro> elapsed =
        std::chrono::steady_clock::now() - start;
    benchmark::DoNotOptimize(out.data());
    state.SetIterationTime(elapsed.count() / 1'000'000.0);
    samplesUs.push_back(elapsed.count());
  }

  const vox::bench::Percentiles percentiles = vox::bench::reportPercentiles(state, samplesUs);
  vox::bench::enforceBudget(
      state.name(), {.metric = "p99", .valueUs = percentiles.p99Us, .budgetUs = ConvertBudgetUs});
}

BENCHMARK(resampleChunk)->UseManualTime()->Iterations(2'000)->Unit(benchmark::kMicrosecond);

} // namespace
