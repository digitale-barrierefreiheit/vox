// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

#if defined(_WIN32)

#  include <atomic>
#  include <cstdint>
#  include <future>
#  include <stdexcept>
#  include <string>
#  include <thread>

#  include <vox/input/command_handler.hpp>
#  include <vox/input/command_map.hpp>
#  include <vox/input/key_event.hpp>
#  include <vox/input/keyboard_hook.hpp>

#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>

namespace vox::input {

namespace {

/// Reads the live modifier state at the moment a key event is processed. In a
/// low-level hook `GetAsyncKeyState` reflects the current physical key state, so
/// modifiers pressed before this key are already visible.
KeyModifiers currentModifiers() {
  KeyModifiers modifiers = KeyModifiers::None;
  const auto held = [](int virtualKey) {
    return (static_cast<unsigned int>(::GetAsyncKeyState(virtualKey)) & 0x8000U) != 0U;
  };
  if (held(VK_SHIFT)) {
    modifiers |= KeyModifiers::Shift;
  }
  if (held(VK_CONTROL)) {
    modifiers |= KeyModifiers::Control;
  }
  if (held(VK_MENU)) {
    modifiers |= KeyModifiers::Alt;
  }
  if (held(VK_LWIN) || held(VK_RWIN)) {
    modifiers |= KeyModifiers::Win;
  }
  return modifiers;
}

} // namespace

class KeyboardHook::Impl {
public:
  Impl(ICommandHandler& handler, CommandMap map) : handler_(handler), map_(map) {}

  void start() {
    if (running_) {
      throw std::runtime_error("KeyboardHook: already started");
    }
    // A WH_KEYBOARD_LL callback runs on the thread that installed it, and that
    // thread must pump messages — so install and pump on a dedicated thread, and
    // wait until it has installed (or failed) before returning.
    std::promise<void> ready;
    std::future<void> readyFuture = ready.get_future();
    error_.clear();
    thread_ = std::thread([this, &ready] { run(ready); });
    readyFuture.wait();
    if (!error_.empty()) {
      thread_.join();
      throw std::runtime_error(error_);
    }
    running_ = true;
  }

  void stop() {
    if (!thread_.joinable()) {
      return;
    }
    ::PostThreadMessageW(threadId_.load(std::memory_order_acquire), WM_QUIT, 0, 0);
    thread_.join();
    running_ = false;
  }

private:
  void run(std::promise<void>& ready) {
    threadId_.store(::GetCurrentThreadId(), std::memory_order_release);
    // Force the message queue to exist before we signal ready, so a stop() that
    // races right after start() can always post WM_QUIT to it.
    MSG queuePrimer{};
    ::PeekMessageW(&queuePrimer, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    Impl* expected = nullptr;
    if (!active_.compare_exchange_strong(expected, this, std::memory_order_acq_rel)) {
      error_ = "KeyboardHook: another keyboard hook is already active";
      ready.set_value();
      return;
    }
    hook_ = ::SetWindowsHookExW(WH_KEYBOARD_LL, &Impl::hookProc, ::GetModuleHandleW(nullptr), 0);
    if (hook_ == nullptr) {
      active_.store(nullptr, std::memory_order_release);
      error_ = "KeyboardHook: SetWindowsHookEx failed";
      ready.set_value();
      return;
    }
    ready.set_value();

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }

    ::UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
    active_.store(nullptr, std::memory_order_release);
  }

  static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    Impl* self = active_.load(std::memory_order_acquire);
    if (code == HC_ACTION && self != nullptr) {
      const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
      const bool pressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
      const KeyEvent event{.virtualKey = static_cast<std::uint32_t>(info->vkCode),
                           .modifiers = currentModifiers(),
                           .pressed = pressed};
      if (routeKeyEvent(event, self->map_, self->handler_)) {
        return 1; // consumed (reader-control key) — hide it from the focused app
      }
    }
    return ::CallNextHookEx(nullptr, code, wParam, lParam);
  }

  ICommandHandler& handler_;
  CommandMap map_;
  std::thread thread_;
  std::atomic<DWORD> threadId_{0};
  std::string error_;
  bool running_{false};
  HHOOK hook_{nullptr};

  // A WH_KEYBOARD_LL callback gets no user pointer, so the active hook publishes
  // itself here for hookProc. The compare-exchange in run() enforces one at a time.
  static inline std::atomic<Impl*> active_{nullptr};
};

KeyboardHook::KeyboardHook(ICommandHandler& handler, CommandMap map)
    : impl_(std::make_unique<Impl>(handler, map)) {}

KeyboardHook::~KeyboardHook() {
  impl_->stop();
}

void KeyboardHook::start() {
  impl_->start();
}

void KeyboardHook::stop() {
  impl_->stop();
}

} // namespace vox::input

#endif // defined(_WIN32)
