# REQ-005: Announcement vocabulary is per-language data, not code

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking screen-reader users, and the localizers/power users who
refine the spoken role and state words for them. They need the words Vox speaks
(e.g. "Schaltfläche", "aktiviert") to be correct and adjustable for their language
without anyone touching or rebuilding C++.

## Context
At startup Vox must choose the announcement vocabulary (control-role and
control-state words) for the active language. The pure `vox::german::Lexicon`
never touches the filesystem; the app-layer loader resolves a file at startup from
`VOX_LEXICON` (an explicit file) or `<lexiconDir>/<tag>.lex` (`VOX_LANGUAGE`,
default `de`), the directory living next to the executable. Each shipped table
(`data/lexicon/de.lex`, `data/lexicon/en.lex`) is a single-source `key = value`
text file that also serves as the German build-time embedded default (#34, #61).

## Requirement
Vox **shall** obtain its announcement role and state words at startup from a
single-source, user-editable per-language lexicon file
(`data/lexicon/<tag>.lex`, selected by `VOX_LANGUAGE` or `VOX_LEXICON`), applying
edits to that file on the next start without any code change or rebuild.

## Quality goals
- **Q3 (German quality):** role/state words are data (e.g.
  `role.button = Schaltfläche`), so German wording can be corrected and refined to
  natural German without code; a user-supplied table's words are spoken verbatim
  end-to-end.
- **Maintainability:** one canonical table per language; adding a language is
  copy–translate–set-`language` with no C++ change; `en.lex` doubles as the
  contributor template.

## Acceptance / test concept
- Loader resolution — `tests/app/lexicon_loader_test.cpp`
  (`LexiconLoaderTest.LoadsTheGermanFileWhenNoLanguageIsRequested`,
  `.LoadsTheRequestedLanguageFile`, `.TheLanguageMatchIsAsciiCaseInsensitive`;
  `ShippedLexicons.GermanFileLoadsThroughTheLoader`,
  `ShippedLexicons.EnglishFileLoadsThroughTheLoader`).
- End-to-end through the composition root at the Output Manager seam —
  `tests/app/default_app_test.cpp`
  (`DefaultAppTest.SpeaksTheWordsOfTheFileVoxLexiconPointsAt` asserts the file's
  word "Knopf-aus-Datei" is spoken via `deps.output.announce(...).text`;
  `DefaultAppTest.VoxLanguageSelectsALanguageFileNextToTheExecutable`).
- The shipped data files `data/lexicon/de.lex` and `data/lexicon/en.lex` carry the
  words and the `language` declaration.

Traces: issues #34, #61 · architecture §1.2 Q3, §5.2, §10, ADR-07, ADR-12.

## Notes / open questions
The pure `Lexicon` API (`parse`/`role`/`state`/`missingRequiredKeys`,
`src/german/include/vox/german/lexicon.hpp`) is the data model; the filesystem
loader is `src/app/src/lexicon_loader.cpp`. `role.unknown` is intentionally never
spoken (enforced in code, not data). Wholesale-replacement and the validity rules
are REQ-006; the always-speaks fallback is REQ-007.
