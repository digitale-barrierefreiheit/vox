// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Covers the production composition root (makeDefaultDependencies)
///        hardware-free. The SAPI engine is the only dependency whose
///        constructor needs a voice, so the #68 SAPI seam + mock chain lets the
///        whole factory run with no installed voice, device, or UI Automation.
#if defined(_WIN32)

#  include <Windows.h>

#  include <cwchar>
#  include <filesystem>
#  include <fstream>
#  include <ios>
#  include <memory>
#  include <optional>
#  include <string>
#  include <string_view>

#  include <gmock/gmock.h>
#  include <gtest/gtest.h>

#  include <vox/app/default_app.hpp>
#  include <vox/input/command.hpp>
#  include <vox/input/command_handler.hpp>
#  include <vox/model/accessible_node.hpp>
#  include <vox/model/role.hpp>
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
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;

/// A do-nothing command handler, just to hand the hook factory a callback target.
class NullHandler : public vox::input::ICommandHandler {
public:
  void onCommand(vox::input::Command /*command*/) override {
    // The factory test never issues commands; this only needs to exist.
  }
};

/// A focused "Speichern" button, as the provider would deliver it.
vox::model::AccessibleNode saveButton() {
  vox::model::AccessibleNode node;
  node.role = vox::model::Role::Button;
  node.name = "Speichern";
  return node;
}

/// A complete lexicon table declaring @p language. Tests append a distinctive
/// `role.button` override line — later keys win in the parser.
std::string completeTable(std::string_view language) {
  std::string text = "language = " + std::string(language) + "\n";
  for (const std::string_view key :
       {"role.button", "role.checkbox", "role.radiobutton", "role.edit", "role.combobox",
        "role.listitem", "role.menuitem", "role.link", "role.statictext", "state.checked",
        "state.unchecked", "state.mixed", "state.expanded", "state.collapsed", "state.selected",
        "state.disabled", "state.readonly", "state.emptyvalue"}) {
    text += std::string(key) + " = wort\n";
  }
  return text;
}

void writeFile(const std::filesystem::path& file, std::string_view content) {
  std::ofstream stream(file, std::ios::binary);
  stream << content;
}

/// The directory holding this test executable — where the composition root
/// looks for `lexicon\<tag>.lex` (#61). Grows to any path length; empty on
/// failure, so a test fails on the missing directory instead of touching a
/// CWD-relative path.
std::filesystem::path testExecutableDirectory() {
  std::wstring path(8, L'\0');
  DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  while (length == path.size()) { // filled buffer = truncated: grow and retry
    path.resize(path.size() * 2);
    length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  }
  path.resize(length);
  return std::filesystem::path(path).parent_path();
}

/// The current value of environment variable @p name, or nullopt when unset.
/// Grows to the required size, so any length is captured faithfully — and a
/// variable that exists with an empty value is captured as "", not as unset.
std::optional<std::wstring> readEnvironmentValue(const wchar_t* name) {
  std::wstring value(8, L'\0');
  ::SetLastError(ERROR_SUCCESS);
  DWORD written = ::GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
  while (written >= value.size()) {
    value.resize(written); // too small: `written` is the required size incl. the NUL
    written = ::GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
  }
  if (written == 0 && ::GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
    return std::nullopt;
  }
  value.resize(written);
  return value;
}

/// Sets an environment variable for one test and restores the previous state
/// (the prior value, or unset) on exit, so nothing leaks across tests or into
/// a developer's environment.
class ScopedEnvironment {
public:
  ScopedEnvironment(const wchar_t* name, const std::wstring& value)
      : name_(name), previous_(readEnvironmentValue(name)) {
    ::SetEnvironmentVariableW(name_, value.c_str());
  }

  ~ScopedEnvironment() {
    ::SetEnvironmentVariableW(name_, previous_.has_value() ? previous_->c_str() : nullptr);
  }

  ScopedEnvironment(const ScopedEnvironment&) = delete;
  ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
  ScopedEnvironment(ScopedEnvironment&&) = delete;
  ScopedEnvironment& operator=(ScopedEnvironment&&) = delete;

private:
  const wchar_t* name_;
  std::optional<std::wstring> previous_;
};

