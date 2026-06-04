# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::provider — accessibility provider seam (ADR-03/ADR-04). The IProvider
# interface and the pure UIA->AccessibleNode mapper are portable (compiled and
# sanitizer-/clang-tidy-covered everywhere); the COM UiaProvider is Windows-only.
# The FakeProvider test double is NOT here — it lives in tests/support (vox::testing).

add_library(vox_provider STATIC
  src/mapper.cpp)
add_library(vox::provider ALIAS vox_provider)

target_include_directories(vox_provider
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(vox_provider PUBLIC vox::model)

if(WIN32)
  # The Windows UI Automation extractor. Excluded from the Linux/sanitizer build;
  # it delegates all logic to the portable mapper, which is tested there.
  target_sources(vox_provider PRIVATE src/uia_provider.cpp)
  target_link_libraries(vox_provider PRIVATE ole32 oleaut32 uuid)
endif()

vox_configure_target(vox_provider)

set_target_properties(vox_provider PROPERTIES FOLDER "libs")
