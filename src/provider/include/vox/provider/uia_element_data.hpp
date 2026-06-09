// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Plain, COM-free snapshot of a UIA element's relevant properties.
///
/// The Windows `UiaProvider` fills this in from a batched `CacheRequest`; the
/// pure `mapElement()` turns it into an `AccessibleNode`. Keeping it free of
/// COM types is what lets the mapping be unit-tested and sanitizer-covered.
/// Each `has*` flag means the corresponding value was successfully *extracted*
/// (the pattern was present and the property read), so "absent or unreadable"
/// stays distinguishable from "present". The one exception is `hasValuePattern`,
/// which records pattern presence alone (read-only-ness is captured even when
/// the value text does not read).
#ifndef VOX_PROVIDER_UIA_ELEMENT_DATA_HPP
#define VOX_PROVIDER_UIA_ELEMENT_DATA_HPP

#include <string>

namespace vox::provider {

/// Extracted UIA properties for one element (UTF-8 strings).
struct UiaElementData {
  int controlTypeId{0};            ///< UIA CONTROLTYPEID (see uia_ids.hpp).
  std::string name;                ///< Cached Name property.
  bool isEnabled{true};            ///< IsEnabled (false -> Disabled).
  bool hasKeyboardFocus{false};    ///< HasKeyboardFocus (-> Focused).
  bool isKeyboardFocusable{false}; ///< IsKeyboardFocusable (-> Focusable).
  bool hasToggle{false};           ///< TogglePattern present and ToggleState read.
  int toggleState{0};              ///< ToggleState when hasToggle.
  bool hasExpandCollapse{false};   ///< ExpandCollapsePattern present and state read.
  int expandCollapseState{0};      ///< ExpandCollapseState when hasExpandCollapse.
  bool hasSelectionItem{false};    ///< SelectionItemPattern present and IsSelected read.
  bool isSelected{false};          ///< IsSelected when hasSelectionItem.
  bool hasValuePattern{false};     ///< ValuePattern present (gates ReadOnly).
  bool isReadOnly{false};          ///< ValuePattern IsReadOnly (-> ReadOnly).
  bool hasValue{false};            ///< Value text readable (-> AccessibleNode value).
  std::string value;               ///< ValuePattern Value when hasValue.
  // LegacyIAccessiblePattern fallback for standard Win32 controls (MSAA bridge),
  // which expose state/value through the legacy path, not the modern patterns above.
  unsigned legacyState{0};    ///< MSAA STATE_SYSTEM_* bits (0 = unread/none).
  bool hasLegacyValue{false}; ///< LegacyIAccessiblePattern Value read (gates the value below).
  std::string legacyValue;    ///< LegacyIAccessiblePattern Value when hasLegacyValue.
};

} // namespace vox::provider

#endif // VOX_PROVIDER_UIA_ELEMENT_DATA_HPP