/// Installs the SAPI chain the engine's constructor walks (voice + category ->
/// enumerator -> one German token -> its "Attributes" data key) plus a failing
/// UIA factory, so makeDefaultDependencies() runs fully hardware-free. The
/// mocks are fixture members so they outlive the engine that borrows them; the
/// seams are restored in TearDown.
class DefaultAppTest : public ::testing::Test {
protected:
  void SetUp() override {
    ON_CALL(category_, SetId(_, _)).WillByDefault(Return(S_OK));
    ON_CALL(category_, GetDefaultTokenId(_)).WillByDefault([](LPWSTR* out) {
      *out = coTaskString(L"VOX-DEFAULT-VOICE");
      return S_OK;
    });
    ON_CALL(category_, EnumTokens(_, _, _))
        .WillByDefault([this](LPCWSTR, LPCWSTR, IEnumSpObjectTokens** out) {
          *out = &enumTokens_;
          return S_OK;
        });
    ON_CALL(enumTokens_, Next(_, _, _))
        .WillByDefault([this](ULONG, ISpObjectToken** pelt, ULONG* fetched) {
          if (tokenServed_) {
            if (fetched != nullptr) {
              *fetched = 0;
            }
            return S_FALSE;
          }
          tokenServed_ = true;
          *pelt = &token_;
          if (fetched != nullptr) {
            *fetched = 1;
          }
          return S_OK;
        });
    ON_CALL(token_, GetId(_)).WillByDefault([](LPWSTR* out) {
      *out = coTaskString(L"VOX-DEFAULT-VOICE");
      return S_OK;
    });
    ON_CALL(token_, OpenKey(_, _)).WillByDefault([this](LPCWSTR, ISpDataKey** out) {
      *out = &dataKey_;
      return S_OK;
    });
    ON_CALL(dataKey_, GetStringValue(_, _)).WillByDefault([](LPCWSTR name, LPWSTR* out) {
      *out = (name != nullptr && std::wcscmp(name, L"Language") == 0) ? coTaskString(L"407")
                                                                      : coTaskString(L"Hedda");
      return S_OK;
    });
    ON_CALL(voice_, SetVoice(_)).WillByDefault(Return(S_OK));

    vox::tts::testing::setVoiceFactory([this](ISpVoice** out) {
      *out = &voice_;
      return 0L;
    });
    vox::tts::testing::setTokenCategoryFactory([this](ISpObjectTokenCategory** out) {
      *out = &category_;
      return 0L;
    });
    // Keep the UIA provider off the real automation client; it tolerates a
    // failed creation (degrades to "no focused element") and still constructs.
    vox::provider::testing::setAutomationFactory([](IUIAutomation** out) {
      *out = nullptr;
      return static_cast<long>(0x80004005U); // E_FAIL
    });
  }

  void TearDown() override {
    vox::tts::testing::setVoiceFactory({});
    vox::tts::testing::setTokenCategoryFactory({});
    vox::provider::testing::setAutomationFactory({});
  }

private:
  NiceMock<MockSpVoice> voice_;
  NiceMock<MockSpObjectTokenCategory> category_;
  NiceMock<MockEnumSpObjectTokens> enumTokens_;
  NiceMock<MockSpObjectToken> token_;
  NiceMock<MockSpDataKey> dataKey_;
  bool tokenServed_ = false;
};

TEST_F(DefaultAppTest, BuildsTheRealDependenciesWithMockedCom) {
  const AppDependencies deps = makeDefaultDependencies();

  EXPECT_NE(deps.provider, nullptr);
  EXPECT_NE(deps.tts, nullptr);
  EXPECT_NE(deps.audio, nullptr);
  ASSERT_TRUE(static_cast<bool>(deps.makeHook));

  // Exercise the hook factory itself: it builds a real, un-started KeyboardHook
  // (construction installs no hook), so this is safe headless.
  NullHandler handler;
  EXPECT_NE(deps.makeHook(handler), nullptr);
}

// No lexicon is configured (and none ships next to this test binary), so the
// composition root must fall back to the embedded German default and speak.
TEST_F(DefaultAppTest, SpeaksGermanFromTheEmbeddedDefaultWithoutConfiguration) {
  const AppDependencies deps = makeDefaultDependencies();

  EXPECT_THAT(deps.output.announce(saveButton()).text, HasSubstr("Schaltfläche"));
}

// VOX_LEXICON points at a user table: the composition root must speak its
// words instead of the embedded default's (#61, end-to-end).
TEST_F(DefaultAppTest, SpeaksTheWordsOfTheFileVoxLexiconPointsAt) {
  const std::filesystem::path file =
      std::filesystem::path(::testing::TempDir()) / "vox-default-app-test.lex";
  writeFile(file, completeTable("de") + "role.button = Knopf-aus-Datei\n");
  const ScopedEnvironment env(L"VOX_LEXICON", file.wstring());

  const AppDependencies deps = makeDefaultDependencies();

  EXPECT_THAT(deps.output.announce(saveButton()).text, HasSubstr("Knopf-aus-Datei"));
  std::filesystem::remove(file);
}

// VOX_LANGUAGE selects a per-language file from `lexicon` next to the
// executable — the path users take to add their own languages (#61).
TEST_F(DefaultAppTest, VoxLanguageSelectsALanguageFileNextToTheExecutable) {
  const std::filesystem::path lexiconDir = testExecutableDirectory() / "lexicon";
  std::filesystem::create_directories(lexiconDir);
  writeFile(lexiconDir / "en.lex", completeTable("en") + "role.button = button-from-en-file\n");
  const ScopedEnvironment env(L"VOX_LANGUAGE", L"en");

  const AppDependencies deps = makeDefaultDependencies();

  EXPECT_THAT(deps.output.announce(saveButton()).text, HasSubstr("button-from-en-file"));
  std::filesystem::remove_all(lexiconDir);
}

} // namespace

#endif // defined(_WIN32)
