# REQ-012: One VOX_LANGUAGE setting drives both the lexicon and the voice language

> **Status:** Implemented (milestone 1).

## Target audience
German-speaking users (and users who switch Vox to another language) who expect a
single language choice to make *both* the spoken announcement words (role/state
vocabulary) *and* the TTS voice match that language, without configuring two
settings that could silently disagree.

## Context
The composition root builds the production stack at startup. The announcement
vocabulary comes from a per-language lexicon table and the spoken voice comes from
SAPI voice selection — two independent subsystems. If each read its own language
input they could diverge (German words spoken by an English voice, or vice versa).
A single user-facing language input must drive both as one choice, defaulting to
German.

## Requirement
Vox **shall** read one `VOX_LANGUAGE` setting (a validated BCP-47 language tag,
defaulting to German per ADR-07, with an invalid value reported on stderr and
ignored in favour of the default) and use that single language to drive *both* the
requested TTS voice language *and* the per-language announcement lexicon
resolution, so that words and voice are requested as one language choice.

## Quality goals
- **Q3 (German quality):** the same requested tag selects the matching-language
  voice and the matching-language lexicon file (`lexicon\<tag>.lex`), so German
  announcements are spoken in German; the default is `de`.
- **Reliability:** an unusable `VOX_LANGUAGE` never aborts startup — it is reported
  on stderr and startup proceeds with the German default for both voice and lexicon
  (the reader always speaks).

## Acceptance / test concept
- End-to-end through the composition root with mock COM —
  `tests/app/default_app_test.cpp`
  (`DefaultAppTest.VoxLanguageSelectsALanguageFileNextToTheExecutable` —
  `VOX_LANGUAGE=qaa` loads `lexicon\qaa.lex` and the announcement uses its words;
  `.AnUnmatchedVoxLanguageFallsBackToTheDefaultVoiceWithAWarning`;
  `.AnInvalidVoxLanguageFallsBackToGermanWithAWarning` — invalid tag → German voice
  + German embedded lexicon + a warning;
  `.SpeaksGermanFromTheEmbeddedDefaultWithoutConfiguration` — no config → German).
- Loader unit tests — `tests/app/lexicon_loader_test.cpp`
  (`LexiconLoaderTest.AnAbsentFileFallsBackToTheEmbeddedGermanDefault` and the
  language-resolution cases; `IsLanguageTag.AcceptsBcp47ShapedTags`,
  `.RejectsEverythingThatCouldEscapeAFileName`, `.RejectsMalformedTagStructure`).

Traces: issues #88, #61 · architecture §1.2 Q3, ADR-07.

## Notes / open questions
`requestedLanguage()` (`src/app/src/default_app.cpp`) computes the validated tag
once (default `de`) and feeds it to *both* `voiceRequest.language` and
`loadConfiguredLexicon(languageTag)`. `isLanguageTag()` deliberately rejects
anything that could escape a file name (slashes, dots, `..`) since the tag becomes
a `<tag>.lex` stem. Per-part overrides of this single choice are REQ-013; the
divergence diagnostic is REQ-014.
