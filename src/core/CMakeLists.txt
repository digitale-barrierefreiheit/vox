# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::core — seed library. Currently exposes build/version metadata; it is the
# placeholder that proves the toolchain end to end (configure, compile, link,
# install-layout, test). Real portable cores will be added as sibling libraries.

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/vox/core/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/vox/core/version.hpp"
  @ONLY)

add_library(vox_core STATIC
  src/version.cpp)
add_library(vox::core ALIAS vox_core)

target_include_directories(vox_core
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

vox_configure_target(vox_core)

set_target_properties(vox_core PROPERTIES FOLDER "libs")
