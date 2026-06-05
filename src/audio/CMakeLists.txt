# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::audio — the small, OS-independent PCM seam shared by the speech path:
# AudioFormat + PcmBuffer. The TTS subsystem (#35) produces PCM in this shape and
# the WASAPI audio pipeline (#36) consumes it, so the contract lives in one place.
# Pure value types — compiled and sanitizer-/clang-tidy-covered everywhere.

add_library(vox_audio STATIC
  src/audio_format.cpp)
add_library(vox::audio ALIAS vox_audio)

target_include_directories(vox_audio
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

vox_configure_target(vox_audio)

set_target_properties(vox_audio PROPERTIES FOLDER "libs")
