// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Windows UI Automation implementation of vox::provider::UiaProvider.
///
/// Thin COM extraction only: every cached value is pulled into a plain
/// `UiaElementData` and handed to the pure `mapElement()`. All HRESULTs are
/// checked so a missing property/pattern degrades to "absent" rather than
/// crashing.
#include <optional>
#include <string>
#include <utility>

#include <vox/provider/uia_provider.hpp>

// The Windows UIA headers are include-order sensitive (windows.h must lead), so
// this block is exempt from clang-format's include sorting.
// clang-format off
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <uiautomation.h>
#pragma warning(push)
#pragma warning(disable : 4265)  // WRL FtmBase: non-virtual dtor in a system header
#include <wrl/client.h>
#include <wrl/implements.h>
#pragma warning(pop)
// clang-format on

#include <vox/model/accessible_node.hpp>
#include <vox/provider/mapper.hpp>
#include <vox/provider/uia_element_data.hpp>

namespace vox::provider {

namespace {

using Microsoft::WRL::ComPtr;

/// Converts a UTF-16 BSTR to a UTF-8 std::string (empty for null/empty input).
std::string toUtf8(BSTR text) {
  if (text == nullptr) {
    return {};
  }
  const int length = static_cast<int>(::SysStringLen(text));
  if (length == 0) {
    return {};
  }
  const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string out(static_cast<std::size_t>(bytes), '\0');
  const int written =
      ::WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), bytes, nullptr, nullptr);
  if (written != bytes) {
    return {}; // conversion failed — degrade to empty rather than return filler
  }
  return out;
}

/// Pulls the cached properties/patterns of @p element into a plain snapshot.
UiaElementData extract(IUIAutomationElement* element) {
  UiaElementData data;
  if (element == nullptr) {
    return data;
  }

  CONTROLTYPEID controlType = 0;
  if (SUCCEEDED(element->get_CachedControlType(&controlType))) {
    data.controlTypeId = controlType;
  }

  BSTR name = nullptr;
  if (SUCCEEDED(element->get_CachedName(&name)) && name != nullptr) {
    data.name = toUtf8(name);
    ::SysFreeString(name);
  }

  BOOL flag = FALSE;
  if (SUCCEEDED(element->get_CachedIsEnabled(&flag))) {
    data.isEnabled = flag != FALSE;
  }
  if (SUCCEEDED(element->get_CachedHasKeyboardFocus(&flag))) {
    data.hasKeyboardFocus = flag != FALSE;
  }
  if (SUCCEEDED(element->get_CachedIsKeyboardFocusable(&flag))) {
    data.isKeyboardFocusable = flag != FALSE;
  }

  ComPtr<IUIAutomationTogglePattern> toggle;
  if (SUCCEEDED(element->GetCachedPatternAs(UIA_TogglePatternId, IID_PPV_ARGS(&toggle))) &&
      toggle) {
    ToggleState state = ToggleState_Off;
    if (SUCCEEDED(toggle->get_CachedToggleState(&state))) {
      data.hasToggle = true;
      data.toggleState = state;
    }
  }

  ComPtr<IUIAutomationExpandCollapsePattern> expand;
  if (SUCCEEDED(element->GetCachedPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&expand))) &&
      expand) {
    ExpandCollapseState state = ExpandCollapseState_Collapsed;
    if (SUCCEEDED(expand->get_CachedExpandCollapseState(&state))) {
      data.hasExpandCollapse = true;
      data.expandCollapseState = state;
    }
  }

  ComPtr<IUIAutomationSelectionItemPattern> selection;
  if (SUCCEEDED(
          element->GetCachedPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(&selection))) &&
      selection) {
    BOOL selected = FALSE;
    if (SUCCEEDED(selection->get_CachedIsSelected(&selected))) {
      data.hasSelectionItem = true;
      data.isSelected = selected != FALSE;
    }
  }

  ComPtr<IUIAutomationValuePattern> value;
  if (SUCCEEDED(element->GetCachedPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value))) && value) {
    // The pattern's presence (and its read-only-ness) is independent of whether
    // the value text reads — capture it regardless.
    data.hasValuePattern = true;
    BOOL readOnly = FALSE;
    if (SUCCEEDED(value->get_CachedIsReadOnly(&readOnly))) {
      data.isReadOnly = readOnly != FALSE;
    }
    // Mark the value *present* only once it actually reads, so a failed read
    // stays "absent" instead of collapsing to a spurious empty value. A genuine
    // empty BSTR (or null) is a real empty value and is kept.
    BSTR text = nullptr;
    if (SUCCEEDED(value->get_CachedValue(&text))) {
      data.hasValue = true;
      data.value = toUtf8(text); // toUtf8 treats null as ""
      ::SysFreeString(text);
    }
  }

  return data;
}

