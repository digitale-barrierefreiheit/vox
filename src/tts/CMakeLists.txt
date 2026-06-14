# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::tts — the text-to-speech seam (ADR-05). The ITtsEngine interface and the
# portable pieces (voice selection, rate clamping) are compiled and
# sanitizer-/clang-tidy-covered everywhere; the SAPI5 COM backend is Windows-only.
# The FakeTtsEngine test double is NOT here — it lives in tests/support (vox::testing).

add_library(vox_tts STATIC
  src/voice_selection.cpp
  src/rate.cpp)
add_library(vox::tts ALIAS vox_tts)

target_include_directories(vox_tts
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

# vox::core for the OS-error base behind vox::tts::EngineError (errors.hpp).
target_link_libraries(vox_tts PUBLIC vox::audio vox::core)

if(WIN32)
  # The Windows SAPI5 backend. Excluded from the Linux/sanitizer build; it
  # delegates voice choice to the portable selectVoice(), which is tested there.
  target_sources(vox_tts PRIVATE src/sapi_tts_engine.cpp)
  target_link_libraries(vox_tts PRIVATE ole32 oleaut32 sapi)
endif()

vox_configure_target(vox_tts)

set_target_properties(vox_tts PROPERTIES FOLDER "libs")
