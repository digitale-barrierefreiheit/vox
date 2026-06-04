// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Integer-to-German-words for the small range the MVP announces.
///
/// Used for spoken positions such as "3 von 10" -> "drei von zehn" (§5.2). Only
/// the MVP-relevant range is spelled out; see numberToWords().
#ifndef VOX_GERMAN_NUMBERS_HPP
#define VOX_GERMAN_NUMBERS_HPP

#include <string>

namespace vox::german {

/// @brief Renders @p value as German cardinal words (e.g. 21 -> "einundzwanzig").
///
/// Covers 0–9999 with correct German forms (`eins`/`ein`, `einundzwanzig`,
/// `dreißig`, `sechzehn`/`sechzig`, `siebzehn`/`siebzig`). Outside that range —
/// including negatives — it falls back to the decimal digits, so callers always
/// get a usable string. Pure; its only failure mode is allocating the result
/// (`std::bad_alloc`).
[[nodiscard]] std::string numberToWords(int value);

} // namespace vox::german

#endif // VOX_GERMAN_NUMBERS_HPP
