# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
#
# Converts google-benchmark JSON (with the p50_us/p99_us/p999_us counters from
# percentile_report.hpp) into github-action-benchmark's customSmallerIsBetter
# format, for the relative-regression comparison against the dev baseline (#41).
#
# Only the deterministic pipeline benchmark feeds the relative gate: the real
# SAPI engine's latency varies with runner load, so its budget is enforced
# absolutely in-process (VOX_REQUIRE_TTFA_BUDGET) instead of relatively here.
[ .benchmarks[]
  | select(.run_type == "iteration")
  | select(.name | startswith("ttfaPipeline"))
  | . as $bench
  | to_entries[]
  | select(.key | test("^p(50|99|999)_us$"))
  | { name: (($bench.name | sub("/.*$"; "")) + " " + (.key | rtrimstr("_us"))),
      unit: "us",
      value: .value }
]
