// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only gmock doubles for the WASAPI COM device-acquisition chain
///        (issue #68). A test installs a factory (via the #68 seam) that returns
///        a `MockMMDeviceEnumerator`; its `GetDefaultAudioEndpoint` hands back a
///        `MockMMDevice`, whose `Activate` hands back a `MockAudioClient`, whose
///        `GetService` hands back a `MockAudioRenderClient`. That lets the sink's
///        whole acquisition path — and every `DeviceError` it can throw — run
///        with no real audio device, anywhere (incl. the coverage job).
///
/// Lifetime: the `IUnknown` methods are no-ops (AddRef/Release return 1), so each
/// mock's lifetime is owned by the test (stack/`NiceMock`), never by the sink's
/// `ComPtr` refcounting. The mocks must therefore outlive the `WasapiAudioSink`
/// that borrows them — declare them before the sink so they destruct after it.
#ifndef VOX_TESTS_AUDIO_WASAPI_COM_MOCKS_HPP
#define VOX_TESTS_AUDIO_WASAPI_COM_MOCKS_HPP

#if defined(_WIN32)

#  include <gmock/gmock.h>

// The Windows audio headers are include-order sensitive (windows.h must lead),
// so this block is exempt from clang-format's include sorting.
// clang-format off
#define NOMINMAX
#include <Windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
// clang-format on

namespace vox::audio::testing {

/// Base that satisfies `IUnknown` with no-op refcounting, so the test owns each
/// mock's lifetime. `QueryInterface` hands back `this` (the chain never asks for
/// a different interface), so a `ComPtr` copy keeps working.
template<class Interface>
class ComMockBase : public Interface {
public:
  ComMockBase() = default;
  virtual ~ComMockBase() = default; // first virtual dtor in the COM hierarchy

  ComMockBase(const ComMockBase&) = delete;
  ComMockBase& operator=(const ComMockBase&) = delete;
  ComMockBase(ComMockBase&&) = delete;
  ComMockBase& operator=(ComMockBase&&) = delete;

  ULONG STDMETHODCALLTYPE AddRef() override {
    return 1;
  }

  ULONG STDMETHODCALLTYPE Release() override {
    return 1;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    if (riid == __uuidof(IUnknown) || riid == __uuidof(Interface)) {
      *ppv = this;
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE; // unsupported IID, like a real COM object
  }
};

/// Mock `IAudioRenderClient` — the leaf the render loop copies samples into.
class MockAudioRenderClient : public ComMockBase<IAudioRenderClient> {
public:
  MOCK_METHOD(HRESULT, GetBuffer, (UINT32 numFramesRequested, BYTE** ppData),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, ReleaseBuffer, (UINT32 numFramesWritten, DWORD dwFlags),
              (override, Calltype(STDMETHODCALLTYPE)));
};

/// Mock `IAudioClient` — the device-side stream the sink configures and drives.
class MockAudioClient : public ComMockBase<IAudioClient> {
public:
  MOCK_METHOD(HRESULT, Initialize,
              (AUDCLNT_SHAREMODE shareMode, DWORD streamFlags, REFERENCE_TIME hnsBufferDuration,
               REFERENCE_TIME hnsPeriodicity, const WAVEFORMATEX* pFormat,
               LPCGUID audioSessionGuid),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, GetBufferSize, (UINT32 * pNumBufferFrames),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, GetCurrentPadding, (UINT32 * pNumPaddingFrames),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, GetMixFormat, (WAVEFORMATEX * *ppDeviceFormat),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, Start, (), (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, Stop, (), (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, Reset, (), (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, SetEventHandle, (HANDLE eventHandle),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, GetService, (REFIID riid, void** ppv),
              (override, Calltype(STDMETHODCALLTYPE)));

  // Unused by the sink — stubbed so the class is concrete.
  HRESULT STDMETHODCALLTYPE GetStreamLatency(REFERENCE_TIME* /*phnsLatency*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE IsFormatSupported(AUDCLNT_SHAREMODE /*shareMode*/,
                                              const WAVEFORMATEX* /*pFormat*/,
                                              WAVEFORMATEX** /*ppClosestMatch*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDevicePeriod(REFERENCE_TIME* /*phnsDefaultDevicePeriod*/,
                                            REFERENCE_TIME* /*phnsMinimumDevicePeriod*/) override {
    return E_NOTIMPL;
  }
};

/// Mock `IMMDevice` — the endpoint the sink activates an `IAudioClient` from.
class MockMMDevice : public ComMockBase<IMMDevice> {
public:
  MOCK_METHOD(HRESULT, Activate,
              (REFIID iid, DWORD dwClsCtx, PROPVARIANT* pActivationParams, void** ppInterface),
              (override, Calltype(STDMETHODCALLTYPE)));

  // Unused by the sink — stubbed so the class is concrete.
  HRESULT STDMETHODCALLTYPE OpenPropertyStore(DWORD /*stgmAccess*/,
                                              IPropertyStore** /*ppProperties*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetId(LPWSTR* /*ppstrId*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetState(DWORD* /*pdwState*/) override {
    return E_NOTIMPL;
  }
};

/// Mock `IMMDeviceEnumerator` — the root the #68 factory returns.
class MockMMDeviceEnumerator : public ComMockBase<IMMDeviceEnumerator> {
public:
  MOCK_METHOD(HRESULT, GetDefaultAudioEndpoint,
              (EDataFlow dataFlow, ERole role, IMMDevice** ppEndpoint),
              (override, Calltype(STDMETHODCALLTYPE)));

  // Unused by the sink — stubbed so the class is concrete.
  HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow /*dataFlow*/, DWORD /*dwStateMask*/,
                                               IMMDeviceCollection** /*ppDevices*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR /*pwstrId*/, IMMDevice** /*ppDevice*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  RegisterEndpointNotificationCallback(IMMNotificationClient* /*pClient*/) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  UnregisterEndpointNotificationCallback(IMMNotificationClient* /*pClient*/) override {
    return E_NOTIMPL;
  }
};

/// Allocates a `WAVEFORMATEX` with `CoTaskMemAlloc` (the sink frees it with
/// `CoTaskMemFree`), so `GetMixFormat` can hand back an owned mix format. Returns
/// a 16-bit PCM stereo-or-mono format the sink supports by default.
inline WAVEFORMATEX* makeMixFormat(WORD formatTag = WAVE_FORMAT_PCM, WORD bitsPerSample = 16,
                                   DWORD samplesPerSec = 48000, WORD channels = 2) {
  auto* format = static_cast<WAVEFORMATEX*>(::CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
  if (format == nullptr) {
    return nullptr; // COM contract: out-param stays null on allocation failure
  }
  format->wFormatTag = formatTag;
  format->nChannels = channels;
  format->nSamplesPerSec = samplesPerSec;
  format->wBitsPerSample = bitsPerSample;
  format->nBlockAlign = static_cast<WORD>(channels * (bitsPerSample / 8U));
  format->nAvgBytesPerSec = samplesPerSec * format->nBlockAlign;
  format->cbSize = 0;
  return format;
}

} // namespace vox::audio::testing

#endif // defined(_WIN32)

#endif // VOX_TESTS_AUDIO_WASAPI_COM_MOCKS_HPP
