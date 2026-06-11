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
#  include <optional>
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

/// One synthesize() guarded against engine exceptions: a transient SAPI
/// failure must surface as a recorded benchmark failure (clear error +
/// non-zero exit), not escape and abort the whole benchmark run.
bool synthesizeOrFail(benchmark::State& state, vox::tts::SapiTtsEngine& engine,
                      const std::string& text, const vox::tts::ITtsEngine::PcmSink& sink) {
  try {
    engine.synthesize(text, sink);
    return true;
  } catch (const std::exception& error) {
    vox::bench::failBenchmark(state, std::string("synthesize() failed: ") + error.what());
    return false;
  }
}

/// Builds the engine (German preferred, as the product does), or records a
/// failure and returns null. Running this benchmark is an explicit opt-in, so
/// an unusable engine is a hard failure (non-zero exit) with the original
/// diagnostics — the CI perf gate must not pass silently.
std::unique_ptr<vox::tts::SapiTtsEngine> makeEngineOrFail(benchmark::State& state) {
  try {
    return std::make_unique<vox::tts::SapiTtsEngine>(vox::tts::VoiceSelectionPolicy::PreferGerman);
  } catch (const std::runtime_error& error) {
    vox::bench::failBenchmark(state, std::string("the SAPI engine is unusable: ") + error.what());
    return nullptr;
  }
}

/// One measured cycle: synthesize() until the engine delivers its first PCM
/// chunk, then cancel (from within the sink, as the barge-in path does).
/// Returns the request-to-first-chunk latency, or nullopt after a recorded
/// failure.
std::optional<double> measureFirstChunkUs(benchmark::State& state, vox::tts::SapiTtsEngine& engine,
                                          const std::string& text) {
  bool seenFirstChunk = false;
  std::chrono::steady_clock::time_point firstChunkAt;
  const auto requested = std::chrono::steady_clock::now();
  const bool synthesized = synthesizeOrFail(
      state, engine, text, [&seenFirstChunk, &firstChunkAt, &engine](std::span<const std::byte>) {
        if (!seenFirstChunk) {
          firstChunkAt = std::chrono::steady_clock::now();
          seenFirstChunk = true;
          engine.cancel(); // first chunk is the measurement; skip the rest
        }
      });
  if (!synthesized) {
    return std::nullopt;
  }
  if (!seenFirstChunk) {
    vox::bench::failBenchmark(state, "the engine delivered no PCM");
    return std::nullopt;
  }
  return std::chrono::duration<double, std::micro>(firstChunkAt - requested).count();
}

/// The benchmark: warm up once, then sample request-to-first-chunk cycles.
void ttfaSapiFirstChunk(benchmark::State& state) {
  if (!envFlag("VOX_BENCH_SAPI")) {
    state.SkipWithMessage("set VOX_BENCH_SAPI=1 to run against the real SAPI engine");
    return;
  }
  const std::unique_ptr<vox::tts::SapiTtsEngine> engine = makeEngineOrFail(state);
  if (!engine) {
    return;
  }

  const std::string text = vox::bench::makeOutput().announce(vox::bench::savedButton()).text;

  // Warm up: the very first synthesize() pays one-time engine/voice
  // initialization (>1 s observed on CI) — app-startup cost, not the
  // steady-state announce latency Q1 budgets. One throwaway call excludes it.
  if (!synthesizeOrFail(state, *engine, text,
                        [&engine](std::span<const std::byte>) { engine->cancel(); })) {
    return;
  }

  std::vector<double> samplesUs;
  samplesUs.reserve(static_cast<std::size_t>(state.max_iterations));

  for ([[maybe_unused]] auto keepRunning : state) {
    const std::optional<double> ttfaUs = measureFirstChunkUs(state, *engine, text);
    if (!ttfaUs) {
      return;
    }
    state.SetIterationTime(*ttfaUs / 1'000'000.0);
    samplesUs.push_back(*ttfaUs);
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
