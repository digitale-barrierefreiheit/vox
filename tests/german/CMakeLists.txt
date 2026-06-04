# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

vox_add_test(
  NAME german_test
  SOURCES
    lexicon_test.cpp
    numbers_test.cpp
    normalize_test.cpp
  LINK vox::german)

set_target_properties(german_test PROPERTIES FOLDER "tests")
