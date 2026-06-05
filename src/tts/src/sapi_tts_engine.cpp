// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows SAPI5 implementation of vox::tts::SapiTtsEngine.
///
/// Thin COM glue: enumerate SAPI voices into plain VoiceDescriptors, apply the
/// pure selectVoice(), and synthesize to a custom ISpStreamFormat whose Write()
/// forwards raw PCM to the caller's sink. All HRESULTs are checked; an exception
/// never crosses the COM ABI boundary.
#include <atomic>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/tts/itts_engine.hpp>
#include <vox/tts/rate.hpp>
#include <vox/tts/sapi_tts_engine.hpp>
#include <vox/tts/voice_selection.hpp>

// The Windows/SAPI headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting.
// clang-format off
#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <sapi.h>
#pragma warning(push)
#pragma warning(disable : 4265)  // WRL FtmBase: non-virtual dtor in a system header
#include <wrl/client.h>
#include <wrl/implements.h>
#pragma warning(pop)
// clang-format on

namespace vox::tts {

namespace {

using Microsoft::WRL::ChainInterfaces;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

/// The single PCM shape the engine forces SAPI output into.
constexpr vox::audio::AudioFormat OutputFormat{22050, 16, 1};

/// The WAVEFORMATEX matching @ref OutputFormat.
WAVEFORMATEX makeWaveFormat() {
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
std::wstring toWide(std::string_view utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int length = static_cast<int>(utf8.size());
  const int chars =
      ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), length, nullptr, 0);
  if (chars <= 0) {
    return {};
  }
  std::wstring out(static_cast<std::size_t>(chars), L'\0');
  const int written =
      ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), length, out.data(), chars);
  if (written != chars) {
    return {};
  }
  return out;
}

/// Converts a UTF-16 C-string to UTF-8 (empty for null/empty/invalid input).
std::string toUtf8(const wchar_t* text) {
  if (text == nullptr) {
    return {};
  }
  const int length = static_cast<int>(::wcslen(text));
  if (length == 0) {
    return {};
  }
  const int bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length, nullptr, 0,
                                          nullptr, nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string out(static_cast<std::size_t>(bytes), '\0');
  const int written = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length, out.data(),
                                            bytes, nullptr, nullptr);
  if (written != bytes) {
    return {};
  }
  return out;
}

/// True if any LCID in a SAPI "Language" attribute is a German primary language.
/// The attribute is a ';'-separated list of hex LCIDs (e.g. "407;c07").
bool languageIsGerman(const std::wstring& languageAttribute) {
  std::size_t start = 0;
  while (start <= languageAttribute.size()) {
    const std::size_t end = languageAttribute.find(L';', start);
    const std::size_t count = end == std::wstring::npos ? std::wstring::npos : end - start;
    const std::wstring token = languageAttribute.substr(start, count);
    if (!token.empty()) {
      const auto lcid = static_cast<unsigned long>(::wcstoul(token.c_str(), nullptr, 16));
      if (PRIMARYLANGID(static_cast<LANGID>(lcid)) == LANG_GERMAN) {
        return true;
      }
    }
    if (end == std::wstring::npos) {
      break;
    }
    start = end + 1;
  }
  return false;
}

