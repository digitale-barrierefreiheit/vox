# REQ-014: A divergence between voice and lexicon language is reported, not rejected

> **Status:** Implemented (milestone 1).

## Target audience
Users who deliberately or accidentally combine a `VOX_LANGUAGE` with a `VOX_VOICE`
or `VOX_LEXICON` of a different language; they need a clear diagnostic that the
parts disagree, without losing speech.

## Context
Because `VOX_VOICE` / `VOX_LEXICON` each override their own part and win over
`VOX_LANGUAGE` (REQ-013), the chosen voice's language can differ from the requested
language, or an explicit lexicon file can declare a different language than
`VOX_LANGUAGE`. The user must be told the parts diverge (so a surprising mix is
explained), but the divergence must not block startup or suppress the override —
the override still wins and the reader still speaks. A difference by region only
(same primary subtag, e.g. de vs de-AT) is not a real divergence and must not warn.

## Requirement
Vox **shall**, when an explicit voice or explicit lexicon file resolves to a
language whose primary subtag differs from the requested `VOX_LANGUAGE`, report
that divergence on stderr (naming the parts and that the explicit choice wins) and
still use the explicit choice — never rejecting it — while a difference only in
region (matching primary subtags, compared case-insensitively) shall not be
reported.

## Quality goals
- **Reliability:** a diverging override is honoured and startup continues (the
  reader speaks); the divergence is a non-fatal stderr diagnostic emitted by the
  app layer, while the TTS and lexicon modules expose outcomes and do no I/O.
- **Q3 (German quality):** no false divergence warning when the override and
  request share a primary language subtag (de matches de-AT, DE matches de), so
  region-only differences do not produce spurious noise.

## Acceptance / test concept
- Voice-side — `tests/app/default_app_test.cpp`
  (`DefaultAppTest.AVoxVoiceDivergingFromVoxLanguageWinsWithAWarning` —
  `VOX_LANGUAGE=en` + a German `VOX_VOICE` → explicit voice chosen, "the explicit
  voice wins"; `.AVoxVoiceAgreeingWithVoxLanguageDoesNotWarnAboutDivergence`).
- Lexicon-side — `tests/app/lexicon_loader_test.cpp`
  (`LexiconLoaderTest.AnExplicitFileWithADifferentLanguageWinsWithAWarning`,
  `.AnExplicitFileSharingThePrimaryLanguageDoesNotWarn`).
- The primary-subtag comparison underpinning both —
  `tests/tts/voice_selection_test.cpp`
  (`SameLanguage.ComparesPrimarySubtagsCaseInsensitively`,
  `PrimarySubtag.CutsAtTheFirstHyphen`).

Traces: issues #88, #61 · architecture ADR-07, ADR-12 (diagnostics by the app, not
the modules).

## Notes / open questions
Voice divergence is reported by `reportVoiceOutcome()`
(`src/app/src/default_app.cpp`), lexicon divergence by the explicit-file branch in
`loadLexicon()`, both using a case-insensitive primary-subtag comparison. Distinct
from the override-precedence requirement (REQ-013): precedence governs *which*
source wins; this governs the *observable diagnostic* when the winning override's
language differs from the request, and the suppression of region-only false
positives. Per ADR-07, all fallback/divergence reporting is done by the app on
stderr; the lexicon and TTS modules expose outcomes but do no I/O.
