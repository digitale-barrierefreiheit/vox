// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Real-engine time-to-first-audio measurement (#41) — the Q1 number.
///
/// Drives the real SapiTtsEngine with the same utterance as the pipeline
/// benchmark and measures synthesize() to the first delivered PCM chunk — the
/// dominant share of the user-felt time-to-first-audio (the pipeline benchmark
/// covers Vox's own µs-scale overhead; only the device-period tail is outside
/// software measurement). Each cycle cancels after the first chunk, so an
/// iteration costs the synthesis *start*, not a whole spoken utterance.
///
/// Opt-in via VOX_BENCH_SAPI=1 (needs an installed SAPI voice; German is
/// preferred, matching the product). With VOX_REQUIRE_TTFA_BUDGET=1 the §1.2
/// Q1 budget — first audio under 200 ms — becomes a hard pass/fail.
#if defined(_WIN32)

#  include <benchmark/benchmark.h>
#  include <chrono>
#  include <cstddef>
#  include <cstdlib>
#  include <memory>
#  include <ratio>
#  include <span>
#  include <stdexcept>
#  include <string>
#  include <string_view>
#  include <vector>

#  include <vox/tts/sapi_tts_engine.hpp>
#  include <vox/tts/voice_selection.hpp>

#  include "announce_fixture.hpp"
#  include "percentile_report.hpp"

namespace {

/// Q1 (§1.2): time-to-first-audio for short uncached text, in microseconds.
constexpr double Q1BudgetUs = 200'000.0;

/// True when the environment variable @p name is set to "1" (_dupenv_s — the
/// CRT's bounds-checked getenv, same pattern as the integration tests).
bool envFlag(const char* name) {
  char* value = nullptr;
  if (std::size_t size = 0; ::_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
    return false;
  }
  const bool set = std::string_view(value) == "1";
  std::free(value);
  return set;
}

/// One cycle: synthesize() until the engine delivers its first PCM chunk, then
/// cancel (from within the sink, as the barge-in path does).
void ttfaSapiFirstChunk(benchmark::State& state) {
  if (!envFlag("VOX_BENCH_SAPI")) {
    state.SkipWithMessage("set VOX_BENCH_SAPI=1 to run against the real SAPI engine");
    return;
  }
  std::unique_ptr<vox::tts::SapiTtsEngine> engine;
  try {
    engine =
        std::make_unique<vox::tts::SapiTtsEngine>(vox::tts::VoiceSelectionPolicy::PreferGerman);
  } catch (const std::runtime_error&) {
    state.SkipWithError("no usable SAPI voice on this machine");
    return;
  }

  const std::string text = vox::bench::makeOutput().announce(vox::bench::savedButton()).text;
  std::vector<double> samplesUs;
  samplesUs.reserve(static_cast<std::size_t>(state.max_iterations));

  for ([[maybe_unused]] auto keepRunning : state) {
    bool seenFirstChunk = false;
    std::chrono::steady_clock::time_point firstChunkAt;
    const auto requested = std::chrono::steady_clock::now();
    engine->synthesize(text, [&seenFirstChunk, &firstChunkAt, &engine](std::span<const std::byte>) {
      if (!seenFirstChunk) {
        firstChunkAt = std::chrono::steady_clock::now();
        seenFirstChunk = true;
        engine->cancel(); // first chunk is the measurement; skip the rest
      }
    });
    if (!seenFirstChunk) {
      state.SkipWithError("the engine delivered no PCM");
      return;
    }
    const std::chrono::duration<double, std::micro> ttfa = firstChunkAt - requested;
    state.SetIterationTime(ttfa.count() / 1'000'000.0);
    samplesUs.push_back(ttfa.count());
  }

  const vox::bench::Percentiles percentiles = vox::bench::reportPercentiles(state, samplesUs);
  if (envFlag("VOX_REQUIRE_TTFA_BUDGET")) {
    vox::bench::enforceBudget(
        state.name(), {.metric = "p50", .valueUs = percentiles.p50Us, .budgetUs = Q1BudgetUs});
  }
}

BENCHMARK(ttfaSapiFirstChunk)->UseManualTime()->Iterations(100)->Unit(benchmark::kMillisecond);

} // namespace

#endif // defined(_WIN32)
