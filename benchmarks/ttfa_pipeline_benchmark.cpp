// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Time-to-first-audio pipeline microbenchmark (#41, §8.6.4) — the gate.
///
/// Measures everything Vox itself adds between a focus event and the first PCM
/// chunk reaching the audio sink: the event-thread handoff, the worker wakeup,
/// the German utterance construction, the synthesis dispatch, and the first
/// chunk delivery. Provider and engine are the deterministic test fakes, so the
/// numbers regression-track Vox's own overhead — not SAPI, not the device. The
/// p99 must stay within a small share of the §1.2 Q1 budget (200 ms): the
/// pipeline may never eat what the synthesizer needs.
#include <benchmark/benchmark.h>
#include <chrono>
#include <cstddef>
#include <ratio>
#include <vector>

#include <vox/app/reader.hpp>
#include <vox/audio/audio_format.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/testing/fake_provider.hpp>
#include <vox/testing/fake_tts_engine.hpp>

#include "announce_fixture.hpp"
#include "percentile_report.hpp"
#include "timing_sink.hpp"

namespace {

/// The SAPI5 wire format (22.05 kHz / 16-bit / mono), so the fake streams what
/// the real engine would.
constexpr vox::audio::AudioFormat EngineFormat{22050, 16, 1};

/// Synthetic PCM per utterance byte. Small on purpose: the chunks themselves
/// must not dominate what is a latency (not throughput) measurement.
constexpr std::size_t PcmBytesPerTextByte = 2;

/// Vox's own pipeline overhead may consume at most 10% of the Q1 budget
/// (200 ms time-to-first-audio, §1.2); the rest belongs to the synthesizer
/// and the device. Actual values are microseconds — the headroom is what makes
/// this absolute gate robust on noisy hosted runners.
constexpr double PipelineBudgetUs = 20'000.0;

/// One cycle: focus event -> worker wakeup -> announce -> synthesize -> first
/// PCM write. The iteration time is taken inside the sink on the worker thread.
void ttfaPipeline(benchmark::State& state) {
  vox::testing::FakeProvider provider;
  vox::testing::FakeTtsEngine tts(EngineFormat, PcmBytesPerTextByte);
  vox::bench::TimingSink sink;
  vox::app::Reader reader(provider, tts, sink, vox::bench::makeOutput());

  const vox::model::AccessibleNode node = vox::bench::savedButton();
  // Draining target: the whole utterance's PCM. Waiting for it between cycles
  // keeps the worker idle at each trigger, so no cycle barges into the last.
  const std::size_t utterancePcmBytes =
      vox::bench::makeOutput().announce(node).text.size() * PcmBytesPerTextByte;

  std::vector<double> samplesUs;
  samplesUs.reserve(static_cast<std::size_t>(state.max_iterations));

  reader.start();
  for ([[maybe_unused]] auto keepRunning : state) {
    sink.arm();
    const auto requested = std::chrono::steady_clock::now();
    provider.simulateFocusChange(node);
    sink.awaitFirstWrite();
    const std::chrono::duration<double, std::micro> ttfa = sink.firstWriteAt() - requested;
    state.SetIterationTime(ttfa.count() / 1'000'000.0);
    samplesUs.push_back(ttfa.count());
    sink.awaitBytes(utterancePcmBytes);
  }
  reader.stop();

  const vox::bench::Percentiles percentiles = vox::bench::reportPercentiles(state, samplesUs);
  vox::bench::enforceBudget(
      state.name(), {.metric = "p99", .valueUs = percentiles.p99Us, .budgetUs = PipelineBudgetUs});
}

BENCHMARK(ttfaPipeline)->UseManualTime()->Iterations(10'000)->Unit(benchmark::kMicrosecond);

} // namespace
