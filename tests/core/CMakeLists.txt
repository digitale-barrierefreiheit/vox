# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

vox_add_test(
  NAME core_version_test
  SOURCES version_test.cpp
  LINK vox::core)

set_target_properties(core_version_test PROPERTIES FOLDER "tests")

vox_add_test(
  NAME core_os_error_test
  SOURCES os_error_test.cpp
  LINK vox::core)

set_target_properties(core_os_error_test PROPERTIES FOLDER "tests")
