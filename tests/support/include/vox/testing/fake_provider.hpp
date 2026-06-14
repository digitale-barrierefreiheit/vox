// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A scriptable in-memory IProvider for tests (no Windows, no COM).
///
/// Test-support only — this double is never part of a shipped library. It lets
/// object-model and pipeline tests (#37, #39, …) drive the provider seam
/// without a live UIA tree.
#ifndef VOX_TESTING_FAKE_PROVIDER_HPP
#define VOX_TESTING_FAKE_PROVIDER_HPP

#include <optional>
#include <utility>

#include <vox/model/accessible_node.hpp>
#include <vox/provider/iprovider.hpp>

namespace vox::testing {

/// An IProvider whose focused element and focus events are driven by tests.
class FakeProvider : public vox::provider::IProvider {
public:
  /// Sets what `focusedElement()` returns (without firing an event).
  void setFocusedElement(std::optional<vox::model::AccessibleNode> node) {
    focused_ = std::move(node);
  }

  [[nodiscard]] std::optional<vox::model::AccessibleNode> focusedElement() const override {
    return focused_;
  }

  void start(FocusChangedCallback onFocusChanged) override {
    callback_ = std::move(onFocusChanged);
    running_ = true;
  }

  void stop() override {
    running_ = false;
    callback_ = nullptr;
  }

  /// @brief Test hook: makes @p node the focused element and, if started,
  ///        delivers a focus-change notification for it.
  void simulateFocusChange(vox::model::AccessibleNode node) {
    focused_ = std::move(node);
    if (running_ && callback_) {
      callback_(focused_.value());
    }
  }

private:
  std::optional<vox::model::AccessibleNode> focused_;
  FocusChangedCallback callback_;
  bool running_{false};
};

} // namespace vox::testing

#endif // VOX_TESTING_FAKE_PROVIDER_HPP
