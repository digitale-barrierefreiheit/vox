# REQ-018: Time-to-first-audio budget for short uncached text

> **Status:** Implemented (milestone 1) — measured and gated in CI.

## Target audience
Blind, native-German screen-reader users who navigate real applications and
generate thousands of tiny UI utterances per hour, for whom each announcement must
feel instant or navigation becomes unbearable (§1.2 Q1).

## Context
When focus moves to a UI element whose announcement is not already cached (e.g. a
focused "Speichern" button announced as "Schaltfläche, Speichern"), Vox composes
the German utterance through the Output Manager and synthesizes it through the SAPI
engine. This is the dominant, repeatedly-felt latency on a no-GPU laptop — Vox's
own pipeline overhead (event-thread handoff, worker wakeup, utterance construction,
synthesis dispatch, first-chunk delivery) plus the synthesizer's own
synthesis-start cost.

## Requirement
For short uncached text, Vox **shall** reach the first delivered PCM chunk in under
200 ms time-to-first-audio at p50 on a no-GPU laptop, of which Vox's own pipeline
overhead (focus event to first PCM write, excluding the synthesizer) **shall**
consume at most 10% (p99 under 20 ms).

## Quality goals
- **Q1 (latency):** uncached short text p50 < 200 ms time-to-first-audio (real
  SAPI); Vox pipeline overhead p99 < 20 ms (= 10% of the 200 ms Q1 budget).

## Acceptance / test concept
Two percentile-budget microbenchmarks gate this:
- `benchmarks/ttfa_pipeline_benchmark.cpp` (the `ttfaPipeline` benchmark) drives
  the real `Reader`/`OutputManager` over deterministic fakes (10,000 iterations)
  and unconditionally enforces **p99 ≤ 20,000 µs** (`PipelineBudgetUs`, 10% of the
  Q1 budget) via `enforceBudget`.
- `benchmarks/ttfa_sapi_benchmark.cpp` (the `ttfaSapiFirstChunk` benchmark)
  measures request-to-first-chunk against the real German SAPI engine (100
  iterations, opt-in via `VOX_BENCH_SAPI=1`) and, when `VOX_REQUIRE_TTFA_BUDGET=1`,
  enforces **p50 ≤ 200,000 µs** (`Q1BudgetUs`).

Both announce the same short uncached utterance
(`benchmarks/announce_fixture.hpp`, "Schaltfläche, Speichern") through the
production Output Manager with the embedded German lexicon; percentiles and the
pass/fail budget come from `benchmarks/percentile_report.hpp`. They run on the CI
`benchmarks` job (#41); a violation is a non-zero exit.

Traces: issue #41 · architecture §1.2 Q1, §8.6.4, §10.1, R15.

## Notes / open questions
The pipeline benchmark's p99 budget is the authoritative absolute gate; only its
p50 feeds the relative CI regression gate, because µs-scale tails vary up to ≈ 8×
between runs on shared hosted runners (§8.6.4). The real-SAPI budget is opt-in and
Windows-only and needs a German-voice-provisioned runner; it warms up once to
exclude one-time engine init, and the 200 ms p50 is the synthesis-start number, not
end-to-end-including-device-period (the device-period tail is outside software
measurement). The cached-instant and continuous-reading targets are not yet
measured — REQ-019.
