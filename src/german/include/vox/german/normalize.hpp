// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Minimal text normalization for announcements (§5.2, ADR-07).
///
/// The MVP slice only tidies whitespace; full normalization (numbers in running
/// text, dates, currency, abbreviations, compounds) is deliberately out of scope.
#ifndef VOX_GERMAN_NORMALIZE_HPP
#define VOX_GERMAN_NORMALIZE_HPP

#include <string>
#include <string_view>

namespace vox::german {

/// @brief Trims surrounding ASCII whitespace and collapses internal runs to a
///        single space (e.g. "  OK \t Button " -> "OK Button").
///
/// Operates on bytes; UTF-8 multibyte sequences are preserved unchanged (their
/// continuation bytes are never ASCII whitespace). Pure; its only failure mode
/// is allocating the result (`std::bad_alloc`).
[[nodiscard]] std::string normalizeName(std::string_view name);

} // namespace vox::german

#endif // VOX_GERMAN_NORMALIZE_HPP
