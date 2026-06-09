// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief WIN32-only gmock doubles for the UI Automation COM chain UiaProvider
///        drives (issue #68). A test installs a factory (via the #68 seam)
///        returning a MockUiAutomation; its CreateCacheRequest yields a
///        MockUiCacheRequest, GetFocusedElementBuildCache yields a MockUiElement,
///        and GetCachedPatternAs yields the per-pattern mocks. That lets the
///        provider's focused-element extraction and focus-event paths run with no
///        real UI Automation tree, anywhere (incl. the coverage job).
///
/// Only the handful of methods the provider actually calls are gmock'd; the rest
/// of each (large) UIA interface is stubbed E_NOTIMPL so the class is concrete
/// (stub bodies mechanically derived from the SDK signatures). No-op IUnknown
/// refcounting lets the test own each mock's lifetime.
#ifndef VOX_TESTS_PROVIDER_UIA_COM_MOCKS_HPP
#define VOX_TESTS_PROVIDER_UIA_COM_MOCKS_HPP

#if defined(_WIN32)

#  include <gmock/gmock.h>

// The Windows UIA headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting.
// clang-format off
#define NOMINMAX
#include <Windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
// clang-format on

// The stub overrides below exist only to make the large UIA interfaces concrete;
// they intentionally ignore their parameters, so C4100 (unreferenced formal
// parameter, an error under /W4 /WX) is silenced for this file.
#  pragma warning(push)
#  pragma warning(disable : 4100)

