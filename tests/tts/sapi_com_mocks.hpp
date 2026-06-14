// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only gmock doubles for the SAPI COM chain SapiTtsEngine drives
///        (issue #68). A test installs factories (via the #68 seam) returning a
///        MockSpVoice and a MockSpObjectTokenCategory; the category enumerates a
///        MockEnumSpObjectTokens, which yields MockSpObjectTokens, whose
///        "Attributes" sub-key is a MockSpDataKey. That lets the engine's
///        construction, voice-selection, and synthesis paths — and every
///        EngineError they throw — run with no installed SAPI voice, anywhere
///        (incl. the coverage job).
///
/// Only the handful of methods the engine actually calls are gmock'd; the rest
/// of each (large) SAPI interface is stubbed `E_NOTIMPL` so the class is
/// concrete. The stub bodies are mechanically derived from the SDK signatures.
///
/// Lifetime: the `IUnknown` methods are no-ops (AddRef/Release return 1), so each
/// mock's lifetime is owned by the test, never by the engine's `ComPtr`
/// refcounting — declare the mocks so they outlive the engine that borrows them.
#ifndef VOX_TESTS_TTS_SAPI_COM_MOCKS_HPP
#define VOX_TESTS_TTS_SAPI_COM_MOCKS_HPP

#if defined(_WIN32)

#  include <algorithm>
#  include <cstddef>

#  include <gmock/gmock.h>

// The Windows/SAPI headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting.
// clang-format off
#define NOMINMAX
#include <Windows.h>
#include <objbase.h>
#include <sapi.h>
// clang-format on

// The stub overrides below exist only to make the large SAPI interfaces
// concrete; they intentionally ignore their parameters, so C4100 (unreferenced
// formal parameter, an error under /W4 /WX) is silenced for this file.
#  pragma warning(push)
#  pragma warning(disable : 4100)

