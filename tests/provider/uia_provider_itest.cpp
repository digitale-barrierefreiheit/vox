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
/// The app cycles keyboard focus through its focusable controls; this polls the
/// provider's focused element to collect each one, then asserts the mapped role,
/// name, state, and value. Gated like the other *_itest: the Windows CI jobs set
/// `VOX_REQUIRE_UIA_TREE=1`, under which a missing app or unreadable controls are a
/// hard failure; without the flag (dev box / non-interactive runner) the test skips.
/// CMake passes the freshly built app path via the `VOX_UIA_TEST_APP` environment.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/model/state.hpp>
#include <vox/provider/uia_provider.hpp>

namespace {

using vox::model::AccessibleNode;
using vox::model::Role;
using vox::model::State;
using vox::provider::UiaProvider;

constexpr DWORD ReadyTimeoutMs = 10000;

// One expected control. An empty `name` matches by role alone (the edit has no
// accessible name); `state`, if set, must be present; a non-empty `value` must match.
struct ExpectedControl {
  Role role;
  std::string_view name;
  std::optional<State> state;
  std::string_view value;
};

// The focusable controls the app exposes (one per focusable, provider-mappable role).
constexpr std::array<ExpectedControl, 5> ExpectedControls{{
    {.role = Role::Button, .name = "Speichern", .state = std::nullopt, .value = ""},
    {.role = Role::Checkbox, .name = "Kapitel anzeigen", .state = State::Checked, .value = ""},
    {.role = Role::Checkbox, .name = "Teilauswahl", .state = State::Mixed, .value = ""},
    {.role = Role::RadioButton, .name = "Deutsch", .state = State::Selected, .value = ""},
    {.role = Role::Edit, .name = "", .state = std::nullopt, .value = "Hallo"},
}};

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

bool uiaTreeRequired() {
  return envValue(L"VOX_REQUIRE_UIA_TREE") == L"1";
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

// Finds a collected control by role (and name, unless the expected name is empty).
std::optional<AccessibleNode> find(const std::vector<AccessibleNode>& seen, Role role,
                                   std::string_view name) {
  for (const AccessibleNode& node : seen) {
    if (node.role == role && (name.empty() || node.name == name)) {
      return node;
    }
  }
  return std::nullopt;
}

// Adds a control unless one with the same role + name was already collected.
void addDistinct(std::vector<AccessibleNode>& seen, const AccessibleNode& node) {
  if (!find(seen, node.role, node.name).has_value()) {
    seen.push_back(node);
  }
}

bool haveAllExpected(const std::vector<AccessibleNode>& seen) {
  return std::ranges::all_of(ExpectedControls, [&seen](const ExpectedControl& expected) {
    return find(seen, expected.role, expected.name).has_value();
  });
}

std::string describe(const AccessibleNode& node) {
  return "role=" + std::to_string(static_cast<int>(node.role)) + " name='" + node.name +
         "' value='" + node.value.value_or("<none>") + "'";
}

// Fails when VOX_REQUIRE_UIA_TREE demands a working tree, otherwise skips.
void skipOrFail(const std::string& failMessage, const std::string& skipMessage) {
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

// Polls the focused element while the app cycles focus, collecting each distinct
// control until all expected appear or the budget (~10s) runs out.
std::vector<AccessibleNode> collectFocusedControls(const UiaProvider& provider) {
  std::vector<AccessibleNode> seen;
  for (int attempt = 0; attempt < 200 && !haveAllExpected(seen); ++attempt) {
    if (std::optional<AccessibleNode> node = provider.focusedElement(); node.has_value()) {
      addDistinct(seen, *node);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return seen;
}

TEST_F(UiaProviderItest, ReadsEachFocusableControl) {
  const UiaProvider provider;
  const std::vector<AccessibleNode> seen = collectFocusedControls(provider);

  if (!haveAllExpected(seen)) {
    skipOrFail("VOX_REQUIRE_UIA_TREE is set but the provider did not read every focusable control.",
               "Did not read all focusable controls (no interactive focus on this machine?).");
    return;
  }

  for (const ExpectedControl& expected : ExpectedControls) {
    const std::optional<AccessibleNode> node = find(seen, expected.role, expected.name);
    if (!node.has_value()) {
      continue; // haveAllExpected guaranteed presence; this guard keeps the access checked
    }
    if (expected.state.has_value()) {
      EXPECT_TRUE(node->states.test(*expected.state))
          << "missing state " << static_cast<int>(*expected.state) << " on " << describe(*node);
    }
    if (!expected.value.empty()) {
      EXPECT_EQ(node->value.value_or(""), expected.value)
          << "value mismatch on " << describe(*node);
    }
  }
}

} // namespace
