// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief The provider seam: read the focused element and observe focus changes.
///
/// `IProvider` abstracts the accessibility backend (UIA today; IA2/MSAA later,
/// ADR-04) so Core and tests depend on the interface, not COM. The Windows
/// `UiaProvider` implements it; `FakeProvider` fakes it for object-model tests.
#ifndef VOX_PROVIDER_IPROVIDER_HPP
#define VOX_PROVIDER_IPROVIDER_HPP

#include <functional>
#include <optional>

#include <vox/model/accessible_node.hpp>

namespace vox::provider {

/// Interface to the accessibility provider for the focused UI element.
class IProvider {
public:
  /// Invoked with the newly focused element when focus changes (while started).
  using FocusChangedCallback = std::function<void(const vox::model::AccessibleNode&)>;

  IProvider() = default;
  IProvider(const IProvider&) = delete;
  IProvider& operator=(const IProvider&) = delete;
  IProvider(IProvider&&) = delete;
  IProvider& operator=(IProvider&&) = delete;
  virtual ~IProvider() = default;

  /// @brief The currently focused element, or `std::nullopt` if none/unreadable.
  [[nodiscard]] virtual std::optional<vox::model::AccessibleNode> focusedElement() const = 0;

  /// @brief Begins delivering focus-change notifications to @p onFocusChanged.
  virtual void start(FocusChangedCallback onFocusChanged) = 0;

  /// @brief Stops delivering focus-change notifications.
  virtual void stop() = 0;
};

} // namespace vox::provider

#endif // VOX_PROVIDER_IPROVIDER_HPP
