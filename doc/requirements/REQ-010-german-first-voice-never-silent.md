# REQ-010: German-first voice selection with a never-silent fallback

> **Status:** Implemented (milestone 1).

## Target audience
Blind and visually impaired German-speaking users, for whom the screen reader is
the primary (often only) channel to operate Windows; they expect German speech out
of the box and must never be left with a silent reader on a machine that lacks a
German voice.

## Context
At startup the TTS engine must pick exactly one installed voice. The default
requested language is German (ADR-07). The machine may have a German voice, only
an English/other voice, or only voices whose language is unknown. Because the user
operates the machine *through* the reader, a startup that finds no German voice
must still produce intelligible speech rather than going silent.

## Requirement
Vox **shall** select an installed voice whose primary language subtag matches the
requested language (German by default) and, when no such voice exists, fall back to
the system-default voice — or, lacking one, the first available voice — so the
engine constructs successfully and speaks rather than staying silent, failing
construction only when no voice at all is installed or when a German voice is
explicitly required and none is found.

## Quality goals
- **Q3 (German quality):** prefer a voice whose primary subtag equals the
  requested German tag so German text is spoken by a German voice; the match is
  ASCII case-insensitive and region-agnostic (de, de-AT, DE all match de).
- **Reliability:** `selectVoice()` returns a usable voice for every non-empty
  voice set under a non-required request (never `std::nullopt`); the SAPI engine
  throws `EngineError` only when the voice set is empty or an explicit/required
  match is absent.

## Acceptance / test concept
- Pure selection — `tests/tts/voice_selection_test.cpp`
  (`SelectVoice.PicksTheVoiceMatchingTheRequestedLanguage`,
  `.MatchesByPrimarySubtagCaseInsensitively`, `.FallsBackToTheDefaultVoice`,
  `.FallsBackToTheFirstVoiceWhenNoneIsDefault`,
  `.AVoiceWithoutAKnownLanguageNeverMatches`, `.RequiredYieldsNothingWithoutAMatch`,
  `.EmptySetYieldsNothingUnderAnyRequest`).
- SAPI engine with mock COM — `tests/tts/sapi_tts_engine_test.cpp`
  (`SapiEngineTest.ConstructsAndSelectsTheGermanVoice`,
  `.FallsBackToTheEnglishVoiceWhenNoGermanOneExists`,
  `.ThrowsWhenNoVoiceIsAvailable`,
  `.ThrowsWhenGermanIsRequiredButTheVoiceIsEnglish`).
- Live SAPI — `tests/tts/sapi_tts_engine_itest.cpp`
  (`SapiTtsEngineTest.RequireGermanSelectsAGermanVoiceWhereAvailable`; the de-DE CI
  job sets `VOX_REQUIRE_GERMAN_VOICE=1` so a missing German voice is a hard failure
  there).

Traces: issues #35, #52, #88 · architecture §1.2 Q3, ADR-07.

## Notes / open questions
`selectVoice()` is a pure function over plain `VoiceDescriptor`s
(`src/tts/src/voice_selection.cpp`), so the branching is unit-tested without COM or
an installed voice; the SAPI backend enumerates real tokens into descriptors and
applies it. The chosen voice's provenance (`RequestedLanguage` / `Fallback` /
`ExplicitName`) is exposed via `selectedVoice()`; reporting a fallback is the app's
job (REQ-014). The "required" flag is how the de-DE CI gate turns a missing German
voice into a failure instead of a silent English fallback. Finding a German voice
installed via Windows Settings depends on REQ-011 (OneCore discovery).
