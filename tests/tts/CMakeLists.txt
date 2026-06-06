# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Unit tests over the portable TTS pieces (voice selection, rate, and the fake).
# The COM SapiTtsEngine is integration-tested separately (Windows-only, below).
vox_add_test(
  NAME tts_test
  SOURCES
    voice_selection_test.cpp
    rate_test.cpp
    fake_tts_engine_test.cpp
  LINK vox::tts vox::audio vox::testing)

set_target_properties(tts_test PROPERTIES FOLDER "tests")

if(WIN32)
  # Unit tests for the SAPI engine's COM construction/synthesis error handling,
  # driven through the #68 test seam with mock COM — no installed voice, so they
  # run everywhere (incl. the coverage job) and fault-inject the failure paths.
  add_executable(tts_sapi_test sapi_tts_engine_test.cpp)
  vox_configure_target(tts_sapi_test)
  target_link_libraries(tts_sapi_test PRIVATE
    vox::tts
    vox::audio
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests(tts_sapi_test DISCOVERY_MODE PRE_TEST)
  set_target_properties(tts_sapi_test PROPERTIES FOLDER "tests")

  # Integration test driving the real SAPI5 engine to a memory stream (no audio
  # device). Labelled "integration" so it is distinct from the pure-core suite;
  # it skips itself when no SAPI voice is installed.
  add_executable(tts_sapi_itest sapi_tts_engine_itest.cpp)
  vox_configure_target(tts_sapi_itest)
  target_link_libraries(tts_sapi_itest PRIVATE
    vox::tts
    vox::audio
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests(tts_sapi_itest
    PROPERTIES LABELS "integration"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(tts_sapi_itest PROPERTIES FOLDER "tests")
endif()
