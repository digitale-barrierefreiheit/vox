// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for UiaProvider's COM extraction and focus-event handling,
///        driven through the #68 test seam with mock COM — no real UI Automation
///        tree required, so these run anywhere (incl. the coverage job) and
///        fault-inject the degraded paths.
#if defined(_WIN32)

#  include <optional>
#  include <stdexcept>
#  include <string>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/model/accessible_node.hpp>
#  include <vox/model/state.hpp>
#  include <vox/provider/uia_ids.hpp>
#  include <vox/provider/uia_provider.hpp>
#  include <vox/provider/uia_test_seam.hpp>

#  include "uia_com_mocks.hpp"

namespace {

using vox::model::AccessibleNode;
using vox::provider::UiaProvider;
using vox::provider::testing::bstr;
using vox::provider::testing::MockUiAutomation;
using vox::provider::testing::MockUiCacheRequest;
using vox::provider::testing::MockUiElement;
using vox::provider::testing::MockUiExpandCollapsePattern;
using vox::provider::testing::MockUiSelectionItemPattern;
using vox::provider::testing::MockUiTogglePattern;
using vox::provider::testing::MockUiValuePattern;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr long ErrorFail = static_cast<long>(0x80004005U); // E_FAIL

/// A dedicated exception a focus callback might raise (S112: not a generic one),
/// to prove the event handler firewalls it at the COM boundary.
class CallbackError : public std::runtime_error {
public:
  CallbackError() : std::runtime_error("callback boom") {}
};

class SeamGuard {
public:
  SeamGuard() = default;

  ~SeamGuard() {
    vox::provider::testing::setAutomationFactory({});
  }

  SeamGuard(const SeamGuard&) = delete;
  SeamGuard& operator=(const SeamGuard&) = delete;
  SeamGuard(SeamGuard&&) = delete;
  SeamGuard& operator=(SeamGuard&&) = delete;
};

/// A failing factory: the provider cannot create the automation client and must
/// degrade to "no focused element" rather than crash.
TEST(UiaProviderDegraded, FocusedElementIsNulloptWhenAutomationCreationFails) {
  [[maybe_unused]] const SeamGuard guard;
  vox::provider::testing::setAutomationFactory([](struct IUIAutomation** out) {
    *out = nullptr;
    return ErrorFail;
  });

  const UiaProvider provider;
  EXPECT_FALSE(provider.focusedElement().has_value());
}

/// Fixture wiring the full mock UIA chain: the seam hands back `automation_`,
/// whose CreateCacheRequest yields `cache_`, GetFocusedElementBuildCache yields
/// `element_`, and GetCachedPatternAs yields the per-pattern mocks. The happy
/// defaults describe an enabled, focused button named "OK" with a value of
/// "hello"; each test then breaks one link.
class UiaProviderTest : public ::testing::Test {
protected:
  void SetUp() override {
    vox::provider::testing::setAutomationFactory([this](IUIAutomation** out) {
      *out = &automation_;
      return 0L; // S_OK
    });
    installHappyChain();
  }

  void TearDown() override {
    vox::provider::testing::setAutomationFactory({});
  }

  void installHappyChain() {
    installAutomationDefaults();
    installElementDefaults();
    installPatternDefaults();
  }

  /// The automation client: cache request, focused element, event registration.
  void installAutomationDefaults() {
    ON_CALL(automation_, CreateCacheRequest(_))
        .WillByDefault([this](IUIAutomationCacheRequest** out) {
          *out = &cache_;
          return S_OK;
        });
    ON_CALL(cache_, AddProperty(_)).WillByDefault(Return(S_OK));
    ON_CALL(cache_, AddPattern(_)).WillByDefault(Return(S_OK));
    ON_CALL(automation_, GetFocusedElementBuildCache(_, _))
        .WillByDefault([this](IUIAutomationCacheRequest*, IUIAutomationElement** out) {
          *out = &element_;
          return S_OK;
        });
    ON_CALL(automation_, AddFocusChangedEventHandler(_, _))
        .WillByDefault(
            [this](IUIAutomationCacheRequest*, IUIAutomationFocusChangedEventHandler* h) {
              capturedHandler_ = h;
              return S_OK;
            });
    ON_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillByDefault(Return(S_OK));
  }

