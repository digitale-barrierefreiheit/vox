# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::testing — shared, header-only test-support (fakes/doubles) for first-party
# tests. INTERFACE target: it is linked by test executables only and is never
# part of a shipped library.
add_library(vox_testing INTERFACE)
add_library(vox::testing ALIAS vox_testing)

target_include_directories(vox_testing
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(vox_testing INTERFACE vox::provider vox::tts vox::audio vox::input)
