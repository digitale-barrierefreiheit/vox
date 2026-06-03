# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Developer reference, generated from doc comments (AGENTS.md requirement).
# Enabled with -DVOX_BUILD_DOCS=ON; output lands in doc/reference (git-ignored).
find_package(Doxygen REQUIRED)

set(VOX_DOXYGEN_INPUT_DIR  "${PROJECT_SOURCE_DIR}/src")
set(VOX_DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/doc/reference")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
  "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
  @ONLY)

add_custom_target(docs
  COMMAND Doxygen::doxygen "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating developer reference with Doxygen"
  VERBATIM)
