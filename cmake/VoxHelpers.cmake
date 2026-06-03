# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Convenience helpers so every first-party target picks up the same standard,
# warnings, sanitizer, and static-analysis settings without repetition.

# Apply the shared Vox build settings to a target. Third-party code must NOT be
# passed here (it should not inherit /WX or clang-tidy).
function(vox_configure_target target)
  target_link_libraries("${target}" PRIVATE vox_project_options vox_project_warnings)
  vox_enable_static_analysis("${target}")
endfunction()

# Register a GoogleTest-based test executable and discover its cases for CTest.
# Usage: vox_add_test(NAME ring_test SOURCES ring_test.cpp LINK vox::ring)
function(vox_add_test)
  cmake_parse_arguments(ARG "" "NAME" "SOURCES;LINK" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SOURCES)
    message(FATAL_ERROR "vox_add_test requires NAME and SOURCES.")
  endif()
  add_executable("${ARG_NAME}" ${ARG_SOURCES})
  vox_configure_target("${ARG_NAME}")
  target_link_libraries("${ARG_NAME}" PRIVATE
    ${ARG_LINK}
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests("${ARG_NAME}"
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST)
endfunction()
