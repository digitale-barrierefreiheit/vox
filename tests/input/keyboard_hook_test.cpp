// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Unit tests for the keyboard hook's per-key decision and start/stop
///        lifecycle, driven through the #68 test seams — no real WH_KEYBOARD_LL
///        install required, so these run anywhere (incl. the coverage job).
#if defined(_WIN32)

#  include <array>
#  include <cstddef>
#  include <cstdint>
#  include <stdexcept>
#  include <vector>

#  include <gtest/gtest.h>

#  include <vox/input/command.hpp>
#  include <vox/input/command_handler.hpp>
#  include <vox/input/command_map.hpp>
#  include <vox/input/errors.hpp>
#  include <vox/input/key_event.hpp>
#  include <vox/input/keyboard_hook.hpp>
#  include <vox/input/keyboard_test_seam.hpp>

namespace {

using vox::input::Command;
using vox::input::CommandMap;
using vox::input::HookAction;
using vox::input::HookError;
using vox::input::ICommandHandler;
using vox::input::KeyboardHook;
using vox::input::KeyEvent;
using vox::input::KeyModifiers;
using vox::input::detail::dispatchLowLevelKey;
using vox::input::detail::processKey;

// Windows virtual-key codes (spelled out so this test needs no <windows.h>).
constexpr std::uint32_t VkTab = 0x09;
constexpr std::uint32_t VkA = 0x41;
constexpr std::uint32_t VkQ = 0x51;

// WH_KEYBOARD_LL message ids and flags (spelled out for the same reason).
constexpr std::uintptr_t WmKeyDown = 0x0100;
constexpr std::uintptr_t WmSysKeyDown = 0x0104;
constexpr std::uintptr_t WmKeyUp = 0x0101;
constexpr std::uint32_t LlkhfInjected = 0x00000010;

/// Records the commands it is handed.
class RecordingHandler : public ICommandHandler {
public:
  void onCommand(Command command) override {
    commands.push_back(command);
  }

  std::vector<Command> commands;
};

/// A dedicated exception a misbehaving handler might raise (S112: not a generic
/// std::runtime_error).
class HandlerError : public std::runtime_error {
public:
  HandlerError() : std::runtime_error("handler boom") {}
};

/// Throws from onCommand, to prove the hook never lets that cross the boundary.
class ThrowingHandler : public ICommandHandler {
public:
  void onCommand(Command /*command*/) override {
    throw HandlerError{};
  }
};

/// The reader-control chord Control+Shift+Q (a consumed key), pressed.
KeyEvent quitChord(bool pressed) {
  return KeyEvent{.virtualKey = VkQ,
                  .modifiers = KeyModifiers{.shift = true, .control = true},
                  .pressed = pressed,
                  .injected = false};
}

// ---- processKey: the per-key decision -------------------------------------

TEST(KeyboardHookProcessKey, ConsumesAndRemembersAReaderControlKey) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  EXPECT_EQ(processKey(true, VkQ, quitChord(true), consumed, map, handler), HookAction::Consume);
  EXPECT_TRUE(consumed[VkQ]);
  ASSERT_EQ(handler.commands.size(), 1U);
  EXPECT_EQ(handler.commands.front(), Command::Quit);
}

TEST(KeyboardHookProcessKey, SwallowsAutoRepeatOfAConsumedKeyWithoutReRouting) {
  std::array<bool, 256> consumed{};
  consumed[VkQ] = true; // its key-down was already consumed
  RecordingHandler handler;
  const CommandMap map;
  EXPECT_EQ(processKey(true, VkQ, quitChord(true), consumed, map, handler), HookAction::Consume);
  EXPECT_TRUE(handler.commands.empty()); // not routed again
}

TEST(KeyboardHookProcessKey, SwallowsAndClearsTheKeyUpOfAConsumedKey) {
  std::array<bool, 256> consumed{};
  consumed[VkQ] = true;
  RecordingHandler handler;
  const CommandMap map;
  EXPECT_EQ(processKey(false, VkQ, quitChord(false), consumed, map, handler), HookAction::Consume);
  EXPECT_FALSE(consumed[VkQ]);
}

TEST(KeyboardHookProcessKey, PassesThroughTheKeyUpOfAnUnconsumedKey) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  EXPECT_EQ(processKey(false, VkQ, quitChord(false), consumed, map, handler),
            HookAction::PassThrough);
}

TEST(KeyboardHookProcessKey, RoutesButPassesThroughNavigationKeys) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  const KeyEvent tab{.virtualKey = VkTab, .modifiers = {}, .pressed = true, .injected = false};
  EXPECT_EQ(processKey(true, VkTab, tab, consumed, map, handler), HookAction::PassThrough);
  EXPECT_FALSE(consumed[VkTab]); // navigation is not swallowed
  ASSERT_EQ(handler.commands.size(), 1U);
  EXPECT_EQ(handler.commands.front(), Command::NavigateNext);
}

TEST(KeyboardHookProcessKey, PassesThroughUnboundKeysWithoutRouting) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  const KeyEvent letter{.virtualKey = VkA, .modifiers = {}, .pressed = true, .injected = false};
  EXPECT_EQ(processKey(true, VkA, letter, consumed, map, handler), HookAction::PassThrough);
  EXPECT_TRUE(handler.commands.empty());
}

