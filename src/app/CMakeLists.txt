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
  src/app.cpp
  # The lexicon loader (#61) is the app's one filesystem seam: it reads the
  # per-language .lex files the pure vox::german deliberately cannot.
  src/lexicon_loader.cpp)
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
    vox::german
    Threads::Threads)

if(WIN32)
  # The composition root builds the real Windows stack, so it is Windows-only.
  target_sources(vox_app PRIVATE src/default_app.cpp)
endif()

vox_configure_target(vox_app)

set_target_properties(vox_app PROPERTIES FOLDER "libs")

if(WIN32)
  add_executable(vox src/main.cpp)
  vox_configure_target(vox)
  target_link_libraries(vox PRIVATE vox::app)
  set_target_properties(vox PROPERTIES FOLDER "apps")

  # Embed the Windows version metadata, application icon, and licence text (#103).
  # vox.rc is generated from vox.rc.in so the version tracks project(VERSION) and
  # the LICENSE/NOTICE are read from the repo files at build time (single source).
  set(VOX_ICON_PATH    "${PROJECT_SOURCE_DIR}/assets/vox.ico")
  set(VOX_LICENSE_PATH "${PROJECT_SOURCE_DIR}/LICENSE")
  set(VOX_NOTICE_PATH  "${PROJECT_SOURCE_DIR}/NOTICE")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/vox.rc.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/vox.rc" @ONLY)
  target_sources(vox PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/vox.rc")
  # vox.rc embeds these by reference, so recompile the .res when they change.
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/vox.rc" PROPERTIES
    OBJECT_DEPENDS "${VOX_ICON_PATH};${VOX_LICENSE_PATH};${VOX_NOTICE_PATH}")

  # Ship the per-language lexicon files next to the executable, where the
  # loader looks for them (lexicon\<tag>.lex, #61).
  add_custom_command(TARGET vox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/data/lexicon"
            "$<TARGET_FILE_DIR:vox>/lexicon"
    COMMENT "Copying lexicon files next to vox")

  # Release install layout (#103): the executable, the lexicon data it loads at
  # runtime (lexicon\<tag>.lex, #61), and the licence/notice/changelog files —
  # all at the package root so the ZIP is a self-contained, runnable folder.
  install(TARGETS vox RUNTIME DESTINATION ".")
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/data/lexicon" DESTINATION ".")
  install(FILES
            "${PROJECT_SOURCE_DIR}/LICENSE"
            "${PROJECT_SOURCE_DIR}/NOTICE"
            "${PROJECT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
            "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
          DESTINATION ".")
endif()
