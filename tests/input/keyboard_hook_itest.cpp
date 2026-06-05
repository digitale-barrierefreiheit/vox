// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Integration test for vox::input::KeyboardHook against the live OS hook.
///
/// OS glue, not the pure-core suite: it installs a real WH_KEYBOARD_LL hook and
/// injects a key with SendInput, asserting it flows hook -> handler. A headless
/// runner may lack the interactive desktop an LL hook needs, so the test skips
/// when the hook cannot run — unless VOX_REQUIRE_INPUT_HOOK=1 (a provisioned CI
/// job), where it fails instead. The command mapping itself is covered by the
/// portable unit tests; here we only prove the hook plumbing works end to end.
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#include <vox/input/command.hpp>
#include <vox/input/keyboard_hook.hpp>
#include <vox/testing/fake_command_handler.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

using vox::input::Command;
using vox::input::KeyboardHook;
using vox::testing::FakeCommandHandler;

/// True on the CI input job: a hook that cannot run must fail, not skip.
bool inputHookRequired() {
  char* value = nullptr;
  std::size_t size = 0;
  if (::_dupenv_s(&value, &size, "VOX_REQUIRE_INPUT_HOOK") != 0 || value == nullptr) {
    return false;
  }
  const bool required = std::string_view(value) == "1";
  std::free(value);
  return required;
}

/// Synthesizes a Tab press + release; a low-level hook sees injected keys.
/// Returns true if both events were injected.
bool sendTab() {
  std::array<INPUT, 2> inputs{};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_TAB;
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = VK_TAB;
  inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
  const auto count = static_cast<UINT>(inputs.size());
  return ::SendInput(count, inputs.data(), sizeof(INPUT)) == count;
}

TEST(KeyboardHookITest, InjectedTabReachesHandlerAsNavigateNext) {
  FakeCommandHandler handler;
  KeyboardHook hook(handler);
  try {
    hook.start();
  } catch (const std::runtime_error& error) {
    if (inputHookRequired()) {
      FAIL() << "VOX_REQUIRE_INPUT_HOOK is set but the hook could not start: " << error.what();
    }
    GTEST_SKIP() << "Keyboard hook unavailable on this desktop (" << error.what() << ").";
  }

  if (!sendTab()) {
    hook.stop();
    if (inputHookRequired()) {
      FAIL() << "VOX_REQUIRE_INPUT_HOOK is set but SendInput could not inject the key.";
    }
    GTEST_SKIP() << "SendInput could not inject a key on this desktop.";
  }

  // The hook callback runs on its own thread; poll briefly for the command.
  for (int attempt = 0; attempt < 200 && handler.count() == 0; ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  hook.stop();

  if (handler.count() == 0) {
    if (inputHookRequired()) {
      FAIL() << "VOX_REQUIRE_INPUT_HOOK is set but the injected key never arrived.";
    }
    GTEST_SKIP() << "Injected key did not reach the hook on this desktop.";
  }
  EXPECT_EQ(handler.commands().front(), Command::NavigateNext);
}

} // namespace
