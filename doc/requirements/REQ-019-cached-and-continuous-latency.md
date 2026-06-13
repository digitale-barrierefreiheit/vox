# REQ-019: Latency for cached UI utterances and continuous reading

> **Status:** Recognized, not yet implemented — no phrase cache and no
> streaming-reading first-audio benchmark exist yet.

## Target audience
Blind, native-German screen-reader users who re-encounter the same UI
announcements constantly (the cached path) and who read paragraphs, documents, and
browse-mode content (the continuous-reading path).

## Context
Two latency paths beyond first-encounter uncached short text: (1) a UI utterance
whose synthesized audio is already cached, where only playback remains and the
result should feel effectively instant; and (2) continuous reading of longer text,
synthesized incrementally and streamed so the user hears speech promptly without
waiting for the whole passage. These correspond to the tiered-voice plan
(architecture §6: Tier-1 fast neural for short uncached text, Tier-2 streaming
voice for documents).

## Requirement
For a cached UI utterance Vox **shall** begin audio with playback latency only
(effectively instant, no re-synthesis), and for continuous reading Vox **shall**
reach first audio in under approximately 300 ms by streaming synthesis
incrementally.

## Quality goals
- **Q1 (latency):** cached UI utterance — effectively instant (playback only, no
  synthesis); continuous reading — first audio < ≈ 300 ms.

## Acceptance / test concept
Pending. No utterance/phrase cache and no streaming-reading first-audio benchmark
exist yet — verified by absence: `src/` contains no phrase/utterance cache (the
only `cache` symbols are the unrelated UIA element cache in `src/provider` and the
PCM ring in `src/audio`), and the only time-to-first-audio benchmarks
(`benchmarks/ttfa_pipeline_benchmark.cpp`, `benchmarks/ttfa_sapi_benchmark.cpp`)
exercise only the short *uncached* single-utterance path. When built, this shall be
gated by percentile-budget benchmarks per §8.6.4 — a cached-playback latency
benchmark and a continuous-reading first-audio benchmark — mirroring the REQ-018
gate.

Traces: issue #41 (TTFA benchmarking, general) · architecture §1.2 Q1, §6, §8.6.4,
§10.1.

## Notes / open questions
Architecture §1.2 Q1, §10.1, and §6 all state these targets, but no code or
benchmark realizes the cache or the streaming-reading measurement yet — milestone-1
work only gates the uncached single-utterance path (REQ-018). The "≈ 300 ms" figure
is intentionally approximate per the architecture ("first audio ~300 ms acceptable;
quality prioritized"). Open question: assign dedicated tracking issues for the
phrase cache and the streaming-reading benchmark (separate from #41).
