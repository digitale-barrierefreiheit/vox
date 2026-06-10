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
/// provider's focused element to collect each one, then asserts the mapped role, name,
/// focus, and state/value. Standard Win32 controls reach UIA via the legacy MSAA bridge,
/// which exposes state/value through LegacyIAccessiblePattern (the IAccessible state bits
/// + value) rather than the modern Toggle/Value/SelectionItem patterns — so the provider's
/// mapper falls back to the legacy path, which this exercises end to end against a real
/// tree (the mock-COM unit tests cover the mapping in isolation).
///
/// Gated like the other *_itest: the Windows CI jobs set `VOX_REQUIRE_UIA_TREE=1`,
/// under which a missing app or unreadable controls are a hard failure; without the
/// flag (dev box / non-interactive runner) the test skips. CMake passes the freshly
/// built app path via the `VOX_UIA_TEST_APP` environment.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <vox/german/de_lex_data.hpp>
#include <vox/german/lexicon.hpp>
#include <vox/model/accessible_node.hpp>
#include <vox/model/role.hpp>
#include <vox/model/state.hpp>
#include <vox/output/output_manager.hpp>
#include <vox/provider/uia_provider.hpp>

namespace {

using vox::german::Lexicon;
using vox::model::AccessibleNode;
using vox::model::Role;
using vox::model::State;
using vox::output::OutputManager;
using vox::provider::UiaProvider;

constexpr DWORD ReadyTimeoutMs = 10000;
constexpr int MaxPollAttempts = 200; // ~10s of polling at PollIntervalMs while focus cycles
constexpr int PollIntervalMs = 50;

// One expected control. `name` must match exactly (every control here is named — the edits
// via their preceding STATIC label); `state`, if set, must be present; a non-empty `value`
// must match. The states and value come from the legacy MSAA bridge (standard Win32 controls
// expose them there, not via the modern Toggle/Value patterns) — see the provider's mapper
// fallback. `utterance`, if set, is the German text OutputManager::announce() must render
// from the read node (the end-to-end check). (find() treats an empty `name` as a wildcard,
// but none is used now.)
struct ExpectedControl {
  Role role;
  std::string_view name;
  std::optional<State> state;
  std::string_view value;
  std::string_view utterance;
};

// The focusable controls the app exposes: Button, Checkbox (checked + tri-state), RadioButton,
// and three labelled Edits (a value, an empty one -> "leer", and a read-only one). A checked
// Win32 radio reports STATE_SYSTEM_CHECKED (-> Checked), not Selected; a labelled edit takes
// its accessible name from the preceding STATIC.
constexpr std::array<ExpectedControl, 7> ExpectedControls{{
    {.role = Role::Button,
     .name = "Speichern",
     .state = std::nullopt,
     .value = "",
     .utterance = "Schaltfläche, Speichern"},
    {.role = Role::Checkbox,
     .name = "Kapitel anzeigen",
     .state = State::Checked,
     .value = "",
     .utterance = "Kontrollkästchen, Kapitel anzeigen, aktiviert"},
    {.role = Role::Checkbox,
     .name = "Teilauswahl",
     .state = State::Mixed,
     .value = "",
     .utterance = "Kontrollkästchen, Teilauswahl, teilweise aktiviert"},
    {.role = Role::RadioButton,
     .name = "Deutsch",
     .state = State::Checked,
     .value = "",
     .utterance = "Optionsfeld, Deutsch, aktiviert"},
    {.role = Role::Edit,
     .name = "Name",
     .state = std::nullopt,
     .value = "Hallo",
     .utterance = "Eingabefeld, Name, Hallo"},
    {.role = Role::Edit,
     .name = "Suche",
     .state = std::nullopt,
     .value = "",
     .utterance = "Eingabefeld, Suche, leer"},
    {.role = Role::Edit,
     .name = "Pfad",
     .state = State::ReadOnly,
     .value = "system32",
     .utterance = "Eingabefeld, Pfad, schreibgeschützt, system32"},
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

/// The test-app executable: the path CMake injects via VOX_UIA_TEST_APP (the normal
/// case), with a best-effort fallback to a sibling of this test executable for a
/// co-located build. CMake build trees keep them in separate target directories, so
/// the fallback rarely fires — a missing/empty VOX_UIA_TEST_APP just skips the test.
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
    const bool nameMatches = name.empty() || node.name == name;
    if (node.role == role && nameMatches) {
      return node;
    }
  }
  return std::nullopt;
}

// Adds a control unless one with the same role AND exact name was already collected.
// (Uses exact equality, not find()'s empty-name wildcard, so an unnamed control is not
// mistakenly deduped against a different same-role control.)
void addDistinct(std::vector<AccessibleNode>& seen, const AccessibleNode& node) {
  const bool present = std::ranges::any_of(seen, [&node](const AccessibleNode& existing) {
    return existing.role == node.role && existing.name == node.name;
  });
  if (!present) {
    seen.push_back(node);
  }
}

bool haveAllExpected(const std::vector<AccessibleNode>& seen) {
  return std::ranges::all_of(ExpectedControls, [&seen](const ExpectedControl& expected) {
    return find(seen, expected.role, expected.name).has_value();
  });
}

std::string describe(const AccessibleNode& node) {
  return std::format("role={} name='{}'", vox::model::toString(node.role), node.name);
}

// Joins the collected controls into a diagnostic string (CI triage of focus/UIA issues).
std::string summarize(const std::vector<AccessibleNode>& seen) {
  if (seen.empty()) {
    return "(none)";
  }
  std::string out;
  for (const AccessibleNode& node : seen) {
    if (!out.empty()) {
      out += "; ";
    }
    out += describe(node);
  }
  return out;
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
      releaseApp(); // a SetUp skip/fail does not run TearDown — release the child here
      skipOrFail("VOX_REQUIRE_UIA_TREE is set but the UIA test app never signalled ready.",
                 "UIA test app did not signal ready in time.");
      return; // skipOrFail's FAIL()/GTEST_SKIP() only returns from skipOrFail, not SetUp
    }
  }

  void TearDown() override {
    releaseApp();
  }

