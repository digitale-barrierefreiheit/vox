# REQ-002: Keyboard-only operation via a low-level keyboard hook

> **Status:** Implemented (milestone 1).

## Target audience
Blind German-speaking users who operate the machine entirely from the keyboard,
with no mouse, and need navigation keys both to reach the application and to drive
the reader.

## Context
Vox installs a low-level keyboard hook (#38) so it observes keystrokes globally
without the host application's cooperation and without any mouse interaction. A
pure, testable seam (`processKey` / `routeKeyEvent` over a `CommandMap`) decides
per key whether to pass it through to the focused application or consume it as a
reader control.

## Requirement
Vox **shall** be operable by keyboard alone: a low-level keyboard hook shall route
each key event through the command map so that navigation keys (Tab, Shift+Tab,
arrows) are dispatched as navigation commands while still passing through to the
application, reader-control chords (Control+Shift+Q / Control+Shift+S) are
dispatched and consumed, and all other keys pass through unmodified.

## Quality goals
- **Reliability:** no handler exception crosses the OS ABI boundary (a throwing
  handler passes through rather than consumes); auto-repeat of a consumed key is
  swallowed without re-routing; an install failure surfaces as a `HookError`
  rather than a crash.
- **Q2 (overhead):** the per-key decision is a pure, allocation-light function so
  the `WH_KEYBOARD_LL` callback returns promptly and does not stall host input.

## Acceptance / test concept
- Pure `processKey` seam — `tests/input/keyboard_hook_test.cpp`
  (`KeyboardHookProcessKey.RoutesButPassesThroughNavigationKeys`,
  `.PassesThroughUnboundKeysWithoutRouting`, `.AThrowingHandlerDoesNotConsume`,
  `.SwallowsAutoRepeatOfAConsumedKeyWithoutReRouting`;
  `KeyboardHookDispatch.ConsumesAReaderControlChord`, `.TreatsSysKeyDownAsAPress`;
  `KeyboardHookLifecycle.StartThrowsWhenTheInstallFails`, `.StartTwiceThrows`).
- Command bindings — `tests/input/command_map_test.cpp`
  (`CommandMap.TabNavigatesNextAndPrevious`, `.ArrowsNavigate`,
  `.ModifiersMustMatchExactly`; `RouteKeyEvent.NavigationDispatchesAndPassesThrough`,
  `RouteKeyEvent.ReaderControlDispatchesAndConsumes`).
- Live OS hook plumbing (gated; skips without an interactive desktop, hard failure
  under `VOX_REQUIRE_INPUT_HOOK`) — `tests/input/keyboard_hook_itest.cpp`
  (`KeyboardHookITest.InjectedTabReachesHandlerAsNavigateNext`).

Traces: issue #38 · architecture §3.1, §5.1 (Input Layer), §1.2 Q2.

## Notes / open questions
Modifier matching is exact (AltGr = Control+Alt on German layouts is not
navigation; Shift+arrow is application text-selection, not reader navigation).
Navigation keys are routed *and* passed through so the application still moves
focus; the announcement then arrives via the focus-changed event (REQ-001). The
hook seam (#68) lets the per-key decision and the start/stop lifecycle be
unit-tested with no real `WH_KEYBOARD_LL` install.
