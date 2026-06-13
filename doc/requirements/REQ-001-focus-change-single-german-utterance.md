# REQ-001: Focus change announced as one German utterance

> **Status:** Implemented (milestone 1).

## Target audience
Blind and low-vision German-speaking users who navigate the Windows desktop and
standard controls entirely by ear and rely on each focus change being spoken
concisely and completely.

## Context
When keyboard focus moves to a control (on app start over an already-focused
control, or on a subsequent focus change), Vox reads the newly focused element.
The out-of-process UIA reader (#37) captures it into an `AccessibleNode` (role,
name, state set, optional value), and the Output Manager renders it for speech
*before* it reaches TTS — the inspectable speech seam (ADR-12, #33).

## Requirement
On a keyboard focus change, Vox **shall** announce the focused control as exactly
one utterance that conveys, in German and in the order *role, name, state, value*,
the control's role word, accessible name, relevant states, and value — speaking
"leer" for a present-but-empty value and omitting absent values.

## Quality goals
- **Q3 (German quality):** correct German role and state vocabulary (e.g.
  "Schaltfläche"; "Kontrollkästchen … aktiviert / nicht aktiviert / teilweise
  aktiviert"; "Eingabefeld … schreibgeschützt"; "leer" for an empty value), spoken
  in a fixed role–name–state–value order.
- **Reliability:** provider COM/read failures degrade to absent fields rather than
  crashing; a synthesis failure never escapes the reader worker.

## Acceptance / test concept
Asserted on the would-be-spoken **text** at the Output Manager seam (ADR-12), not
on audio:
- Golden utterance text — `tests/output/output_manager_test.cpp`
  (`OutputManager.FocusedButton`, `OutputManager.IndeterminateCheckbox`,
  `OutputManager.EmptyEditSpeaksLeer`, `OutputManager.ReadOnlyEditOrdersStateBeforeValue`,
  `OutputManager.SelectedAndDisabledWordsAppearInOrder`).
- End-to-end from a real UIA element through the provider to the utterance —
  `tests/provider/uia_provider_itest.cpp` (`UiaProviderItest.ReadsEachFocusableControl`),
  asserting `output.announce(node).text` against the shared `ControlTree`
  (`tests/support/uia_test_app/control_tree.hpp`, the single source of truth).
- UIA extraction and role/state mapping — `tests/provider/uia_provider_test.cpp`,
  `tests/provider/mapper_test.cpp`.
- Reader-level focus → announcement — `tests/app/reader_test.cpp`
  (`Reader.SpeaksInitialFocusInGerman` → "Schaltfläche, OK"; `Reader.SpeaksFocusChange`).

Traces: issues #33, #37, #39, #40 · architecture §1.2 Q3, §5.1 (Output Manager),
§6.1, ADR-12.

## Notes / open questions
Single-utterance ordering and German vocabulary are owned by
`vox::output::OutputManager`. Name/value whitespace normalization is its own
requirement (REQ-008). The `Utterance` carries priority/source metadata, but the
priority queue that uses it is a later milestone.
