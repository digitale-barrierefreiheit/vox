# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::app — the MVP reader (#39). The Reader orchestrator and the App run-loop
# are portable: they wire the module interfaces (IProvider / ITtsEngine /
# IAudioSink / IInputHook) and are tested with fakes everywhere. The Windows-only
# composition root (default_app.cpp) constructs the real UIA / SAPI / WASAPI /
# keyboard-hook implementations; the `vox` executable's main() just builds the
# App from it and runs.

add_library(vox_app STATIC
  src/reader.cpp
  src/app.cpp)
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

if(WIN32)
  # The composition root builds the real Windows stack, so it is Windows-only and
  # pulls in the German lexicon the default output uses.
  target_sources(vox_app PRIVATE src/default_app.cpp)
  target_link_libraries(vox_app PUBLIC vox::german)
endif()

vox_configure_target(vox_app)

set_target_properties(vox_app PROPERTIES FOLDER "libs")

if(WIN32)
  add_executable(vox src/main.cpp)
  vox_configure_target(vox)
  target_link_libraries(vox PRIVATE vox::app)
  set_target_properties(vox PROPERTIES FOLDER "apps")
endif()
