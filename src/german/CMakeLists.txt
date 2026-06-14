# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::german — German announcement vocabulary + minimal normalization (ADR-07,
# §5.2). Pure and OS-independent: words come from a `key = value` table and the
# canonical `data/lexicon/de.lex` is embedded at configure time, so neither the
# library nor its tests touch the filesystem. The app loads per-language .lex
# files from disk at runtime (#61); this embedded copy is its always-speakable
# fallback. Full utterance composition is the Output Manager's (#33).

set(_vox_de_lex "${PROJECT_SOURCE_DIR}/data/lexicon/de.lex")
file(READ "${_vox_de_lex}" VOX_DE_LEX_CONTENT)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/de_lex_data.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/vox/german/de_lex_data.hpp"
  @ONLY)
# Re-run configure when the table changes so the embedded copy stays in sync.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_vox_de_lex}")

add_library(vox_german STATIC
  src/lexicon.cpp
  src/numbers.cpp
  src/normalize.cpp)
add_library(vox::german ALIAS vox_german)

target_include_directories(vox_german
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(vox_german PUBLIC vox::model)

vox_configure_target(vox_german)

set_target_properties(vox_german PROPERTIES FOLDER "libs")
