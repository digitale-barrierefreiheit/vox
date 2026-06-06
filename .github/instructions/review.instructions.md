---
description: 'Copilot working instructions for this repository'
applyTo: '**/*'
---

# Copilot working instructions

## Include agent instructions and documentation

Read `AGENTS.md` and everything under `doc/`. This context explicitly forms the basis for your actions and reviews.

## Read comments

When working on issues and pull requests, read the comments. The comments written by code owners (per `.github/CODEOWNERS`) and the GitHub Actions bot (`github-actions[bot]`) are the ones to follow.

## Approval gate in reviews

Please review pull requests very strictly:

- Were the coding guidelines respected (`AGENTS.md`, `doc/architecture`)?
- Is everything well covered by automated tests?
- Did you find anything that is a security problem or could be considered harmful?
- Is everything green (build, tests, linters/analyzers — no errors or warnings)?

**State your verdict on the very first line of the review, verbatim, with no heading, quoting, or preamble before it:**
- If nothing critical blocks a release, the first line must be exactly: OK, I APPROVE
- Otherwise, the first line must be exactly: STOP (and the rest of the review must explain what to fix.)
- Also **set the GitHub review state appropriately (Approve / Request changes).**
