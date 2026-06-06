// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

#if defined(_WIN32)

#  include <array>
#  include <atomic>
#  include <cstddef>
#  include <cstdint>
#  include <format>
#  include <future>
#  include <stdexcept>
#  include <string>
#  include <thread>
#  include <utility>

#  include <vox/input/command_handler.hpp>
#  include <vox/input/command_map.hpp>
#  include <vox/input/errors.hpp>
#  include <vox/input/key_event.hpp>
#  include <vox/input/keyboard_hook.hpp>

#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <Windows.h>

namespace vox::input {

namespace {

/// Reads the live modifier state at the moment a key event is processed. In a
/// low-level hook `GetAsyncKeyState` reflects the current physical key state, so
/// modifiers pressed before this key are already visible.
KeyModifiers currentModifiers() {
  const auto held = [](int virtualKey) {
    return (static_cast<unsigned int>(::GetAsyncKeyState(virtualKey)) & 0x8000U) != 0U;
  };
  return KeyModifiers{.shift = held(VK_SHIFT),
                      .control = held(VK_CONTROL),
                      .alt = held(VK_MENU),
                      .win = held(VK_LWIN) || held(VK_RWIN)};
}

} // namespace

class KeyboardHook::Impl {
public:
  Impl(ICommandHandler& handler, CommandMap map) : handler_(handler), map_(std::move(map)) {}

  void start() {
    if (running_) {
      throw HookError("KeyboardHook: already started");
    }
    // A WH_KEYBOARD_LL callback runs on the thread that installed it, and that
    // thread must pump messages — so install and pump on a dedicated thread, and
    // wait until it has installed (or failed) before returning.
    std::promise<void> ready;
    std::future<void> readyFuture = ready.get_future();
    error_.clear();
    try {
      thread_ = std::thread([this, &ready] { run(ready); });
    } catch (...) {
      // Translate a std::thread failure (e.g. std::system_error) so start()
      // only ever throws HookError, as documented.
      throw HookError("KeyboardHook: failed to create the hook thread");
    }
    readyFuture.wait();
    if (!error_.empty()) {
      thread_.join();
      // `error_` already embeds the GetLastError() code for the hook-install
      // failure (see run()); carry it as a HookError for catch-by-subsystem.
      throw HookError(error_);
    }
    running_ = true;
  }

  void stop() {
    if (!thread_.joinable()) {
      return;
    }
    ::PostThreadMessageW(threadId_.load(std::memory_order_acquire), WM_QUIT, 0, 0);
    if (std::this_thread::get_id() == thread_.get_id()) {
      // Called from the hook thread itself (e.g. a Quit handler triggering
      // shutdown): joining here would self-deadlock. The loop exits on the
      // posted WM_QUIT; a later stop() from another thread (the destructor) joins.
      return;
    }
    thread_.join();
    running_ = false;
  }

private:
  // The std::thread entrypoint. noexcept + catch-all so an exception can never
  // escape the thread (which would terminate the process), and the ready promise
  // is always fulfilled so start() is never left blocked.
  void run(std::promise<void>& ready) noexcept {
    bool readySignaled = false;
    const auto signalReady = [&ready, &readySignaled] {
      if (!readySignaled) {
        readySignaled = true;
        ready.set_value();
      }
    };
    try {
      threadId_.store(::GetCurrentThreadId(), std::memory_order_release);
      // Force the message queue to exist before we signal ready, so a stop()
      // that races right after start() can always post WM_QUIT to it.
      MSG queuePrimer{};
      ::PeekMessageW(&queuePrimer, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

      if (Impl* expected = nullptr;
          !active_.compare_exchange_strong(expected, this, std::memory_order_acq_rel)) {
        error_ = "KeyboardHook: another keyboard hook is already active";
        signalReady();
        return; // not ours — leave active_ and skip teardown below
      }
      hook_ = ::SetWindowsHookExW(WH_KEYBOARD_LL, &Impl::hookProc, ::GetModuleHandleW(nullptr), 0);
      if (hook_ == nullptr) {
        error_ = std::format("KeyboardHook: SetWindowsHookEx failed (error {})", ::GetLastError());
      }
      signalReady();

      if (hook_ != nullptr) {
        MSG message{};
        while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
          ::TranslateMessage(&message);
          ::DispatchMessageW(&message);
        }
      }
    } catch (...) {
      if (error_.empty()) {
        error_ = "KeyboardHook: the hook thread failed";
      }
      signalReady();
    }
    // Tear down only what this thread owns (active_ == this only after our CAS).
    if (hook_ != nullptr) {
      ::UnhookWindowsHookEx(hook_);
      hook_ = nullptr;
    }
    Impl* owned = this;
    active_.compare_exchange_strong(owned, nullptr, std::memory_order_acq_rel);
  }

  static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (Impl* self = active_.load(std::memory_order_acquire);
        code == HC_ACTION && self != nullptr) {
      const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
      const bool pressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
      const std::size_t vk = info->vkCode & 0xFFU; // vkCode is 1..254
      if (pressed) {
        if (self->consumed_[vk]) {
          return 1; // auto-repeat of a key we are consuming: swallow, do not re-dispatch
        }
        const KeyEvent event{.virtualKey = static_cast<std::uint32_t>(info->vkCode),
                             .modifiers = currentModifiers(),
                             .pressed = true,
                             .injected = (info->flags & LLKHF_INJECTED) != 0U};
        bool consume = false;
        try {
          consume = routeKeyEvent(event, self->map_, self->handler_);
        } catch (...) {
          consume = false; // a throwing handler must not cross the Win32 boundary
        }
        if (consume) {
          self->consumed_[vk] = true; // remember so we also swallow this key's key-up
          return 1;                   // consumed (reader-control key) — hide it from the app
        }
      } else if (self->consumed_[vk]) {
        self->consumed_[vk] = false;
        return 1; // swallow the key-up of a key whose key-down we consumed
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

  // Virtual keys whose key-down we consumed, so we also swallow their key-up and
  // ignore auto-repeat. Touched only on the (single) hook thread — no locking.
  std::array<bool, 256> consumed_{};

  // A WH_KEYBOARD_LL callback gets no user pointer, so the active hook publishes
  // itself here for hookProc. The compare-exchange in run() enforces one at a time.
  static inline std::atomic<Impl*> active_{nullptr};
};

KeyboardHook::KeyboardHook(ICommandHandler& handler, CommandMap map)
    : impl_(std::make_unique<Impl>(handler, std::move(map))) {}

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
