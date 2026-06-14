# REQ-013: VOX_VOICE and VOX_LEXICON override their part of the language choice

> **Status:** Implemented (milestone 1).

## Target audience
Power users who want to keep one overall language but pin a specific voice
(`VOX_VOICE`) or a specific announcement table (`VOX_LEXICON`) independently — e.g.
a preferred German voice by name, or a hand-tuned lexicon file.

## Context
`VOX_LANGUAGE` sets one language for both voice and lexicon (REQ-012). A user may
still want to override just one part: name an explicit voice, or point at an
explicit lexicon file. Each override must take precedence over the
`VOX_LANGUAGE`-derived choice for its own part only, and a *broken* override (a
named voice that is not installed, or an unreadable `VOX_LEXICON` file) must not
silently re-enable the part it replaced — it must fall back to the never-silent /
embedded-default behaviour, not to the `VOX_LANGUAGE`-derived value it was meant to
override.

## Requirement
Vox **shall** let `VOX_VOICE` override the selected voice (by name, ASCII
case-insensitive, winning over the requested language) and `VOX_LEXICON` override
the announcement lexicon (an explicit authoritative file, winning over the
per-language directory lookup), each overriding only its own part, and a broken
override (named voice not installed, or unreadable/rejected explicit file) shall
skip the part it replaced and fall back rather than silently reverting to the
`VOX_LANGUAGE`-derived choice.

## Quality goals
- **Reliability:** a broken `VOX_VOICE` falls back to the system/fallback voice
  (not to the requested-language voice) and a broken `VOX_LEXICON` falls back to
  the embedded German default (not to the directory lookup); in both cases the
  reader still speaks and the fallback is reported.
- **Maintainability:** both overrides follow the same precedence rule
  (override > `VOX_LANGUAGE` > fallback), so the behaviour is described once and
  mirrored in the voice and lexicon code paths.

## Acceptance / test concept
- Voice side, pure — `tests/tts/voice_selection_test.cpp`
  (`SelectVoice.AnExplicitVoiceNameWinsOverTheLanguagePreference`,
  `.TheExplicitNameIsCaseInsensitive`, `.AMissingExplicitVoiceSkipsTheLanguagePreference`,
  `.AMissingExplicitVoiceUnderRequiredYieldsNothing`).
- Voice side end-to-end — `tests/app/default_app_test.cpp`
  (`DefaultAppTest.VoxVoiceSelectsTheNamedVoice`, `.AnUnknownVoxVoiceFallsBackWithAWarning`).
- Lexicon side — `tests/app/lexicon_loader_test.cpp`
  (`LexiconLoaderTest.AnExplicitFileWinsAndItsDeclaredLanguageStands`,
  `.ABrokenExplicitFileFallsBackToTheDefaultNotTheDirectory`) and end-to-end
  `DefaultAppTest.SpeaksTheWordsOfTheFileVoxLexiconPointsAt`.

Traces: issues #88, #61 · architecture ADR-07.

## Notes / open questions
Voice override: `preferredVoice()` (`src/tts/src/voice_selection.cpp`) returns
`nullopt` (not the language match) when a set `VOX_VOICE` name is missing, so
`selectVoice` proceeds to fallback. Lexicon override: `loadLexicon()`
(`src/app/src/lexicon_loader.cpp`) treats a non-empty `VOX_LEXICON` as
authoritative and does no directory lookup after it; a broken explicit file falls
back to the embedded default, never to the directory. The diagnostic emitted when a
winning override's language *diverges* from `VOX_LANGUAGE` is REQ-014.
