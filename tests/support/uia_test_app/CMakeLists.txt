# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# A tiny Win32 app exposing a known control tree, launched as a child process by
# the UIA provider integration test (#40). Windows-only; a test fixture, never
# shipped. Console subsystem (main): launched with CREATE_NO_WINDOW, so no window
# flashes; standard common controls give it a UIA tree without any UIA code here.
add_executable(uia_test_app main.cpp)
vox_configure_target(uia_test_app)
target_link_libraries(uia_test_app PRIVATE user32)
set_target_properties(uia_test_app PROPERTIES FOLDER "tests")
