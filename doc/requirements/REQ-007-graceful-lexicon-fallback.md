# REQ-007: Graceful fallback to the embedded German default when the lexicon is absent or invalid

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking users operating the machine through Vox: even with a
missing, mistyped, or corrupt lexicon configuration the reader must keep speaking
so they are never locked out.

## Context
The requested lexicon file may be absent, unreadable (a directory or device name
in its place), declare no or the wrong language, or be incomplete; the lexicon
directory may be unknown; `VOX_LANGUAGE` may be an invalid tag. In every such case
the loader must still return a usable, complete German lexicon and explain why it
fell back — on stderr — rather than refuse to start or read from an untrusted
working-directory-relative path.

## Requirement
When the requested lexicon file is absent, unreadable, or invalid, Vox **shall**
fall back to the embedded complete German default lexicon and continue speaking,
emitting one self-contained diagnostic line per fallback on stderr stating the
reason.

## Quality goals
- **Reliability:** `loadLexicon` always returns a speakable lexicon (origin
  `EmbeddedDefault` on any failure); the embedded German default has no missing
  required keys; no failure path degrades to a working-directory-relative read.
- **Maintainability:** each fallback produces exactly one diagnostic line naming
  the cause (file unreadable, declares no language, language mismatch, missing
  keys, unknown directory, invalid tag).

## Acceptance / test concept
`tests/app/lexicon_loader_test.cpp`
(`LexiconLoaderTest.AnAbsentFileFallsBackToTheEmbeddedGermanDefault` — origin
`EmbeddedDefault`, speaks "Schaltfläche", diagnostic "could not be read";
`.ABrokenExplicitFileFallsBackToTheDefaultNotTheDirectory`; plus the
unknown-directory, directory-in-place, and invalid-tag fallbacks); end-to-end
`DefaultAppTest.SpeaksGermanFromTheEmbeddedDefaultWithoutConfiguration` (announces
"Schaltfläche" with no configuration). The one-line-per-fallback contract is
asserted by the test's `expectFallback` helper (`diagnostics.size() == 1`).

Traces: issue #61 · architecture §1.2 (reliability), §10, ADR-07, ADR-12.

## Notes / open questions
Diagnostics are returned in `LoadedLexicon::diagnostics` (one string per fallback,
"for stderr"); the composition root writes them to `std::cerr`. `readFile` guards
regular-file-only, so a hostile tag cannot open a device ("CON.lex") or a
directory. The reporting-not-rejecting rule for *diverging* explicit choices is a
distinct requirement (REQ-014).
