// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Percentile reporting + absolute latency budgets for the benchmarks.
///
/// Architecture §8.6.4: hot-path benchmarks track p50/p99/p99.9 — tail latency
/// is what users feel, not the mean — and encode the §1.2 budgets as pass/fail.
/// google-benchmark's own statistics aggregate across repetitions, so the
/// benchmarks collect one duration per iteration themselves and report true
/// per-sample percentiles as counters. A budget violation is recorded here and
/// turned into a non-zero exit code by main() after all benchmarks ran.
#ifndef VOX_BENCHMARKS_PERCENTILE_REPORT_HPP
#define VOX_BENCHMARKS_PERCENTILE_REPORT_HPP

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace vox::bench {

/// Per-sample percentiles of one benchmark's iteration latencies.
struct Percentiles {
  double p50Us{0.0};
  double p99Us{0.0};
  double p999Us{0.0};
};

/// Nearest-rank percentile of @p sortedUs (ascending). @p pct in (0, 100].
inline double percentileOf(const std::vector<double>& sortedUs, double pct) {
  const auto count = static_cast<double>(sortedUs.size());
  const auto rank = static_cast<std::size_t>(std::max(1.0, std::ceil(pct / 100.0 * count)));
  return sortedUs[rank - 1];
}

/// Computes p50/p99/p99.9 of @p samplesUs (sorted in place) and attaches them
/// to @p state as counters (p50_us/p99_us/p999_us), which lands them in the
/// JSON output the CI baseline comparison reads.
inline Percentiles reportPercentiles(benchmark::State& state, std::vector<double>& samplesUs) {
  std::ranges::sort(samplesUs);
  const Percentiles percentiles{.p50Us = percentileOf(samplesUs, 50.0),
                                .p99Us = percentileOf(samplesUs, 99.0),
                                .p999Us = percentileOf(samplesUs, 99.9)};
  state.counters["p50_us"] = percentiles.p50Us;
  state.counters["p99_us"] = percentiles.p99Us;
  state.counters["p999_us"] = percentiles.p999Us;
  return percentiles;
}

/// The budget violations recorded so far (empty = all budgets held).
inline std::vector<std::string>& budgetViolations() {
  static std::vector<std::string> violations;
  return violations;
}

/// Fails @p state with @p message AND records it as a violation: google-
/// benchmark reports a skipped-with-error benchmark but still exits 0, so the
/// registry is what turns a broken harness into a non-zero exit (see main()).
inline void failBenchmark(benchmark::State& state, const std::string& message) {
  state.SkipWithError(message);
  budgetViolations().push_back(state.name() + ": " + message);
}

/// One absolute-budget assertion: @p metric (e.g. "p99") measured at
/// @p valueUs against the allowed @p budgetUs.
struct BudgetCheck {
  std::string_view metric{};
  double valueUs{0.0};
  double budgetUs{0.0};
};

/// Records a violation when the check's value exceeds its budget. The process
/// then exits non-zero (see main()), making the absolute budget a hard CI gate.
inline void enforceBudget(std::string_view benchmarkName, const BudgetCheck& check) {
  if (check.valueUs > check.budgetUs) {
    budgetViolations().push_back(std::format("{} {} = {:.1f} us exceeds the budget of {:.0f} us",
                                             benchmarkName, check.metric, check.valueUs,
                                             check.budgetUs));
  }
}

} // namespace vox::bench

#endif // VOX_BENCHMARKS_PERCENTILE_REPORT_HPP
