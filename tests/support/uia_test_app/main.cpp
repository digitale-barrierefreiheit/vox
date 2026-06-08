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

#include <array>

namespace {

constexpr const wchar_t* WindowClassName = L"VoxUiaTestAppWindow";
constexpr const wchar_t* WindowTitle = L"Vox UIA Test App";
constexpr int EventNameBufferChars = 256;

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

// Signals the launcher's ready event once startup is done — the window is created and
// foreground + focus have been requested (whether focus actually lands is what the
// launcher polls for, since SetForegroundWindow/SetFocus can be refused). The event
// name arrives as a narrow argv (the CRT decoded it from the wide command line via the
// active code page), so decode it back with CP_ACP and open the Unicode-named event the
// launcher created with CreateEventW. An un-openable event is not fatal.
void signalReady(const char* eventName) {
  std::array<wchar_t, EventNameBufferChars> wide{};
  const int written =
      ::MultiByteToWideChar(CP_ACP, 0, eventName, -1, wide.data(), EventNameBufferChars);
  if (written <= 0) {
    return; // conversion failed or the name did not fit the buffer
  }
  if (HANDLE ready = ::OpenEventW(EVENT_MODIFY_STATE, FALSE, wide.data())) {
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
