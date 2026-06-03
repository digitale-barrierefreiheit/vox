# Instructions for AI Agents

WHen working on this project, please respect follow guidelines!

## Goal

The goal is to realize a high efficient and optimized screenreader, initially for Windows 11+ systems, later also for Linux and macOS.

It is important that the screenreader is high performant and does NOT slow down the system. It should have an efficient memory management. It should have good audio language, especially for German language.

## Specification

Please document discussed requirements as specification in `doc/requirements` including target audience, context, quality goals and test concepts.

## Architecture

The architecture is described in `doc/architecture`. Please consider it for all implementations.
If new decisions are made, keep the documentation up to date!
Technical Debts and risks need to be documented.

## Plan

Plan your steps in `doc/todo` (one markdown file per open topic).
DON'T be greedy, too big steps would cause problems. So plan smaller steps.

## Quality & Tests

Please implement test driven. Specification should be mapped to tests. Tests should ensure high quality for functional behavior and quality goals.

## Clean Code

Please use modern C++ language level, C++ language standards and conventions, widely known community conventions and best practices. Linting and static code analysis should be used to ensure that.

Please use a good object orientied but optimized design. Use clear names, avoid abbreviations. Methods should be small and self explaining (the truth is the code). Avoid inline comments, they should not be needed to understand code. Naming and structure should make clear what is happening. Doc comments should be used, even for private members and functions. A developers reference should be generated from doc comments.

Please respect single responisibility for methods, units and modules. Structure should follow domain and bounded contexts.

## Licensing & file headers

The project is licensed under **Apache-2.0** (see `LICENSE`). Every first-party
source file — `.cpp`, `.hpp`, `.h`, `.inl`, `.ipp`, `.cmake`, `CMakeLists.txt`,
and scripts — MUST begin with an SPDX header, using the file type's comment
syntax (`//` for C++, `#` for CMake/Python/YAML):

```cpp
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors
```

All contributions require signing the **Contributor License Agreement**
(`CLA.md`), enforced by CI. This consolidates copyright so future versions can be
relicensed if needed. **GPL/LGPL (or other copyleft) code must never be linked
into a first-party Vox binary** — it would override the project's licensing
freedom. Where such a component is unavoidable (e.g. the espeak-ng G2P), it runs
as a separate, optional, arm's-length process, never linked into Core. See
ADR-15/ADR-16 in `doc/architecture/architecture.md`.

## Be interactive

Please cooperate with user. If way isn't clear or u are unsure, ask the user instead of guessing what is wanted.
If you recognize implicit requiremtns talk about it and transform them into explicit requirements.
The project is a collaborative learning journey for you and the user.
