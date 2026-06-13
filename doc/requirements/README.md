# Requirements

This folder holds the product requirements, documented as discussed (AGENTS.md).
Each topic is one Markdown file using the template below. Requirements are
written so they map to tests (architecture §8.6.1): each one should be
verifiable.

## How requirements map to the rest of the project

- **Quality goals** trace to [`doc/architecture/architecture.md`](../architecture/architecture.md)
  §1.2 and §10 (latency, system overhead, German quality).
- **Functional requirements** trace to tests under `tests/` and, for spoken
  output, to utterance-text assertions at the Output Manager seam (ADR-12).
- Open planning lives in [GitHub issues](https://github.com/digitale-barrierefreiheit/vox/issues).

## Template for a requirement document

```markdown
# REQ-<id>: <short title>

## Target audience
Who needs this and why (e.g. blind German-speaking power users).

## Context
The situation/scenario in which the requirement applies.

## Requirement
A single, testable statement. Use "shall"; avoid ambiguity.

## Quality goals
Which of Q1 (latency) / Q2 (overhead) / Q3 (German quality) / reliability /
maintainability this serves, with concrete targets where measurable.

## Acceptance / test concept
How this is verified — unit test, integration against the test application,
golden corpus, latency benchmark, or usability session with target users.

## Notes / open questions
```

## Captured requirements

Milestone 1 ("MVP: speak the focused control") discovered the requirements below;
each links to its `REQ-<id>` document. **Implemented** means verified by the cited
tests/benchmarks on `dev`; **Pending** means recognized/decided but not yet built
(traced to its open issue). Add a new requirement by creating the next
`REQ-<id>-<slug>.md` from the template above and listing it here.

### Focus, input & interruption

| ID | Requirement | Status | Issues | Quality goals |
|----|-------------|--------|--------|---------------|
| [REQ-001](REQ-001-focus-change-single-german-utterance.md) | Focus change announced as one German utterance | Implemented | #33 #37 #39 #40 | Q3, reliability |
| [REQ-002](REQ-002-keyboard-only-operation.md) | Keyboard-only operation via a low-level keyboard hook | Implemented | #38 | Q2, reliability |
| [REQ-003](REQ-003-instant-barge-in.md) | Instant barge-in interrupts current speech | Implemented | #36 #38 | Q1, reliability |
| [REQ-004](REQ-004-robust-focus-subscription.md) | Focus announcements survive UIA handler re-registration | Implemented | #60 | reliability |

### German front-end (announcement vocabulary & normalization)

| ID | Requirement | Status | Issues | Quality goals |
|----|-------------|--------|--------|---------------|
| [REQ-005](REQ-005-announcement-vocabulary-is-data.md) | Announcement vocabulary is per-language data, not code | Implemented | #34 #61 | Q3, maintainability |
| [REQ-006](REQ-006-lexicon-replaces-default-wholesale.md) | A loaded lexicon replaces the default wholesale | Implemented | #34 #61 | Q3, reliability |
| [REQ-007](REQ-007-graceful-lexicon-fallback.md) | Graceful fallback to the embedded German default | Implemented | #61 | reliability, maintainability |
| [REQ-008](REQ-008-name-and-value-normalization.md) | Spoken names and values are whitespace-normalized | Implemented | #34 | Q3 |
| [REQ-009](REQ-009-german-number-to-words.md) | German number-to-words expansion in announcements | Pending | #34 | Q3 |

### Voice & language selection

| ID | Requirement | Status | Issues | Quality goals |
|----|-------------|--------|--------|---------------|
| [REQ-010](REQ-010-german-first-voice-never-silent.md) | German-first voice selection with a never-silent fallback | Implemented | #35 #52 #88 | Q3, reliability |
| [REQ-011](REQ-011-voice-discovery-classic-and-onecore.md) | Discover voices in both the classic and OneCore catalogues | Implemented | #52 #88 | Q3, maintainability |
| [REQ-012](REQ-012-language-couples-voice-and-lexicon.md) | One `VOX_LANGUAGE` drives both lexicon and voice language | Implemented | #61 #88 | Q3, reliability |
| [REQ-013](REQ-013-per-part-language-overrides.md) | `VOX_VOICE` / `VOX_LEXICON` override their part | Implemented | #61 #88 | reliability, maintainability |
| [REQ-014](REQ-014-language-divergence-reported.md) | A language divergence is reported, not rejected | Implemented | #61 #88 | reliability, Q3 |

### TTS engine & audio output

| ID | Requirement | Status | Issues | Quality goals |
|----|-------------|--------|--------|---------------|
| [REQ-015](REQ-015-tts-engine-cancellable-pcm.md) | TTS engine synthesizes cancellable PCM in a fixed format | Implemented | #35 | Q1, reliability |
| [REQ-016](REQ-016-speech-rate-control.md) | Normalized speaking-rate control | Implemented | #35 | Q3, reliability |
| [REQ-017](REQ-017-windowed-sinc-resampling.md) | TTS PCM resampled to the device format without quality loss | Implemented | #55 | Q3, Q2, reliability |

### Latency

| ID | Requirement | Status | Issues | Quality goals |
|----|-------------|--------|--------|---------------|
| [REQ-018](REQ-018-ttfa-uncached-budget.md) | Time-to-first-audio budget for short uncached text | Implemented | #41 | Q1 |
| [REQ-019](REQ-019-cached-and-continuous-latency.md) | Latency for cached utterances and continuous reading | Pending | #41 | Q1 |

### User acceptance

| ID | Requirement | Status | Issues | Quality goals |
|----|-------------|--------|--------|---------------|
| [REQ-020](REQ-020-dogfooding-acceptance.md) | Acceptance by blind native-German users (dogfooding) | Pending | #42 | Q1, Q3 |

_Scope note:_ the offline/local-synthesis privacy constraint (architecture C6 /
ADR-16) is a design-phase constraint, not a milestone-1-discovered requirement, and
no milestone-1 code realizes it yet; it will be captured when the neural TTS worker
milestone begins.
