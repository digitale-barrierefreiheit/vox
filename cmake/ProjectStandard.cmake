# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Language standard policy.
#
# Project decision: target the latest C++ (C++26 via `/std:c++latest`), and fall
# back to C++23 where a toolchain lacks a needed feature. Configure with
# `-DVOX_CXX_STANDARD=23` to pin the fallback explicitly.
#
# The flag is applied through the `vox_project_options` interface target rather
# than CMAKE_CXX_STANDARD so that `/std:c++latest` is emitted verbatim (CMake has
# no stable mapping for "latest").

set(VOX_CXX_STANDARD "26" CACHE STRING "Baseline C++ standard: 26 (=/std:c++latest) or 23.")
set_property(CACHE VOX_CXX_STANDARD PROPERTY STRINGS 26 23)

set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # `if(MSVC)` is also true for clang-cl, which accepts the same /std flags.
  if(VOX_CXX_STANDARD STREQUAL "26")
    target_compile_options(vox_project_options INTERFACE /std:c++latest)
  else()
    target_compile_options(vox_project_options INTERFACE /std:c++${VOX_CXX_STANDARD})
  endif()
  target_compile_options(vox_project_options INTERFACE
    /Zc:__cplusplus   # report the real __cplusplus value
    /Zc:preprocessor  # conforming preprocessor
    /permissive-      # strict conformance
    /utf-8            # source and execution charset = UTF-8
    /EHsc)            # C++ exceptions, assume extern "C" never throws
else()
  if(VOX_CXX_STANDARD STREQUAL "26")
    target_compile_options(vox_project_options INTERFACE -std=c++2c)
  else()
    target_compile_options(vox_project_options INTERFACE -std=c++${VOX_CXX_STANDARD})
  endif()
endif()

message(STATUS "Vox: C++ standard = ${VOX_CXX_STANDARD}")
