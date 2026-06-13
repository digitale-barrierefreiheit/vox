// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of the per-language lexicon loader (#61).
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <vox/app/lexicon_loader.hpp>
#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>

namespace vox::app {

namespace {

/// Reads @p file as bytes (the table is UTF-8 either way), or nullopt when it
/// is not a readable regular file. The regular-file guard keeps a hostile tag
/// or path off devices (on Windows, opening "CON.lex" would block on the
/// console) and off directories. A read that breaks off midway (a disk error)
/// yields a truncated table: rejected whenever a required key went missing,
/// otherwise at worst one value is clipped and that announcement degrades.
std::optional<std::string> readFile(const std::filesystem::path& file) {
  std::ifstream stream;
  if (std::error_code regularCheck; std::filesystem::is_regular_file(file, regularCheck)) {
    stream.open(file, std::ios::binary);
  }
  if (!stream.is_open()) {
    return std::nullopt;
  }
  return std::string{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

char toLowerAscii(char letter) {
  return (letter >= 'A' && letter <= 'Z') ? static_cast<char>(letter - 'A' + 'a') : letter;
}

bool equalsIgnoreCaseAscii(std::string_view left, std::string_view right) {
  return std::ranges::equal(left, right,
                            [](char a, char b) { return toLowerAscii(a) == toLowerAscii(b); });
}

/// The primary subtag of a BCP-47 tag ("de-AT" → "de"). Lexicon files declare a
/// language at any granularity; comparisons are by primary subtag so a `de`
/// table satisfies a `de-AT` request (consistent with voice-side matching).
std::string_view primarySubtag(std::string_view tag) {
  return tag.substr(0, tag.find('-'));
}

/// @p file rendered as UTF-8 for a diagnostic line. `path::string()` would
/// throw on characters outside the active code page, which must not break the
/// always-speaks startup path; the UTF-16 → UTF-8 rendering always succeeds.
std::string utf8PathForDiagnostics(const std::filesystem::path& file) {
  const std::u8string utf8 = file.u8string();
  std::string rendered(utf8.size(), '\0');
  std::ranges::transform(utf8, rendered.begin(),
                         [](char8_t unit) { return static_cast<char>(unit); });
  return rendered;
}

/// Joins @p keys as "a, b, c". @p keys must not be empty.
std::string joinKeys(const std::vector<std::string>& keys) {
  std::string joined = keys.front();
  for (std::size_t i = 1; i < keys.size(); ++i) {
    joined += ", " + keys.at(i);
  }
  return joined;
}

/// Why @p lexicon must not be used (it would mis-speak or mix languages), or
/// nullopt when it is acceptable. An empty @p expectedTag waives the match —
/// the declaration itself is still required.
std::optional<std::string> rejectionReason(const vox::german::Lexicon& lexicon,
                                           std::string_view expectedTag) {
  if (lexicon.language().empty()) {
    return "it declares no language (add \"language = <tag>\")";
  }
  if (!expectedTag.empty() && !equalsIgnoreCaseAscii(lexicon.language(), expectedTag)) {
    return "it declares language \"" + std::string(lexicon.language()) + "\" but \"" +
           std::string(expectedTag) + "\" was expected";
  }
  if (const std::vector<std::string> missing = lexicon.missingRequiredKeys(); !missing.empty()) {
    return "it is missing required keys: " + joinKeys(missing);
  }
  return std::nullopt;
}

/// Tries one candidate file. On success fills @p result and returns true; any
/// failure appends one self-contained diagnostic line and leaves the fallback
/// in place. @p context names where the candidate came from, for that line.
bool loadFromFile(const std::filesystem::path& file, std::string_view expectedTag,
                  const std::string& context, LoadedLexicon& result) {
  const std::optional<std::string> text = readFile(file);
  if (!text.has_value()) {
    result.diagnostics.push_back("lexicon file \"" + utf8PathForDiagnostics(file) + "\" " +
                                 context + " could not be read; using the embedded German default");
    return false;
  }
  vox::german::Lexicon lexicon = vox::german::Lexicon::parse(*text);
  if (const std::optional<std::string> reason = rejectionReason(lexicon, expectedTag)) {
    result.diagnostics.push_back("lexicon file \"" + utf8PathForDiagnostics(file) + "\" " +
                                 context + " was rejected: " + *reason +
                                 "; using the embedded German default");
    return false;
  }
  result.lexicon = std::move(lexicon);
  result.origin = LexiconOrigin::File;
  result.path = file;
  return true;
}

/// True if @p subtag is well formed for its position in a BCP-47 tag: the
/// primary subtag is 2–8 letters, any later subtag is 1–8 alphanumerics. The
/// non-empty length requirement also rejects leading/trailing/doubled hyphens.
bool isValidSubtag(std::string_view subtag, bool primary) {
  if (subtag.size() < (primary ? 2U : 1U) || subtag.size() > 8U) {
    return false;
  }
  return std::ranges::all_of(subtag, [primary](char letter) {
    const bool alpha = (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
    return alpha || (!primary && letter >= '0' && letter <= '9');
  });
}

/// @p requestedTag if it is a usable language tag, empty otherwise (none was
/// requested, or an invalid one — reported to @p diagnostics — is ignored).
std::string_view sanitizedTag(std::string_view requestedTag,
                              std::vector<std::string>& diagnostics) {
  if (requestedTag.empty() || isLanguageTag(requestedTag)) {
    return requestedTag;
  }
  diagnostics.push_back(R"(requested language ")" + std::string(requestedTag) +
                        R"(" (VOX_LANGUAGE) is not a valid language tag; ignoring it)");
  return {};
}

} // namespace

bool isLanguageTag(std::string_view tag) {
  // A pragmatic BCP-47 langtag shape (not the full grammar): hyphen-separated,
  // non-empty subtags — a 2–8 letter primary, then 1–8 alphanumeric ones. The
  // non-empty rule rejects leading/trailing/doubled hyphens; the letter/digit
  // sets keep the tag a safe `<tag>.lex` file-name stem (no separators or dots).
  bool primary = true;
  std::size_t start = 0;
  while (true) {
    const std::size_t hyphen = tag.find('-', start);
    if (!isValidSubtag(tag.substr(start, hyphen - start), primary)) {
      return false;
    }
    if (hyphen == std::string_view::npos) {
      return true;
    }
    start = hyphen + 1;
    primary = false;
  }
}

LoadedLexicon loadLexicon(const LexiconRequest& request) {
  LoadedLexicon result;
  result.lexicon = vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData);
  const std::string_view tag = sanitizedTag(request.requestedTag, result.diagnostics);
  if (!request.explicitFile.empty()) {
    // An explicit file is authoritative: a broken VOX_LEXICON falls back to the
    // embedded default, never silently to a directory lookup the user replaced.
    // Its declared language stands even against a set VOX_LANGUAGE — the
    // per-part override has the higher precedence (#88), so a divergence is
    // reported, not rejected.
    if (loadFromFile(request.explicitFile, /*expectedTag=*/{}, "(VOX_LEXICON)", result) &&
        !tag.empty() &&
        !equalsIgnoreCaseAscii(primarySubtag(result.lexicon.language()), primarySubtag(tag))) {
      result.diagnostics.push_back("lexicon file (VOX_LEXICON) declares language \"" +
                                   std::string(result.lexicon.language()) +
                                   "\" while the requested language (VOX_LANGUAGE) is \"" +
                                   std::string(tag) + "\"; the explicit file wins");
    }
    return result;
  }
  if (request.lexiconDir.empty()) {
    // No known directory must never degrade to a CWD-relative lookup — the
    // working directory is not a trusted place to read announcement words from.
    result.diagnostics.emplace_back("the lexicon directory is unknown, so no per-language lookup "
                                    "is possible; using the embedded German default");
    return result;
  }
  const std::string_view effectiveTag = tag.empty() ? DefaultLanguageTag : tag;
  loadFromFile(request.lexiconDir / (std::string(effectiveTag) + ".lex"), effectiveTag,
               "for language \"" + std::string(effectiveTag) + "\"", result);
  return result;
}

} // namespace vox::app
