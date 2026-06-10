# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Tests over the portable provider pieces (mapper + fake). The COM UiaProvider's
# extraction/event paths are unit-tested with mock COM below (Windows-only); it is
# additionally integration-tested against the UIA test app (#40).
vox_add_test(
  NAME provider_test
  SOURCES
    mapper_test.cpp
    fake_provider_test.cpp
  LINK vox::provider vox::testing)

set_target_properties(provider_test PROPERTIES FOLDER "tests")

if(WIN32)
  # Unit tests for the UIA provider's COM extraction/focus-event handling, driven
  # through the #68 test seam with mock COM — no real UI Automation tree, so they
  # run everywhere (incl. the coverage job) and fault-inject the degraded paths.
  add_executable(provider_uia_test uia_provider_test.cpp)
  vox_configure_target(provider_uia_test)
  target_link_libraries(provider_uia_test PRIVATE
    vox::provider
    GTest::gtest
    GTest::gmock
    GTest::gtest_main)
  gtest_discover_tests(provider_uia_test
    PROPERTIES LABELS "unit"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(provider_uia_test PROPERTIES FOLDER "tests")

  # Integration test driving the real UiaProvider against the purpose-built UIA
  # test app (#40). Labelled "integration"; it skips without an interactive
  # desktop and fails instead when VOX_REQUIRE_UIA_TREE=1 (the Windows CI jobs).
  # The test-app path is injected via the test environment so the test launches
  # the exe this build just produced.
  add_executable(provider_uia_itest uia_provider_itest.cpp)
  vox_configure_target(provider_uia_itest)
  # No gmock: this itest uses plain gtest assertions (no mocks). vox::output drives the
  # end-to-end check that a provider-read node renders to the expected German utterance.
  target_link_libraries(provider_uia_itest PRIVATE
    vox::provider
    vox::output
    GTest::gtest
    GTest::gtest_main)
  # The single-source control tree (#40) lives with the test app; both consume it.
  target_include_directories(provider_uia_itest PRIVATE "${CMAKE_SOURCE_DIR}/tests/support")
  add_dependencies(provider_uia_itest uia_test_app)
  gtest_discover_tests(provider_uia_itest
    PROPERTIES
      LABELS "integration"
      ENVIRONMENT "VOX_UIA_TEST_APP=$<TARGET_FILE:uia_test_app>"
    DISCOVERY_MODE PRE_TEST)
  set_target_properties(provider_uia_itest PROPERTIES FOLDER "tests")
endif()
