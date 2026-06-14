# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

cmake_minimum_required(VERSION 3.25)

# The vcpkg toolchain is supplied by CMakePresets.json (or the user). Detect a
# local vcpkg root as a convenience when neither is set.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "vcpkg toolchain file")
endif()

project(vox
  VERSION 0.0.0
  DESCRIPTION "A high-efficiency, low-latency screen reader (Windows-first)."
  HOMEPAGE_URL "https://github.com/digitale-barrierefreiheit/vox"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Top-level guards and global settings
# ---------------------------------------------------------------------------
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed. Configure into ./build.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Export a compile database for clang-tidy / clangd / IDEs.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ADR-08: statically link the compiler runtime (/MT) so there is no Visual C++
# redistributable dependency. Requires the matching static vcpkg triplet
# (x64-windows-static), selected by the presets.
if(MSVC)
  cmake_policy(SET CMP0091 NEW)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# Shared interface targets: standard, warnings, sanitizers, analysis
# ---------------------------------------------------------------------------
add_library(vox_project_options INTERFACE)
add_library(vox::project_options ALIAS vox_project_options)
add_library(vox_project_warnings INTERFACE)
add_library(vox::project_warnings ALIAS vox_project_warnings)

include(ProjectStandard)
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalysis)
include(VoxHelpers)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
include(CMakeDependentOption)
option(VOX_BUILD_TESTS "Build the Vox test suite." ON)
option(VOX_BUILD_BENCHMARKS
       "Build the Vox microbenchmarks (architecture §8.6.4; needs the 'benchmarks' vcpkg feature)."
       OFF)
option(VOX_BUILD_DOCS "Generate the developer reference with Doxygen." OFF)

# ---------------------------------------------------------------------------
# Build tree
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(VOX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(VOX_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(VOX_BUILD_DOCS)
  add_subdirectory(doc)
endif()

# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------
message(STATUS "Vox ${PROJECT_VERSION}")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests     : ${VOX_BUILD_TESTS}")
message(STATUS "  Benchmarks: ${VOX_BUILD_BENCHMARKS}")
message(STATUS "  Docs      : ${VOX_BUILD_DOCS}")
