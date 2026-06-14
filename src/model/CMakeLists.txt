# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

# vox::model — the normalized, provider-independent Accessibility Object Model.
# A pure, OS-independent core (no Windows headers): the AccessibleNode value type
# plus its Role and StateSet building blocks. The provider layer (#37) produces
# these; the Output Manager (#33) and German lexicon (#34) consume them.

add_library(vox_model STATIC
  src/role.cpp
  src/state.cpp)
add_library(vox::model ALIAS vox_model)

target_include_directories(vox_model
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

vox_configure_target(vox_model)

set_target_properties(vox_model PROPERTIES FOLDER "libs")
