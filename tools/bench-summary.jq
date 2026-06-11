# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
#
# Renders one markdown table row per benchmark (use with jq -r) for the per-run
# summary / PR comment (#41): | name | p50 | p99 | p99.9 | budget |.
# A skipped benchmark (e.g. the SAPI one without VOX_BENCH_SAPI) has no
# counters and renders em-dashes.
def fmtus:
  if . == null then "—"
  elif . >= 1000 then (((. / 100 | round) / 10 | tostring) + " ms")
  else (((. * 10 | round) / 10 | tostring) + " µs")
  end;
.benchmarks[]
| select(.run_type == "iteration")
| (.counters // .) as $c
| (.name | sub("/.*$"; "")) as $name
| ( if $name == "ttfaPipeline" then "p99 ≤ 20 ms (10% of Q1)"
    elif $name == "ttfaSapiFirstChunk" then "p50 < 200 ms (Q1)"
    else "—" end ) as $budget
| "| `\($name)` | \($c.p50_us | fmtus) | \($c.p99_us | fmtus) | \($c.p999_us | fmtus) | \($budget) |"