namespace vox::tts::testing {

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

/// Duplicates the string literal @p text into a `CoTaskMemAlloc` buffer (the
/// engine frees SAPI string out-params with `CoTaskMemFree`). Returns nullptr on
/// allocation failure, matching the COM contract. Taking the literal by array
/// reference gives the length (incl. the terminator) at compile time, so no
/// run-time scan of a possibly-unterminated buffer is needed.
template<std::size_t N>
LPWSTR coTaskString(const wchar_t (&text)[N]) {
  auto* copy = static_cast<LPWSTR>(::CoTaskMemAlloc(N * sizeof(wchar_t)));
  if (copy != nullptr) {
    std::copy_n(static_cast<const wchar_t*>(text), N, copy); // N elements, incl. terminator
  }
  return copy;
}

/// Mock `ISpDataKey` — a voice token's "Attributes" sub-key (Name/Language).
class MockSpDataKey : public ComMockBase<ISpDataKey> {
public:
  HRESULT STDMETHODCALLTYPE SetData(LPCWSTR pszValueName, ULONG cbData,
                                    const BYTE* pData) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetData(LPCWSTR pszValueName, ULONG* pcbData, BYTE* pData) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetStringValue(_In_opt_ LPCWSTR pszValueName,
                                           LPCWSTR pszValue) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, GetStringValue, (_In_opt_ LPCWSTR pszValueName, _Outptr_ LPWSTR* ppszValue),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE SetDWORD(LPCWSTR pszValueName, DWORD dwValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDWORD(LPCWSTR pszValueName, DWORD* pdwValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE OpenKey(LPCWSTR pszSubKeyName,
                                    _Outptr_ ISpDataKey** ppSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateKey(LPCWSTR pszSubKey, _Outptr_ ISpDataKey** ppSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DeleteKey(LPCWSTR pszSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DeleteValue(LPCWSTR pszValueName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumKeys(ULONG Index, _Outptr_ LPWSTR* ppszSubKeyName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumValues(ULONG Index, _Outptr_ LPWSTR* ppszValueName) override {
    return E_NOTIMPL;
  }
};

/// Mock `ISpObjectToken` — one enumerated voice. `GetId` and `OpenKey` are used;
/// the rest of `ISpDataKey`/`ISpObjectToken` is stubbed.
class MockSpObjectToken : public ComMockBase<ISpObjectToken> {
public:
  HRESULT STDMETHODCALLTYPE SetData(LPCWSTR pszValueName, ULONG cbData,
                                    const BYTE* pData) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetData(LPCWSTR pszValueName, ULONG* pcbData, BYTE* pData) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetStringValue(_In_opt_ LPCWSTR pszValueName,
                                           LPCWSTR pszValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetStringValue(_In_opt_ LPCWSTR pszValueName,
                                           _Outptr_ LPWSTR* ppszValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetDWORD(LPCWSTR pszValueName, DWORD dwValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDWORD(LPCWSTR pszValueName, DWORD* pdwValue) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, OpenKey, (LPCWSTR pszSubKeyName, _Outptr_ ISpDataKey** ppSubKey),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE CreateKey(LPCWSTR pszSubKey, _Outptr_ ISpDataKey** ppSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DeleteKey(LPCWSTR pszSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DeleteValue(LPCWSTR pszValueName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumKeys(ULONG Index, _Outptr_ LPWSTR* ppszSubKeyName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumValues(ULONG Index, _Outptr_ LPWSTR* ppszValueName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetId(_In_opt_ LPCWSTR pszCategoryId, LPCWSTR pszTokenId,
                                  BOOL fCreateIfNotExist) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, GetId, (_Outptr_ LPWSTR * ppszCoMemTokenId),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  GetCategory(_Outptr_ ISpObjectTokenCategory** ppTokenCategory) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, DWORD dwClsContext, REFIID riid,
                                           void** ppvObject) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetStorageFileName(REFCLSID clsidCaller, _In_ LPCWSTR pszValueName,
                                               _In_opt_ LPCWSTR pszFileNameSpecifier, ULONG nFolder,
                                               _Outptr_ LPWSTR* ppszFilePath) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE RemoveStorageFileName(REFCLSID clsidCaller, _In_ LPCWSTR pszKeyName,
                                                  BOOL fDeleteFile) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Remove(const _In_opt_ CLSID* pclsidCaller) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE IsUISupported(LPCWSTR pszTypeOfUI, void* pvExtraData, ULONG cbExtraData,
                                          IUnknown* punkObject, BOOL* pfSupported) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DisplayUI(HWND hwndParent, LPCWSTR pszTitle, LPCWSTR pszTypeOfUI,
                                      void* pvExtraData, ULONG cbExtraData,
                                      IUnknown* punkObject) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE MatchesAttributes(LPCWSTR pszAttributes, BOOL* pfMatches) override {
    return E_NOTIMPL;
  }
};

/// Mock `ISpObjectTokenCategory` — the voice catalogue. `SetId`, `EnumTokens`,
/// and `GetDefaultTokenId` are used; the rest of `ISpDataKey`/category is stubbed.
class MockSpObjectTokenCategory : public ComMockBase<ISpObjectTokenCategory> {
public:
  HRESULT STDMETHODCALLTYPE SetData(LPCWSTR pszValueName, ULONG cbData,
                                    const BYTE* pData) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetData(LPCWSTR pszValueName, ULONG* pcbData, BYTE* pData) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetStringValue(_In_opt_ LPCWSTR pszValueName,
                                           LPCWSTR pszValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetStringValue(_In_opt_ LPCWSTR pszValueName,
                                           _Outptr_ LPWSTR* ppszValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetDWORD(LPCWSTR pszValueName, DWORD dwValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDWORD(LPCWSTR pszValueName, DWORD* pdwValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE OpenKey(LPCWSTR pszSubKeyName,
                                    _Outptr_ ISpDataKey** ppSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateKey(LPCWSTR pszSubKey, _Outptr_ ISpDataKey** ppSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DeleteKey(LPCWSTR pszSubKey) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DeleteValue(LPCWSTR pszValueName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumKeys(ULONG Index, _Outptr_ LPWSTR* ppszSubKeyName) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumValues(ULONG Index, _Outptr_ LPWSTR* ppszValueName) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, SetId, (LPCWSTR pszCategoryId, BOOL fCreateIfNotExist),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE GetId(_Outptr_ LPWSTR* ppszCoMemCategoryId) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetDataKey(SPDATAKEYLOCATION spdkl,
                                       _Outptr_ ISpDataKey** ppDataKey) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, EnumTokens,
              (_In_opt_ LPCWSTR pzsReqAttribs, _In_opt_ LPCWSTR pszOptAttribs,
               IEnumSpObjectTokens** ppEnum),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE SetDefaultTokenId(LPCWSTR pszTokenId) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, GetDefaultTokenId, (_Outptr_ LPWSTR * ppszCoMemTokenId),
              (override, Calltype(STDMETHODCALLTYPE)));
};

/// Mock `IEnumSpObjectTokens` — the token enumerator. Only `Next` is used.
class MockEnumSpObjectTokens : public ComMockBase<IEnumSpObjectTokens> {
public:
  MOCK_METHOD(HRESULT, Next, (ULONG celt, ISpObjectToken** pelt, _Out_opt_ ULONG* pceltFetched),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE Skip(ULONG celt) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Reset() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Clone(IEnumSpObjectTokens** ppEnum) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Item(ULONG Index, ISpObjectToken** ppToken) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetCount(ULONG* pCount) override {
    return E_NOTIMPL;
  }
};

/// Mock `ISpVoice` — the synthesizer. `SetOutput`, `SetVoice`, `Speak`, and
/// `SetRate` are used; the rest of the (large) `ISpVoice`/`ISpEventSource`/
/// `ISpNotifySource` surface is stubbed.
class MockSpVoice : public ComMockBase<ISpVoice> {
public:
  HRESULT STDMETHODCALLTYPE SetNotifySink(__RPC__in_opt ISpNotifySink* pNotifySink) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetNotifyWindowMessage(HWND hWnd, UINT Msg, WPARAM wParam,
                                                   LPARAM lParam) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetNotifyCallbackFunction(SPNOTIFYCALLBACK* pfnCallback, WPARAM wParam,
                                                      LPARAM lParam) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetNotifyCallbackInterface(ISpNotifyCallback* pSpCallback,
                                                       WPARAM wParam, LPARAM lParam) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetNotifyWin32Event() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE WaitForNotifyEvent(DWORD dwMilliseconds) override {
    return E_NOTIMPL;
  }

  HANDLE STDMETHODCALLTYPE GetNotifyEventHandle() override {
    return nullptr;
  }

  HRESULT STDMETHODCALLTYPE SetInterest(ULONGLONG ullEventInterest,
                                        ULONGLONG ullQueuedInterest) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetEvents(ULONG ulCount, SPEVENT* pEventArray,
                                      ULONG* pulFetched) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetInfo(SPEVENTSOURCEINFO* pInfo) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, SetOutput, (IUnknown * pUnkOutput, BOOL fAllowFormatChanges),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE GetOutputObjectToken(_Outptr_ ISpObjectToken** ppObjectToken) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetOutputStream(ISpStreamFormat** ppStream) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Pause() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Resume() override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, SetVoice, (ISpObjectToken * pToken),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE GetVoice(_Outptr_ ISpObjectToken** ppToken) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, Speak,
              (_In_opt_ LPCWSTR pwcs, DWORD dwFlags, _Out_opt_ ULONG* pulStreamNumber),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE SpeakStream(IStream* pStream, DWORD dwFlags,
                                        _Out_opt_ ULONG* pulStreamNumber) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetStatus(SPVOICESTATUS* pStatus,
                                      _Outptr_ LPWSTR* ppszLastBookmark) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Skip(LPCWSTR pItemType, long lNumItems, ULONG* pulNumSkipped) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetPriority(SPVPRIORITY ePriority) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetPriority(SPVPRIORITY* pePriority) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetAlertBoundary(SPEVENTENUM eBoundary) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetAlertBoundary(SPEVENTENUM* peBoundary) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, SetRate, (long RateAdjust), (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE GetRate(long* pRateAdjust) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetVolume(USHORT usVolume) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetVolume(USHORT* pusVolume) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE WaitUntilDone(ULONG msTimeout) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetSyncSpeakTimeout(ULONG msTimeout) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetSyncSpeakTimeout(ULONG* pmsTimeout) override {
    return E_NOTIMPL;
  }

  HANDLE STDMETHODCALLTYPE SpeakCompleteEvent() override {
    return nullptr;
  }

  HRESULT STDMETHODCALLTYPE IsUISupported(LPCWSTR pszTypeOfUI, void* pvExtraData, ULONG cbExtraData,
                                          BOOL* pfSupported) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DisplayUI(HWND hwndParent, LPCWSTR pszTitle, LPCWSTR pszTypeOfUI,
                                      void* pvExtraData, ULONG cbExtraData) override {
    return E_NOTIMPL;
  }
};

} // namespace vox::tts::testing

#  pragma warning(pop)

#endif // defined(_WIN32)

#endif // VOX_TESTS_TTS_SAPI_COM_MOCKS_HPP
