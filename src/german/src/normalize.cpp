// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::german::normalizeName.
#include <string>
#include <string_view>

#include <vox/german/normalize.hpp>

namespace vox::german {

namespace {

constexpr std::string_view Whitespace = " \t\r\n\f\v";

bool isWhitespace(char character) {
  return Whitespace.contains(character);
}

} // namespace

std::string normalizeName(std::string_view name) {
  std::string out;
  out.reserve(name.size());
  bool pendingSpace = false;
  for (const char character : name) {
    if (isWhitespace(character)) {
      pendingSpace = !out.empty(); // never leads with a space
      continue;
    }
    if (pendingSpace) {
      out += ' ';
      pendingSpace = false;
    }
    out += character;
  }
  return out;
}

} // namespace vox::german
