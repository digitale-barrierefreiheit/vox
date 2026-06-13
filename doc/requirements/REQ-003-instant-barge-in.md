# REQ-003: Instant barge-in interrupts current speech

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking power users listening at high speech rates who navigate
rapidly and must not have to wait for a previous announcement to finish before the
next control speaks.

## Context
A new announcement (a focus change) or a navigation/mute keypress arrives while
Vox is still speaking the previous utterance. To keep navigation responsive, the
reader interrupts immediately: the audio sink is flushed and the in-flight
synthesis is cancelled, so stale audio is dropped (#36, #38).

## Requirement
When a new announcement or a navigation/mute keypress occurs while speech is
playing, Vox **shall** barge in by immediately flushing the audio sink and
cancelling the in-flight synthesis, so the current speech stops at once and any
stale resampler tail from the interrupted utterance is discarded.

## Quality goals
- **Q1 (latency):** a new announcement preempts the old without waiting for it to
  finish — small audio buffers, a cancellable synthesis worker, and a flushable
  queue.
- **Reliability:** a drain landing *after* a barge-in is dropped via the sink's
  flush-generation guard (no audio bleed from a cancelled utterance); stopping
  mid-utterance skips the end-of-stream drain.

## Acceptance / test concept
- Reader-level interruption — `tests/app/reader_test.cpp`
  (`Reader.NavigationKeyBargesIn` — `NavigateNext` raises `tts.cancelCount()` and
  `audio.flushCount()`; `Reader.ToggleSpeechMutesThenUnmutes`;
  `Reader.SkipsTheEndOfStreamDrainWhenStoppingMidUtterance`).
- Audio-sink cancel/flush semantics — `tests/audio/wasapi_audio_sink_test.cpp`
  (`WasapiAcquisitionTest.DrainAfterAFlushDropsTheStaleTail`,
  `.AWriteAfterAFlushResetsTheConverter`, `.AWriteRacingABargeInReArmsTheFlush`).
- Flush drops buffered audio while keeping the cumulative count —
  `tests/audio/fake_audio_sink_test.cpp`
  (`FakeAudioSink.FlushDropsBufferedButKeepsCumulativeCount`).

Traces: issues #36, #38 · architecture §1.2 Q1, §4 (instant barge-in), §5
(interruption), §6 (barge-in across Input → Output → TTS → Audio).

## Notes / open questions
Barge-in is implemented in `vox::app::Reader::bargeIn()` as `audio_.flush()` +
`tts_.cancel()`; the WASAPI sink bumps a flush generation so a stale converter
tail (drain) arriving after the flush is dropped, and the first write of the new
stream resets the converter so the previous utterance's filter history never
bleeds in. The architecture ultimately frames barge-in as owned by the Output
Manager priority queue (a later milestone); in milestone 1 the `Reader` drives it
directly.
