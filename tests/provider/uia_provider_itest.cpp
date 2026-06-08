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

// Reads an environment variable as wide text. The 'W' API avoids the active-code-page
// round-trip the 'A' API would impose, so a path with non-ASCII characters survives.
std::wstring envValue(const wchar_t* name) {
  const DWORD size = ::GetEnvironmentVariableW(name, nullptr, 0);
  if (size == 0) {
    return {}; // unset
  }
  std::wstring value(size, L'\0');
  const DWORD written = ::GetEnvironmentVariableW(name, value.data(), size);
  if (written == 0 || written >= size) {
    return {}; // read failed, or the value grew between the size query and the read
  }
  value.resize(written); // written excludes the null terminator
  return value;
}

bool envEquals(const wchar_t* name, std::wstring_view expected) {
  return envValue(name) == expected;
}

bool uiaTreeRequired() {
  return envEquals(L"VOX_REQUIRE_UIA_TREE", L"1");
}

/// The test-app executable: the path CMake injected, else a sibling of this test
/// executable (both land in the same build output directory).
std::wstring locateTestApp() {
  if (std::wstring fromEnv = envValue(L"VOX_UIA_TEST_APP"); !fromEnv.empty()) {
    return fromEnv;
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

bool isSpeichernButton(const std::optional<AccessibleNode>& node) {
  return node.has_value() && node->role == Role::Button && node->name == "Speichern";
}

std::string describe(const std::optional<AccessibleNode>& node) {
  if (!node.has_value()) {
    return "no focused element";
  }
  return "role=" + std::to_string(static_cast<int>(node->role)) + " name='" + node->name + "'";
}

// Fails when VOX_REQUIRE_UIA_TREE demands a working tree, otherwise skips — the shared
// reaction for every "couldn't read the focused button" path. Keeping it here flattens
// the call sites (no `if-required-else-skip` nested in each guard).
void skipOrFail(const char* failMessage, const std::string& skipMessage) {
  if (uiaTreeRequired()) {
    FAIL() << failMessage; // fatal — returns, so the skip below is the not-required path
  }
  GTEST_SKIP() << skipMessage;
}

class UiaProviderItest : public ::testing::Test {
protected:
  void SetUp() override {
    const std::wstring exe = locateTestApp();
    if (exe.empty() || ::GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
      skipOrFail("VOX_REQUIRE_UIA_TREE is set but the UIA test app was not found.",
                 "UIA test app not found (VOX_UIA_TEST_APP unset and no sibling exe).");
      return;
    }
    if (!launchApp(exe)) {
      skipOrFail("VOX_REQUIRE_UIA_TREE is set but the UIA test app failed to launch.",
                 "Could not launch the UIA test app.");
      return;
    }
    if (::WaitForSingleObject(readyEvent_, ReadyTimeoutMs) != WAIT_OBJECT_0) {
      skipOrFail("VOX_REQUIRE_UIA_TREE is set but the UIA test app never signalled ready.",
                 "UIA test app did not signal ready in time.");
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
      ::CloseHandle(readyEvent_); // don't leak the event if the child never started
      readyEvent_ = nullptr;
      return false;
    }
    ::CloseHandle(info.hThread);
    process_ = info.hProcess;
    return true;
  }

  HANDLE process_ = nullptr;
  HANDLE readyEvent_ = nullptr;
};

// The app focuses its "Speichern" button on startup; the provider must read exactly
// that element through the real UIA stack. Polls until it appears (focus can take a
// moment to settle, especially on x86). A miss skips unless VOX_REQUIRE_UIA_TREE=1.
TEST_F(UiaProviderItest, ReadsTheFocusedButton) {
  const UiaProvider provider;

  std::optional<AccessibleNode> node = provider.focusedElement();
  for (int attempt = 0; attempt < 50 && !isSpeichernButton(node); ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    node = provider.focusedElement();
  }

  if (isSpeichernButton(node)) {
    SUCCEED() << "Provider read the focused \"Speichern\" button.";
    return;
  }
  skipOrFail("VOX_REQUIRE_UIA_TREE is set but the provider never read the focused button.",
             "Could not read the test app's button (focused: " + describe(node) + ").");
}

} // namespace
