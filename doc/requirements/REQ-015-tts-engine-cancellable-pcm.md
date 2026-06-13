# REQ-015: The TTS engine synthesizes cancellable PCM in a fixed format

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking users whose every announcement is produced by the TTS
engine; they need synthesis that starts streaming audio promptly, stops instantly
on barge-in, and never crashes the reader on odd input.

## Context
The Output Manager hands an utterance's text to the TTS engine, which synthesizes
it to PCM and streams it to the audio sink chunk by chunk. The engine sits behind
the `ITtsEngine` interface (with a SAPI5 backend, #35), so the reader and the audio
pipeline depend on a stable synthesis contract: a fixed output format, chunked
delivery, prompt cancellation (for barge-in, REQ-003), and defined behaviour on
empty or malformed text.

## Requirement
Vox **shall** synthesize an utterance's UTF-8 text to 16-bit / 22.05 kHz / mono
PCM, delivering it chunk by chunk to the audio sink and aborting synthesis promptly
when cancelled, treating empty text as a no-op and rejecting invalid UTF-8.

## Quality goals
- **Q1 (latency):** PCM is delivered incrementally (first chunk as soon as the
  engine produces it, enabling the time-to-first-audio budget of REQ-018) and
  cancellation aborts without waiting for the whole utterance.
- **Reliability:** empty text is a no-op (no spurious audio); invalid UTF-8 and
  engine `Speak` failures raise a typed `EngineError` rather than crashing;
  cancellation from inside the sink callback stops further chunks without throwing.

## Acceptance / test concept
- SAPI engine with mock COM — `tests/tts/sapi_tts_engine_test.cpp`
  (`SapiEngineTest.SynthesizeForwardsPcmToTheSink`, `.SynthesizeEmptyTextIsANoOp`,
  `.SynthesizeThrowsOnInvalidUtf8`, `.CancelDuringSynthesisAbortsWithoutThrowing`,
  `.FormatReportsTheFixedOutputShape` — 22050 / 16 / mono).
- Live SAPI — `tests/tts/sapi_tts_engine_itest.cpp`
  (`SapiTtsEngineTest.SynthesizesTextToWholeFramePcm`,
  `.CancelFromTheSinkPreventsFurtherChunks`, `.ExposesAVoiceAndTheForcedFormat`).

Traces: issue #35 · architecture §5.2 (Tier-1), §1.2 Q1, ADR-12.

## Notes / open questions
The fixed 22.05 kHz / 16-bit / mono output is the SAPI5 wire format; the audio
pipeline resamples it to the device mix format (REQ-017). `ITtsEngine` is the
strategy seam (ADR-13) that lets the reader and benchmarks run over a deterministic
fake engine, while the real SAPI path is exercised by the labelled `*_itest`.
Speech-rate control over the same interface is REQ-016.
