# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Sanitizer policy (architecture §8.6.3 / ADR-14).
#
# The OS-independent cores (ring, codec, German normalization, resync, braille)
# run under ASan/TSan/UBSan in a CI-only Clang build. MSVC ships ASan but not
# TSan/UBSan, so the sanitizer presets select Clang/clang-cl.
#
# Select one with `-DVOX_SANITIZER=address|thread|undefined|address+undefined`.
# Flags are added to the shared `vox_project_options` target so every first-party
# target is instrumented consistently.

set(VOX_SANITIZER "none" CACHE STRING
    "Sanitizer to enable: none | address | thread | undefined | address+undefined")
set_property(CACHE VOX_SANITIZER PROPERTY STRINGS
    none address thread undefined address+undefined)

if(VOX_SANITIZER STREQUAL "none")
  return()
endif()

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(NOT VOX_SANITIZER STREQUAL "address")
    message(FATAL_ERROR
      "MSVC supports only AddressSanitizer. Use a Clang/clang-cl preset for "
      "thread/undefined (see ADR-14). Requested: ${VOX_SANITIZER}")
  endif()
  target_compile_options(vox_project_options INTERFACE /fsanitize=address)
  return()
endif()

set(_vox_san_flags "")
if(VOX_SANITIZER MATCHES "address")
  list(APPEND _vox_san_flags -fsanitize=address)
endif()
if(VOX_SANITIZER MATCHES "undefined")
  list(APPEND _vox_san_flags -fsanitize=undefined -fno-sanitize-recover=undefined)
endif()
if(VOX_SANITIZER STREQUAL "thread")
  list(APPEND _vox_san_flags -fsanitize=thread)
endif()

list(APPEND _vox_san_flags -fno-omit-frame-pointer -g)

target_compile_options(vox_project_options INTERFACE ${_vox_san_flags})
target_link_options(vox_project_options INTERFACE ${_vox_san_flags})

message(STATUS "Vox: sanitizer = ${VOX_SANITIZER}")
