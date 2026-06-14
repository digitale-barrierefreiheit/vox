# REQ-004: Focus announcements survive UIA handler unregister/re-register

> **Status:** Implemented (milestone 1).

## Target audience
Blind users who rely on hearing what just received keyboard focus; a missed focus
event leaves them not knowing where they are, and a duplicated or stale one
announces a control they have already left.

## Context
Vox subscribes to UI Automation focus-changed events through a COM sink and must
unregister and re-register that sink across its lifetime (stop()/start() cycles,
e.g. when pausing/resuming the reader). UIA's `RemoveFocusChangedEventHandler` can
fail and keep invoking the old sink; registration can fail; the provider can be
torn down while still registered. Across all of these the user must keep getting
exactly one announcement per focus change (#60).

## Requirement
Vox **shall** continue delivering exactly one focus announcement per focus change
across UIA focus-handler unregister/re-register cycles — detaching a stopped sink
so it forwards nothing even if UIA keeps invoking it, and registering a fresh
working sink on the next start — with no missed or duplicated focus events.

## Quality goals
- **Reliability:** `stop()` silences the callback unconditionally (detach-first,
  independent of removal success); `start()` after a failed removal always
  registers a fresh handler that delivers; stuck handlers are shelved (cap 8) and
  removal is retried, escalating to `RemoveAllEventHandlers` rather than leaking or
  growing unbounded; teardown never escalates if the shelf was cleared.

## Acceptance / test concept
`tests/provider/uia_provider_test.cpp`:
`UiaProviderTest.StopSilencesTheCallbackEvenWhenRemovalFails` (the detached sink
swallows the event), `UiaProviderTest.StartAfterAFailedRemovalRegistersAFreshHandler`
(a new, distinct handler delivers — no silent-dead state),
`UiaProviderTest.RepeatedRemovalFailuresEscalateInsteadOfGrowingTheShelf` (9 cycles,
shelf cap 8 → one `RemoveAllEventHandlers`),
`UiaProviderTest.DestructionEscalatesToRemoveAllWhenRemovalKeepsFailing`,
`UiaProviderTest.StartDropsTheHandlerWhenRegistrationFails`,
`UiaProviderTest.FocusEventCallbackExceptionsAreSwallowed` (no exception crosses
the COM ABI). Implementation: `src/provider/src/uia_provider.cpp`
(`FocusEventHandler::detach`, `shelveStuckHandler`/`retryShelvedRemovals`/
`escalateLeftoverRemovals`).

Traces: issue #60 · architecture §1.2 (reliability), §10, ADR-12.

## Notes / open questions
The sink is detached *before* unregistration, so correctness does not depend on
`RemoveFocusChangedEventHandler` succeeding. An invocation already past the sink's
callback copy is dropped by the Reader's focus guard (`reader.hpp`); that
tail-drop is reader-side, outside this provider test. "No duplicated events" is
realized via a single active handler plus detach.
