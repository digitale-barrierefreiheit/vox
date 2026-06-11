// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Benchmark entry point: run, then enforce the absolute latency budgets.
///
/// google-benchmark's default main, plus the §8.6.4 pass/fail: benchmarks
/// record budget violations (percentile_report.hpp) while running, and any
/// violation turns into a non-zero exit code — so CI fails on a blown budget
/// even when every benchmark "ran fine".
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <iostream>
#include <string>

#include "percentile_report.hpp"

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return EXIT_FAILURE;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  const auto& violations = vox::bench::budgetViolations();
  for (const std::string& violation : violations) {
    std::cerr << "BUDGET VIOLATION: " << violation << '\n';
  }
  return violations.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}
