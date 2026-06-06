# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Tests over the portable PCM seam (AudioFormat + PcmBuffer) and the lock-free
# PcmRing. The ring's concurrent test spawns threads, so it needs Threads.
find_package(Threads REQUIRED)

vox_add_test(
  NAME audio_test
  SOURCES
    audio_format_test.cpp
    pcm_buffer_test.cpp
    pcm_ring_test.cpp
    pcm_converter_test.cpp
    fake_audio_sink_test.cpp
  LINK vox::audio vox::testing Threads::Threads)

set_target_properties(audio_test PROPERTIES FOLDER "tests")

if(WIN32)
  # Unit tests for the WASAPI sink's device-acquisition error handling, driven
  # through the #68 test seam with mock COM — no real device, so they run
  # everywhere (incl. the coverage job) and fault-inject the failure paths.
  add_executable(audio_wasapi_test wasapi_audio_sink_test.cpp)
  vox_configure_target(audio_wasapi_test)
  target_link_libraries(audio_wasapi_test PRIVATE
    vox::audio
    Threads::Threads
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests(audio_wasapi_test
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(audio_wasapi_test PROPERTIES FOLDER "tests")

  # Integration test driving the real WASAPI render device. Labelled
  # "integration"; it skips when no device is present (and fails instead when
  # VOX_REQUIRE_AUDIO_DEVICE=1, i.e. on the virtual-audio CI job).
  add_executable(audio_wasapi_itest wasapi_audio_sink_itest.cpp)
  vox_configure_target(audio_wasapi_itest)
  target_link_libraries(audio_wasapi_itest PRIVATE
    vox::audio
    Threads::Threads
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests(audio_wasapi_itest
    PROPERTIES LABELS "integration"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(audio_wasapi_itest PROPERTIES FOLDER "tests")
endif()
