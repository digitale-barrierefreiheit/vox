// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for UiaProvider's COM extraction and focus-event handling,
///        driven through the #68 test seam with mock COM — no real UI Automation
///        tree required, so these run anywhere (incl. the coverage job) and
///        fault-inject the degraded paths.
#if defined(_WIN32)

#  include <optional>
#  include <string>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/model/accessible_node.hpp>
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
  const SeamGuard guard;
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

    ON_CALL(element_, get_CachedControlType(_)).WillByDefault([](CONTROLTYPEID* out) {
      *out = UIA_ButtonControlTypeId;
      return S_OK;
    });
    ON_CALL(element_, get_CachedName(_)).WillByDefault([](BSTR* out) {
      *out = bstr(L"OK");
      return S_OK;
    });
    ON_CALL(element_, get_CachedIsEnabled(_)).WillByDefault([](BOOL* out) {
      *out = TRUE;
      return S_OK;
    });
    ON_CALL(element_, get_CachedHasKeyboardFocus(_)).WillByDefault([](BOOL* out) {
      *out = TRUE;
      return S_OK;
    });
    ON_CALL(element_, get_CachedIsKeyboardFocusable(_)).WillByDefault([](BOOL* out) {
      *out = TRUE;
      return S_OK;
    });
    ON_CALL(element_, GetCachedPatternAs(_, _, _))
        .WillByDefault([this](PATTERNID id, REFIID, void** out) {
          *out = nullptr;
          switch (id) {
          case UIA_TogglePatternId:
            *out = static_cast<IUIAutomationTogglePattern*>(&toggle_);
            return S_OK;
          case UIA_ExpandCollapsePatternId:
            *out = static_cast<IUIAutomationExpandCollapsePattern*>(&expand_);
            return S_OK;
          case UIA_SelectionItemPatternId:
            *out = static_cast<IUIAutomationSelectionItemPattern*>(&selection_);
            return S_OK;
          case UIA_ValuePatternId:
            *out = static_cast<IUIAutomationValuePattern*>(&value_);
            return S_OK;
          default:
            return S_FALSE;
          }
        });

    ON_CALL(toggle_, get_CachedToggleState(_)).WillByDefault([](ToggleState* out) {
      *out = ToggleState_On;
      return S_OK;
    });
    ON_CALL(expand_, get_CachedExpandCollapseState(_)).WillByDefault([](ExpandCollapseState* out) {
      *out = ExpandCollapseState_Expanded;
      return S_OK;
    });
    ON_CALL(selection_, get_CachedIsSelected(_)).WillByDefault([](BOOL* out) {
      *out = TRUE;
      return S_OK;
    });
    ON_CALL(value_, get_CachedValue(_)).WillByDefault([](BSTR* out) {
      *out = bstr(L"hello");
      return S_OK;
    });
    ON_CALL(value_, get_CachedIsReadOnly(_)).WillByDefault([](BOOL* out) {
      *out = FALSE;
      return S_OK;
    });
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

TEST_F(UiaProviderTest, StartForwardsFocusEventsToTheCallback) {
  UiaProvider provider;
  AccessibleNode received;
  bool called = false;
  provider.start([&](const AccessibleNode& node) {
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
  provider.start([](const AccessibleNode&) {});
  EXPECT_CALL(automation_, RemoveFocusChangedEventHandler(_)).WillOnce(Return(S_OK));
  provider.stop();
}

} // namespace

#endif // defined(_WIN32)
