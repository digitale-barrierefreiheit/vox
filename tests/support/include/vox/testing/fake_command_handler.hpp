// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A recording ICommandHandler for input-layer tests.
///
/// Test-support only. It records the commands the router emits so tests can
/// assert "this key produced this command". Intended for single-threaded test
/// use (like FakeAudioSink); a test that drives the real hook from another
/// thread should use a lock-free handler so it never contends with the
/// latency-critical hook callback.
#ifndef VOX_TESTING_FAKE_COMMAND_HANDLER_HPP
#define VOX_TESTING_FAKE_COMMAND_HANDLER_HPP

#include <cstddef>
#include <vector>

#include <vox/input/command.hpp>
#include <vox/input/command_handler.hpp>

namespace vox::testing {

/// An ICommandHandler that records every command it receives, in order.
class FakeCommandHandler : public vox::input::ICommandHandler {
public:
  void onCommand(vox::input::Command command) override {
    commands_.push_back(command);
  }

  /// @brief The commands received so far, in order.
  [[nodiscard]] const std::vector<vox::input::Command>& commands() const noexcept {
    return commands_;
  }

  /// @brief How many commands have been received.
  [[nodiscard]] std::size_t count() const noexcept {
    return commands_.size();
  }

private:
  std::vector<vox::input::Command> commands_;
};

} // namespace vox::testing

#endif // VOX_TESTING_FAKE_COMMAND_HANDLER_HPP
