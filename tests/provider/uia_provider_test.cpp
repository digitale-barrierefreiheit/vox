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
using vox::provider::testing::MockUiCondition;
using vox::provider::testing::MockUiElement;
using vox::provider::testing::MockUiExpandCollapsePattern;
using vox::provider::testing::MockUiSelectionItemPattern;
using vox::provider::testing::MockUiTogglePattern;
using vox::provider::testing::MockUiValuePattern;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr long ErrorFail = static_cast<long>(0x80004005U); // E_FAIL

// A non-null but otherwise-bogus window handle for nodeByName tests; the mocked
// ElementFromHandle ignores its value, so any non-null HWND suffices.
HWND fakeWindow() {
  static int marker = 0;
  return static_cast<HWND>(static_cast<void*>(&marker));
}

// Routes ElementFromHandle on @p automation to @p window (the subtree root these tests read).
void routeElementFromHandle(MockUiAutomation& automation, MockUiElement& window) {
  ON_CALL(automation, ElementFromHandle(_, _))
      .WillByDefault([&window](UIA_HWND, IUIAutomationElement** out) {
        *out = &window;
        return S_OK;
      });
}

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

  /// A GetCachedPropertyValue action yielding the MSAA state bits @p state for the legacy
  /// State property and the BSTR @p text for the legacy Value property (a fresh BSTR per
  /// call, freed by the provider via VariantClear), and an empty VARIANT for anything else.
  static auto legacyStateAndValue(unsigned state, const wchar_t* text) {
    return [state, text](PROPERTYID id, VARIANT* out) {
      ::VariantInit(out);
      if (id == UIA_LegacyIAccessibleStatePropertyId) {
        out->vt = VT_I4;
        out->lVal = static_cast<LONG>(state);
      } else if (id == UIA_LegacyIAccessibleValuePropertyId) {
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

// Standard Win32 controls reach UIA through the MSAA bridge, which exposes state and value
// as the legacy IAccessible *properties* rather than the modern patterns. With those patterns
// absent, the provider reads both legacy properties: a read-only edit reports ReadOnly (from
// the legacy state bits) and its text value.
TEST_F(UiaProviderTest, ReadsLegacyStateAndValuePropertiesWhenModernPatternsAbsent) {
  ON_CALL(element_, get_CachedControlType(_)).WillByDefault(setControlType(UIA_EditControlTypeId));
  ON_CALL(element_, GetCachedPatternAs(_, _, _))
      .WillByDefault(vox::provider::testing::patternDispatch(nullptr, nullptr, nullptr, nullptr));
  ON_CALL(element_, GetCachedPropertyValue(_, _))
      .WillByDefault(legacyStateAndValue(vox::provider::UiaLegacyStateReadOnly, L"Hallo"));
  const AccessibleNode node = focusedNodeOrFail();
  EXPECT_TRUE(node.states.test(vox::model::State::ReadOnly));
  EXPECT_EQ(node.value.value_or(""), "Hallo");
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

// nodeByName reads a non-focusable element by name: ElementFromHandle(window) ->
// FindFirstBuildCache(Name == name) -> extract -> map (here a static text "Hinweis").
TEST_F(UiaProviderTest, NodeByNameReadsTheNamedElementInTheWindowSubtree) {
  NiceMock<MockUiElement> windowElement;
  NiceMock<MockUiElement> found;
  NiceMock<MockUiCondition> condition;
  routeElementFromHandle(automation_, windowElement);
  ON_CALL(automation_, CreatePropertyCondition(_, _, _))
      .WillByDefault([&condition](PROPERTYID, VARIANT, IUIAutomationCondition** out) {
        *out = &condition;
        return S_OK;
      });
  ON_CALL(windowElement, FindFirstBuildCache(_, _, _, _))
      .WillByDefault([&found](enum TreeScope, IUIAutomationCondition*, IUIAutomationCacheRequest*,
                              IUIAutomationElement** out) {
        *out = &found;
        return S_OK;
      });
  ON_CALL(found, get_CachedControlType(_)).WillByDefault([](CONTROLTYPEID* out) {
    *out = UIA_TextControlTypeId;
    return S_OK;
  });
  ON_CALL(found, get_CachedName(_)).WillByDefault([](BSTR* out) {
    *out = bstr(L"Hinweis");
    return S_OK;
  });
  ON_CALL(found, get_CachedIsEnabled(_)).WillByDefault(setBool(TRUE));

  const UiaProvider provider;
  const std::optional<AccessibleNode> node = provider.nodeByName(fakeWindow(), "Hinweis");
  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->role, vox::model::Role::StaticText);
  EXPECT_EQ(node->name, "Hinweis");
}

// A degraded nodeByName: if the named element is not found, returns nullopt.
TEST_F(UiaProviderTest, NodeByNameIsNulloptWhenNotFound) {
  routeElementFromHandle(automation_, element_);
  ON_CALL(automation_, CreatePropertyCondition(_, _, _))
      .WillByDefault([](PROPERTYID, VARIANT, IUIAutomationCondition** out) {
        *out = nullptr;
        return ErrorFail; // condition creation fails -> nodeByName degrades
      });
  const UiaProvider provider;
  EXPECT_FALSE(provider.nodeByName(fakeWindow(), "Fehlt").has_value());
}

TEST_F(UiaProviderTest, NodeByNameIsNulloptForNullHandle) {
  const UiaProvider provider;
  EXPECT_FALSE(provider.nodeByName(nullptr, "X").has_value());
}

TEST_F(UiaProviderTest, NodeByNameIsNulloptWhenElementFromHandleFails) {
  ON_CALL(automation_, ElementFromHandle(_, _))
      .WillByDefault([](UIA_HWND, IUIAutomationElement** out) {
        *out = nullptr;
        return ErrorFail;
      });
  const UiaProvider provider;
  EXPECT_FALSE(provider.nodeByName(fakeWindow(), "X").has_value());
}

TEST_F(UiaProviderTest, NodeByNameIsNulloptWhenFindFirstFails) {
  NiceMock<MockUiElement> windowElement;
  NiceMock<MockUiCondition> condition;
  routeElementFromHandle(automation_, windowElement);
  ON_CALL(automation_, CreatePropertyCondition(_, _, _))
      .WillByDefault([&condition](PROPERTYID, VARIANT, IUIAutomationCondition** out) {
        *out = &condition;
        return S_OK;
      });
  ON_CALL(windowElement, FindFirstBuildCache(_, _, _, _))
      .WillByDefault([](enum TreeScope, IUIAutomationCondition*, IUIAutomationCacheRequest*,
                        IUIAutomationElement** out) {
        *out = nullptr;
        return ErrorFail;
      });
  const UiaProvider provider;
  EXPECT_FALSE(provider.nodeByName(fakeWindow(), "X").has_value());
}

// An invalid-UTF-8 name converts to empty, so makeNameCondition refuses to search for it.
TEST_F(UiaProviderTest, NodeByNameIsNulloptForInvalidUtf8Name) {
  NiceMock<MockUiElement> windowElement;
  routeElementFromHandle(automation_, windowElement);
  const UiaProvider provider;
  EXPECT_FALSE(provider.nodeByName(fakeWindow(), "\xC3").has_value()); // lone UTF-8 lead byte
}

// An empty name likewise converts to empty, exercising fromUtf8's empty-input path.
TEST_F(UiaProviderTest, NodeByNameIsNulloptForEmptyName) {
  NiceMock<MockUiElement> windowElement;
  routeElementFromHandle(automation_, windowElement);
  const UiaProvider provider;
  EXPECT_FALSE(provider.nodeByName(fakeWindow(), "").has_value());
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

// --- the #60 contract: stop() silences unconditionally, start() always resumes ---

TEST_F(UiaProviderTest, StopSilencesTheCallbackEvenWhenRemovalFails) {
  UiaProvider provider;
  bool called = false;
  provider.start([&called](const AccessibleNode&) { called = true; });
  ASSERT_NE(capturedHandler_, nullptr);
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillRepeatedly(Return(ErrorFail));
  provider.stop();
  // UIA still holds the handler and may keep invoking it — the detached sink
  // must swallow the event instead of forwarding it.
  EXPECT_EQ(capturedHandler_->HandleFocusChangedEvent(&element_), S_OK);
  EXPECT_FALSE(called);
}

TEST_F(UiaProviderTest, StartAfterAFailedRemovalRegistersAFreshHandler) {
  UiaProvider provider;
  provider.start([](const AccessibleNode&) { /* the soon-to-be-stuck handler */ });
  IUIAutomationFocusChangedEventHandler* stuck = capturedHandler_;
  ASSERT_NE(stuck, nullptr);
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillRepeatedly(Return(ErrorFail));
  provider.stop();

  // The stuck handler is shelved, not blocking: a new start() must register a
  // fresh handler and deliver events to the new callback (no silent-dead state).
  bool called = false;
  provider.start([&called](const AccessibleNode&) { called = true; });
  ASSERT_NE(capturedHandler_, nullptr);
  EXPECT_NE(capturedHandler_, stuck);
  EXPECT_EQ(capturedHandler_->HandleFocusChangedEvent(&element_), S_OK);
  EXPECT_TRUE(called);
}

TEST_F(UiaProviderTest, ALaterStopRetriesAndReleasesTheShelvedHandler) {
  UiaProvider provider;
  provider.start([](const AccessibleNode&) { /* this test only checks the retry */ });
  // First stop: removal fails, the handler is shelved.
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillOnce(Return(ErrorFail));
  provider.stop();
  // Next stop: the retry succeeds and the shelf empties — so the destructor
  // has nothing left to escalate.
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillOnce(Return(S_OK));
  provider.stop();
  EXPECT_CALL(automation_, RemoveAllEventHandlers()).Times(0);
}

TEST_F(UiaProviderTest, DestructionEscalatesToRemoveAllWhenRemovalKeepsFailing) {
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillRepeatedly(Return(ErrorFail));
  EXPECT_CALL(automation_, RemoveAllEventHandlers()).WillOnce(Return(S_OK));
  {
    UiaProvider provider;
    provider.start([](const AccessibleNode&) { /* this test only checks teardown */ });
    provider.stop(); // shelves the handler (removal keeps failing)
  } // the destructor retries, then escalates to RemoveAllEventHandlers
}

TEST_F(UiaProviderTest, DestructionEscalatesWhenTheActiveHandlerCannotBeRemoved) {
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillRepeatedly(Return(ErrorFail));
  EXPECT_CALL(automation_, RemoveAllEventHandlers()).WillOnce(Return(S_OK));
  {
    UiaProvider provider;
    provider.start([](const AccessibleNode&) { /* destroyed while still registered */ });
  } // no stop(): teardown detaches, fails the removal, and escalates directly
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
