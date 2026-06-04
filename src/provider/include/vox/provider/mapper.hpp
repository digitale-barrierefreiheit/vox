// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Pure UIA-to-AccessibleNode mapping (ADR-04, §5.1).
///
/// This is the provider's interesting logic, kept free of COM so it is
/// unit-tested and sanitizer-covered. The Windows `UiaProvider` extracts raw
/// values into a `UiaElementData` and calls this.
#ifndef VOX_PROVIDER_MAPPER_HPP
#define VOX_PROVIDER_MAPPER_HPP

#include <vox/model/accessible_node.hpp>
#include <vox/provider/uia_element_data.hpp>

namespace vox::provider {

/// @brief Maps an extracted UIA snapshot to a normalized AccessibleNode.
/// @return The node; an unrecognized control type yields `Role::Unknown`.
[[nodiscard]] vox::model::AccessibleNode mapElement(const UiaElementData& data);

} // namespace vox::provider

#endif // VOX_PROVIDER_MAPPER_HPP
