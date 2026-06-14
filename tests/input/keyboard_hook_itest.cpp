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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>
#include <vox/input/keyboard_hook.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace {

using vox::input::Command;
using vox::input::KeyboardHook;

/// A lock-free ICommandHandler. The WH_KEYBOARD_LL callback must return
/// promptly, so the test inspects atomics rather than making the hot path take a
/// mutex while another thread polls.
class AtomicCommandHandler : public vox::input::ICommandHandler {
public:
  void onCommand(Command command) override {
    last_.store(command, std::memory_order_release);
    count_.fetch_add(1, std::memory_order_acq_rel);
  }

  [[nodiscard]] std::size_t count() const {
    return count_.load(std::memory_order_acquire);
  }

  [[nodiscard]] Command last() const {
    return last_.load(std::memory_order_acquire);
  }

private:
  std::atomic<Command> last_{Command::None};
  std::atomic<std::size_t> count_{0};
};

/// True on the CI input job: a hook that cannot run must fail, not skip.
bool inputHookRequired() {
  char* value = nullptr;
  if (std::size_t size = 0;
      ::_dupenv_s(&value, &size, "VOX_REQUIRE_INPUT_HOOK") != 0 || value == nullptr) {
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

/// Records the standard "the hook environment is unavailable" outcome and
/// returns. On the provisioned CI job (VOX_REQUIRE_INPUT_HOOK) that is a
/// failure with @p requiredMessage; everywhere else it is a skip describing
/// @p obstacle. Centralizing the branch keeps each call site in the test flat.
/// The caller must `return` immediately after, exactly as a bare FAIL()/SKIP()
/// would have.
void failIfRequiredOtherwiseSkip(std::string_view requiredMessage, std::string_view obstacle) {
  if (inputHookRequired()) {
    FAIL() << requiredMessage;
  }
  GTEST_SKIP() << obstacle;
}

/// Polls briefly for the hook callback (which runs on its own thread) to deliver
/// a command, then stops the hook. Returns true if a command arrived.
bool awaitCommandThenStop(const AtomicCommandHandler& handler, KeyboardHook& hook) {
  for (int attempt = 0; attempt < 200 && handler.count() == 0; ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  hook.stop();
  return handler.count() != 0;
}

TEST(KeyboardHookITest, InjectedTabReachesHandlerAsNavigateNext) {
  AtomicCommandHandler handler;
  KeyboardHook hook(handler);

  try {
    hook.start();
  } catch (const std::runtime_error& error) {
    const std::string required =
        std::string("VOX_REQUIRE_INPUT_HOOK is set but the hook could not start: ") + error.what();
    const std::string obstacle =
        std::string("Keyboard hook unavailable on this desktop (") + error.what() + ").";
    failIfRequiredOtherwiseSkip(required, obstacle);
    return;
  }

  if (!sendTab()) {
    hook.stop();
    failIfRequiredOtherwiseSkip("VOX_REQUIRE_INPUT_HOOK is set but SendInput could not inject "
                                "the key.",
                                "SendInput could not inject a key on this desktop.");
    return;
  }

  if (!awaitCommandThenStop(handler, hook)) {
    failIfRequiredOtherwiseSkip("VOX_REQUIRE_INPUT_HOOK is set but the injected key never arrived.",
                                "Injected key did not reach the hook on this desktop.");
    return;
  }

  EXPECT_EQ(handler.last(), Command::NavigateNext);
}

} // namespace
