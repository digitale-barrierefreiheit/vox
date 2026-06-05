// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Tests for the pure input seam: CommandMap bindings + routeKeyEvent.
#include <cstdint>

#include <gtest/gtest.h>

#include <vox/input/command.hpp>
#include <vox/input/command_map.hpp>
#include <vox/input/key_event.hpp>
#include <vox/testing/fake_command_handler.hpp>

namespace {

using vox::input::Command;
using vox::input::CommandMap;
using vox::input::KeyEvent;
using vox::input::KeyModifiers;
using vox::input::routeKeyEvent;
using vox::testing::FakeCommandHandler;

// Virtual-key codes used by the tests (Windows numbering).
constexpr std::uint32_t VkTab = 0x09;
constexpr std::uint32_t VkLeft = 0x25;
constexpr std::uint32_t VkUp = 0x26;
constexpr std::uint32_t VkRight = 0x27;
constexpr std::uint32_t VkDown = 0x28;
constexpr std::uint32_t VkA = 0x41;
constexpr std::uint32_t VkQ = 0x51;
constexpr std::uint32_t VkS = 0x53;

constexpr KeyModifiers ReaderChord{.shift = true, .control = true};

KeyEvent down(std::uint32_t virtualKey, KeyModifiers modifiers = {}) {
  return KeyEvent{.virtualKey = virtualKey, .modifiers = modifiers, .pressed = true};
}

TEST(CommandMap, TabNavigatesNextAndPrevious) {
  const CommandMap map;
  EXPECT_EQ(map.map(down(VkTab)), Command::NavigateNext);
  EXPECT_EQ(map.map(down(VkTab, KeyModifiers{.shift = true})), Command::NavigatePrevious);
}

TEST(CommandMap, ArrowsNavigate) {
  const CommandMap map;
  EXPECT_EQ(map.map(down(VkRight)), Command::NavigateNext);
  EXPECT_EQ(map.map(down(VkLeft)), Command::NavigatePrevious);
  EXPECT_EQ(map.map(down(VkUp)), Command::NavigateUp);
  EXPECT_EQ(map.map(down(VkDown)), Command::NavigateDown);
}

TEST(CommandMap, ReaderChordQuitAndToggle) {
  const CommandMap map;
  EXPECT_EQ(map.map(down(VkQ, ReaderChord)), Command::Quit);
  EXPECT_EQ(map.map(down(VkS, ReaderChord)), Command::ToggleSpeech);
}

TEST(CommandMap, KeyUpProducesNoCommand) {
  const CommandMap map;
  const KeyEvent release{.virtualKey = VkTab, .modifiers = {}, .pressed = false};
  EXPECT_EQ(map.map(release), Command::None);
}

TEST(CommandMap, UnboundKeyIsNone) {
  const CommandMap map;
  EXPECT_EQ(map.map(down(VkA)), Command::None); // a plain letter passes through
}

TEST(CommandMap, ModifiersMustMatchExactly) {
  const CommandMap map;
  // AltGr (Control+Alt) on German layouts: Tab with it held is not navigation.
  EXPECT_EQ(map.map(down(VkTab, KeyModifiers{.control = true, .alt = true})), Command::None);
  // Shift+arrow is app text-selection, not navigation.
  EXPECT_EQ(map.map(down(VkDown, KeyModifiers{.shift = true})), Command::None);
  // The reader chord needs both Control and Shift.
  EXPECT_EQ(map.map(down(VkQ, KeyModifiers{.control = true})), Command::None);
}

TEST(RouteKeyEvent, NavigationDispatchesAndPassesThrough) {
  const CommandMap map;
  FakeCommandHandler handler;
  EXPECT_FALSE(routeKeyEvent(down(VkTab), map, handler)); // reaches the app
  ASSERT_EQ(handler.count(), 1U);
  EXPECT_EQ(handler.commands().front(), Command::NavigateNext);
}

TEST(RouteKeyEvent, ReaderControlDispatchesAndConsumes) {
  const CommandMap map;
  FakeCommandHandler handler;
  EXPECT_TRUE(routeKeyEvent(down(VkQ, ReaderChord), map, handler)); // swallowed
  ASSERT_EQ(handler.count(), 1U);
  EXPECT_EQ(handler.commands().front(), Command::Quit);
}

TEST(RouteKeyEvent, UnboundKeyDoesNotDispatch) {
  const CommandMap map;
  FakeCommandHandler handler;
  EXPECT_FALSE(routeKeyEvent(down(VkA), map, handler));
  EXPECT_EQ(handler.count(), 0U);
}

} // namespace
