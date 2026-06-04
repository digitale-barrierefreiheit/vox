# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::output — Output Manager v0 and the inspectable Utterance seam (ADR-12,
# §8.6.1). Pure and OS-independent: it composes an AccessibleNode (#32) and the
# German lexicon (#34) into would-be-spoken text. No TTS, no audio.

add_library(vox_output STATIC
  src/output_manager.cpp)
add_library(vox::output ALIAS vox_output)

target_include_directories(vox_output
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(vox_output PUBLIC vox::model vox::german)

vox_configure_target(vox_output)

set_target_properties(vox_output PROPERTIES FOLDER "libs")
