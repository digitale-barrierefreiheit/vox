# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

vox_add_test(
  NAME model_test
  SOURCES
    state_test.cpp
    role_test.cpp
    accessible_node_test.cpp
  LINK vox::model)

set_target_properties(model_test PROPERTIES FOLDER "tests")