/// Reads one string value under a token's "Attributes" key (empty if absent).
std::wstring readAttribute(ISpObjectToken* token, const wchar_t* valueName) {
  ComPtr<ISpDataKey> attributes;
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
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          ChainInterfaces<ISpStreamFormat, IStream, ISequentialStream>> {
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

/// RAII for the COM apartment: CoUninitialize iff this object initialized it.
class ComApartment {
public:
  ComApartment() : initialized_(SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}

  ~ComApartment() {
    if (initialized_) {
      ::CoUninitialize();
    }
  }

  ComApartment(const ComApartment&) = delete;
  ComApartment& operator=(const ComApartment&) = delete;
  ComApartment(ComApartment&&) = delete;
  ComApartment& operator=(ComApartment&&) = delete;

private:
  bool initialized_;
};

} // namespace

class SapiTtsEngine::Impl {
public:
  explicit Impl(VoiceSelectionPolicy policy) {
    if (FAILED(::CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&voice_)))) {
      throw std::runtime_error("SapiTtsEngine: failed to create the SAPI voice");
    }
    enumerateVoices();
    const std::optional<SelectedVoice> chosen = selectVoice(descriptors_, policy);
    if (!chosen) {
      throw std::runtime_error("SapiTtsEngine: no usable voice for the requested policy");
    }
    selected_ = *chosen;
    const auto token = idToToken_.find(selected_.id);
    if (token == idToToken_.end() || FAILED(voice_->SetVoice(token->second.Get()))) {
      throw std::runtime_error("SapiTtsEngine: failed to activate the selected voice");
    }
  }

  ~Impl() = default;

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  [[nodiscard]] const SelectedVoice& selectedVoice() const noexcept {
    return selected_;
  }

  void synthesize(std::string_view utf8Text, const ITtsEngine::PcmSink& sink) {
    cancelled_.store(false, std::memory_order_relaxed);
    const std::wstring wide = toWide(utf8Text);
    if (wide.empty()) {
      return; // nothing to say (or unconvertible) — not an error
    }

    const WAVEFORMATEX format = makeWaveFormat();
    const ComPtr<PcmSinkStream> output = Make<PcmSinkStream>(sink, cancelled_, format);
    if (!output) {
      throw std::runtime_error("SapiTtsEngine: failed to create the output stream");
    }

    if (FAILED(voice_->SetOutput(output.Get(), FALSE))) {
      throw std::runtime_error("SapiTtsEngine: failed to set the SAPI output");
    }
    const HRESULT spoken = voice_->Speak(wide.c_str(), static_cast<DWORD>(SPF_IS_NOT_XML), nullptr);
    voice_->SetOutput(nullptr, TRUE); // release our stream; restore the default sink

    if (FAILED(spoken) && !cancelled_.load(std::memory_order_relaxed)) {
      throw std::runtime_error("SapiTtsEngine: synthesis failed");
    }
  }

  void cancel() noexcept {
    cancelled_.store(true, std::memory_order_relaxed);
  }

  void setRate(int rate) {
    voice_->SetRate(clampRate(rate)); // best effort; rate is non-critical
  }

private:
  void enumerateVoices() {
    // Classic SAPI5 voices only (SPCAT_VOICES). Voices installed through the
    // modern language features register under Speech_OneCore and are not seen
    // here; discovering those is tracked in #52.
    ComPtr<ISpObjectTokenCategory> category;
    if (FAILED(::CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&category))) ||
        FAILED(category->SetId(SPCAT_VOICES, FALSE))) {
      return;
    }

    std::wstring defaultId;
    LPWSTR rawDefaultId = nullptr;
    if (SUCCEEDED(category->GetDefaultTokenId(&rawDefaultId)) && rawDefaultId != nullptr) {
      defaultId = rawDefaultId;
      ::CoTaskMemFree(rawDefaultId);
    }

    ComPtr<IEnumSpObjectTokens> tokens;
    if (FAILED(category->EnumTokens(nullptr, nullptr, &tokens)) || !tokens) {
      return;
    }

    for (;;) {
      ComPtr<ISpObjectToken> token;
      ULONG fetched = 0;
      if (tokens->Next(1, &token, &fetched) != S_OK || fetched != 1 || !token) {
        break;
      }

      LPWSTR rawId = nullptr;
      if (FAILED(token->GetId(&rawId)) || rawId == nullptr) {
        continue;
      }
      const std::wstring wideId(rawId);
      ::CoTaskMemFree(rawId);

      VoiceDescriptor descriptor;
      descriptor.id = toUtf8(wideId.c_str());
      descriptor.name = toUtf8(readAttribute(token.Get(), L"Name").c_str());
      descriptor.isGerman = languageIsGerman(readAttribute(token.Get(), L"Language"));
      descriptor.isDefault = !defaultId.empty() && wideId == defaultId;
      if (descriptor.id.empty()) {
        continue;
      }

      idToToken_.emplace(descriptor.id, token);
      descriptors_.push_back(std::move(descriptor));
    }
  }

  ComApartment com_; // first member: initialized first, uninitialized last
  ComPtr<ISpVoice> voice_;
  std::vector<VoiceDescriptor> descriptors_;
  std::unordered_map<std::string, ComPtr<ISpObjectToken>> idToToken_;
  SelectedVoice selected_;
  std::atomic<bool> cancelled_{false};
};

SapiTtsEngine::SapiTtsEngine(VoiceSelectionPolicy policy) : impl_(std::make_unique<Impl>(policy)) {}

SapiTtsEngine::~SapiTtsEngine() = default;

vox::audio::AudioFormat SapiTtsEngine::format() const {
  return OutputFormat;
}

void SapiTtsEngine::synthesize(std::string_view utf8Text, const PcmSink& sink) {
  impl_->synthesize(utf8Text, sink);
}

void SapiTtsEngine::cancel() {
  impl_->cancel();
}

void SapiTtsEngine::setRate(int rate) {
  impl_->setRate(rate);
}

const SelectedVoice& SapiTtsEngine::selectedVoice() const {
  return impl_->selectedVoice();
}

} // namespace vox::tts
