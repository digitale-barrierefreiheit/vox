// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Covers the production composition root (makeDefaultDependencies)
///        hardware-free. The SAPI engine is the only dependency whose
///        constructor needs a voice, so the #68 SAPI seam + mock chain lets the
///        whole factory run with no installed voice, device, or UI Automation.
#if defined(_WIN32)

#  include <cwchar>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/app/default_app.hpp>
#  include <vox/provider/uia_test_seam.hpp>
#  include <vox/tts/sapi_test_seam.hpp>

#  include "sapi_com_mocks.hpp" // the tts SAPI mock chain (on the include path)

namespace {

using vox::app::AppDependencies;
using vox::app::makeDefaultDependencies;
using vox::tts::testing::coTaskString;
using vox::tts::testing::MockEnumSpObjectTokens;
using vox::tts::testing::MockSpDataKey;
using vox::tts::testing::MockSpObjectToken;
using vox::tts::testing::MockSpObjectTokenCategory;
using vox::tts::testing::MockSpVoice;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/// Restores every seam on scope exit.
class SeamGuard {
public:
  SeamGuard() = default;

  ~SeamGuard() {
    vox::tts::testing::setVoiceFactory({});
    vox::tts::testing::setTokenCategoryFactory({});
    vox::provider::testing::setAutomationFactory({});
  }

  SeamGuard(const SeamGuard&) = delete;
  SeamGuard& operator=(const SeamGuard&) = delete;
  SeamGuard(SeamGuard&&) = delete;
  SeamGuard& operator=(SeamGuard&&) = delete;
};

TEST(DefaultApp, BuildsTheRealDependenciesWithMockedCom) {
  [[maybe_unused]] const SeamGuard guard;
  constexpr wchar_t tokenId[] = L"VOX-DEFAULT-VOICE";

  // The SAPI chain the engine's constructor walks: voice + category -> enumerator
  // -> one German token -> its "Attributes" data key. Declared before any deps so
  // they outlive the engine that borrows them.
  NiceMock<MockSpVoice> voice;
  NiceMock<MockSpObjectTokenCategory> category;
  NiceMock<MockEnumSpObjectTokens> enumTokens;
  NiceMock<MockSpObjectToken> token;
  NiceMock<MockSpDataKey> dataKey;
  bool tokenServed = false;

  ON_CALL(category, SetId(_, _)).WillByDefault(Return(S_OK));
  ON_CALL(category, GetDefaultTokenId(_)).WillByDefault([&](LPWSTR* out) {
    *out = coTaskString(tokenId);
    return S_OK;
  });
  ON_CALL(category, EnumTokens(_, _, _))
      .WillByDefault([&](LPCWSTR, LPCWSTR, IEnumSpObjectTokens** out) {
        *out = &enumTokens;
        return S_OK;
      });
  ON_CALL(enumTokens, Next(_, _, _))
      .WillByDefault([&](ULONG, ISpObjectToken** pelt, ULONG* fetched) {
        if (tokenServed) {
          if (fetched != nullptr) {
            *fetched = 0;
          }
          return S_FALSE;
        }
        tokenServed = true;
        *pelt = &token;
        if (fetched != nullptr) {
          *fetched = 1;
        }
        return S_OK;
      });
  ON_CALL(token, GetId(_)).WillByDefault([&](LPWSTR* out) {
    *out = coTaskString(tokenId);
    return S_OK;
  });
  ON_CALL(token, OpenKey(_, _)).WillByDefault([&](LPCWSTR, ISpDataKey** out) {
    *out = &dataKey;
    return S_OK;
  });
  ON_CALL(dataKey, GetStringValue(_, _)).WillByDefault([](LPCWSTR name, LPWSTR* out) {
    *out = (name != nullptr && std::wcscmp(name, L"Language") == 0) ? coTaskString(L"407")
                                                                    : coTaskString(L"Hedda");
    return S_OK;
  });
  ON_CALL(voice, SetVoice(_)).WillByDefault(Return(S_OK));

  vox::tts::testing::setVoiceFactory([&](ISpVoice** out) {
    *out = &voice;
    return 0L;
  });
  vox::tts::testing::setTokenCategoryFactory([&](ISpObjectTokenCategory** out) {
    *out = &category;
    return 0L;
  });
  // Keep the UIA provider off the real automation client; it tolerates a failed
  // creation (degrades to "no focused element") and still constructs.
  vox::provider::testing::setAutomationFactory([](IUIAutomation** out) {
    *out = nullptr;
    return static_cast<long>(0x80004005U); // E_FAIL
  });

  const AppDependencies deps = makeDefaultDependencies();

  EXPECT_NE(deps.provider, nullptr);
  EXPECT_NE(deps.tts, nullptr);
  EXPECT_NE(deps.audio, nullptr);
  EXPECT_TRUE(static_cast<bool>(deps.makeHook));
}

} // namespace

#endif // defined(_WIN32)
