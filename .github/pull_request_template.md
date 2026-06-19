## Summary

<!-- What does this change do, and why? Link any related issue or ADR. -->

## Changes

-

<!-- Effort (optional, for the cost ledger — doc/cost-ledger.md). Uncomment the line
     below and set your developer-time estimate; a bot suggests a value from the diff
     size that you can adjust. Format: `Effort: 2h`, `Effort: 1h30m`, or `Effort: 45m`.
     Opt-in — leaving it out never blocks the merge. -->
<!-- Effort: 0m -->

## Review checklist (architecture §8.6.7)

- [ ] Tests added/updated; pure cores covered by TDD.
- [ ] No allocation on the audio callback / in-context hook hot paths.
- [ ] Thread-safety and memory ordering considered for concurrent code.
- [ ] Intention-revealing names; abbreviations only for domain acronyms.
- [ ] Faults degrade gracefully and never crash the host application.
- [ ] Docs updated (architecture / ADRs / requirements / doc comments).
- [ ] CI green (format, clang-tidy, MSVC 32/64-bit, sanitizers).