namespace vox::provider::testing {

/// Base that satisfies `IUnknown` with no-op refcounting, so the test owns each
/// mock's lifetime. `QueryInterface` hands back `this` (the chain only ever asks
/// for the pattern interface it just requested via GetCachedPatternAs, which the
/// test wires explicitly), so a `ComPtr` copy keeps working.
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

/// Allocates a `BSTR` (the provider frees name/value strings with SysFreeString).
inline BSTR bstr(const wchar_t* text) {
  return ::SysAllocString(text);
}

/// Mock `IUIAutomation`.
class MockUiAutomation : public ComMockBase<IUIAutomation> {
public:
  HRESULT STDMETHODCALLTYPE CompareElements(__RPC__in_opt IUIAutomationElement* el1,
                                            __RPC__in_opt IUIAutomationElement* el2,
                                            __RPC__out BOOL* areSame) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CompareRuntimeIds(__RPC__in SAFEARRAY* runtimeId1,
                                              __RPC__in SAFEARRAY* runtimeId2,
                                              __RPC__out BOOL* areSame) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetRootElement(__RPC__deref_out_opt IUIAutomationElement** root) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE ElementFromHandle(
      __RPC__in UIA_HWND hwnd, __RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  ElementFromPoint(POINT pt, __RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetFocusedElement(__RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetRootElementBuildCache(__RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                           __RPC__deref_out_opt IUIAutomationElement** root) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE ElementFromHandleBuildCache(
      __RPC__in UIA_HWND hwnd, __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
      __RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  ElementFromPointBuildCache(POINT pt, __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                             __RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, GetFocusedElementBuildCache,
              (__RPC__in_opt IUIAutomationCacheRequest * cacheRequest,
               __RPC__deref_out_opt IUIAutomationElement** element),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  CreateTreeWalker(__RPC__in_opt IUIAutomationCondition* pCondition,
                   __RPC__deref_out_opt IUIAutomationTreeWalker** walker) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ControlViewWalker(__RPC__deref_out_opt IUIAutomationTreeWalker** walker) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ContentViewWalker(__RPC__deref_out_opt IUIAutomationTreeWalker** walker) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_RawViewWalker(__RPC__deref_out_opt IUIAutomationTreeWalker** walker) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_RawViewCondition(__RPC__deref_out_opt IUIAutomationCondition** condition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ControlViewCondition(__RPC__deref_out_opt IUIAutomationCondition** condition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ContentViewCondition(__RPC__deref_out_opt IUIAutomationCondition** condition) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, CreateCacheRequest,
              (__RPC__deref_out_opt IUIAutomationCacheRequest * *cacheRequest),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  CreateTrueCondition(__RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreateFalseCondition(__RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreatePropertyCondition(PROPERTYID propertyId, VARIANT value,
                          __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreatePropertyConditionEx(PROPERTYID propertyId, VARIANT value, enum PropertyConditionFlags flags,
                            __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreateAndCondition(__RPC__in_opt IUIAutomationCondition* condition1,
                     __RPC__in_opt IUIAutomationCondition* condition2,
                     __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreateAndConditionFromArray(__RPC__in_opt SAFEARRAY* conditions,
                              __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateAndConditionFromNativeArray(
      __RPC__in_ecount_full(conditionCount) IUIAutomationCondition** conditions, int conditionCount,
      __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreateOrCondition(__RPC__in_opt IUIAutomationCondition* condition1,
                    __RPC__in_opt IUIAutomationCondition* condition2,
                    __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreateOrConditionFromArray(__RPC__in_opt SAFEARRAY* conditions,
                             __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateOrConditionFromNativeArray(
      __RPC__in_ecount_full(conditionCount) IUIAutomationCondition** conditions, int conditionCount,
      __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  CreateNotCondition(__RPC__in_opt IUIAutomationCondition* condition,
                     __RPC__deref_out_opt IUIAutomationCondition** newCondition) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE AddAutomationEventHandler(
      EVENTID eventId, __RPC__in_opt IUIAutomationElement* element, enum TreeScope scope,
      __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
      __RPC__in_opt IUIAutomationEventHandler* handler) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  RemoveAutomationEventHandler(EVENTID eventId, __RPC__in_opt IUIAutomationElement* element,
                               __RPC__in_opt IUIAutomationEventHandler* handler) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE AddPropertyChangedEventHandlerNativeArray(
      __RPC__in_opt IUIAutomationElement* element, enum TreeScope scope,
      __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
      __RPC__in_opt IUIAutomationPropertyChangedEventHandler* handler,
      __RPC__in_ecount_full(propertyCount) PROPERTYID* propertyArray, int propertyCount) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  AddPropertyChangedEventHandler(__RPC__in_opt IUIAutomationElement* element, enum TreeScope scope,
                                 __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                                 __RPC__in_opt IUIAutomationPropertyChangedEventHandler* handler,
                                 __RPC__in SAFEARRAY* propertyArray) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE RemovePropertyChangedEventHandler(
      __RPC__in_opt IUIAutomationElement* element,
      __RPC__in_opt IUIAutomationPropertyChangedEventHandler* handler) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE AddStructureChangedEventHandler(
      __RPC__in_opt IUIAutomationElement* element, enum TreeScope scope,
      __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
      __RPC__in_opt IUIAutomationStructureChangedEventHandler* handler) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE RemoveStructureChangedEventHandler(
      __RPC__in_opt IUIAutomationElement* element,
      __RPC__in_opt IUIAutomationStructureChangedEventHandler* handler) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, AddFocusChangedEventHandler,
              (__RPC__in_opt IUIAutomationCacheRequest * cacheRequest,
               __RPC__in_opt IUIAutomationFocusChangedEventHandler* handler),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, RemoveFocusChangedEventHandler,
              (__RPC__in_opt IUIAutomationFocusChangedEventHandler * handler),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE RemoveAllEventHandlers() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  IntNativeArrayToSafeArray(__RPC__in_ecount_full(arrayCount) int* array, int arrayCount,
                            __RPC__deref_out_opt SAFEARRAY** safeArray) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE IntSafeArrayToNativeArray(
      __RPC__in SAFEARRAY* intArray, __RPC__deref_out_ecount_full_opt(*arrayCount) int** array,
      __RPC__out int* arrayCount) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE RectToVariant(RECT rc, __RPC__out VARIANT* var) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE VariantToRect(VARIANT var, __RPC__out RECT* rc) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  SafeArrayToRectNativeArray(__RPC__in SAFEARRAY* rects,
                             __RPC__deref_out_ecount_full_opt(*rectArrayCount) RECT** rectArray,
                             __RPC__out int* rectArrayCount) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateProxyFactoryEntry(
      __RPC__in_opt IUIAutomationProxyFactory* factory,
      __RPC__deref_out_opt IUIAutomationProxyFactoryEntry** factoryEntry) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_ProxyFactoryMapping(
      __RPC__deref_out_opt IUIAutomationProxyFactoryMapping** factoryMapping) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetPropertyProgrammaticName(PROPERTYID property,
                                                        __RPC__deref_out_opt BSTR* name) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetPatternProgrammaticName(PATTERNID pattern,
                                                       __RPC__deref_out_opt BSTR* name) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE PollForPotentialSupportedPatterns(
      __RPC__in_opt IUIAutomationElement* pElement, __RPC__deref_out_opt SAFEARRAY** patternIds,
      __RPC__deref_out_opt SAFEARRAY** patternNames) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE PollForPotentialSupportedProperties(
      __RPC__in_opt IUIAutomationElement* pElement, __RPC__deref_out_opt SAFEARRAY** propertyIds,
      __RPC__deref_out_opt SAFEARRAY** propertyNames) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CheckNotSupported(VARIANT value,
                                              __RPC__out BOOL* isNotSupported) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ReservedNotSupportedValue(__RPC__deref_out_opt IUnknown** notSupportedValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_ReservedMixedAttributeValue(__RPC__deref_out_opt IUnknown** mixedAttributeValue) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  ElementFromIAccessible(__RPC__in_opt IAccessible* accessible, int childId,
                         __RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  ElementFromIAccessibleBuildCache(__RPC__in_opt IAccessible* accessible, int childId,
                                   __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                                   __RPC__deref_out_opt IUIAutomationElement** element) override {
    return E_NOTIMPL;
  }
};

/// Mock `IUIAutomationCacheRequest`.
class MockUiCacheRequest : public ComMockBase<IUIAutomationCacheRequest> {
public:
  MOCK_METHOD(HRESULT, AddProperty, (PROPERTYID propertyId),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, AddPattern, (PATTERNID patternId), (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  Clone(__RPC__deref_out_opt IUIAutomationCacheRequest** clonedRequest) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_TreeScope(__RPC__out enum TreeScope* scope) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE put_TreeScope(enum TreeScope scope) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_TreeFilter(__RPC__deref_out_opt IUIAutomationCondition** filter) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE put_TreeFilter(__RPC__in_opt IUIAutomationCondition* filter) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_AutomationElementMode(__RPC__out enum AutomationElementMode* mode) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE put_AutomationElementMode(enum AutomationElementMode mode) override {
    return E_NOTIMPL;
  }
};

/// Mock `IUIAutomationElement`.
class MockUiElement : public ComMockBase<IUIAutomationElement> {
public:
  HRESULT STDMETHODCALLTYPE SetFocus() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetRuntimeId(__RPC__deref_out_opt SAFEARRAY** runtimeId) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE FindFirst(enum TreeScope scope,
                                      __RPC__in_opt IUIAutomationCondition* condition,
                                      __RPC__deref_out_opt IUIAutomationElement** found) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  FindAll(enum TreeScope scope, __RPC__in_opt IUIAutomationCondition* condition,
          __RPC__deref_out_opt IUIAutomationElementArray** found) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  FindFirstBuildCache(enum TreeScope scope, __RPC__in_opt IUIAutomationCondition* condition,
                      __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                      __RPC__deref_out_opt IUIAutomationElement** found) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  FindAllBuildCache(enum TreeScope scope, __RPC__in_opt IUIAutomationCondition* condition,
                    __RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                    __RPC__deref_out_opt IUIAutomationElementArray** found) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  BuildUpdatedCache(__RPC__in_opt IUIAutomationCacheRequest* cacheRequest,
                    __RPC__deref_out_opt IUIAutomationElement** updatedElement) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetCurrentPropertyValue(PROPERTYID propertyId,
                                                    __RPC__out VARIANT* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetCurrentPropertyValueEx(PROPERTYID propertyId,
                                                      BOOL ignoreDefaultValue,
                                                      __RPC__out VARIANT* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, GetCachedPropertyValue, (PROPERTYID propertyId, __RPC__out VARIANT* retVal),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE GetCachedPropertyValueEx(PROPERTYID propertyId, BOOL ignoreDefaultValue,
                                                     __RPC__out VARIANT* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetCurrentPatternAs(PATTERNID patternId, __RPC__in REFIID riid,
                      __RPC__deref_out_opt void** patternObject) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, GetCachedPatternAs,
              (PATTERNID patternId, __RPC__in REFIID riid,
               __RPC__deref_out_opt void** patternObject),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  GetCurrentPattern(PATTERNID patternId, __RPC__deref_out_opt IUnknown** patternObject) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetCachedPattern(PATTERNID patternId, __RPC__deref_out_opt IUnknown** patternObject) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetCachedParent(__RPC__deref_out_opt IUIAutomationElement** parent) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  GetCachedChildren(__RPC__deref_out_opt IUIAutomationElementArray** children) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentProcessId(__RPC__out int* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentControlType(__RPC__out CONTROLTYPEID* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentLocalizedControlType(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentName(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentAcceleratorKey(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentAccessKey(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentHasKeyboardFocus(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsKeyboardFocusable(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsEnabled(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentAutomationId(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentClassName(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentHelpText(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentCulture(__RPC__out int* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsControlElement(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsContentElement(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsPassword(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentNativeWindowHandle(__RPC__deref_out_opt UIA_HWND* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentItemType(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsOffscreen(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentOrientation(__RPC__out enum OrientationType* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentFrameworkId(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsRequiredForForm(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentItemStatus(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentBoundingRectangle(__RPC__out RECT* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentLabeledBy(__RPC__deref_out_opt IUIAutomationElement** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentAriaRole(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentAriaProperties(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsDataValidForForm(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentControllerFor(__RPC__deref_out_opt IUIAutomationElementArray** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentDescribedBy(__RPC__deref_out_opt IUIAutomationElementArray** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentFlowsTo(__RPC__deref_out_opt IUIAutomationElementArray** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentProviderDescription(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedProcessId(__RPC__out int* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedControlType, (__RPC__out CONTROLTYPEID * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  get_CachedLocalizedControlType(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedName, (__RPC__deref_out_opt BSTR * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE get_CachedAcceleratorKey(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedAccessKey(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedHasKeyboardFocus, (__RPC__out BOOL * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, get_CachedIsKeyboardFocusable, (__RPC__out BOOL * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, get_CachedIsEnabled, (__RPC__out BOOL * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE get_CachedAutomationId(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedClassName(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedHelpText(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedCulture(__RPC__out int* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedIsControlElement(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedIsContentElement(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedIsPassword(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedNativeWindowHandle(__RPC__deref_out_opt UIA_HWND* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedItemType(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedIsOffscreen(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedOrientation(__RPC__out enum OrientationType* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedFrameworkId(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedIsRequiredForForm(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedItemStatus(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedBoundingRectangle(__RPC__out RECT* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedLabeledBy(__RPC__deref_out_opt IUIAutomationElement** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedAriaRole(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedAriaProperties(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CachedIsDataValidForForm(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedControllerFor(__RPC__deref_out_opt IUIAutomationElementArray** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedDescribedBy(__RPC__deref_out_opt IUIAutomationElementArray** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedFlowsTo(__RPC__deref_out_opt IUIAutomationElementArray** retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CachedProviderDescription(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE GetClickablePoint(__RPC__out POINT* clickable,
                                              __RPC__out BOOL* gotClickable) override {
    return E_NOTIMPL;
  }
};

/// A default action for `MockUiElement::GetCachedPatternAs` that hands back the
/// per-pattern mock matching the requested id. Defined here so the COM `void**`
/// out-param stays in the mock header, not the test. Pass nullptr for a pattern
/// the element should report as absent.
inline auto patternDispatch(IUIAutomationTogglePattern* toggle,
                            IUIAutomationExpandCollapsePattern* expand,
                            IUIAutomationSelectionItemPattern* selection,
                            IUIAutomationValuePattern* value) {
  return [toggle, expand, selection, value](PATTERNID id, REFIID, void** out) {
    switch (id) {
    case UIA_TogglePatternId:
      *out = toggle;
      break;
    case UIA_ExpandCollapsePatternId:
      *out = expand;
      break;
    case UIA_SelectionItemPatternId:
      *out = selection;
      break;
    case UIA_ValuePatternId:
      *out = value;
      break;
    default:
      *out = nullptr;
      break;
    }
    return *out != nullptr ? S_OK : S_FALSE;
  };
}

/// Mock `IUIAutomationTogglePattern`.
class MockUiTogglePattern : public ComMockBase<IUIAutomationTogglePattern> {
public:
  HRESULT STDMETHODCALLTYPE Toggle() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentToggleState(__RPC__out enum ToggleState* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedToggleState, (__RPC__out enum ToggleState * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));
};

/// Mock `IUIAutomationExpandCollapsePattern`.
class MockUiExpandCollapsePattern : public ComMockBase<IUIAutomationExpandCollapsePattern> {
public:
  HRESULT STDMETHODCALLTYPE Expand() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Collapse() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentExpandCollapseState(__RPC__out enum ExpandCollapseState* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedExpandCollapseState,
              (__RPC__out enum ExpandCollapseState * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));
};

/// Mock `IUIAutomationSelectionItemPattern`.
class MockUiSelectionItemPattern : public ComMockBase<IUIAutomationSelectionItemPattern> {
public:
  HRESULT STDMETHODCALLTYPE Select() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE AddToSelection() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE RemoveFromSelection() override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsSelected(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  get_CurrentSelectionContainer(__RPC__deref_out_opt IUIAutomationElement** retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedIsSelected, (__RPC__out BOOL * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));

  HRESULT STDMETHODCALLTYPE
  get_CachedSelectionContainer(__RPC__deref_out_opt IUIAutomationElement** retVal) override {
    return E_NOTIMPL;
  }
};

/// Mock `IUIAutomationValuePattern`.
class MockUiValuePattern : public ComMockBase<IUIAutomationValuePattern> {
public:
  HRESULT STDMETHODCALLTYPE SetValue(__RPC__in BSTR val) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentValue(__RPC__deref_out_opt BSTR* retVal) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE get_CurrentIsReadOnly(__RPC__out BOOL* retVal) override {
    return E_NOTIMPL;
  }

  MOCK_METHOD(HRESULT, get_CachedValue, (__RPC__deref_out_opt BSTR * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT, get_CachedIsReadOnly, (__RPC__out BOOL * retVal),
              (override, Calltype(STDMETHODCALLTYPE)));
};

} // namespace vox::provider::testing

#  pragma warning(pop)

#endif // defined(_WIN32)

#endif // VOX_TESTS_PROVIDER_UIA_COM_MOCKS_HPP
