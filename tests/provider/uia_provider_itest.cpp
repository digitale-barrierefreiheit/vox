// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Integration test: drive the real vox::provider::UiaProvider against the
///        purpose-built UIA test app (#40).
///
/// OS glue, not the pure-core suite (architecture §8.6.2): this launches the test
/// app as a child process and reads the focused element through the live
/// `IUIAutomation` stack, so it exercises the path the mock-COM unit tests cannot.
///
/// Dry-run scope: the app focuses a single labelled button and this asserts the
/// provider reads it — proving that keyboard focus and UIA reads work on the
/// hosted CI runner before the full control tree + UIA-driven focus are built.
///
/// Gated like the other *_itest: the Windows CI jobs set `VOX_REQUIRE_UIA_TREE=1`,
/// under which a missing app or an unreadable focus is a hard failure; without the
/// flag (dev box / non-interactive runner) the test skips, so those stay green.
/// CMake passes the freshly built app path via the `VOX_UIA_TEST_APP` environment.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/provider/uia_provider.hpp>

namespace {

using vox::model::AccessibleNode;
using vox::model::Role;
using vox::provider::UiaProvider;

constexpr DWORD ReadyTimeoutMs = 10000;

std::string envValue(const char* name) {
  const DWORD size = ::GetEnvironmentVariableA(name, nullptr, 0);
  if (size == 0) {
    return {}; // unset
  }
  std::string value(size, '\0');
  const DWORD written = ::GetEnvironmentVariableA(name, value.data(), size);
  value.resize(written); // written excludes the null terminator
  return value;
}

bool envEquals(const char* name, std::string_view expected) {
  return envValue(name) == expected;
}

std::wstring toWide(std::string_view utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int length =
      ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(),
                        length);
  return wide;
}

/// The test-app executable: the path CMake injected, else a sibling of this test
/// executable (both land in the same build output directory).
std::wstring locateTestApp() {
  if (const std::string fromEnv = envValue("VOX_UIA_TEST_APP"); !fromEnv.empty()) {
    return toWide(fromEnv);
  }
  std::wstring self(MAX_PATH, L'\0');
  const DWORD count = ::GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size()));
  if (count == 0 || count >= self.size()) {
    return {};
  }
  self.resize(count);
  const std::size_t slash = self.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return {};
  }
  return self.substr(0, slash + 1) + L"uia_test_app.exe";
}

class UiaProviderItest : public ::testing::Test {
protected:
  static bool uiaRequired() {
    return envEquals("VOX_REQUIRE_UIA_TREE", "1");
  }

  void SetUp() override {
    const std::wstring exe = locateTestApp();
    if (exe.empty() || ::GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
      if (uiaRequired()) {
        FAIL() << "VOX_REQUIRE_UIA_TREE is set but the UIA test app was not found.";
      }
      GTEST_SKIP() << "UIA test app not found (VOX_UIA_TEST_APP unset and no sibling exe).";
    }
    if (!launchApp(exe)) {
      if (uiaRequired()) {
        FAIL() << "VOX_REQUIRE_UIA_TREE is set but the UIA test app failed to launch.";
      }
      GTEST_SKIP() << "Could not launch the UIA test app.";
    }
    if (::WaitForSingleObject(readyEvent_, ReadyTimeoutMs) != WAIT_OBJECT_0) {
      if (uiaRequired()) {
        FAIL() << "VOX_REQUIRE_UIA_TREE is set but the UIA test app never signalled ready.";
      }
      GTEST_SKIP() << "UIA test app did not signal ready in time.";
    }
  }

  void TearDown() override {
    if (process_ != nullptr) {
      ::TerminateProcess(process_, 0);
      ::WaitForSingleObject(process_, ReadyTimeoutMs);
      ::CloseHandle(process_);
    }
    if (readyEvent_ != nullptr) {
      ::CloseHandle(readyEvent_);
    }
  }

private:
  bool launchApp(const std::wstring& exe) {
    const std::wstring eventName =
        L"Local\\VoxUiaTestAppReady-" + std::to_wstring(::GetCurrentProcessId());
    readyEvent_ = ::CreateEventW(nullptr, TRUE /* manual reset */, FALSE, eventName.c_str());
    if (readyEvent_ == nullptr) {
      return false;
    }
    std::wstring commandLine = L"\"" + exe + L"\" " + eventName;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    if (::CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, nullptr, &startup, &info) == 0) {
      return false;
    }
    ::CloseHandle(info.hThread);
    process_ = info.hProcess;
    return true;
  }

  HANDLE process_ = nullptr;
  HANDLE readyEvent_ = nullptr;
};

// The app focuses its "Speichern" button on startup; the provider must read that
// focused element through the real UIA stack. Polls briefly so the foreground/
// focus has time to settle on the runner.
TEST_F(UiaProviderItest, ReadsTheFocusedButton) {
  const UiaProvider provider;

  std::optional<AccessibleNode> node;
  for (int attempt = 0; attempt < 50; ++attempt) {
    node = provider.focusedElement();
    if (node && node->role == Role::Button) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (!node.has_value()) {
    ADD_FAILURE() << "Provider read no focused element after polling.";
    return;
  }
  EXPECT_EQ(node->role, Role::Button)
      << "Focused role was " << static_cast<int>(node->role) << ", name='" << node->name << "'.";
  EXPECT_EQ(node->name, "Speichern");
}

} // namespace
