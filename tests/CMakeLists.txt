# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Test tree. Unit/property tests over the portable cores live here, mirroring the
# src/ module layout. GoogleTest + GMock come from vcpkg; gtest_discover_tests
# registers every case with CTest so `ctest` and the IDE test explorers see them.
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_subdirectory(core)
add_subdirectory(model)
add_subdirectory(german)
add_subdirectory(output)