/// COM event sink that forwards UIA focus changes to a std::function.
class FocusEventHandler : public Microsoft::WRL::RuntimeClass<
                              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                              IUIAutomationFocusChangedEventHandler> {
public:
  explicit FocusEventHandler(IProvider::FocusChangedCallback callback)
      : callback_(std::move(callback)) {}

  HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) override {
    // An exception must never escape across the COM ABI boundary (it could
    // terminate the process), so the callback is invoked inside a catch-all.
    try {
      if (sender != nullptr && callback_) {
        callback_(mapElement(extract(sender)));
      }
    } catch (...) { // NOLINT(bugprone-empty-catch) — intentional ABI firewall
    }
    return S_OK;
  }

private:
  IProvider::FocusChangedCallback callback_;
};

} // namespace

class UiaProvider::Impl {
public:
  Impl() {
    comInitialized_ = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    if (FAILED(::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&automation_)))) {
      automation_.Reset();
      return;
    }
    if (FAILED(automation_->CreateCacheRequest(&cacheRequest_)) || !cacheRequest_) {
      cacheRequest_.Reset();
      return;
    }
    bool added = true;
    added &= SUCCEEDED(cacheRequest_->AddProperty(UIA_NamePropertyId));
    added &= SUCCEEDED(cacheRequest_->AddProperty(UIA_ControlTypePropertyId));
    added &= SUCCEEDED(cacheRequest_->AddProperty(UIA_IsEnabledPropertyId));
    added &= SUCCEEDED(cacheRequest_->AddProperty(UIA_HasKeyboardFocusPropertyId));
    added &= SUCCEEDED(cacheRequest_->AddProperty(UIA_IsKeyboardFocusablePropertyId));
    added &= SUCCEEDED(cacheRequest_->AddPattern(UIA_TogglePatternId));
    added &= SUCCEEDED(cacheRequest_->AddPattern(UIA_ExpandCollapsePatternId));
    added &= SUCCEEDED(cacheRequest_->AddPattern(UIA_SelectionItemPatternId));
    added &= SUCCEEDED(cacheRequest_->AddPattern(UIA_ValuePatternId));
    if (!added) {
      // Don't run with a partially populated cache request (that would look like
      // spurious "missing properties"); degrade to no reads instead.
      cacheRequest_.Reset();
    }
  }

  ~Impl() {
    stop();
    cacheRequest_.Reset();
    automation_.Reset();
    if (comInitialized_) {
      ::CoUninitialize();
    }
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  [[nodiscard]] std::optional<vox::model::AccessibleNode> focusedElement() const {
    if (!automation_ || !cacheRequest_) {
      return std::nullopt;
    }
    ComPtr<IUIAutomationElement> element;
    if (FAILED(automation_->GetFocusedElementBuildCache(cacheRequest_.Get(), &element)) ||
        !element) {
      return std::nullopt;
    }
    return mapElement(extract(element.Get()));
  }

  void start(IProvider::FocusChangedCallback onFocusChanged) {
    if (!automation_ || !cacheRequest_) {
      return;
    }
    stop();
    if (handler_) {
      // A previous handler could not be unregistered; don't overwrite it (that
      // would drop the last reference to one still registered with UIA).
      return;
    }
    handler_ = Microsoft::WRL::Make<FocusEventHandler>(std::move(onFocusChanged));
    if (FAILED(automation_->AddFocusChangedEventHandler(cacheRequest_.Get(), handler_.Get()))) {
      handler_.Reset(); // registration failed — nothing for stop() to remove
    }
  }

  void stop() {
    if (!handler_) {
      return;
    }
    if (automation_ && FAILED(automation_->RemoveFocusChangedEventHandler(handler_.Get()))) {
      // Removal failed: UIA may still hold and invoke the handler, so keep our
      // reference (a later stop()/destruction retries) rather than drop it while
      // it is still registered.
      return;
    }
    handler_.Reset();
  }

private:
  bool comInitialized_ = false;
  ComPtr<IUIAutomation> automation_;
  ComPtr<IUIAutomationCacheRequest> cacheRequest_;
  ComPtr<IUIAutomationFocusChangedEventHandler> handler_;
};

UiaProvider::UiaProvider() : impl_(std::make_unique<Impl>()) {}

UiaProvider::~UiaProvider() = default;

std::optional<vox::model::AccessibleNode> UiaProvider::focusedElement() const {
  return impl_->focusedElement();
}

void UiaProvider::start(FocusChangedCallback onFocusChanged) {
  impl_->start(std::move(onFocusChanged));
}

void UiaProvider::stop() {
  impl_->stop();
}

} // namespace vox::provider
