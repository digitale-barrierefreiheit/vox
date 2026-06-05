// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A recording ICommandHandler for input-layer tests.
///
/// Test-support only. It records the commands the keyboard hook / router emits
/// so tests can assert "this key produced this command". Thread-safe, because
/// the real hook calls `onCommand` from its hook thread while the test inspects
/// from another (the integration test, #38).
#ifndef VOX_TESTING_FAKE_COMMAND_HANDLER_HPP
#define VOX_TESTING_FAKE_COMMAND_HANDLER_HPP

#include <cstddef>
#include <mutex>
#include <vector>

#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>

namespace vox::testing {

/// An ICommandHandler that records every command it receives.
class FakeCommandHandler : public vox::input::ICommandHandler {
public:
  void onCommand(vox::input::Command command) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back(command);
  }

  /// @brief A copy of the commands received so far, in order.
  [[nodiscard]] std::vector<vox::input::Command> commands() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return commands_;
  }

  /// @brief How many commands have been received.
  [[nodiscard]] std::size_t count() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return commands_.size();
  }

private:
  mutable std::mutex mutex_;
  std::vector<vox::input::Command> commands_;
};

} // namespace vox::testing

#endif // VOX_TESTING_FAKE_COMMAND_HANDLER_HPP