  /// The focused element: an enabled, focused button named "OK", and the pattern
  /// lookup that hands back the per-pattern mocks.
  void installElementDefaults() {
    ON_CALL(element_, get_CachedControlType(_)).WillByDefault([](CONTROLTYPEID* out) {
      *out = UIA_ButtonControlTypeId;
      return S_OK;
    });
    ON_CALL(element_, get_CachedName(_)).WillByDefault([](BSTR* out) {
      *out = bstr(L"OK");
      return S_OK;
    });
    ON_CALL(element_, get_CachedIsEnabled(_)).WillByDefault(setBool(TRUE));
    ON_CALL(element_, get_CachedHasKeyboardFocus(_)).WillByDefault(setBool(TRUE));
    ON_CALL(element_, get_CachedIsKeyboardFocusable(_)).WillByDefault(setBool(TRUE));
    // The dispatch helper (in the mock header) maps each id to its pattern mock,
    // keeping the COM void** out-param out of this test.
    ON_CALL(element_, GetCachedPatternAs(_, _, _))
        .WillByDefault(
            vox::provider::testing::patternDispatch(&toggle_, &expand_, &selection_, &value_));
  }

  /// The four supported patterns: toggled-on, expanded, selected, value "hello".
  void installPatternDefaults() {
    ON_CALL(toggle_, get_CachedToggleState(_)).WillByDefault([](ToggleState* out) {
      *out = ToggleState_On;
      return S_OK;
    });
    ON_CALL(expand_, get_CachedExpandCollapseState(_)).WillByDefault([](ExpandCollapseState* out) {
      *out = ExpandCollapseState_Expanded;
      return S_OK;
    });
    ON_CALL(selection_, get_CachedIsSelected(_)).WillByDefault(setBool(TRUE));
    ON_CALL(value_, get_CachedValue(_)).WillByDefault([](BSTR* out) {
      *out = bstr(L"hello");
      return S_OK;
    });
    ON_CALL(value_, get_CachedIsReadOnly(_)).WillByDefault(setBool(FALSE));
    // No legacy IAccessible state/value by default (the modern patterns above cover this
    // happy-path button); an empty VARIANT means "absent". The fallback tests override this.
    ON_CALL(element_, GetCachedPropertyValue(_, _)).WillByDefault([](PROPERTYID, VARIANT* out) {
      ::VariantInit(out);
      return S_OK;
    });
  }

  /// A default action that writes @p value into a `BOOL*` out-param and succeeds.
  static auto setBool(BOOL value) {
    return [value](BOOL* out) {
      *out = value;
      return S_OK;
    };
  }

  /// A get_CachedControlType action yielding @p controlType.
  static auto setControlType(CONTROLTYPEID controlType) {
    return [controlType](CONTROLTYPEID* out) {
      *out = controlType;
      return S_OK;
    };
  }

  /// A GetCachedPropertyValue action: the MSAA state bits @p state for the legacy State
  /// property, else empty.
  static auto legacyState(unsigned state) {
    return [state](PROPERTYID id, VARIANT* out) {
      ::VariantInit(out);
      if (id == UIA_LegacyIAccessibleStatePropertyId) {
        out->vt = VT_I4;
        out->lVal = static_cast<LONG>(state);
      }
      return S_OK;
    };
  }

  /// A GetCachedPropertyValue action: a BSTR @p text for the legacy Value property, else empty.
  /// A fresh BSTR is allocated per call, which the provider frees via VariantClear.
  static auto legacyValue(const wchar_t* text) {
    return [text](PROPERTYID id, VARIANT* out) {
      ::VariantInit(out);
      if (id == UIA_LegacyIAccessibleValuePropertyId) {
        out->vt = VT_BSTR;
        out->bstrVal = bstr(text);
      }
      return S_OK;
    };
  }

