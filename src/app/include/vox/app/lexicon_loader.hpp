// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Loads the announcement lexicon from a per-language file (#61).
///
/// The pure `vox::german::Lexicon` never touches the filesystem; this app-layer
/// loader does. Resolution order: an explicit file (`VOX_LEXICON`) wins, else
/// `<lexiconDir>/<tag>.lex` for the requested language (`VOX_LANGUAGE`, default
/// `de`), else the embedded German default. A file replaces the default
/// wholesale — never layers over it — so one announcement can never mix
/// languages: it is accepted only if it declares the expected language and no
/// required key is missing. Every fallback is explained in `diagnostics`; the
/// result is always speakable.
#ifndef VOX_APP_LEXICON_LOADER_HPP
#define VOX_APP_LEXICON_LOADER_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <vox/german/lexicon.hpp>

namespace vox::app {

/// Where the lexicon in a @ref LoadedLexicon came from.
enum class LexiconOrigin : std::uint8_t {
  File,            ///< A `.lex` file was read and validated.
  EmbeddedDefault, ///< The build-time German default (the fallback).
};

/// The outcome of @ref loadLexicon: always a usable lexicon, plus provenance.
struct LoadedLexicon {
  vox::german::Lexicon lexicon;                         ///< Always speakable.
  LexiconOrigin origin{LexiconOrigin::EmbeddedDefault}; ///< Provenance.
  std::filesystem::path path;                           ///< The file, when origin is File.
  std::vector<std::string> diagnostics; ///< Why fallbacks happened (one line each, for stderr).
};

/// The language loaded when none is requested: German (ADR-07).
inline constexpr std::string_view DefaultLanguageTag = "de";

/// What @ref loadLexicon should look for — the composition root gathers this
/// from the environment; tests construct it directly.
struct LexiconRequest {
  std::filesystem::path explicitFile; ///< An explicit `.lex` file (`VOX_LEXICON`), or empty.
  std::filesystem::path lexiconDir;   ///< The per-language directory (`lexicon` next to the exe).
  std::string requestedTag;           ///< The requested language (`VOX_LANGUAGE`), or empty.
};

/// @brief True if @p tag looks like a BCP-47 language tag: non-empty ASCII
///        letters, digits, and `-`. Deliberately strict — a tag becomes a file
///        name (`<tag>.lex`), so no separators or dots can pass.
[[nodiscard]] bool isLanguageTag(std::string_view tag);

/// @brief Loads the announcement lexicon per the #61 resolution rule.
///
/// `request.explicitFile` (empty = unset) is tried first and is authoritative:
/// when set, no directory lookup happens after it, and its declared language
/// stands even against a set `request.requestedTag` — the per-part override
/// has the higher precedence (#88); a divergence is reported as a diagnostic,
/// not rejected. The file must still declare *a* language and be complete.
/// Otherwise `request.lexiconDir / <tag>.lex` is tried, where `<tag>` is
/// `request.requestedTag` or @ref DefaultLanguageTag; the file's declared
/// language must match the tag it was loaded as. An invalid tag is reported
/// and ignored; an empty `request.lexiconDir` skips the lookup (never
/// CWD-relative). Any unreadable or rejected file falls back to the embedded
/// German default with a diagnostic — the reader always speaks.
[[nodiscard]] LoadedLexicon loadLexicon(const LexiconRequest& request);

} // namespace vox::app

#endif // VOX_APP_LEXICON_LOADER_HPP
