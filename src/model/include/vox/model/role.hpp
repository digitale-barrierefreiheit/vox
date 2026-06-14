// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Provider-independent control roles for the Accessibility Object Model.
///
/// `Role` is the normalized "what kind of control is this" axis of an
/// AccessibleNode (architecture §5.1, ADR-04/ADR-12). It is deliberately
/// provider-agnostic: mapping UIA/IA2/MSAA control types onto these values is
/// the provider layer's job (#37), not the model's. The set is intentionally
/// small for the MVP and grows as more controls are supported.
#ifndef VOX_MODEL_ROLE_HPP
#define VOX_MODEL_ROLE_HPP

#include <cstdint>
#include <string_view>

namespace vox::model {

/// Normalized control role. `Unknown` is the default and the catch-all the
/// provider uses when it cannot map a native control type onto a known role.
enum class Role : std::uint8_t {
  Unknown = 0, ///< Unmapped / unrecognized control.
  Button,      ///< Push button.
  Checkbox,    ///< Two- or three-state checkbox (see State::Checked/Mixed).
  RadioButton, ///< Mutually-exclusive option button (distinct from Checkbox).
  Edit,        ///< Single- or multi-line text input.
  Combobox,    ///< Drop-down / combo box.
  ListItem,    ///< Item within a list.
  MenuItem,    ///< Item within a menu.
  Link,        ///< Hyperlink.
  StaticText,  ///< Non-interactive label / text.
};

/// @brief Returns a stable ASCII identifier for @p role (e.g. "Button").
/// @return A diagnostic name for logging, the inspectable speech seam, and test
///         output. This is NOT a user-facing announcement — German announcement
///         text is composed by the lexicon (#34), not here.
[[nodiscard]] std::string_view toString(Role role) noexcept;

} // namespace vox::model

#endif // VOX_MODEL_ROLE_HPP
