# REQ-011: Discover German voices in both the classic and OneCore catalogues

> **Status:** Implemented (milestone 1).

## Target audience
German-speaking users who install a German voice the modern way — through Windows
Settings / language packs — which registers the voice only in the OneCore hive.
Without discovering it, the user would see Vox fall back to English despite having
installed a German voice.

## Context
Windows exposes SAPI-compatible voices through two registry catalogues: the
classic `SPCAT_VOICES` hive and the OneCore hive (`Speech_OneCore\Voices`), where
Windows-Settings / language-pack voices register. Classic SAPI enumeration does not
see OneCore voices, and the same voice can also appear in both hives under
different token ids. Discovery must therefore search both and reconcile duplicates
before selection runs, or a Settings-installed German voice stays invisible (#52).

## Requirement
Vox **shall** enumerate voices from both the classic `SPCAT_VOICES` catalogue and
the OneCore (`Speech_OneCore`) catalogue and merge them into one selectable list —
classic enumerated first and taking precedence on duplicate (same-name) voices,
with surviving OneCore default flags cleared when a classic default exists — so
that a German voice registered in *either* catalogue is located while a voice
present in both is offered only once.

## Quality goals
- **Q3 (German quality):** a German voice installed only via OneCore (Windows
  Settings / language pack) is found and selected for German text, not bypassed in
  favour of an English fallback.
- **Maintainability:** OneCore discovery is a pure list-merge (`mergeVoices`) plus
  one extra enumeration pass; with no `SPCAT_*` constant for the OneCore hive, its
  category id is a single named literal.

## Acceptance / test concept
- Pure merge — `tests/tts/voice_selection_test.cpp`
  (`MergeVoices.AppendsSecondaryVoicesAfterThePrimaryOnes`,
  `.DropsASecondaryVoiceWhoseNameIsAlreadyKnown`,
  `.KeepsDistinctVariantsOfTheSameVoiceFamily`,
  `.ClearsTheSecondaryDefaultWhenThePrimaryHasOne`,
  `.NeverCollapsesUnnamedVoices`; LANGID→tag mapping
  `LanguageTagFromLangId.MapsPrimaryLanguagesAcrossRegions`,
  `.UnmappedLanguagesYieldAnEmptyTag`).
- SAPI discovery with mock COM — `tests/tts/sapi_tts_engine_test.cpp`
  (`SapiEngineTest.EnumeratesTheClassicAndThenTheOneCoreCategory` — asserts the two
  category ids in order; `.DiscoversAVoiceOnlyVisibleInTheOneCoreCategory` — classic
  empty, the German token served only by the OneCore pass, required German still
  selected).
- LCID-parse helpers — `tests/tts/sapi_internal_test.cpp`
  (`SapiFirstLcid.ParsesASingleHexLcid`, `.TakesTheFirstEntryOfAList`).

Traces: issues #52, #88 · architecture ADR-07.

## Notes / open questions
This is a distinct requirement from German-first *selection* (REQ-010): it governs
*where* a voice is located, independent of *how* one is chosen among the located
set. The two compose — discovery (`mergeVoices` over two `enumerateCategory`
passes in `src/tts/src/sapi_tts_engine.cpp`) feeds the located set into
`selectVoice`. The OneCore category id is the literal
`HKLM\SOFTWARE\Microsoft\Speech_OneCore\Voices`; the BCP-47 tag is derived from a
voice's first `Language` LANGID via `firstLcid` + `languageTagFromLangId`.
