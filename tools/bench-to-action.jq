# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
#
# Converts google-benchmark JSON (with the percentile counters from
# percentile_report.hpp) into github-action-benchmark's customSmallerIsBetter
# format, for the relative-regression comparison against the dev baseline (#41).
#
# Only the deterministic pipeline benchmark feeds the relative gate, and only
# its MEDIAN (p50): on shared hosted runners the µs-scale tail percentiles are
# dominated by scheduler noise (observed: p99 0.5 -> 4.1 µs, 8x, between runs
# of identical code), so gating them relatively is a coin flip. The tails are
# protected by the absolute in-process budget (p99 <= 10% of Q1) and reported
# per run in the results comment/summary. The real SAPI engine's latency also
# varies with runner load; its budget is likewise enforced absolutely
# (VOX_REQUIRE_TTFA_BUDGET) instead of relatively here.
# Our google-benchmark serializes user counters as top-level keys of each
# benchmark object (verified against its JSON output); fall back to a nested
# .counters object should a future version move them there.
[ .benchmarks[]
  | select(.run_type == "iteration")
  | select(.name | startswith("ttfaPipeline"))
  | . as $bench
  | ($bench.counters // $bench)
  | to_entries[]
  | select(.key == "p50_us")
  | { name: (($bench.name | sub("/.*$"; "")) + " p50"),
      unit: "us",
      value: .value }
]
