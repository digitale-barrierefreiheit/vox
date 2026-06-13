# REQ-009: German number-to-words expansion in announcements

> **Status:** Recognized, not yet integrated — the converter is built and
> unit-tested, but it is not yet on any production announcement path.

## Target audience
Blind German-speaking users who encounter numeric values (positions such as
"3 von 10", counts, indices) and need them spoken as natural German words rather
than digit-by-digit.

## Context
Issue #34 ("announcement lexicon + minimal normalization") includes spoken-German
number expansion. The pure converter `vox::german::numberToWords`
(`src/german/src/numbers.cpp`) is implemented and unit-tested, with the intended
use documented in its header ("3 von 10" → "drei von zehn"), but it is not yet
called from any production announcement path — the Output Manager does not yet
route numeric content through it.

## Requirement
Vox **shall** render cardinal integers in announcements as spoken German words
(0–9999, with correct contracted forms such as "sechzehn", "einundzwanzig",
"dreißig"), falling back to decimal digits outside that range.

## Quality goals
- **Q3 (German quality):** numbers in announcements are spoken as natural German
  words, not digits, wherever the value is in range.

## Acceptance / test concept
The converter is verified by `tests/german/numbers_test.cpp`
(`NumberToWords.UnitsTeensAndTens`, `.HundredsAndThousands`,
`.OutOfRangeFallsBackToDigits`). Full realization additionally requires wiring
`numberToWords` into the Output Manager announce path and an end-to-end
utterance-text assertion at the seam (ADR-12); that integration is **not yet
done**.

Traces: issue #34 · architecture §1.2 Q3, §5.2, ADR-07, ADR-12 ·
`src/german/src/numbers.cpp` (built; integration pending).

## Notes / open questions
The deterministic core exists and is tested (TDD per §8.6.1), but the behavior is
not observable to a user until it is on the announce path — hence the "recognized,
not yet integrated" status. Open question: track the integration (extend #34's
scope or open a dedicated follow-up) and decide which value-bearing fields (value
patterns, "x von y" positions) are routed through it.
