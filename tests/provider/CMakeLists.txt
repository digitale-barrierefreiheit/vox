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
  gtest_discover_tests(provider_uia_test DISCOVERY_MODE PRE_TEST)
  set_target_properties(provider_uia_test PROPERTIES FOLDER "tests")
endif()
