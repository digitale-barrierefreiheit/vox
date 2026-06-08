// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Digitale Barrierefreiheit e.V. and the Vox contributors

/// @file
/// @brief A tiny Win32 app with a known control tree, for the UIA provider's
///        integration tests (#40).
///
/// Dry-run scope: it exposes a single labelled push button, gives it the
/// keyboard focus, then signals the launching test via a named "ready" event
/// (whose name it receives as `argv[1]`) and pumps messages until terminated.
/// Standard common controls expose UI Automation through the system's default
/// provider, so this app writes no UIA code of its own. The full known tree
/// (one control per mapped Role) is built out once CI confirms focus works.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

constexpr const wchar_t* WindowClassName = L"VoxUiaTestAppWindow";
constexpr const wchar_t* WindowTitle = L"Vox UIA Test App";

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_DESTROY) {
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProcW(window, message, wParam, lParam);
}

HWND createButton(HWND parent, const wchar_t* text) {
  return ::CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                           10, 10, 160, 30, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
}

// Sets the named ready event so the launching test knows the window is up and
// focused. A missing/un-openable event is not fatal (the app is also useful by hand).
void signalReady(const char* eventName) {
  if (HANDLE ready = ::OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName)) {
    ::SetEvent(ready);
    ::CloseHandle(ready);
  }
}

} // namespace

int main(int argc, char** argv) {
  HINSTANCE instance = ::GetModuleHandleW(nullptr);

  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = &windowProc;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = WindowClassName;
  if (::RegisterClassExW(&windowClass) == 0) {
    return 1;
  }

  HWND window =
      ::CreateWindowExW(0, WindowClassName, WindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 400, 200, nullptr, nullptr, instance, nullptr);
  if (window == nullptr) {
    return 1;
  }

  HWND saveButton = createButton(window, L"Speichern");
  if (saveButton == nullptr) {
    return 1;
  }

  ::ShowWindow(window, SW_SHOWNORMAL);
  ::SetForegroundWindow(window);
  ::SetFocus(saveButton);

  if (argc > 1) {
    signalReady(argv[1]);
  }

  MSG message{};
  while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&message);
    ::DispatchMessageW(&message);
  }
  return 0;
}
