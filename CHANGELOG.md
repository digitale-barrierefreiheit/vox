# Changelog

All notable changes to Vox are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Vox adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.0.1] - 2026-06-14

**Vox 0.0.1 — first preview.** Vox is a fast, lightweight, German-first screen
reader for Windows. This first preview announces the control you move keyboard
focus to — its type, name, state, and value — in natural German (for example,
*"Schaltfläche, Speichern"*).

### Added

- **Focus announcements** — speaks the focused control as one German utterance as
  you Tab/arrow through apps (role, name, state, value).
- **German-first speech** — finds your installed German voice, including voices
  added through Windows Settings (OneCore), and never stays silent: it falls back
  to another voice if no German one is present.
- **Instant reaction** — a new announcement interrupts the previous one
  immediately, so navigation feels responsive (first audio in well under a fifth
  of a second).
- **Configurable language** — choose the language with `VOX_LANGUAGE`; the spoken
  role and state words come from editable lexicon files. Override just the voice
  with `VOX_VOICE`, or just the words with `VOX_LEXICON`.
- **Keyboard-driven operation** and clear, high-quality audio output.

### Known limitations

An early preview, validated with a German user as *fast and natural for what it
covers* — not yet a full daily-driver screen reader. It does not yet read text you
type, selected text, tree views (e.g. mail folders), or terminal/console content.

[Unreleased]: https://github.com/digitale-barrierefreiheit/vox/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/digitale-barrierefreiheit/vox/releases/tag/v0.0.1
