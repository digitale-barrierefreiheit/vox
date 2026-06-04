# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

vox_add_test(
  NAME output_test
  SOURCES output_manager_test.cpp
  LINK vox::output)

set_target_properties(output_test PROPERTIES FOLDER "tests")