  /// Reads the focused node through a fresh provider (via the installed seam), asserting
  /// one was returned; yields a default node on failure so callers can still assert fields.
  AccessibleNode focusedNodeOrFail() {
    const UiaProvider provider;
    const std::optional<AccessibleNode> node = provider.focusedElement();
    EXPECT_TRUE(node.has_value());
    return node.value_or(AccessibleNode{});
  }

  IUIAutomationFocusChangedEventHandler* capturedHandler_{nullptr};
  NiceMock<MockUiAutomation> automation_;
  NiceMock<MockUiCacheRequest> cache_;
  NiceMock<MockUiElement> element_;
  NiceMock<MockUiTogglePattern> toggle_;
  NiceMock<MockUiExpandCollapsePattern> expand_;
  NiceMock<MockUiSelectionItemPattern> selection_;
  NiceMock<MockUiValuePattern> value_;
};

TEST_F(UiaProviderTest, FocusedElementExtractsNameAndValue) {
  const UiaProvider provider;
  const std::optional<AccessibleNode> node = provider.focusedElement();
  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->name, "OK");
  ASSERT_TRUE(node->value.has_value());
  EXPECT_EQ(*node->value, "hello");
}

TEST_F(UiaProviderTest, FocusedElementIsNulloptWhenNothingIsFocused) {
  EXPECT_CALL(automation_, GetFocusedElementBuildCache(_, _)).WillOnce(Return(ErrorFail));
  const UiaProvider provider;
  EXPECT_FALSE(provider.focusedElement().has_value());
}

TEST_F(UiaProviderTest, DegradesWhenCacheRequestCreationFails) {
  EXPECT_CALL(automation_, CreateCacheRequest(_)).WillOnce(Return(ErrorFail));
  const UiaProvider provider;
  EXPECT_FALSE(provider.focusedElement().has_value());
}

TEST_F(UiaProviderTest, DegradesWhenAddingACachePropertyFails) {
  EXPECT_CALL(cache_, AddProperty(_)).WillOnce(Return(ErrorFail)).WillRepeatedly(Return(S_OK));
  const UiaProvider provider;
  EXPECT_FALSE(provider.focusedElement().has_value());
}

TEST_F(UiaProviderTest, ValueIsAbsentWhenTheValueReadFails) {
  EXPECT_CALL(value_, get_CachedValue(_)).WillRepeatedly(Return(ErrorFail));
  const UiaProvider provider;
  const std::optional<AccessibleNode> node = provider.focusedElement();
  ASSERT_TRUE(node.has_value());
  EXPECT_FALSE(node->value.has_value());
}

// Standard Win32 controls reach UIA through the MSAA bridge, which exposes state via the
// legacy IAccessible *properties*, not Toggle. With no Toggle pattern, Checked comes thence.
TEST_F(UiaProviderTest, ReadsCheckedFromLegacyStatePropertyWhenTogglePatternAbsent) {
  ON_CALL(element_, get_CachedControlType(_))
      .WillByDefault(setControlType(UIA_CheckBoxControlTypeId));
  ON_CALL(element_, GetCachedPatternAs(_, _, _))
      .WillByDefault(
          vox::provider::testing::patternDispatch(nullptr, &expand_, &selection_, nullptr));
  ON_CALL(element_, GetCachedPropertyValue(_, _))
      .WillByDefault(legacyState(vox::provider::UiaLegacyStateChecked));
  EXPECT_TRUE(focusedNodeOrFail().states.test(vox::model::State::Checked));
}

// With no Value pattern, an edit's value comes from the legacy IAccessible value property.
TEST_F(UiaProviderTest, ReadsValueFromLegacyValuePropertyWhenValuePatternAbsent) {
  ON_CALL(element_, get_CachedControlType(_)).WillByDefault(setControlType(UIA_EditControlTypeId));
  ON_CALL(element_, GetCachedPatternAs(_, _, _))
      .WillByDefault(
          vox::provider::testing::patternDispatch(&toggle_, &expand_, &selection_, nullptr));
  ON_CALL(element_, GetCachedPropertyValue(_, _)).WillByDefault(legacyValue(L"Hallo"));
  EXPECT_EQ(focusedNodeOrFail().value.value_or(""), "Hallo");
}

