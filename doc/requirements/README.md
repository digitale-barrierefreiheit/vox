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
- Open planning lives in [`doc/todo`](../todo/).

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

> No requirements have been captured yet. Add them here as they are discussed,
> starting from the quality goals in the architecture document.
