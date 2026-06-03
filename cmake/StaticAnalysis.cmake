# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Optional in-build static analysis.
#
# `-DVOX_ENABLE_CLANG_TIDY=ON` runs clang-tidy on every first-party translation
# unit during compilation, using the repo-root .clang-tidy. CI also runs tidy as
# a standalone pass over the compile database; this toggle is for local use.
#
# Note: only first-party targets opt in (they set the property explicitly); this
# keeps vcpkg/GoogleTest code out of analysis.

option(VOX_ENABLE_CLANG_TIDY "Run clang-tidy during compilation of Vox targets." OFF)

if(VOX_ENABLE_CLANG_TIDY)
  find_program(VOX_CLANG_TIDY_EXE NAMES clang-tidy)
  if(NOT VOX_CLANG_TIDY_EXE)
    message(FATAL_ERROR "VOX_ENABLE_CLANG_TIDY=ON but clang-tidy was not found on PATH.")
  endif()
  message(STATUS "Vox: clang-tidy = ${VOX_CLANG_TIDY_EXE}")
endif()

# Apply the configured analyzers to a first-party target.
function(vox_enable_static_analysis target)
  if(VOX_ENABLE_CLANG_TIDY)
    set_target_properties("${target}" PROPERTIES
      CXX_CLANG_TIDY "${VOX_CLANG_TIDY_EXE};--warnings-as-errors=*")
  endif()
endfunction()
