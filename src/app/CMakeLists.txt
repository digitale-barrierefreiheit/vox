# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::app — the MVP reader (#39). The Reader orchestrator is portable: it wires
# the module interfaces (IProvider / ITtsEngine / IAudioSink) and is tested with
# fakes everywhere. The `vox` executable is the Windows entrypoint that
# constructs the real UIA / SAPI / WASAPI / keyboard-hook implementations (which
# come from the module libraries' own if(WIN32) sources) and runs the Reader.

add_library(vox_app STATIC
  src/reader.cpp)
add_library(vox::app ALIAS vox_app)

target_include_directories(vox_app
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(Threads REQUIRED)
target_link_libraries(vox_app
  PUBLIC
    vox::provider
    vox::model
    vox::output
    vox::tts
    vox::audio
    vox::input
    Threads::Threads)

vox_configure_target(vox_app)

set_target_properties(vox_app PROPERTIES FOLDER "libs")

if(WIN32)
  add_executable(vox src/main.cpp)
  vox_configure_target(vox)
  target_link_libraries(vox PRIVATE vox::app vox::german)
  set_target_properties(vox PROPERTIES FOLDER "apps")
endif()
