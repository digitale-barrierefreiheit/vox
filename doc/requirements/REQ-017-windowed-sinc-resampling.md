# REQ-017: TTS PCM resampled to the device mix format without audible quality loss

> **Status:** Implemented (milestone 1).

## Target audience
Blind users listening to synthesized speech for hours, including at 1.5–3× rate;
audible resampling artefacts (imaging/aliasing) make fast speech harder to parse
and more fatiguing.

## Context
WASAPI shared mode plays in the device mix format (commonly 48 kHz float32 stereo)
while the SAPI engine emits 16-bit mono at its own rate (e.g. 22.05 kHz).
`PcmConverter` bridges the two on the producer thread — sample-rate conversion,
channel up-mix, and sample-format change — streaming chunk by chunk with state
carried across calls (#55).

## Requirement
Vox **shall** resample TTS PCM to the device mix format using a windowed-sinc
polyphase filter that suppresses spectral images/aliasing to an inaudible level,
and **shall** pass samples through exactly (bit-accurate, zero added delay)
whenever the source and device rates are equal.

## Quality goals
- **Q3 (German quality):** upsampling image attenuation < −70 dB (measured ≈ −88
  dB) and at least 20 dB better than the prior linear interpolator; passband tone
  within 0.05 of unity; the equal-rate path is an exact int16 passthrough that
  preserves the [−32768, 32767] extremes.
- **Q2 (overhead):** converting ≈ 0.19 s of audio (a 4096-sample chunk, 22.05→48
  kHz float32 stereo) stays well under an 8 ms p99 budget (≈ 23× faster than real
  time) with no per-sample heap allocation on the steady-state path (ADR-10).
- **Reliability:** chunk-split streaming is bit-identical to a single call;
  `drain()` flushes the group-delay tail and `reset()`/`drain()` restore clean
  state; non-16-bit / non-mono / zero-rate and non-sample-aligned inputs are
  rejected.

## Acceptance / test concept
`tests/audio/pcm_converter_test.cpp`
(`PcmConverter.WindowedSincSuppressesImagesBetterThanLinear` — Goertzel image at a
−70 dB floor and > 20 dB better than linear, passband within 0.05;
`.SameRateInt16IsExactPassthrough`; `.StreamingAcrossChunksMatchesSingleCall`;
`.DrainFlushesTheGroupDelayTail`; `.MonoToStereoFloatDuplicatesChannels`;
`.RejectsUnsupportedSourceFormat`; `.RejectsNonSampleAlignedInput`). Overhead:
`benchmarks/resampler_benchmark.cpp` (the `resampleChunk` benchmark, p99 enforced
against the 8 ms `ConvertBudgetUs`). Implementation:
`src/audio/src/pcm_converter.cpp` (32-tap, 256-phase Kaiser-windowed sinc; bypass
at equal rates).

Traces: issue #55 · architecture §1.2 Q2, §1.2 Q3, §8.6.4, §10, ADR-10.

## Notes / open questions
Kernel: 32 taps, 256 phases, Kaiser β = 9 (≈ −90 dB stopband); the cutoff is the
lower of source/target Nyquist, so one kernel anti-images on upsampling and
anti-aliases on downsampling. The benchmark's relative median gate (#41) tracks
finer drift than the 8 ms catastrophe budget. This is the audio-quality leg of Q3;
the speech-vocabulary/normalization legs are REQ-005..REQ-009.
