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
#include <cstdint>
#include <cwchar>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/tts/detail/sapi_internal.hpp>
#include <vox/tts/errors.hpp>
#include <vox/tts/itts_engine.hpp>
#include <vox/tts/rate.hpp>
#include <vox/tts/sapi_test_seam.hpp>
#include <vox/tts/sapi_tts_engine.hpp>
#include <vox/tts/voice_selection.hpp>

// The Windows/SAPI headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting.
// clang-format off
#include <Windows.h>
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

// The pure UTF-8/UTF-16 converters, the LCID parser, and the attribute reader
// live in detail/ so the suite tests every branch directly (#68/#72). The COM
// PcmSinkStream below stays here (its IStream ABI surface is exercised through
// the engine), keeping this the only translation unit that needs the SDK glue.
using detail::firstLcid;
using detail::readAttribute;
using detail::toUtf8;
using detail::toWide;

/// The OneCore voice catalogue (#52). Voices installed through the modern
/// language features (Windows Settings, Install-Language) register only here —
/// classic SPCAT_VOICES does not see them. The tokens are SAPI-compatible and
/// drive ISpVoice unchanged; there is no SPCAT_* constant for this hive.
constexpr const wchar_t* OneCoreVoiceCategoryId =
    L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices";

/// Test seams (issue #68): when installed, these factories replace
/// CoCreateInstance for the SAPI voice and the voice-token category, so the
/// engine's construction / voice-selection / synthesis paths are unit-tested
/// with mock COM and no installed voice. Empty in production.
std::function<long(ISpVoice**)>& voiceFactory() {
  static std::function<long(ISpVoice**)> factory;
  return factory;
}

std::function<long(ISpObjectTokenCategory**)>& tokenCategoryFactory() {
  static std::function<long(ISpObjectTokenCategory**)> factory;
  return factory;
}

/// Creates the SAPI voice — via the test factory when one is installed, else the
/// real CoCreateInstance.
HRESULT createVoice(ISpVoice** out) {
  if (const auto& factory = voiceFactory()) {
    return static_cast<HRESULT>(factory(out));
  }
  return ::CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_PPV_ARGS(out));
}

/// Creates the voice-token category — via the test factory when installed, else
/// the real CoCreateInstance.
HRESULT createTokenCategory(ISpObjectTokenCategory** out) {
  if (const auto& factory = tokenCategoryFactory()) {
    return static_cast<HRESULT>(factory(out));
  }
  return ::CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(out));
}

/// The HRESULT to report when a COM call "succeeded" but left its out-param null:
/// keep a genuine failure code, but turn a deceptive S_OK into E_POINTER so the
/// thrown EngineError carries a meaningful native code.
HRESULT effectiveError(HRESULT hr) {
  return FAILED(hr) ? hr : E_POINTER;
}

/// Transparent hash so the voice-id map can be looked up by `std::string_view`
/// without constructing a `std::string` key (heterogeneous lookup).
struct TransparentStringHash {
  using is_transparent = void;

  [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
    return std::hash<std::string_view>{}(text);
  }
};

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
  ComApartment() {
    const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
      // COM is already initialized on this thread in a different (e.g. STA)
      // apartment. That is fine for our use, but we do not own the
      // initialization and must not balance it with CoUninitialize.
      initialized_ = false;
    } else if (FAILED(hr)) {
      throw EngineError(static_cast<std::uint32_t>(hr), "SapiTtsEngine: COM initialization failed");
    } else {
      initialized_ = true; // S_OK or S_FALSE — we own a reference to release
    }
  }

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
  bool initialized_{false};
};

} // namespace

namespace testing {
void setVoiceFactory(VoiceFactory factory) {
  voiceFactory() = std::move(factory);
}

void setTokenCategoryFactory(TokenCategoryFactory factory) {
  tokenCategoryFactory() = std::move(factory);
}
} // namespace testing

