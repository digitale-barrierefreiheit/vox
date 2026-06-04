// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Implementation of vox::german::numberToWords.
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <vox/german/numbers.hpp>

namespace vox::german {

namespace {

/// Standalone units 0–9; index 1 is "eins" (the combining form "ein" is applied
/// separately by combiningOne()).
constexpr std::array<std::string_view, 10> Ones{"null", "eins",  "zwei",   "drei", "vier",
                                                "fünf", "sechs", "sieben", "acht", "neun"};

/// 10–19. German contracts some of these (sechzehn, siebzehn).
constexpr std::array<std::string_view, 10> Teens{"zehn",     "elf",      "zwölf",    "dreizehn",
                                                 "vierzehn", "fünfzehn", "sechzehn", "siebzehn",
                                                 "achtzehn", "neunzehn"};

/// Tens 20–90, indexed by the tens digit (2–9). German contracts some of these
/// (dreißig, sechzig, siebzig).
constexpr std::array<std::string_view, 10> Tens{
    "", "", "zwanzig", "dreißig", "vierzig", "fünfzig", "sechzig", "siebzig", "achtzig", "neunzig"};

/// The combining form of a units digit 1–9: 1 becomes "ein" (einundzwanzig,
/// einhundert), all others keep their standalone spelling.
std::string combiningOne(int digit) {
  if (digit == 1) {
    return "ein";
  }
  return std::string(Ones.at(static_cast<std::size_t>(digit)));
}

/// Words for 0–99; 0 renders as the empty string so callers can concatenate.
std::string below100(int value) {
  if (value == 0) {
    return {};
  }
  if (value < 10) {
    return std::string(Ones.at(static_cast<std::size_t>(value)));
  }
  if (value < 20) {
    return std::string(Teens.at(static_cast<std::size_t>(value - 10)));
  }
  const int tens = value / 10;
  const int units = value % 10;
  std::string out;
  if (units != 0) {
    out += combiningOne(units);
    out += "und";
  }
  out += Tens.at(static_cast<std::size_t>(tens));
  return out;
}

/// Words for 0–999; 0 renders as the empty string.
std::string below1000(int value) {
  const int hundreds = value / 100;
  const int rest = value % 100;
  std::string out;
  if (hundreds != 0) {
    out += combiningOne(hundreds);
    out += "hundert";
  }
  out += below100(rest);
  return out;
}

} // namespace

std::string numberToWords(int value) {
  constexpr int MaxValue = 9999;
  if (value < 0 || value > MaxValue) {
    return std::to_string(value);
  }
  if (value == 0) {
    return "null";
  }
  const int thousands = value / 1000;
  const int rest = value % 1000;
  std::string out;
  if (thousands != 0) {
    out += combiningOne(thousands);
    out += "tausend";
  }
  out += below1000(rest);
  return out;
}

} // namespace vox::german
