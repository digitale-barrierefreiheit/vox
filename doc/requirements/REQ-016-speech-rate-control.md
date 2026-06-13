# REQ-016: Normalized speaking-rate control

> **Status:** Implemented (milestone 1) — the rate control; the high-rate
> intelligibility *acceptance* check (R6) is not yet built (see notes).

## Target audience
Blind German-speaking power users who listen at elevated speech rates (commonly
1.5–3×) for efficiency and need a single, predictable rate control that the active
engine honours.

## Context
Experienced screen-reader users raise the speech rate well above default.
`ITtsEngine` exposes a normalized rate that each backend maps to its own scale; the
SAPI backend clamps to SAPI's −10..+10 range. The control is engine-agnostic so the
same user setting behaves consistently as engines change.

## Requirement
Vox **shall** expose a normalized speaking-rate control (−10..+10) that clamps
out-of-range requests to the nearest bound and forwards the clamped rate to the
active engine, so users can listen at elevated rates.

## Quality goals
- **Q3 (German quality):** users can raise the rate for efficient listening; the
  rate is applied by the engine without re-synthesis of already-queued audio.
- **Reliability:** out-of-range rate requests are clamped (never rejected or passed
  raw to the engine), so a bad value cannot push the backend outside its supported
  range.

## Acceptance / test concept
- Pure clamp — `tests/tts/rate_test.cpp` (`ClampRate.PassesThroughInRangeValues`,
  `.ClampsToTheBoundaries`, `.ClampsOutOfRangeValues`).
- SAPI backend with mock COM — `tests/tts/sapi_tts_engine_test.cpp`
  (`SapiEngineTest.SetRateForwardsClampedValueToSapi`,
  `.SetRateClampsOutOfRangeValues`).
- Live SAPI — `tests/tts/sapi_tts_engine_itest.cpp`
  (`SapiTtsEngineTest.SetRateDoesNotThrow`).

Traces: issue #35 · architecture §1.2 Q3, §8.6 (speech-rate scaling), R6, ADR-12.

## Notes / open questions
`clampRate` is the pure seam; `setRate` forwards the clamped value to SAPI
(`src/tts/src/sapi_tts_engine.cpp`). The architecture's R6 / §8.6.3 **high-rate
intelligibility acceptance check** — verifying a voice stays intelligible at
1.5–3× on any voice change — is *not* yet built; it is a voice-acceptance test that
belongs with the dogfooding/usability track (REQ-020) and a future voice-change
gate.
