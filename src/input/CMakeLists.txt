# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::input — the input layer (#38). The command mapping/routing is a portable
# core (KeyEvent -> Command, the seam where bindings live), compiled and
# sanitizer-/clang-tidy-covered everywhere; the WH_KEYBOARD_LL keyboard hook is
# Windows-only OS glue. The app (#39) wires an ICommandHandler that performs
# barge-in (IAudioSink::flush) and announce-focus. FakeCommandHandler lives in
# tests/support.

add_library(vox_input STATIC
  src/command_map.cpp)
add_library(vox::input ALIAS vox_input)

target_include_directories(vox_input
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(WIN32)
  # The low-level keyboard hook. Excluded from the Linux/sanitizer build; the
  # routing it delegates to (CommandMap/routeKeyEvent) is tested there. Threads:
  # its dedicated message-pump thread; user32: the hook + input APIs.
  find_package(Threads REQUIRED)
  target_sources(vox_input PRIVATE src/keyboard_hook.cpp)
  target_link_libraries(vox_input PRIVATE user32 Threads::Threads)
endif()

vox_configure_target(vox_input)

set_target_properties(vox_input PROPERTIES FOLDER "libs")
