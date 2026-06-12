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
#include <utility>
#include <vector>

#include <vox/app/lexicon_loader.hpp>
#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>

namespace vox::app {

namespace {

/// Reads @p file as bytes (the table is UTF-8 either way), or nullopt when it
/// cannot be opened. A read that breaks off midway yields a truncated table,
/// which the completeness validation rejects downstream.
std::optional<std::string> readFile(const std::filesystem::path& file) {
  std::ifstream stream(file, std::ios::binary);
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
    result.diagnostics.push_back("lexicon file \"" + file.string() + "\" " + context +
                                 " could not be read; using the embedded German default");
    return false;
  }
  vox::german::Lexicon lexicon = vox::german::Lexicon::parse(*text);
  if (const std::optional<std::string> reason = rejectionReason(lexicon, expectedTag)) {
    result.diagnostics.push_back("lexicon file \"" + file.string() + "\" " + context +
                                 " was rejected: " + *reason +
                                 "; using the embedded German default");
    return false;
  }
  result.lexicon = std::move(lexicon);
  result.origin = LexiconOrigin::File;
  result.path = file;
  return true;
}

/// @p requestedTag if it is a usable language tag, empty otherwise (none was
/// requested, or an invalid one — reported to @p diagnostics — is ignored).
std::string_view sanitizedTag(std::string_view requestedTag,
                              std::vector<std::string>& diagnostics) {
  if (requestedTag.empty() || isLanguageTag(requestedTag)) {
    return requestedTag;
  }
  diagnostics.push_back("requested language \"" + std::string(requestedTag) +
                        "\" (VOX_LANGUAGE) is not a language tag (ASCII letters, "
                        "digits, \"-\"); ignoring it");
  return {};
}

} // namespace

bool isLanguageTag(std::string_view tag) {
  return !tag.empty() && std::ranges::all_of(tag, [](char letter) {
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
           (letter >= '0' && letter <= '9') || letter == '-';
  });
}

LoadedLexicon loadLexicon(const LexiconRequest& request) {
  LoadedLexicon result;
  result.lexicon = vox::german::Lexicon::parse(vox::german::DefaultGermanLexiconData);
  const std::string_view tag = sanitizedTag(request.requestedTag, result.diagnostics);
  if (!request.explicitFile.empty()) {
    // An explicit file is authoritative: a broken VOX_LEXICON falls back to the
    // embedded default, never silently to a directory lookup the user replaced.
    loadFromFile(request.explicitFile, tag, "(VOX_LEXICON)", result);
    return result;
  }
  const std::string_view effectiveTag = tag.empty() ? DefaultLanguageTag : tag;
  loadFromFile(request.lexiconDir / (std::string(effectiveTag) + ".lex"), effectiveTag,
               "for language \"" + std::string(effectiveTag) + "\"", result);
  return result;
}

} // namespace vox::app