TEST_F(UiaProviderTest, StartForwardsFocusEventsToTheCallback) {
  UiaProvider provider;
  AccessibleNode received;
  bool called = false;
  provider.start([&received, &called](const AccessibleNode& node) {
    received = node;
    called = true;
  });

  ASSERT_NE(capturedHandler_, nullptr);
  EXPECT_EQ(capturedHandler_->HandleFocusChangedEvent(&element_), S_OK);
  EXPECT_TRUE(called);
  EXPECT_EQ(received.name, "OK");
}

TEST_F(UiaProviderTest, StopRemovesTheRegisteredHandler) {
  UiaProvider provider;
  provider.start([](const AccessibleNode&) { /* no-op: this test only checks stop() */ });
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillOnce(Return(S_OK));
  provider.stop();
}

TEST_F(UiaProviderTest, NameIsEmptyWhenTheNameBstrIsNull) {
  ON_CALL(element_, get_CachedName(_)).WillByDefault([](BSTR* out) {
    *out = nullptr; // null BSTR -> toUtf8 yields empty
    return S_OK;
  });
  const UiaProvider provider;
  const std::optional<AccessibleNode> node = provider.focusedElement();
  ASSERT_TRUE(node.has_value());
  EXPECT_TRUE(node->name.empty());
}

TEST_F(UiaProviderTest, NameIsEmptyWhenTheNameBstrIsEmpty) {
  ON_CALL(element_, get_CachedName(_)).WillByDefault([](BSTR* out) {
    *out = bstr(L""); // zero-length BSTR -> SysStringLen 0 -> empty
    return S_OK;
  });
  const UiaProvider provider;
  const std::optional<AccessibleNode> node = provider.focusedElement();
  ASSERT_TRUE(node.has_value());
  EXPECT_TRUE(node->name.empty());
}

TEST_F(UiaProviderTest, FocusEventCallbackExceptionsAreSwallowed) {
  UiaProvider provider;
  provider.start([](const AccessibleNode&) { throw CallbackError{}; });
  ASSERT_NE(capturedHandler_, nullptr);
  // The handler must never let an exception cross the COM ABI boundary.
  EXPECT_EQ(capturedHandler_->HandleFocusChangedEvent(&element_), S_OK);
}

TEST_F(UiaProviderTest, StartDropsTheHandlerWhenRegistrationFails) {
  EXPECT_CALL(automation_, AddFocusChangedEventHandler(_, _)).WillOnce(Return(ErrorFail));
  UiaProvider provider;
  provider.start([](const AccessibleNode&) { /* this test only checks registration */ });
  // Registration failed, so there is nothing for stop() to remove.
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).Times(0);
  provider.stop();
}

TEST_F(UiaProviderTest, StopKeepsTheHandlerWhenRemovalFailsAndStartStaysIdempotent) {
  UiaProvider provider;
  provider.start([](const AccessibleNode&) { /* this test only checks stop()/start() */ });
  // Removal fails: UIA may still hold the handler, so the provider keeps its
  // reference rather than dropping a still-registered handler.
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillRepeatedly(Return(ErrorFail));
  provider.stop();
  // A subsequent start() sees the retained handler and does not double-register.
  EXPECT_CALL(automation_, AddFocusChangedEventHandler(_, _)).Times(0);
  provider.start([](const AccessibleNode&) { /* retained handler path */ });
}

/// A degraded provider (automation creation failed) accepts start()/stop() as
/// safe no-ops rather than crashing.
TEST(UiaProviderDegraded, StartAndStopAreNoOpsWhenAutomationIsUnavailable) {
  [[maybe_unused]] const SeamGuard guard;
  vox::provider::testing::setAutomationFactory([](struct IUIAutomation** out) {
    *out = nullptr;
    return ErrorFail;
  });
  UiaProvider provider;
  bool called = false;
  provider.start([&called](const AccessibleNode&) { called = true; });
  provider.stop();
  EXPECT_FALSE(called);
}

} // namespace

#endif // defined(_WIN32)
