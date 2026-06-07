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
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vox/audio/audio_format.hpp>
#include <vox/tts/detail/sapi_internal.hpp>
#include <vox/tts/errors.hpp>
#include <vox/tts/rate.hpp>
#include <vox/tts/sapi_test_seam.hpp>
#include <vox/tts/sapi_tts_engine.hpp>
#include <vox/tts/voice_selection.hpp>

namespace vox::tts {

namespace {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

// SAPI internals (converters, LCID parser, attribute reader, PCM output stream)
// live in detail/ so the test suite drives every branch directly (#68/#72).
using detail::languageIsGerman;
using detail::makeOutputWaveFormat;
using detail::OutputFormat;
using detail::PcmSinkStream;
using detail::readAttribute;
using detail::toUtf8;
using detail::toWide;

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
  explicit Impl(VoiceSelectionPolicy policy) {
    if (const HRESULT hr = createVoice(voice_.ReleaseAndGetAddressOf()); FAILED(hr) || !voice_) {
      throw EngineError(static_cast<std::uint32_t>(effectiveError(hr)),
                        "SapiTtsEngine: failed to create the SAPI voice");
    }
    enumerateVoices();
    const std::optional<SelectedVoice> chosen = selectVoice(descriptors_, policy);
    if (!chosen) {
      throw EngineError("SapiTtsEngine: no usable voice for the requested policy");
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

    const WAVEFORMATEX format = makeOutputWaveFormat();
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
    // Classic SAPI5 voices only (SPCAT_VOICES). Voices installed through the
    // modern language features register under Speech_OneCore and are not seen
    // here; discovering those is tracked in #52.
    ComPtr<ISpObjectTokenCategory> category;
    if (FAILED(createTokenCategory(category.ReleaseAndGetAddressOf())) || !category ||
        FAILED(category->SetId(SPCAT_VOICES, FALSE))) {
      return;
    }

    std::wstring defaultId;
    if (LPWSTR rawDefaultId = nullptr;
        SUCCEEDED(category->GetDefaultTokenId(&rawDefaultId)) && rawDefaultId != nullptr) {
      defaultId = rawDefaultId;
      ::CoTaskMemFree(rawDefaultId);
    }

    ComPtr<IEnumSpObjectTokens> tokens;
    if (FAILED(category->EnumTokens(nullptr, nullptr, &tokens)) || !tokens) {
      return;
    }

    for (;;) {
      ComPtr<ISpObjectToken> token;
      if (ULONG fetched = 0; tokens->Next(1, &token, &fetched) != S_OK || fetched != 1 || !token) {
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

      idToToken_.try_emplace(descriptor.id, token);
      descriptors_.push_back(std::move(descriptor));
    }
  }

  ComApartment com_; // first member: initialized first, uninitialized last
  ComPtr<ISpVoice> voice_;
  std::vector<VoiceDescriptor> descriptors_;
  std::unordered_map<std::string, ComPtr<ISpObjectToken>, TransparentStringHash, std::equal_to<>>
      idToToken_;
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
