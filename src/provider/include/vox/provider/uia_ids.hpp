// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief Portable copies of the UIA constants the mapper needs.
///
/// These mirror the well-known values in `UIAutomationClient.h` so the pure
/// mapper (and its tests) compile without Windows headers. The Windows
/// `UiaProvider` reads the platform constants, which equal these.
#ifndef VOX_PROVIDER_UIA_IDS_HPP
#define VOX_PROVIDER_UIA_IDS_HPP

namespace vox::provider {

/// UIA `CONTROLTYPEID`s for the controls the MVP maps.
inline constexpr int UiaButtonControlTypeId = 50000;
inline constexpr int UiaCheckBoxControlTypeId = 50002;
inline constexpr int UiaComboBoxControlTypeId = 50003;
inline constexpr int UiaEditControlTypeId = 50004;
inline constexpr int UiaHyperlinkControlTypeId = 50005;
inline constexpr int UiaListItemControlTypeId = 50007;
inline constexpr int UiaMenuItemControlTypeId = 50011;
inline constexpr int UiaRadioButtonControlTypeId = 50013;
inline constexpr int UiaTextControlTypeId = 50020;

/// UIA `ToggleState`.
inline constexpr int UiaToggleStateOff = 0;
inline constexpr int UiaToggleStateOn = 1;
inline constexpr int UiaToggleStateIndeterminate = 2;

/// UIA `ExpandCollapseState`.
inline constexpr int UiaExpandCollapseStateCollapsed = 0;
inline constexpr int UiaExpandCollapseStateExpanded = 1;
inline constexpr int UiaExpandCollapseStatePartiallyExpanded = 2;
inline constexpr int UiaExpandCollapseStateLeafNode = 3;

/// MSAA `STATE_SYSTEM_*` bits (oleacc.h), used as a fallback for standard Win32
/// controls: they reach UIA through the legacy bridge, which exposes state via
/// `LegacyIAccessiblePattern` (these bits) rather than the modern Toggle/Value/
/// SelectionItem/ExpandCollapse patterns. Mirrored here so the pure mapper stays Windows-free.
inline constexpr unsigned UiaLegacyStateSelected = 0x0002U;
inline constexpr unsigned UiaLegacyStateChecked = 0x0010U;
inline constexpr unsigned UiaLegacyStateMixed = 0x0020U; // == STATE_SYSTEM_INDETERMINATE
inline constexpr unsigned UiaLegacyStateReadOnly = 0x0040U;
inline constexpr unsigned UiaLegacyStateExpanded = 0x0200U;  // == STATE_SYSTEM_EXPANDED
inline constexpr unsigned UiaLegacyStateCollapsed = 0x0400U; // == STATE_SYSTEM_COLLAPSED

} // namespace vox::provider

#endif // VOX_PROVIDER_UIA_IDS_HPP
