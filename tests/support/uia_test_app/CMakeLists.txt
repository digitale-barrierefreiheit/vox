# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# A tiny Win32 app exposing a known control tree, launched as a child process by
# the UIA provider integration test (#40). Windows-only; a test fixture, never
# shipped. Console subsystem (main): the launcher passes CREATE_NO_WINDOW to suppress
# a console window (the app's own top-level window still appears briefly). Standard
# common controls give it a UIA tree without any UIA code here.
add_executable(uia_test_app main.cpp)
vox_configure_target(uia_test_app)
# vox::model for the shared control_tree.hpp types (Role/State); the same tree the itest asserts.
target_link_libraries(uia_test_app PRIVATE user32 comctl32 vox::model)
target_include_directories(uia_test_app PRIVATE "${CMAKE_SOURCE_DIR}/tests/support")
# Declare the comctl32 v6 dependency in the linker-generated manifest so the SysLink (link)
# control class is available. Merged into CMake's own manifest, so there is no duplicate.
target_link_options(uia_test_app PRIVATE
  "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
set_target_properties(uia_test_app PROPERTIES FOLDER "tests")