private:
  void releaseApp() {
    if (process_ != nullptr) {
      ::TerminateProcess(process_, 0);
      ::WaitForSingleObject(process_, ReadyTimeoutMs);
      ::CloseHandle(process_);
      process_ = nullptr;
    }
    if (readyEvent_ != nullptr) {
      ::CloseHandle(readyEvent_);
      readyEvent_ = nullptr;
    }
  }

  bool launchApp(const std::wstring& exe) {
    const std::wstring eventName =
        std::format(L"Local\\VoxUiaTestAppReady-{}", ::GetCurrentProcessId());
    readyEvent_ = ::CreateEventW(nullptr, TRUE /* manual reset */, FALSE, eventName.c_str());
    if (readyEvent_ == nullptr) {
      return false;
    }
    std::wstring commandLine = std::format(L"\"{}\" {}", exe, eventName);
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
  for (int attempt = 0; attempt < MaxPollAttempts; ++attempt) {
    if (std::optional<AccessibleNode> node = provider.focusedElement(); node.has_value()) {
      addDistinct(seen, *node);
    }
    if (haveAllExpected(seen)) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(PollIntervalMs));
  }
  return seen;
}

TEST_F(UiaProviderItest, ReadsEachFocusableControl) {
  const UiaProvider provider;
  const std::vector<AccessibleNode> seen = collectFocusedControls(provider);

  if (!haveAllExpected(seen)) {
    const std::string seenSummary = summarize(seen);
    skipOrFail(
        std::format("VOX_REQUIRE_UIA_TREE is set but the provider did not read every focusable "
                    "control. Observed: {}",
                    seenSummary),
        std::format("Did not read all focusable controls (no interactive focus?). Observed: {}",
                    seenSummary));
    return;
  }

  // Every expected control was read with the right role + name + focusability. For each,
  // assert the legacy-bridge state/value (checkboxes Checked/Mixed, radio Checked, edit value)
  // and, end to end, the German utterance announce() renders from the read node. Focused is
  // intentionally not asserted: with focus cycling it reads on most polls but is timing-
  // sensitive (a control can be collected mid-transition).
  const OutputManager output(Lexicon::parse(vox::german::DefaultGermanLexiconData));
  for (const ExpectedControl& expected : ExpectedControls) {
    const std::optional<AccessibleNode> node = find(seen, expected.role, expected.name);
    if (!node.has_value()) {
      continue; // presence guaranteed by haveAllExpected; this guard keeps the access checked
    }
    EXPECT_TRUE(node->states.test(State::Focusable)) << "not focusable: " << describe(*node);
    if (expected.state.has_value()) {
      EXPECT_TRUE(node->states.test(*expected.state))
          << "expected state not read on " << describe(*node);
    }
    if (!expected.value.empty()) {
      EXPECT_EQ(node->value.value_or(""), expected.value)
          << "value mismatch on " << describe(*node);
    }
    if (!expected.utterance.empty()) {
      // End-to-end: real element -> provider -> AccessibleNode -> German utterance.
      EXPECT_EQ(output.announce(*node).text, expected.utterance)
          << "utterance mismatch on " << describe(*node);
    }
  }
}

} // namespace
