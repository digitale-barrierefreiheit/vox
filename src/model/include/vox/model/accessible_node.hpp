// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The normalized, provider-independent representation of a UI element.
///
/// `AccessibleNode` is the seam every later layer reads from (architecture
/// §5.1, ADR-04/ADR-12): the provider (#37) produces these from UIA/IA2/MSAA,
/// and the Output Manager (#33) plus the German lexicon (#34) consume them to
/// decide what to speak. It is a pure value type — no provider code, no Windows
/// headers, no announcement text — so it lives in the OS-independent core and
/// runs under the Clang sanitizer matrix.
#ifndef VOX_MODEL_ACCESSIBLE_NODE_HPP
#define VOX_MODEL_ACCESSIBLE_NODE_HPP

#include <optional>
#include <string>

#include <vox/model/role.hpp>
#include <vox/model/state.hpp>

namespace vox::model {

/// A normalized snapshot of one UI element.
///
/// Strings are UTF-8; the provider transcodes from the platform's UTF-16 at the
/// Windows boundary so the core sees a single encoding. `value` is optional to
/// distinguish a control with no value concept (e.g. a button, `std::nullopt`)
/// from one whose value is the empty string (e.g. an empty edit field, `""`).
struct AccessibleNode {
  Role role{Role::Unknown};         ///< What kind of control this is.
  std::string name;                 ///< Accessible name (UTF-8), possibly empty.
  StateSet states;                  ///< Normalized state flags.
  std::optional<std::string> value; ///< Value (UTF-8) if the control has one.

  /// Value equality across every field — used by the inspectable-seam tests and
  /// the later focus-diff/resync logic.
  [[nodiscard]] friend bool operator==(const AccessibleNode&, const AccessibleNode&) = default;
};

} // namespace vox::model

#endif // VOX_MODEL_ACCESSIBLE_NODE_HPP
