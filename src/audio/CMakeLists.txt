# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::audio — the audio module shared by the speech path. The pure pieces
# (AudioFormat, PcmBuffer, the lock-free PcmRing, the PcmConverter, the IAudioSink
# seam) are compiled and sanitizer-/clang-tidy-covered everywhere; the WASAPI
# render sink is Windows-only. The TTS subsystem (#35) produces PCM in this shape
# and the audio pipeline (#36) consumes it. FakeAudioSink lives in tests/support.

add_library(vox_audio STATIC
  src/audio_format.cpp
  src/pcm_ring.cpp
  src/pcm_converter.cpp)
add_library(vox::audio ALIAS vox_audio)

target_include_directories(vox_audio
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(WIN32)
  # The Windows WASAPI render sink. Excluded from the Linux/sanitizer build; it
  # delegates resampling and hand-off to the portable PcmConverter/PcmRing, which
  # are tested there. Threads: its render thread; avrt: MMCSS render priority.
  find_package(Threads REQUIRED)
  target_sources(vox_audio PRIVATE src/wasapi_audio_sink.cpp)
  target_link_libraries(vox_audio PRIVATE ole32 oleaut32 uuid avrt Threads::Threads)
endif()

vox_configure_target(vox_audio)

set_target_properties(vox_audio PROPERTIES FOLDER "libs")
