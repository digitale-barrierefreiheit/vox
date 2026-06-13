# REQ-008: Spoken names and values are whitespace-normalized

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking users for whom an accessible name copied from a UI control
may carry stray newlines, tabs, or runs of spaces that, if spoken literally,
produce awkward pauses or garbled output.

## Context
Accessible names and values read from UIA can contain irregular whitespace
(leading/trailing spaces, embedded newlines or tabs, multiple consecutive spaces).
Before such text is placed into an utterance, `vox::german::normalizeName` cleans
it; the Output Manager applies it to both the name and the value on the announce
path (#34's "minimal normalization" leg).

## Requirement
Vox **shall** normalize each spoken name and value by trimming surrounding
whitespace and collapsing internal whitespace runs to a single space, preserving
UTF-8 multibyte content (umlauts, ß) unchanged.

## Quality goals
- **Q3 (German quality):** announcements read cleanly — no stray pauses from
  newlines/tabs or doubled spaces — and German multibyte characters are never
  corrupted by the normalization.

## Acceptance / test concept
`tests/german/normalize_test.cpp` (`NormalizeName.TrimsAndCollapsesWhitespace`,
`.CollapsesNewlinesAndTabs`, `.EmptyAndAllWhitespaceBecomeEmpty`,
`.PreservesUtf8Multibyte`, `.AlreadyNormalIsUnchanged`). The function is wired into
the live announce path in `src/output/src/output_manager.cpp` (both `node.name`
and `node.value` pass through `normalizeName`), so it is also exercised by the
REQ-001 Output Manager seam tests.

Traces: issue #34 · architecture §1.2 Q3, §5.2, ADR-07, ADR-12.

## Notes / open questions
This is the whitespace leg of issue #34's "minimal normalization". The
number-to-words leg of #34 is built but not yet integrated into an announcement
path — see REQ-009.
