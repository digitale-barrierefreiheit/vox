# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Vox microbenchmarks (#41, architecture §8.6.4): percentile-budget benchmarks
# on google-benchmark (the `benchmarks` vcpkg manifest feature). Not built by
# default — configure with the x64-msvc-bench / linux-clang-bench preset. Like
# the tests, benchmark harness code is never part of a shipped library; run it
# in Release only (a Debug benchmark measures the wrong build).
find_package(benchmark CONFIG REQUIRED)

# The harness reuses the shared test doubles (vox::testing) as its
# deterministic provider/engine; pull them in if the test tree is off.
if(NOT TARGET vox_testing)
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests/support" "${CMAKE_CURRENT_BINARY_DIR}/tests_support")
endif()

add_executable(vox_benchmarks
  main.cpp
  ttfa_pipeline_benchmark.cpp)
if(WIN32)
  # The real-engine measurement drives SAPI and is Windows-only.
  target_sources(vox_benchmarks PRIVATE ttfa_sapi_benchmark.cpp)
endif()

vox_configure_target(vox_benchmarks)
target_link_libraries(vox_benchmarks PRIVATE
  vox::app
  vox::german
  vox::testing
  benchmark::benchmark)

set_target_properties(vox_benchmarks PROPERTIES FOLDER "benchmarks")
