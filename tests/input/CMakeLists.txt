# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Tests over the portable input seam: the CommandMap bindings and the
# routeKeyEvent helper the keyboard hook delegates to.
vox_add_test(
  NAME input_test
  SOURCES command_map_test.cpp
  LINK vox::input vox::testing)

set_target_properties(input_test PROPERTIES FOLDER "tests")

if(WIN32)
  # Integration test driving the real WH_KEYBOARD_LL hook via SendInput.
  # Labelled "integration"; it skips when the desktop cannot host an LL hook
  # (and fails instead when VOX_REQUIRE_INPUT_HOOK=1, i.e. on the CI input job).
  add_executable(input_keyboard_hook_itest keyboard_hook_itest.cpp)
  vox_configure_target(input_keyboard_hook_itest)
  target_link_libraries(input_keyboard_hook_itest PRIVATE
    vox::input
    vox::testing
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests(input_keyboard_hook_itest
    PROPERTIES LABELS "integration"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(input_keyboard_hook_itest PROPERTIES FOLDER "tests")
endif()
