// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only internals of SapiTtsEngine, exposed for unit testing.
///
/// These pieces are pure COM/string glue with no installed-voice dependency:
/// the UTF-8/UTF-16 converters, the SAPI "Language" LCID parser, the token
/// attribute reader, and the `PcmSinkStream` that SAPI writes synthesized PCM
/// into. Keeping them here (rather than in an anonymous namespace in the .cpp)
/// lets the test suite drive every branch directly with mock COM and no SAPI
/// voice (ADR-12, issues #68 / #72). Production code includes this exactly like
/// any other internal header.
#ifndef VOX_TTS_DETAIL_SAPI_INTERNAL_HPP
#define VOX_TTS_DETAIL_SAPI_INTERNAL_HPP

#if defined(_WIN32)

#  include <atomic>
#  include <cstddef>
#  include <span>
#  include <string>
#  include <string_view>

#  include <vox/audio/audio_format.hpp>
#  include <vox/tts/itts_engine.hpp>

// The Windows/SAPI headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting.
// clang-format off
#  include <Windows.h>
#  include <objbase.h>
#  include <mmreg.h>
#  include <sapi.h>
#  pragma warning(push)
#  pragma warning(disable : 4265)  // WRL FtmBase: non-virtual dtor in a system header
#  include <wrl/client.h>
#  include <wrl/implements.h>
#  pragma warning(pop)
// clang-format on

namespace vox::tts::detail {

/// The single PCM shape the engine forces SAPI output into.
inline constexpr vox::audio::AudioFormat OutputFormat{22050, 16, 1};

/// The WAVEFORMATEX matching @ref OutputFormat.
inline WAVEFORMATEX makeOutputWaveFormat() {
  WAVEFORMATEX wfx{};
  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = OutputFormat.channels;
  wfx.nSamplesPerSec = OutputFormat.sampleRate;
  wfx.wBitsPerSample = OutputFormat.bitsPerSample;
  wfx.nBlockAlign = static_cast<WORD>(bytesPerFrame(OutputFormat));
  wfx.nAvgBytesPerSec = bytesPerSecond(OutputFormat);
  wfx.cbSize = 0;
  return wfx;
}

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
inline std::wstring readAttribute(ISpObjectToken* token, const wchar_t* valueName) {
  Microsoft::WRL::ComPtr<ISpDataKey> attributes;
  if (FAILED(token->OpenKey(L"Attributes", &attributes)) || !attributes) {
    return {};
  }
  LPWSTR value = nullptr;
  if (FAILED(attributes->GetStringValue(valueName, &value)) || value == nullptr) {
    return {};
  }
  std::wstring out(value);
  ::CoTaskMemFree(value);
  return out;
}

/// A SAPI output stream that forwards each PCM block to a PcmSink and aborts
/// promptly once cancellation is requested. Lives only for one synthesize()
/// call, so it can hold references to the sink and the cancel flag.
class PcmSinkStream final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::ChainInterfaces<ISpStreamFormat, IStream, ISequentialStream>> {
public:
  PcmSinkStream(const ITtsEngine::PcmSink& sink, const std::atomic<bool>& cancelled,
                const WAVEFORMATEX& format)
      : sink_(sink), cancelled_(cancelled), format_(format) {}

  // ISequentialStream
  HRESULT STDMETHODCALLTYPE Read(void* /*pv*/, ULONG /*cb*/, ULONG* /*read*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Write(const void* pv, ULONG cb, ULONG* written) override {
    if (written != nullptr) {
      *written = 0; // keep pcbWritten meaningful on every path, including failures
    }
    if (cancelled_.load(std::memory_order_relaxed)) {
      return E_ABORT; // makes the synchronous Speak() unwind promptly
    }
    if (pv == nullptr && cb != 0) {
      return E_POINTER;
    }
    if (cb != 0 && sink_) {
      const auto* bytes = static_cast<const std::byte*>(pv);
      try {
        sink_(std::span<const std::byte>(bytes, cb));
      } catch (...) { // never let an exception cross the COM ABI boundary
        return E_FAIL;
      }
    }
    if (written != nullptr) {
      *written = cb;
    }
    position_ += cb;
    return S_OK;
  }

  // IStream
  HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin,
                                 ULARGE_INTEGER* newPosition) override {
    long long base = 0;
    switch (origin) {
    case STREAM_SEEK_SET:
      base = 0;
      break;
    case STREAM_SEEK_CUR:
    case STREAM_SEEK_END:
      base = static_cast<long long>(position_); // append-only: no real end
      break;
    default:
      return STG_E_INVALIDFUNCTION;
    }
    const long long target = base + move.QuadPart;
    if (target < 0) {
      return STG_E_INVALIDFUNCTION;
    }
    position_ = static_cast<ULONGLONG>(target);
    if (newPosition != nullptr) {
      newPosition->QuadPart = position_;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER /*size*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CopyTo(IStream* /*dst*/, ULARGE_INTEGER /*cb*/,
                                   ULARGE_INTEGER* /*read*/, ULARGE_INTEGER* /*written*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Commit(DWORD /*flags*/) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Revert() override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER /*offset*/, ULARGE_INTEGER /*cb*/,
                                       DWORD /*type*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER /*offset*/, ULARGE_INTEGER /*cb*/,
                                         DWORD /*type*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Stat(STATSTG* stat, DWORD /*flags*/) override {
    if (stat == nullptr) {
      return STG_E_INVALIDPOINTER;
    }
    *stat = STATSTG{};
    stat->type = STGTY_STREAM;
    stat->cbSize.QuadPart = position_; // no name reported (STATFLAG_NONAME semantics)
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Clone(IStream** /*clone*/) override {
    return E_NOTIMPL;
  }

  // ISpStreamFormat
  HRESULT STDMETHODCALLTYPE GetFormat(GUID* formatId, WAVEFORMATEX** waveFormat) override {
    if (formatId != nullptr) {
      *formatId = SPDFID_WaveFormatEx;
    }
    if (waveFormat != nullptr) {
      *waveFormat = nullptr;
      auto* copy = static_cast<WAVEFORMATEX*>(::CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
      if (copy == nullptr) {
        return E_OUTOFMEMORY;
      }
      *copy = format_;
      *waveFormat = copy;
    }
    return S_OK;
  }

private:
  const ITtsEngine::PcmSink& sink_;
  const std::atomic<bool>& cancelled_;
  WAVEFORMATEX format_;
  ULONGLONG position_{0};
};

} // namespace vox::tts::detail

#endif // defined(_WIN32)

#endif // VOX_TTS_DETAIL_SAPI_INTERNAL_HPP