TEST(KeyboardHookProcessKey, AThrowingHandlerDoesNotConsume) {
  std::array<bool, 256> consumed{};
  ThrowingHandler handler;
  const CommandMap map;
  EXPECT_EQ(processKey(true, VkQ, quitChord(true), consumed, map, handler),
            HookAction::PassThrough);
  EXPECT_FALSE(consumed[VkQ]);
}

// ---- dispatchLowLevelKey: the hookProc translation ------------------------

TEST(KeyboardHookDispatch, ConsumesAReaderControlChord) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  const KeyModifiers chord{.shift = true, .control = true};
  EXPECT_TRUE(dispatchLowLevelKey({WmKeyDown, VkQ, 0, chord}, consumed, map, handler));
  EXPECT_TRUE(consumed[VkQ]);
  ASSERT_EQ(handler.commands.size(), 1U);
  EXPECT_EQ(handler.commands.front(), Command::Quit);
}

TEST(KeyboardHookDispatch, PassesThroughNavigationKeys) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  EXPECT_FALSE(dispatchLowLevelKey({WmKeyDown, VkTab, 0, KeyModifiers{}}, consumed, map, handler));
  ASSERT_EQ(handler.commands.size(), 1U);
  EXPECT_EQ(handler.commands.front(), Command::NavigateNext);
}

TEST(KeyboardHookDispatch, TreatsSysKeyDownAsAPress) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  // WM_SYSKEYDOWN (Alt held) must count as pressed, so the key still routes
  // (navigation is not consumed, so the call returns false).
  EXPECT_FALSE(
      dispatchLowLevelKey({WmSysKeyDown, VkTab, 0, KeyModifiers{}}, consumed, map, handler));
  ASSERT_EQ(handler.commands.size(), 1U);
  EXPECT_EQ(handler.commands.front(), Command::NavigateNext);
}

TEST(KeyboardHookDispatch, SwallowsTheKeyUpOfAConsumedKey) {
  std::array<bool, 256> consumed{};
  consumed[VkQ] = true; // its key-down was consumed
  RecordingHandler handler;
  const CommandMap map;
  EXPECT_TRUE(dispatchLowLevelKey({WmKeyUp, VkQ, 0, KeyModifiers{}}, consumed, map, handler));
  EXPECT_FALSE(consumed[VkQ]);
}

TEST(KeyboardHookDispatch, RecordsTheInjectedFlagButStillRoutes) {
  std::array<bool, 256> consumed{};
  RecordingHandler handler;
  const CommandMap map;
  const KeyModifiers chord{.shift = true, .control = true};
  // The injected flag is parsed into the event; it does not block routing.
  EXPECT_TRUE(dispatchLowLevelKey({WmKeyDown, VkQ, LlkhfInjected, chord}, consumed, map, handler));
  ASSERT_EQ(handler.commands.size(), 1U);
  EXPECT_EQ(handler.commands.front(), Command::Quit);
}

// ---- start()/stop(): lifecycle through the install seam --------------------

/// Restores the real hook install on scope exit.
class SeamGuard {
public:
  SeamGuard() = default;

  ~SeamGuard() {
    vox::input::testing::setInstallHookOverride({});
  }

  SeamGuard(const SeamGuard&) = delete;
  SeamGuard& operator=(const SeamGuard&) = delete;
  SeamGuard(SeamGuard&&) = delete;
  SeamGuard& operator=(SeamGuard&&) = delete;
};

TEST(KeyboardHookLifecycle, StartsAndStopsWithAFakeHook) {
  [[maybe_unused]] const SeamGuard guard;
  vox::input::testing::setInstallHookOverride([] { return true; }); // fake install succeeds
  RecordingHandler handler;
  KeyboardHook hook(handler, CommandMap{});
  EXPECT_NO_THROW(hook.start());
  EXPECT_NO_THROW(hook.stop());
}

TEST(KeyboardHookLifecycle, StartThrowsWhenTheInstallFails) {
  [[maybe_unused]] const SeamGuard guard;
  vox::input::testing::setInstallHookOverride([] { return false; }); // fake install fails
  RecordingHandler handler;
  KeyboardHook hook(handler, CommandMap{});
  EXPECT_THROW(hook.start(), HookError);
}

TEST(KeyboardHookLifecycle, StartTwiceThrows) {
  [[maybe_unused]] const SeamGuard guard;
  vox::input::testing::setInstallHookOverride([] { return true; });
  RecordingHandler handler;
  KeyboardHook hook(handler, CommandMap{});
  hook.start();
  EXPECT_THROW(hook.start(), HookError);
  hook.stop();
}

TEST(KeyboardHookLifecycle, ASecondHookReportsAnotherIsAlreadyActive) {
  [[maybe_unused]] const SeamGuard guard;
  vox::input::testing::setInstallHookOverride([] { return true; });
  RecordingHandler handler;
  KeyboardHook first(handler, CommandMap{});
  first.start();

  KeyboardHook second(handler, CommandMap{});
  EXPECT_THROW(second.start(), HookError);

  first.stop();
}

} // namespace

#endif // defined(_WIN32)
