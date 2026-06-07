// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only pure helpers of SapiTtsEngine, exposed for unit testing.
///
/// The UTF-8/UTF-16 converters, the SAPI "Language" LCID parser, and the token
/// attribute reader are pure COM/string glue with no installed-voice dependency.
/// Keeping them here (rather than in an anonymous namespace in the .cpp) lets the
/// test suite drive every branch directly with mock COM and no SAPI voice (ADR-12,
/// issues #68 / #72). Production code includes this like any internal header.
#ifndef VOX_TTS_DETAIL_SAPI_INTERNAL_HPP
#define VOX_TTS_DETAIL_SAPI_INTERNAL_HPP

#if defined(_WIN32)

#  include <cstddef>
#  include <string>
#  include <string_view>

// The Windows/SAPI headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting. WIN32_LEAN_AND_MEAN
// trims the heavy sub-includes and NOMINMAX keeps min/max as functions.
// clang-format off
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#  include <objbase.h>
#  include <sapi.h>
// clang-format on

namespace vox::tts::detail {

/// Converts UTF-8 text to a UTF-16 string (empty for empty/invalid input).
inline std::wstring toWide(std::string_view utf8) {
  if (utf8.empty()) {
    return {};
  }
  const auto length = static_cast<int>(utf8.size());
  const int chars =
      ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), length, nullptr, 0);
  if (chars <= 0) {
    return {};
  }
  std::wstring out(static_cast<std::size_t>(chars), L'\0');
  if (const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), length,
                                                out.data(), chars);
      written != chars) {
    return {};
  }
  return out;
}

/// Converts a UTF-16 C-string to UTF-8 (empty for null/empty/invalid input).
inline std::string toUtf8(const wchar_t* text) {
  if (text == nullptr || text[0] == L'\0') {
    return {};
  }
  // -1: `text` is null-terminated, so let the API walk it (no wcslen). The byte
  // count it returns/needs then includes the terminator, which we trim below.
  const int bytes =
      ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
  if (bytes <= 1) { // 1 == only the terminating null, i.e. empty content
    return {};
  }
  std::string out(static_cast<std::size_t>(bytes), '\0');
  if (const int written = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, out.data(),
                                                bytes, nullptr, nullptr);
      written != bytes) {
    return {};
  }
  out.resize(static_cast<std::size_t>(bytes) - 1U); // drop the embedded terminator
  return out;
}

/// True if any LCID in a SAPI "Language" attribute is a German primary language.
/// The attribute is a ';'-separated list of hex LCIDs (e.g. "407;c07").
inline bool languageIsGerman(std::wstring_view languageAttribute) {
  std::size_t start = 0;
  while (start <= languageAttribute.size()) {
    const std::size_t end = languageAttribute.find(L';', start);
    const std::size_t count =
        end == std::wstring_view::npos ? std::wstring_view::npos : end - start;
    if (const std::wstring token{languageAttribute.substr(start, count)}; !token.empty()) {
      const auto lcid = ::wcstoul(token.c_str(), nullptr, 16);
      if (PRIMARYLANGID(static_cast<LANGID>(lcid)) == LANG_GERMAN) {
        return true;
      }
    }
    if (end == std::wstring_view::npos) {
      break;
    }
    start = end + 1;
  }
  return false;
}

/// Reads one string value under a token's "Attributes" key (empty if absent).
/// Uses a raw key pointer + Release (no WRL ComPtr) so this header needs no
/// <wrl/client.h> and thus no warning-suppression pragma.
inline std::wstring readAttribute(ISpObjectToken* token, const wchar_t* valueName) {
  ISpDataKey* attributes = nullptr;
  if (FAILED(token->OpenKey(L"Attributes", &attributes)) || attributes == nullptr) {
    return {};
  }
  LPWSTR value = nullptr;
  const HRESULT hr = attributes->GetStringValue(valueName, &value);
  attributes->Release();
  if (FAILED(hr) || value == nullptr) {
    return {};
  }
  std::wstring out(value);
  ::CoTaskMemFree(value);
  return out;
}

} // namespace vox::tts::detail

#endif // defined(_WIN32)

#endif // VOX_TTS_DETAIL_SAPI_INTERNAL_HPP