class SapiTtsEngine::Impl {
public:
  explicit Impl(const VoiceSelectionRequest& request) {
    if (const HRESULT hr = createVoice(voice_.ReleaseAndGetAddressOf()); FAILED(hr) || !voice_) {
      throw EngineError(static_cast<std::uint32_t>(effectiveError(hr)),
                        "SapiTtsEngine: failed to create the SAPI voice");
    }
    enumerateVoices();
    const std::optional<SelectedVoice> chosen = selectVoice(descriptors_, request);
    if (!chosen) {
      throw EngineError("SapiTtsEngine: no usable voice for the requested language or voice name");
    }
    selected_ = *chosen;
    const auto token = idToToken_.find(selected_.id);
    if (token == idToToken_.end()) {
      throw EngineError("SapiTtsEngine: the selected voice token is no longer available");
    }
    if (const HRESULT hr = voice_->SetVoice(token->second.Get()); FAILED(hr)) {
      throw EngineError(static_cast<std::uint32_t>(hr),
                        "SapiTtsEngine: failed to activate the selected voice");
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
    if (utf8Text.empty()) {
      return; // nothing to say
    }
    const std::wstring wide = toWide(utf8Text);
    if (wide.empty()) {
      // Non-empty input that did not convert is invalid UTF-8 — surface it
      // rather than silently producing no audio.
      throw EngineError("SapiTtsEngine: input text is not valid UTF-8");
    }

    const WAVEFORMATEX format = makeWaveFormat();
    const ComPtr<PcmSinkStream> output = Make<PcmSinkStream>(sink, cancelled_, format);
    if (!output) {
      throw EngineError("SapiTtsEngine: failed to create the output stream");
    }

    if (const HRESULT hr = voice_->SetOutput(output.Get(), FALSE); FAILED(hr)) {
      throw EngineError(static_cast<std::uint32_t>(hr),
                        "SapiTtsEngine: failed to set the SAPI output");
    }
    const HRESULT spoken = voice_->Speak(wide.c_str(), static_cast<DWORD>(SPF_IS_NOT_XML), nullptr);
    voice_->SetOutput(nullptr, TRUE); // release our stream; restore the default sink

    if (FAILED(spoken) && !cancelled_.load(std::memory_order_relaxed)) {
      throw EngineError(static_cast<std::uint32_t>(spoken), "SapiTtsEngine: synthesis failed");
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
    // Classic SAPI5 voices (SPCAT_VOICES) plus the OneCore catalogue (#52):
    // voices installed through the modern language features (Windows Settings,
    // Install-Language) register only under Speech_OneCore, invisible to
    // classic enumeration. Their tokens drive ISpVoice unchanged, so this is
    // purely a discovery extension. Classic is enumerated first and wins on
    // duplicates — see mergeVoices() for the precedence rules.
    std::vector<VoiceDescriptor> classic = enumerateCategory(SPCAT_VOICES);
    const std::vector<VoiceDescriptor> oneCore = enumerateCategory(OneCoreVoiceCategoryId);
    descriptors_ = mergeVoices(std::move(classic), oneCore);
  }

  /// Enumerates one voice-token category into descriptors (and registers each
  /// token in the id map). An unavailable category yields an empty list.
  std::vector<VoiceDescriptor> enumerateCategory(const wchar_t* categoryId) {
    std::vector<VoiceDescriptor> found;
    const ComPtr<ISpObjectTokenCategory> category = openVoiceCategory(categoryId);
    if (!category) {
      return found;
    }
    const std::wstring defaultId = readDefaultTokenId(*category.Get());

    ComPtr<IEnumSpObjectTokens> tokens;
    if (FAILED(category->EnumTokens(nullptr, nullptr, &tokens)) || !tokens) {
      return found;
    }
    for (ComPtr<ISpObjectToken> token = nextToken(tokens.Get()); token;
         token = nextToken(tokens.Get())) {
      addVoice(token, defaultId, found);
    }
    return found;
  }

  /// Opens the voice category @p categoryId, or null if it is unavailable
  /// (e.g. the OneCore hive on a system that does not have it).
  ComPtr<ISpObjectTokenCategory> openVoiceCategory(const wchar_t* categoryId) {
    ComPtr<ISpObjectTokenCategory> category;
    // The init-statement runs before the condition, so category is set by the
    // call before the null-check reads it.
    if (const HRESULT created = createTokenCategory(category.ReleaseAndGetAddressOf());
        FAILED(created) || !category) {
      return nullptr;
    }
    if (FAILED(category->SetId(categoryId, FALSE))) {
      return nullptr;
    }
    return category;
  }

  /// The id of the category's default voice token (empty if there is none).
  static std::wstring readDefaultTokenId(ISpObjectTokenCategory& category) {
    std::wstring defaultId;
    if (LPWSTR raw = nullptr; SUCCEEDED(category.GetDefaultTokenId(&raw)) && raw != nullptr) {
      defaultId = raw;
      ::CoTaskMemFree(raw);
    }
    return defaultId;
  }

  /// The next token in the enumeration, or null at the end.
  static ComPtr<ISpObjectToken> nextToken(IEnumSpObjectTokens* tokens) {
    ComPtr<ISpObjectToken> token;
    if (ULONG fetched = 0; tokens->Next(1, &token, &fetched) != S_OK || fetched != 1) {
      return nullptr;
    }
    return token;
  }

  /// Reads @p token into @p found (and the id→token map); skips a token whose
  /// id cannot be read or is empty.
  void addVoice(const ComPtr<ISpObjectToken>& token, std::wstring_view defaultId,
                std::vector<VoiceDescriptor>& found) {
    LPWSTR rawId = nullptr;
    if (FAILED(token->GetId(&rawId)) || rawId == nullptr) {
      return;
    }
    const std::wstring wideId(rawId);
    ::CoTaskMemFree(rawId);

    VoiceDescriptor descriptor;
    descriptor.id = toUtf8(wideId.c_str());
    descriptor.name = toUtf8(readAttribute(token.Get(), L"Name").c_str());
    descriptor.language =
        std::string(languageTagFromLangId(firstLcid(readAttribute(token.Get(), L"Language"))));
    descriptor.isDefault = !defaultId.empty() && wideId == defaultId;
    if (descriptor.id.empty()) {
      return;
    }
    idToToken_.try_emplace(descriptor.id, token);
    found.push_back(std::move(descriptor));
  }

  ComApartment com_; // first member: initialized first, uninitialized last
  ComPtr<ISpVoice> voice_;
  std::vector<VoiceDescriptor> descriptors_;
  std::unordered_map<std::string, ComPtr<ISpObjectToken>, TransparentStringHash, std::equal_to<>>
      idToToken_;
  SelectedVoice selected_;
  std::atomic<bool> cancelled_{false};
};

SapiTtsEngine::SapiTtsEngine(const VoiceSelectionRequest& request)
    : impl_(std::make_unique<Impl>(request)) {}

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
