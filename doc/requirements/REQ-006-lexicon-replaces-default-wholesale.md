# REQ-006: A loaded lexicon replaces the default wholesale; one announcement never mixes languages

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking users (and other-language users adding their own table) who
must never hear a single announcement built from two languages' words — a
half-German, half-English utterance is confusing and unintelligible.

## Context
When a per-language lexicon file is found, the loader could in principle layer it
over the embedded German default (filling gaps from German). Instead it accepts a
file only if the file is self-complete and declares the language it was loaded as,
then uses that table alone. A file is rejected (no partial use) if it declares no
language, declares a mismatched language, or is missing any required role/state key.

## Requirement
When Vox loads a lexicon file it **shall** use that file's words wholesale — never
layering them over or mixing them with the embedded default — accepting the file
only if it declares the expected language and contains every required role and
state key, so that a single announcement can never combine words from two
languages.

## Quality goals
- **Q3 (German quality):** no mixed-language utterance; an accepted table supplies
  all of its own role/state words; a table that is incomplete or declares the wrong
  language is rejected rather than gap-filled from German.
- **Reliability:** validation (declared-language match + completeness via
  `missingRequiredKeys`) runs before any file is used.

## Acceptance / test concept
`tests/app/lexicon_loader_test.cpp`
(`LexiconLoaderTest.AnExplicitFileWinsAndItsDeclaredLanguageStands`; the rejection
cases — a file without a `language` declaration, a mismatched declaration (English
content placed in `de.lex` is never spoken as German), and an incomplete table
naming a missing required key); end-to-end
`DefaultAppTest.SpeaksTheWordsOfTheFileVoxLexiconPointsAt` (the file's own word,
not the embedded default, is spoken). The required-key set is anchored by
`tests/german/lexicon_test.cpp` (`Lexicon.MissingRequiredKeysCountsGaps`).

Traces: issues #34, #61 · architecture §1.2 Q3, §5.2, ADR-07, ADR-12.

## Notes / open questions
The header contract is explicit
(`src/app/include/vox/app/lexicon_loader.hpp`): "A file replaces the default
wholesale — never layers over it — so one announcement can never mix languages."
The required-key set is the `role.*` / `state.*` keys (`role.unknown` excluded).
The always-speaks fallback when validation fails is REQ-007.
