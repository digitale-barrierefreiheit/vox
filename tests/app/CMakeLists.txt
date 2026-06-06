# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# End-to-end tests over the portable Reader: focus -> German utterance -> TTS ->
# audio, driven entirely through fakes (no UIA/SAPI/WASAPI). The Reader runs a
# synthesis worker thread, so the test needs Threads.
find_package(Threads REQUIRED)

vox_add_test(
  NAME app_test
  SOURCES
    reader_test.cpp
    app_test.cpp
  LINK vox::app vox::german vox::testing Threads::Threads)

set_target_properties(app_test PROPERTIES FOLDER "tests")

if(WIN32)
  # Covers the composition root (makeDefaultDependencies) hardware-free: the SAPI
  # engine it builds is the only constructor that needs a voice, so the #68 SAPI
  # seam + mock chain lets the whole factory run with no installed voice/device.
  add_executable(app_default_app_test default_app_test.cpp)
  vox_configure_target(app_default_app_test)
  target_link_libraries(app_default_app_test PRIVATE
    vox::app
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  # Reuse the SAPI COM mock chain that backs the tts unit tests.
  target_include_directories(app_default_app_test PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/tts")
  gtest_discover_tests(app_default_app_test
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(app_default_app_test PROPERTIES FOLDER "tests")
endif()
