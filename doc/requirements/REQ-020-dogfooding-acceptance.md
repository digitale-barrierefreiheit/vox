# REQ-020: Acceptance by blind native-German target users (dogfooding)

> **Status:** Recognized, not yet implemented — the first session is tracked by
> issue #42 and has not yet been conducted.

## Target audience
Blind, native-German-speaking users operating real Windows applications through
Vox — the people for whom the product exists and whose lived experience, not a
coverage number, is the decisive measure of success.

## Context
Automated latency and quality gates can all pass while the product still feels
wrong in real use (R14). The architecture (§8.6.8) makes continuous dogfooding with
blind native-German speakers from the first milestone the real acceptance test:
usability sessions each release set priorities, because an engineering team can pass
every automated gate and still build the wrong thing. Issue #42 is the first such
session.

## Requirement
In a structured dogfooding session, a blind native-German user operating a real
application through Vox **shall** report the speech output as both fast (responsive
enough for fluid navigation) and natural (correct, intelligible German), with their
feedback recorded to set subsequent priorities.

## Quality goals
- **Q1 (latency):** the user perceives navigation as responsive/instant in
  real-application use (subjective acceptance, no automated threshold).
- **Q3 (German quality):** the user, as a native German speaker, judges prosody,
  normalization, and pronunciation natural and intelligible at their preferred
  reading rate.

## Acceptance / test concept
Verified by a usability/dogfooding session with at least one blind native-German
user navigating a real application through Vox — **not** by an automated test. The
session records qualitative judgements of speed and naturalness and a prioritized
feedback list, per architecture §8.6.8 and risk R14. Tracked as the first
dogfooding session, issue #42; not yet conducted.

Traces: issue #42 · architecture §8.6.8, §10.1, R14.

## Notes / open questions
Pending and inherently non-automatable: the human acceptance test that complements
the automated Q1/Q3 gates. The automated TTFA budget (REQ-018) and the
German golden-corpus/intelligibility checks (§8.6.1) provide objective evidence the
user's subjective judgement is cross-checked against, but do not substitute for the
session. Open question: define a repeatable session protocol (tasks, applications,
rating scale) so results are comparable release-over-release.
