## Summary

<!-- What does this change do, and why? Link any related issue or ADR. -->

## Changes

-

## Review checklist (architecture §8.6.7)

- [ ] Tests added/updated; pure cores covered by TDD.
- [ ] No allocation on the audio callback / in-context hook hot paths.
- [ ] Thread-safety and memory ordering considered for concurrent code.
- [ ] Intention-revealing names; abbreviations only for domain acronyms.
- [ ] Faults degrade gracefully and never crash the host application.
- [ ] Docs updated (architecture / ADRs / requirements / doc comments).
- [ ] CI green (format, clang-tidy, MSVC 32/64-bit, sanitizers).
