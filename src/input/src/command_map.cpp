// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

#include <cstdint>

#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>
#include <vox/input/command_map.hpp>
#include <vox/input/key_event.hpp>

namespace {

// Virtual-key codes (Windows numbering), spelled out here so the pure mapper
// stays OS-independent and testable everywhere without including <windows.h>.
constexpr std::uint32_t VkTab = 0x09;
constexpr std::uint32_t VkLeftArrow = 0x25;
constexpr std::uint32_t VkUpArrow = 0x26;
constexpr std::uint32_t VkRightArrow = 0x27;
constexpr std::uint32_t VkDownArrow = 0x28;
constexpr std::uint32_t VkQ = 0x51;
constexpr std::uint32_t VkS = 0x53;

} // namespace

namespace vox::input {

CommandMap::CommandMap()
    : bindings_{{
          // Navigation: plain keys (Shift only distinguishes Tab direction).
          {VkTab, KeyModifiers::None, Command::NavigateNext},
          {VkTab, KeyModifiers::Shift, Command::NavigatePrevious},
          {VkRightArrow, KeyModifiers::None, Command::NavigateNext},
          {VkLeftArrow, KeyModifiers::None, Command::NavigatePrevious},
          {VkUpArrow, KeyModifiers::None, Command::NavigateUp},
          {VkDownArrow, KeyModifiers::None, Command::NavigateDown},
          // Reader-control chord: Control+Shift+<key>. No Alt, so it never clashes
          // with AltGr (Control+Alt) used for German keyboard typing.
          {VkQ, KeyModifiers::Control | KeyModifiers::Shift, Command::Quit},
          {VkS, KeyModifiers::Control | KeyModifiers::Shift, Command::ToggleSpeech},
      }} {}

Command CommandMap::map(const KeyEvent& event) const {
  if (!event.pressed) {
    return Command::None; // commands fire on key-down only
  }
  for (const Binding& binding : bindings_) {
    if (binding.virtualKey == event.virtualKey && binding.modifiers == event.modifiers) {
      return binding.command;
    }
  }
  return Command::None;
}

bool routeKeyEvent(const KeyEvent& event, const CommandMap& map, ICommandHandler& handler) {
  const Command command = map.map(event);
  if (command == Command::None) {
    return false;
  }
  handler.onCommand(command);
  return consumesKey(command);
}

} // namespace vox::input
