# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# Tests over the portable provider pieces (mapper + fake). The COM UiaProvider is
# integration-tested against the UIA test app (#40), not here.
vox_add_test(
  NAME provider_test
  SOURCES
    mapper_test.cpp
    fake_provider_test.cpp
  LINK vox::provider vox::testing)

set_target_properties(provider_test PROPERTIES FOLDER "tests")
